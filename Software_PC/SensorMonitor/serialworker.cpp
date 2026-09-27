#include "serialworker.h"

/* ==========================================================================
 * 协议常量：12 字节统一帧（与 app_tasks.c 的 BuildFrame 严格对应）
 *
 *  [0]  0xAA   帧头高
 *  [1]  0x55   帧头低
 *  [2]  TYPE   0x04=温湿度上报  0xFF=紧急报警  0xFE=延迟诊断
 *  [3]  SEQ    帧序号，每帧 +1，用于丢帧检测（quint8 自然回绕）
 *  [4..8]      5 字节载荷（按 TYPE 解释）
 *  [9]  CRC_H  CRC16 校验 [0..8]，【高字节在前】（与下位机约定一致）
 *  [10] CRC_L
 *  [11] 0x5D   帧尾
 *
 *  TYPE=0x04 载荷：[4][5]=温度 int16 ×100（大端）  [6][7]=湿度 uint16 ×100（大端）  [8]=状态码
 *  TYPE=0xFF 载荷：[4]=报警源  [5..8]=设备 Tick 时间戳（小端，下位机 memcpy(uint32) 所致）
 *  TYPE=0xFE 载荷：[4..7]=最坏延迟时钟周期数（小端）  [8]=0
 * ========================================================================== */
static const int     FRAME_LEN   = 12;
static const uint8_t FRAME_TAIL  = 0x5D;
static const uint8_t TYPE_DATA   = 0x04;
static const uint8_t TYPE_ALARM  = 0xFF;
static const uint8_t TYPE_DIAG   = 0xFE;
static const QByteArray FRAME_HEADER("\xAA\x55", 2);

SerialWorker::SerialWorker(QObject *parent)
    : QObject(parent)
    , serial(new QSerialPort(this))
    , lastSeq(0)
    , seqInitialized(false)
{
    // 注意：QSerialPort 必须在子线程内部实例化，才能将收发事件绑定到子线程的事件循环中
    connect(serial, &QSerialPort::readyRead, this, &SerialWorker::readData);
}

SerialWorker::~SerialWorker()
{
    if (serial->isOpen()) {
        serial->close();
    }
}

void SerialWorker::openPort(const QString &portName, int baudRate)
{
    if (serial->isOpen()) serial->close();

    serial->setPortName(portName);
    serial->setBaudRate(baudRate);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);

    // 新会话开始，SEQ 连续性检测重新初始化
    seqInitialized = false;

    if (serial->open(QIODevice::ReadWrite)) {
        emit portOpenedStatus(true, "");
    } else {
        emit portOpenedStatus(false, serial->errorString());
    }
}

void SerialWorker::closePort()
{
    if (serial->isOpen()) {
        serial->close();
    }
}

void SerialWorker::sendCommand(char cmd)
{
    if (serial->isOpen()) {
        serial->write(&cmd, 1);
        serial->flush();
    }
}

void SerialWorker::readData()
{
    buffer.append(serial->readAll());

    while (buffer.size() >= FRAME_LEN)
    {
        int headerIndex = buffer.indexOf(FRAME_HEADER);
        if (headerIndex == -1) {
            // 没找到帧头：只保留最后 1 字节。
            // 它可能是被串口分包截断的半个帧头（如 0xAA），留给下一包数据拼接
            buffer.remove(0, buffer.size() - 1);
            break;
        }
        if (headerIndex > 0) buffer.remove(0, headerIndex);
        if (buffer.size() < FRAME_LEN) break;

        // 走到这里，buffer 一定以 AA 55 开头且长度足够
        bool tailOk = (static_cast<uint8_t>(buffer[11]) == FRAME_TAIL);
        uint16_t crc_calc = calculateCRC16(buffer, 9);
        uint16_t crc_recv = (static_cast<uint8_t>(buffer[9]) << 8)
                            |  static_cast<uint8_t>(buffer[10]);

        if (!tailOk || crc_calc != crc_recv) {
            // 校验失败：只丢掉这 1 个假帧头字节，继续向后搜索。
            // 不整帧丢弃，避免误吞残帧内部恰好出现的真帧头
            buffer.remove(0, 1);
            continue;
        }

        uint8_t frameType = static_cast<uint8_t>(buffer[2]);
        uint8_t seq       = static_cast<uint8_t>(buffer[3]);

        // SEQ 连续性检测：quint8 减法天然处理 256 回绕
        if (seqInitialized) {
            quint8 diff = static_cast<quint8>(seq - lastSeq);
            if (diff > 1) emit frameLossDetected(diff - 1);
        }
        lastSeq = seq;
        seqInitialized = true;

        if (frameType == TYPE_DATA) {
            int16_t  rawTemp = static_cast<int16_t>(
                (static_cast<uint8_t>(buffer[4]) << 8) | static_cast<uint8_t>(buffer[5]));
            uint16_t rawHum =
                (static_cast<uint8_t>(buffer[6]) << 8) | static_cast<uint8_t>(buffer[7]);

            // 【核心解耦点】：不直接操作 UI，通过信号将干净的数据发给主线程
            emit dataParsed(rawTemp / 100.0, rawHum / 100.0);
        }
        else if (frameType == TYPE_ALARM) {
            quint32 source = static_cast<uint8_t>(buffer[4]);
            quint32 tick   = static_cast<uint8_t>(buffer[5])
                           | (static_cast<uint8_t>(buffer[6]) << 8)
                           | (static_cast<quint32>(static_cast<uint8_t>(buffer[7])) << 16)
                           | (static_cast<quint32>(static_cast<uint8_t>(buffer[8])) << 24);
            emit alarmTriggered(source, tick);
        }
        else if (frameType == TYPE_DIAG) {
            quint32 cycles = static_cast<uint8_t>(buffer[4])
            | (static_cast<uint8_t>(buffer[5]) << 8)
                | (static_cast<quint32>(static_cast<uint8_t>(buffer[6])) << 16)
                | (static_cast<quint32>(static_cast<uint8_t>(buffer[7])) << 24);
            emit latencyReported(cycles);
        }
        // 未知 TYPE：帧完整且校验通过，静默丢弃即可

        // 校验通过才消费整帧
        buffer.remove(0, FRAME_LEN);
    }
}

uint16_t SerialWorker::calculateCRC16(const QByteArray &data, int len)
{
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < len; ++pos) {
        crc ^= (uint8_t)data[pos];
        for (int i = 8; i != 0; --i) {
            if ((crc & 0x0001) != 0) { crc >>= 1; crc ^= 0xA001; }
            else { crc >>= 1; }
        }
    }
    return crc;
}

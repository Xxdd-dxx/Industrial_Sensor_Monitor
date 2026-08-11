#include "serialworker.h"

SerialWorker::SerialWorker(QObject *parent) : QObject(parent)
{
    // 注意：QSerialPort 必须在子线程内部实例化，才能将收发事件绑定到子线程的事件循环中
    serial = new QSerialPort(this);
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

    while (buffer.size() >= 10) {
        int headerIndex = buffer.indexOf("\xAA\x55");
        if (headerIndex == -1) { buffer.clear(); break; }
        if (headerIndex > 0) buffer.remove(0, headerIndex);
        if (buffer.size() < 10) break;

        QByteArray frame = buffer.left(10);
        buffer.remove(0, 10);

        if (static_cast<uint8_t>(frame[9]) != 0x5D) continue;

        uint16_t crc_calc = calculateCRC16(frame, 7);
        uint16_t crc_recv = (static_cast<uint8_t>(frame[7]) << 8) | static_cast<uint8_t>(frame[8]);

        if (crc_calc == crc_recv) {
            uint8_t frameType = static_cast<uint8_t>(frame[2]);
            if (frameType == 0x04) {
                int16_t rawTemp = (static_cast<uint8_t>(frame[3]) << 8) | static_cast<uint8_t>(frame[4]);
                uint16_t rawHum = (static_cast<uint8_t>(frame[5]) << 8) | static_cast<uint8_t>(frame[6]);

                // 【核心解耦点】：不再直接操作 UI，而是通过信号将干净的数据发给主线程
                emit dataParsed(rawTemp / 100.0, rawHum / 100.0);
            } else if (frameType == 0xFF) {
                emit alarmTriggered();
            }
        }
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
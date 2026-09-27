#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QByteArray>

class SerialWorker : public QObject
{
    Q_OBJECT
public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker();

public slots:
    // 供主线程调用的控制接口
    void openPort(const QString &portName, int baudRate);
    void closePort();
    void sendCommand(char cmd);

private slots:
    // 独立在子线程中运行的接收逻辑
    void readData();

signals:
    // 向上层界面汇报状态的信号
    void portOpenedStatus(bool success, const QString &errorMsg);
    void dataParsed(double temp, double hum);
    void alarmTriggered(quint32 source, quint32 tick);   // 改：带上报警源与设备时间戳
    void latencyReported(quint32 maxCycles);             // 新：DWT 最坏延迟诊断帧
    void frameLossDetected(int lostCount);               // 新：基于 SEQ 的丢帧检测

private:
    QSerialPort *serial;
    QByteArray buffer;

    // SEQ 连续性检测
    quint8 lastSeq;
    bool   seqInitialized;

    // 将耗时的校验算法也封装在后台
    uint16_t calculateCRC16(const QByteArray &data, int len);
};

#endif // SERIALWORKER_H

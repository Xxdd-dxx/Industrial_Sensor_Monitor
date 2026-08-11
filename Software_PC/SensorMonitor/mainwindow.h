#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPortInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QMessageBox>
#include <QTimer>
#include <QStringList>
#include <QPushButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QThread>
#include <QList>          // 新增：列表容器头文件

#include "serialworker.h"

#include <QtCharts>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
QT_CHARTS_USE_NAMESPACE
#endif

    QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ==========================================
// 新增：用于在内存中缓存数据的结构体
// ==========================================
struct SensorRecord {
    QString time;
    double temp;
    double hum;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnOpen_clicked();
    void scanPorts();
    void sendLedOn();
    void sendLedOff();

    void onPortOpenedStatus(bool success, const QString &errorMsg);
    void onDataParsed(double temp, double hum);
    void onAlarmTriggered();

signals:
    void reqOpenPort(const QString &portName, int baudRate);
    void reqClosePort();
    void reqSendCommand(char cmd);

private:
    Ui::MainWindow *ui;

    QThread *workerThread;
    SerialWorker *serialWorker;
    bool isSerialOpen;

    // 图表与数据库组件
    QSqlDatabase db;
    QTimer *portTimer;
    QStringList currentPortList;

    QCheckBox *cbAutoAlarm;
    QDoubleSpinBox *spinTempThresh;
    QDoubleSpinBox *spinHumThresh;
    bool isAlarmActive;

    QChart *chart;
    QLineSeries *tempSeries;
    QLineSeries *humSeries;
    QValueAxis *axisX;
    QValueAxis *axisY;
    int timeCount;

    // ==========================================
    // 新增：数据库缓存队列与批量写入函数
    // ==========================================
    QList<SensorRecord> dbCache;
    void flushDatabaseCache();

    void initUI();
    void initDatabase();
    void initChart();
};

#endif // MAINWINDOW_H
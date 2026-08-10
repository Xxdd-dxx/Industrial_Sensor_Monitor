#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>         // 新增：定时器头文件
#include <QStringList>    // 新增：字符串列表头文件

// QtCharts 宏命名空间
#include <QtCharts>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
QT_CHARTS_USE_NAMESPACE
#endif

    QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnOpen_clicked();
    void readSerialData();
    void scanPorts(); // 新增：用于定时扫描串口列表的槽函数

private:
    Ui::MainWindow *ui;

    // 核心组件
    QSerialPort *serial;
    QSqlDatabase db;
    QTimer *portTimer;           // 新增：扫描定时器
    QStringList currentPortList; // 新增：记录当前存在的串口号列表

    // 图表组件
    QChart *chart;
    QLineSeries *tempSeries;
    QLineSeries *humSeries;
    QValueAxis *axisX;
    QValueAxis *axisY;
    int timeCount;

    // 数据缓存
    QByteArray buffer;

    // 初始化函数
    void initUI();
    void initDatabase();
    void initChart();

    // 工业级协议解析与校验
    void parseData(const QByteArray &frame);
    uint16_t calculateCRC16(const QByteArray &data, int len);
};

#endif // MAINWINDOW_H
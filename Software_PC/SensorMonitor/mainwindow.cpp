#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isSerialOpen(false)
    , portTimer(new QTimer(this))
    , isAlarmActive(false)
    , timeCount(0)
{
    ui->setupUi(this);
    ui->splitter->setSizes(QList<int>() << 280 << 800);

    // QSS 样式表已被解耦到 main.cpp 和 style.qss 中

    initUI();
    initChart();
    initDatabase();

    workerThread = new QThread(this);
    serialWorker = new SerialWorker();
    serialWorker->moveToThread(workerThread);

    connect(workerThread, &QThread::finished, serialWorker, &QObject::deleteLater);
    connect(this, &MainWindow::reqOpenPort, serialWorker, &SerialWorker::openPort);
    connect(this, &MainWindow::reqClosePort, serialWorker, &SerialWorker::closePort);
    connect(this, &MainWindow::reqSendCommand, serialWorker, &SerialWorker::sendCommand);

    connect(serialWorker, &SerialWorker::portOpenedStatus, this, &MainWindow::onPortOpenedStatus);
    connect(serialWorker, &SerialWorker::dataParsed, this, &MainWindow::onDataParsed);
    connect(serialWorker, &SerialWorker::alarmTriggered, this, &MainWindow::onAlarmTriggered);

    workerThread->start();

    connect(portTimer, &QTimer::timeout, this, &MainWindow::scanPorts);
    portTimer->start(1000);
}

MainWindow::~MainWindow()
{
    flushDatabaseCache();

    emit reqClosePort();
    workerThread->quit();
    workerThread->wait();
    delete ui;
}

void MainWindow::initUI()
{
    scanPorts();
    ui->cbBaud->setCurrentText("115200");

    QGroupBox *gbThreshold = new QGroupBox("自动阈值巡航", this);
    QVBoxLayout *threshLayout = new QVBoxLayout(gbThreshold);
    threshLayout->setSpacing(10);

    cbAutoAlarm = new QCheckBox("启用自动报警联动", this);
    cbAutoAlarm->setStyleSheet("QCheckBox { color: #4facfe; font-weight: bold; font-size: 15px; margin-bottom: 5px;}");

    QHBoxLayout *tempLayout = new QHBoxLayout();
    QLabel *lblTempThresh = new QLabel("温度上限 (℃):", this);
    spinTempThresh = new QDoubleSpinBox(this);
    spinTempThresh->setRange(-40.0, 120.0);
    spinTempThresh->setValue(35.0);
    spinTempThresh->setStyleSheet("QDoubleSpinBox { background-color: #2d2d30; color: #ff5252; border: 1px solid #4a4a4a; padding: 3px; font-size: 15px; font-weight:bold;}");
    tempLayout->addWidget(lblTempThresh);
    tempLayout->addWidget(spinTempThresh);

    QHBoxLayout *humLayout = new QHBoxLayout();
    QLabel *lblHumThresh = new QLabel("湿度上限 (%):", this);
    spinHumThresh = new QDoubleSpinBox(this);
    spinHumThresh->setRange(0.0, 100.0);
    spinHumThresh->setValue(80.0);
    spinHumThresh->setStyleSheet("QDoubleSpinBox { background-color: #2d2d30; color: #00e5ff; border: 1px solid #4a4a4a; padding: 3px; font-size: 15px; font-weight:bold;}");
    humLayout->addWidget(lblHumThresh);
    humLayout->addWidget(spinHumThresh);

    threshLayout->addWidget(cbAutoAlarm);
    threshLayout->addLayout(tempLayout);
    threshLayout->addLayout(humLayout);

    QPushButton *btnLedOn = new QPushButton("强制开启远端警报", this);
    QPushButton *btnLedOff = new QPushButton("强制关闭远端警报", this);
    btnLedOn->setStyleSheet("background-color: #d32f2f; margin-top: 10px;");
    btnLedOff->setStyleSheet("background-color: #388e3c;");

    QVBoxLayout *controlLayout = qobject_cast<QVBoxLayout*>(ui->gbControl->layout());
    if(controlLayout) {
        controlLayout->insertWidget(controlLayout->count() - 1, gbThreshold);
        controlLayout->insertWidget(controlLayout->count() - 1, btnLedOn);
        controlLayout->insertWidget(controlLayout->count() - 1, btnLedOff);
    }

    connect(btnLedOn, &QPushButton::clicked, this, &MainWindow::sendLedOn);
    connect(btnLedOff, &QPushButton::clicked, this, &MainWindow::sendLedOff);

    connect(cbAutoAlarm, &QCheckBox::stateChanged, this, [this](int state){
        if (state == Qt::Unchecked && isAlarmActive) {
            isAlarmActive = false;
            sendLedOff();
            ui->lblTemp->setStyleSheet("");
            ui->lblHum->setStyleSheet("");
        }
    });
}

void MainWindow::scanPorts()
{
    QStringList newPortList;
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        newPortList << info.portName();
    }
    if (newPortList == currentPortList) return;

    currentPortList = newPortList;
    QString currentSelection = ui->cbPort->currentData().toString();
    bool isCurrentSelectionStillExist = false;

    ui->cbPort->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        QString displayText = QString("%1 #%2").arg(info.portName()).arg(info.description());
        ui->cbPort->addItem(displayText, info.portName());
        if (info.portName() == currentSelection) isCurrentSelectionStillExist = true;
    }

    if (isCurrentSelectionStillExist) {
        int index = ui->cbPort->findData(currentSelection);
        if (index != -1) ui->cbPort->setCurrentIndex(index);
    } else {
        if (isSerialOpen) {
            emit reqClosePort();
            isSerialOpen = false;
            ui->btnOpen->setText("打开串口");
            ui->btnOpen->setStyleSheet("background-color: #007acc;");
            ui->cbPort->setEnabled(true);
            ui->cbBaud->setEnabled(true);
            QMessageBox::warning(this, "设备断开", "当前通信的串口设备已被拔出！连接已断开。");
        }
    }
}

void MainWindow::initChart()
{
    chart = new QChart();
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);
    chart->setTheme(QChart::ChartThemeDark);
    chart->setBackgroundBrush(QBrush(QColor("#1e1e1e")));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor("#2d2d30")));
    chart->setPlotAreaBackgroundVisible(true);

    QFont titleFont = chart->titleFont();
    titleFont.setBold(true);
    titleFont.setPointSize(16);
    chart->setTitleFont(titleFont);
    chart->setTitleBrush(QBrush(QColor("#d4d4d4")));
    chart->setTitle("环境实时监控波形图");

    tempSeries = new QSplineSeries();
    tempSeries->setName("温度 (℃)");
    QPen tempPen(QColor("#ff5252"));
    tempPen.setWidth(2);
    tempSeries->setPen(tempPen);

    humSeries = new QSplineSeries();
    humSeries->setName("湿度 (%)");
    QPen humPen(QColor("#00e5ff"));
    humPen.setWidth(2);
    humSeries->setPen(humPen);

    chart->addSeries(tempSeries);
    chart->addSeries(humSeries);

    QFont axisFont;
    axisFont.setPointSize(11);

    axisX = new QValueAxis();
    axisX->setTitleText("数据点 (Data Points)");
    axisX->setRange(0, 60);
    axisX->setTickCount(7);
    axisX->setLabelFormat("%d");
    axisX->setGridLineColor(QColor("#4a4a4a"));
    axisX->setLabelsColor(QColor("#d4d4d4"));
    axisX->setTitleBrush(QBrush(QColor("#888888")));
    axisX->setLabelsFont(axisFont);
    chart->addAxis(axisX, Qt::AlignBottom);

    QValueAxis *axisY_Temp = new QValueAxis();
    axisY_Temp->setTitleText("温度 (℃)");
    axisY_Temp->setRange(0, 50);
    axisY_Temp->setTickCount(6);
    axisY_Temp->setGridLineColor(QColor("#4a4a4a"));
    axisY_Temp->setLabelsColor(QColor("#ff5252"));
    axisY_Temp->setTitleBrush(QBrush(QColor("#ff5252")));
    axisY_Temp->setLabelsFont(axisFont);
    chart->addAxis(axisY_Temp, Qt::AlignLeft);

    QValueAxis *axisY_Hum = new QValueAxis();
    axisY_Hum->setTitleText("湿度 (%)");
    axisY_Hum->setRange(0, 100);
    axisY_Hum->setTickCount(11);
    axisY_Hum->setGridLineVisible(false);
    axisY_Hum->setLabelsColor(QColor("#00e5ff"));
    axisY_Hum->setTitleBrush(QBrush(QColor("#00e5ff")));
    axisY_Hum->setLabelsFont(axisFont);
    chart->addAxis(axisY_Hum, Qt::AlignRight);

    tempSeries->attachAxis(axisX);
    tempSeries->attachAxis(axisY_Temp);
    humSeries->attachAxis(axisX);
    humSeries->attachAxis(axisY_Hum);

    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignTop);
    chart->legend()->setLabelColor(QColor("#d4d4d4"));
    QFont legendFont = chart->legend()->font();
    legendFont.setPointSize(12);
    chart->legend()->setFont(legendFont);

    ui->chartView->setChart(chart);
    ui->chartView->setRenderHint(QPainter::Antialiasing);
}

void MainWindow::initDatabase()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("sensor_data.db");
    if (!db.open()) return;

    QSqlQuery query;
    QString createTableSql = "CREATE TABLE IF NOT EXISTS history_data ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                             "timestamp DATETIME, "
                             "temperature REAL, "
                             "humidity REAL)";
    query.exec(createTableSql);
}

void MainWindow::on_btnOpen_clicked()
{
    if (isSerialOpen) {
        emit reqClosePort();
        isSerialOpen = false;
        ui->btnOpen->setText("打开串口");
        ui->btnOpen->setStyleSheet("background-color: #007acc;");
        ui->cbPort->setEnabled(true);
        ui->cbBaud->setEnabled(true);
    } else {
        emit reqOpenPort(ui->cbPort->currentData().toString(), ui->cbBaud->currentText().toInt());
    }
}

void MainWindow::onPortOpenedStatus(bool success, const QString &errorMsg)
{
    if (success) {
        isSerialOpen = true;
        ui->btnOpen->setText("关闭串口");
        ui->btnOpen->setStyleSheet("background-color: #d32f2f;");
        ui->cbPort->setEnabled(false);
        ui->cbBaud->setEnabled(false);
    } else {
        QMessageBox::warning(this, "错误", "无法打开串口：" + errorMsg);
    }
}

void MainWindow::sendLedOn()
{
    if (isSerialOpen) emit reqSendCommand(0x01);
}

void MainWindow::sendLedOff()
{
    if (isSerialOpen) emit reqSendCommand(0x00);
}

void MainWindow::onDataParsed(double temp, double hum)
{
    ui->lblTemp->setText(QString("T: %1 ℃").arg(temp, 0, 'f', 1));
    ui->lblHum->setText(QString("H: %1 %").arg(hum, 0, 'f', 1));

    if (cbAutoAlarm->isChecked()) {
        bool shouldAlarm = (temp >= spinTempThresh->value()) || (hum >= spinHumThresh->value());

        if (shouldAlarm && !isAlarmActive) {
            isAlarmActive = true;
            sendLedOn();
            ui->lblTemp->setStyleSheet("color: white; background-color: #d32f2f;");
            ui->lblHum->setStyleSheet("color: white; background-color: #d32f2f;");
        }
        else if (!shouldAlarm && isAlarmActive) {
            isAlarmActive = false;
            sendLedOff();
            ui->lblTemp->setStyleSheet("");
            ui->lblHum->setStyleSheet("");
        }
    }

    timeCount++;
    tempSeries->append(timeCount, temp);
    humSeries->append(timeCount, hum);

    if (tempSeries->count() > 60) {
        tempSeries->remove(0);
        humSeries->remove(0);
    }

    if (timeCount > 60) {
        axisX->setRange(timeCount - 60, timeCount);
    }

    SensorRecord record;
    record.time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    record.temp = temp;
    record.hum = hum;
    dbCache.append(record);

    if (dbCache.size() >= 10) {
        flushDatabaseCache();
    }
}

void MainWindow::flushDatabaseCache()
{
    if (dbCache.isEmpty() || !db.isOpen()) {
        return;
    }

    db.transaction();

    QSqlQuery query(db);
    query.prepare("INSERT INTO history_data (timestamp, temperature, humidity) VALUES (:time, :temp, :hum)");

    for (int i = 0; i < dbCache.size(); ++i) {
        query.bindValue(":time", dbCache[i].time);
        query.bindValue(":temp", dbCache[i].temp);
        query.bindValue(":hum", dbCache[i].hum);
        query.exec();
    }

    db.commit();
    dbCache.clear();
}

void MainWindow::onAlarmTriggered()
{
    QMessageBox::critical(this, "⚠️ 紧急报警", "收到下位机硬件触发的紧急报警信号！");
}
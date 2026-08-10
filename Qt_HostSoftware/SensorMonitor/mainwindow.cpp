#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serial(new QSerialPort(this))
    , portTimer(new QTimer(this))
    , timeCount(0)
{
    ui->setupUi(this);

    // 设置初始比例：左侧面板占280px，剩余给波形图
    ui->splitter->setSizes(QList<int>() << 280 << 800);

    // 修复后的工业级暗黑主题 (QSS) 包含 Splitter 拖动条样式
    // 修复后的工业级暗黑主题 (QSS) 包含 Splitter 拖动条样式，并去除白边
    this->setStyleSheet(
        "QMainWindow, QWidget#centralwidget { background-color: #1e1e1e; }" // 修复外围白边

        /* 拆分器(拖动条)样式设计 */
        "QSplitter::handle { background-color: transparent; width: 6px; }"
        "QSplitter::handle:horizontal { border-left: 1px dashed #4a4a4a; border-right: 1px dashed #4a4a4a; margin: 20px 1px; }"
        "QSplitter::handle:hover { background-color: #007acc; border: none; border-radius: 3px; }"
        "QSplitter::handle:pressed { background-color: #005c99; border: none; }"

        /* 分组框样式优化 */
        "QGroupBox { color: #4facfe; border: 1px solid #3a3a3a; border-radius: 5px; margin-top: 2ex; font-weight: bold; font-size: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 5px; left: 10px; }"

        /* 基础标签字体放大 */
        "QLabel { color: #d4d4d4; font-size: 16px; }"

        /* 针对温湿度大屏显示进行字体定制 */
        "QLabel#lblTemp { color: #ff5252; font-size: 32px; font-weight: bold; font-family: Consolas; }"
        "QLabel#lblHum { color: #00e5ff; font-size: 32px; font-weight: bold; font-family: Consolas; }"

        /* 下拉框样式优化与字体放大 */
        "QComboBox { background-color: #2d2d30; color: #ffffff; border: 1px solid #4a4a4a; padding: 5px 15px 5px 10px; border-radius: 3px; font-size: 16px; min-height: 25px; }"
        "QComboBox QAbstractItemView { background-color: #2d2d30; color: #ffffff; selection-background-color: #007acc; font-size: 16px; outline: none; }"

        /* 纯 CSS 绘制白色的下拉三角形图标 */
        "QComboBox::drop-down { border: none; width: 25px; }"
        "QComboBox::down-arrow { image: none; border-left: 5px solid transparent; border-right: 5px solid transparent; border-top: 5px solid #d4d4d4; margin-top: 2px; }"

        /* 按钮样式优化 */
        "QPushButton { background-color: #007acc; color: white; border: none; border-radius: 4px; font-weight: bold; font-size: 18px; }"
        "QPushButton:hover { background-color: #0098ff; }"
        "QPushButton:pressed { background-color: #005c99; }"
        "QPushButton:disabled { background-color: #3f3f46; color: #7a7a7a; }"
        );

    initUI();
    initChart();
    initDatabase();

    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readSerialData);

    connect(portTimer, &QTimer::timeout, this, &MainWindow::scanPorts);
    portTimer->start(1000);
}

MainWindow::~MainWindow()
{
    if (serial->isOpen()) {
        serial->close();
    }
    delete ui;
}

void MainWindow::initUI()
{
    scanPorts();
    ui->cbBaud->setCurrentText("115200");
}

void MainWindow::scanPorts()
{
    QStringList newPortList;
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        newPortList << info.portName();
    }

    if (newPortList == currentPortList) {
        return;
    }

    currentPortList = newPortList;
    QString currentSelection = ui->cbPort->currentData().toString();
    bool isCurrentSelectionStillExist = false;

    ui->cbPort->clear();

    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        QString displayText = QString("%1 #%2").arg(info.portName()).arg(info.description());
        ui->cbPort->addItem(displayText, info.portName());

        if (info.portName() == currentSelection) {
            isCurrentSelectionStillExist = true;
        }
    }

    if (isCurrentSelectionStillExist) {
        int index = ui->cbPort->findData(currentSelection);
        if (index != -1) {
            ui->cbPort->setCurrentIndex(index);
        }
    } else {
        if (serial->isOpen()) {
            serial->close();
            ui->btnOpen->setText("打开串口");
            ui->btnOpen->setStyleSheet("background-color: #007acc;"); // 恢复蓝色按钮
            ui->cbPort->setEnabled(true);
            ui->cbBaud->setEnabled(true);
            QMessageBox::warning(this, "设备断开", "当前通信的串口设备已被拔出！连接已断开。");
        }
    }
}

void MainWindow::initChart()
{
    chart = new QChart();

    // 1. 去除图表自带的内外边距，让图表撑满整个区域
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);

    // 配置图表工业级暗黑主题
    chart->setTheme(QChart::ChartThemeDark);
    chart->setBackgroundBrush(QBrush(QColor("#1e1e1e")));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor("#2d2d30")));
    chart->setPlotAreaBackgroundVisible(true);

    // 修改标题字体
    QFont titleFont = chart->titleFont();
    titleFont.setBold(true);
    titleFont.setPointSize(16);
    chart->setTitleFont(titleFont);
    chart->setTitleBrush(QBrush(QColor("#d4d4d4")));
    chart->setTitle("环境实时监控波形图");

    // 2. 升级为 QSplineSeries (平滑曲线)
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

    // 3. X轴设置 (增加网格密度)
    axisX = new QValueAxis();
    axisX->setTitleText("数据点 (Data Points)");
    axisX->setRange(0, 60);
    axisX->setTickCount(7); // 显示 0, 10, 20, 30, 40, 50, 60 共7个大刻度
    axisX->setLabelFormat("%d");
    axisX->setGridLineColor(QColor("#4a4a4a"));
    axisX->setLabelsColor(QColor("#d4d4d4"));
    axisX->setTitleBrush(QBrush(QColor("#888888")));
    axisX->setLabelsFont(axisFont);
    chart->addAxis(axisX, Qt::AlignBottom);

    // 4. 左侧 Y 轴：专门用于温度 (量程 0~50℃)
    QValueAxis *axisY_Temp = new QValueAxis();
    axisY_Temp->setTitleText("温度 (℃)");
    axisY_Temp->setRange(0, 50);
    axisY_Temp->setTickCount(6); // 显示 0, 10, 20, 30, 40, 50
    axisY_Temp->setGridLineColor(QColor("#4a4a4a"));
    axisY_Temp->setLabelsColor(QColor("#ff5252")); // 刻度颜色与曲线一致
    axisY_Temp->setTitleBrush(QBrush(QColor("#ff5252")));
    axisY_Temp->setLabelsFont(axisFont);
    chart->addAxis(axisY_Temp, Qt::AlignLeft);

    // 5. 右侧 Y 轴：专门用于湿度 (量程 0~100%)
    QValueAxis *axisY_Hum = new QValueAxis();
    axisY_Hum->setTitleText("湿度 (%)");
    axisY_Hum->setRange(0, 100);
    axisY_Hum->setTickCount(11); // 显示 0, 10, 20 ... 100
    axisY_Hum->setGridLineVisible(false); // 隐藏副Y轴的网格，避免与主Y轴网格交叉错乱
    axisY_Hum->setLabelsColor(QColor("#00e5ff")); // 刻度颜色与曲线一致
    axisY_Hum->setTitleBrush(QBrush(QColor("#00e5ff")));
    axisY_Hum->setLabelsFont(axisFont);
    chart->addAxis(axisY_Hum, Qt::AlignRight);

    // 6. 绑定对应的坐标轴
    tempSeries->attachAxis(axisX);
    tempSeries->attachAxis(axisY_Temp); // 温度绑左轴

    humSeries->attachAxis(axisX);
    humSeries->attachAxis(axisY_Hum);   // 湿度绑右轴

    // 图例设置
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

    if (!db.open()) {
        QMessageBox::critical(this, "错误", "无法打开数据库!");
        return;
    }

    QSqlQuery query;
    QString createTableSql = "CREATE TABLE IF NOT EXISTS history_data ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                             "timestamp DATETIME, "
                             "temperature REAL, "
                             "humidity REAL)";
    if (!query.exec(createTableSql)) {
        qDebug() << "建表失败:" << query.lastError().text();
    }
}

void MainWindow::on_btnOpen_clicked()
{
    if (serial->isOpen()) {
        serial->close();
        ui->btnOpen->setText("打开串口");
        ui->btnOpen->setStyleSheet("background-color: #007acc;"); // 蓝色
        ui->cbPort->setEnabled(true);
        ui->cbBaud->setEnabled(true);
    } else {
        serial->setPortName(ui->cbPort->currentData().toString());
        serial->setBaudRate(ui->cbBaud->currentText().toInt());
        serial->setDataBits(QSerialPort::Data8);
        serial->setParity(QSerialPort::NoParity);
        serial->setStopBits(QSerialPort::OneStop);

        if (serial->open(QIODevice::ReadWrite)) {
            ui->btnOpen->setText("关闭串口");
            ui->btnOpen->setStyleSheet("background-color: #d32f2f;"); // 开启后变为红色警示按钮
            ui->cbPort->setEnabled(false);
            ui->cbBaud->setEnabled(false);
        } else {
            QMessageBox::warning(this, "错误", "无法打开串口：" + serial->errorString());
        }
    }
}

void MainWindow::readSerialData()
{
    buffer.append(serial->readAll());

    while (buffer.size() >= 10) {
        int headerIndex = buffer.indexOf("\xAA\x55");

        if (headerIndex == -1) {
            buffer.clear();
            break;
        }

        if (headerIndex > 0) {
            buffer.remove(0, headerIndex);
        }

        if (buffer.size() < 10) {
            break;
        }

        QByteArray frame = buffer.left(10);
        buffer.remove(0, 10);

        if (static_cast<uint8_t>(frame[9]) != 0x5D) {
            continue;
        }

        uint16_t crc_calc = calculateCRC16(frame, 7);
        uint16_t crc_recv = (static_cast<uint8_t>(frame[7]) << 8) | static_cast<uint8_t>(frame[8]);

        if (crc_calc == crc_recv) {
            parseData(frame);
        } else {
            qDebug() << "CRC 校验失败!";
        }
    }
}

void MainWindow::parseData(const QByteArray &frame)
{
    int16_t rawTemp = (static_cast<uint8_t>(frame[3]) << 8) | static_cast<uint8_t>(frame[4]);
    uint16_t rawHum = (static_cast<uint8_t>(frame[5]) << 8) | static_cast<uint8_t>(frame[6]);

    double temp = rawTemp / 100.0;
    double hum = rawHum / 100.0;

    // 更新 UI 标签
    ui->lblTemp->setText(QString("T: %1 ℃").arg(temp, 0, 'f', 1));
    ui->lblHum->setText(QString("H: %1 %").arg(hum, 0, 'f', 1));

    // 更新波形图
    timeCount++;
    tempSeries->append(timeCount, temp);
    humSeries->append(timeCount, hum);

    if (timeCount > 60) {
        axisX->setRange(timeCount - 60, timeCount);
    }

    QSqlQuery query;
    query.prepare("INSERT INTO history_data (timestamp, temperature, humidity) "
                  "VALUES (:time, :temp, :hum)");
    query.bindValue(":time", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    query.bindValue(":temp", temp);
    query.bindValue(":hum", hum);
    query.exec();
}

uint16_t MainWindow::calculateCRC16(const QByteArray &data, int len)
{
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < len; ++pos) {
        crc ^= (uint8_t)data[pos];
        for (int i = 8; i != 0; --i) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
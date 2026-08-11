#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 1. 读取并应用全局 QSS 样式表
    QFile qssFile(":/style.qss");
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream ts(&qssFile);
        a.setStyleSheet(ts.readAll());
        qssFile.close();
    } else {
        qDebug() << "警告：无法加载 style.qss，请检查资源文件配置！";
    }

    MainWindow w;
    w.show();
    return a.exec();
}
/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "QtCharts"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout_Main;
    QSplitter *splitter;
    QGroupBox *gbControl;
    QVBoxLayout *verticalLayout;
    QLabel *label;
    QComboBox *cbPort;
    QLabel *label_2;
    QComboBox *cbBaud;
    QPushButton *btnOpen;
    QSpacerItem *verticalSpacer;
    QGroupBox *gbData;
    QVBoxLayout *verticalLayout_2;
    QLabel *lblTemp;
    QLabel *lblHum;
    QSpacerItem *verticalSpacer_2;
    QChartView *chartView;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(1024, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        horizontalLayout_Main = new QHBoxLayout(centralwidget);
        horizontalLayout_Main->setSpacing(15);
        horizontalLayout_Main->setObjectName(QString::fromUtf8("horizontalLayout_Main"));
        horizontalLayout_Main->setContentsMargins(15, 15, 15, 15);
        splitter = new QSplitter(centralwidget);
        splitter->setObjectName(QString::fromUtf8("splitter"));
        splitter->setOrientation(Qt::Horizontal);
        splitter->setHandleWidth(6);
        gbControl = new QGroupBox(splitter);
        gbControl->setObjectName(QString::fromUtf8("gbControl"));
        gbControl->setMinimumSize(QSize(260, 0));
        verticalLayout = new QVBoxLayout(gbControl);
        verticalLayout->setSpacing(12);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        label = new QLabel(gbControl);
        label->setObjectName(QString::fromUtf8("label"));

        verticalLayout->addWidget(label);

        cbPort = new QComboBox(gbControl);
        cbPort->setObjectName(QString::fromUtf8("cbPort"));
        cbPort->setMinimumSize(QSize(0, 30));

        verticalLayout->addWidget(cbPort);

        label_2 = new QLabel(gbControl);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        verticalLayout->addWidget(label_2);

        cbBaud = new QComboBox(gbControl);
        cbBaud->addItem(QString());
        cbBaud->addItem(QString());
        cbBaud->setObjectName(QString::fromUtf8("cbBaud"));
        cbBaud->setMinimumSize(QSize(0, 30));

        verticalLayout->addWidget(cbBaud);

        btnOpen = new QPushButton(gbControl);
        btnOpen->setObjectName(QString::fromUtf8("btnOpen"));
        btnOpen->setMinimumSize(QSize(0, 45));
        btnOpen->setCursor(QCursor(Qt::PointingHandCursor));

        verticalLayout->addWidget(btnOpen);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        gbData = new QGroupBox(gbControl);
        gbData->setObjectName(QString::fromUtf8("gbData"));
        verticalLayout_2 = new QVBoxLayout(gbData);
        verticalLayout_2->setSpacing(20);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        lblTemp = new QLabel(gbData);
        lblTemp->setObjectName(QString::fromUtf8("lblTemp"));
        QFont font;
        font.setFamily(QString::fromUtf8("Consolas"));
        font.setPointSize(20);
        font.setBold(true);
        font.setWeight(75);
        lblTemp->setFont(font);
        lblTemp->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(lblTemp);

        lblHum = new QLabel(gbData);
        lblHum->setObjectName(QString::fromUtf8("lblHum"));
        lblHum->setFont(font);
        lblHum->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(lblHum);


        verticalLayout->addWidget(gbData);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_2);

        splitter->addWidget(gbControl);
        chartView = new QChartView(splitter);
        chartView->setObjectName(QString::fromUtf8("chartView"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(chartView->sizePolicy().hasHeightForWidth());
        chartView->setSizePolicy(sizePolicy);
        splitter->addWidget(chartView);

        horizontalLayout_Main->addWidget(splitter);

        MainWindow->setCentralWidget(centralwidget);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "\345\267\245\344\270\232\347\272\247\346\270\251\346\271\277\345\272\246\346\225\260\346\215\256\351\207\207\351\233\206\347\263\273\347\273\237 - V1.0", nullptr));
        gbControl->setTitle(QCoreApplication::translate("MainWindow", "\351\200\232\344\277\241\344\270\216\346\216\247\345\210\266\351\235\242\346\235\277", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "\344\270\262\345\217\243\351\200\211\346\213\251:", nullptr));
        label_2->setText(QCoreApplication::translate("MainWindow", "\346\263\242\347\211\271\347\216\207:", nullptr));
        cbBaud->setItemText(0, QCoreApplication::translate("MainWindow", "9600", nullptr));
        cbBaud->setItemText(1, QCoreApplication::translate("MainWindow", "115200", nullptr));

        btnOpen->setText(QCoreApplication::translate("MainWindow", "\346\211\223\345\274\200\344\270\262\345\217\243", nullptr));
        gbData->setTitle(QCoreApplication::translate("MainWindow", "\345\256\236\346\227\266\346\225\260\346\215\256\347\233\221\346\265\213", nullptr));
        lblTemp->setStyleSheet(QCoreApplication::translate("MainWindow", "color: #ff5252;", nullptr));
        lblTemp->setText(QCoreApplication::translate("MainWindow", "T: --.- \342\204\203", nullptr));
        lblHum->setStyleSheet(QCoreApplication::translate("MainWindow", "color: #00e5ff;", nullptr));
        lblHum->setText(QCoreApplication::translate("MainWindow", "H: --.- %", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H

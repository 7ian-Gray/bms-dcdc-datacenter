#include "mainwindow.h"

#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("BMS-DCDC 能量系统数据监控平台"));
    resize(1400, 900);

    auto *placeholder = new QLabel(QStringLiteral("Stage 1: Qt Widgets 桌面应用初始化完成。"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);
}

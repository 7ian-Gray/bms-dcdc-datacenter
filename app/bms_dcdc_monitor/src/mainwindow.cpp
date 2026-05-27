#include "mainwindow.h"

#include "MockDataGenerator.h"

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPainter>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <limits>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("BMS-DCDC 能量回收系统数据监控平台"));
    resize(1400, 900);
    loadMockData();
    setupUi();
    updateCurrentTime();

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateCurrentTime);
    timer->start(1000);
}

void MainWindow::loadMockData()
{
    dashboardData_ = MockDataGenerator::createDashboardData();
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("dashboardRoot"));

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(12);

    mainLayout->addWidget(setupTopBar(), 0);

    auto *contentLayout = new QGridLayout();
    contentLayout->setHorizontalSpacing(12);
    contentLayout->setVerticalSpacing(12);

    contentLayout->addWidget(setupTopLeftSummaryArea(), 0, 0);
    contentLayout->addWidget(setupTopRightTableArea(), 0, 1);
    contentLayout->addWidget(setupBottomLeftChartArea(), 1, 0);
    contentLayout->addWidget(setupBottomRightBarChartArea(), 1, 1);
    contentLayout->setColumnStretch(0, 58);
    contentLayout->setColumnStretch(1, 42);
    contentLayout->setRowStretch(0, 30);
    contentLayout->setRowStretch(1, 62);

    mainLayout->addLayout(contentLayout, 1);

    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("Stage 1: 工业监控主界面已加载静态假数据"), 4000);

    setStyleSheet(QStringLiteral(R"(
        QMainWindow {
            background-color: #dde4ec;
        }
        QWidget#dashboardRoot {
            background-color: #dde4ec;
            color: #1f2d3d;
        }
        QGroupBox {
            background-color: #f7f9fc;
            border: 1px solid #c6d0dc;
            border-radius: 8px;
            margin-top: 14px;
            font-weight: 700;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 14px;
            padding: 0 6px;
            color: #22364d;
        }
        QFrame#metricCard {
            background-color: #ffffff;
            border: 1px solid #d7e0ea;
            border-radius: 6px;
        }
        QLabel#cardTitle {
            color: #5c6c7e;
            font-size: 11px;
            font-weight: 500;
        }
        QLabel#cardValue {
            color: #16324f;
            font-size: 15px;
            font-weight: 700;
        }
        QLabel#statusBadge {
            color: #ffffff;
            border-radius: 12px;
            padding: 5px 12px;
            font-size: 12px;
            font-weight: 700;
        }
        QTableWidget {
            background-color: #ffffff;
            border: 1px solid #d7e0ea;
            border-radius: 6px;
            gridline-color: #e6ebf1;
            alternate-background-color: #f3f6fa;
        }
        QHeaderView::section {
            background-color: #e8eef5;
            color: #29415d;
            padding: 6px;
            border: none;
            border-right: 1px solid #d7e0ea;
            border-bottom: 1px solid #d7e0ea;
            font-weight: 700;
        }
        QPushButton {
            background-color: #2a6fb0;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 14px;
            font-weight: 700;
        }
        QPushButton:hover {
            background-color: #255f95;
        }
        QPushButton:checked {
            background-color: #173b63;
        }
    )"));
}

void MainWindow::updateCurrentTime()
{
    if (currentTimeLabel != nullptr) {
        currentTimeLabel->setText(QStringLiteral("当前时间  %1")
                                      .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    }
}

QWidget *MainWindow::setupTopBar()
{
    auto *bar = new QFrame(this);
    bar->setFrameShape(QFrame::StyledPanel);
    bar->setStyleSheet(QStringLiteral("QFrame { background-color: #f7f9fc; border: 1px solid #c6d0dc; border-radius: 8px; }"));
    bar->setMinimumHeight(72);

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(14);

    auto *iconBlock = new QFrame(bar);
    iconBlock->setFixedSize(18, 18);
    iconBlock->setStyleSheet(QStringLiteral("background-color: #2a6fb0; border-radius: 4px;"));

    auto *titleLabel = new QLabel(QStringLiteral("BMS-DCDC 能量回收系统数据监控平台"), bar);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 800; color: #1a2c42;"));

    auto *titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(10);
    titleLayout->addWidget(iconBlock);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto *titleContainer = new QWidget(bar);
    titleContainer->setLayout(titleLayout);

    auto *canStatus = createStatusBadge(QStringLiteral("CAN"), QStringLiteral("Online"), QStringLiteral("#1f8f5f"));
    auto *modbusStatus = createStatusBadge(QStringLiteral("Modbus"), QStringLiteral("Online"), QStringLiteral("#1f8f5f"));
    currentTimeLabel = new QLabel(bar);
    currentTimeLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; color: #29415d;"));

    auto *exportButton = new QPushButton(QStringLiteral("导出数据"), bar);
    exportButton->setMinimumWidth(112);
    connect(exportButton, &QPushButton::clicked, this, [this]() {
        statusBar()->showMessage(QStringLiteral("导出功能将在 CSV 模块接入后启用"), 3000);
    });

    layout->addWidget(titleContainer, 1);
    layout->addWidget(canStatus);
    layout->addWidget(modbusStatus);
    layout->addWidget(currentTimeLabel);
    layout->addWidget(exportButton);

    return bar;
}

QGroupBox *MainWindow::setupTopLeftSummaryArea()
{
    auto *group = new QGroupBox(QStringLiteral("核心数据汇总卡片区"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(createBmsSummaryGroup());
    layout->addWidget(createDcdcSummaryGroup());
    return group;
}

QGroupBox *MainWindow::setupTopRightTableArea()
{
    auto *group = new QGroupBox(QStringLiteral("电压温度列表与统计信息"), this);
    auto *layout = new QHBoxLayout(group);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);
    layout->addWidget(createVoltageTemperatureTable(), 3);
    layout->addWidget(createStatisticsPanel(), 2);
    return group;
}

QGroupBox *MainWindow::setupBottomLeftChartArea()
{
    auto *group = new QGroupBox(QStringLiteral("运行数据折线图"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->addWidget(createRuntimeChartPanel());
    return group;
}

QGroupBox *MainWindow::setupBottomRightBarChartArea()
{
    auto *group = new QGroupBox(QStringLiteral("单体电压柱状图与一致性状态"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(createCellVoltageChartPanel(), 3);
    layout->addWidget(createConsistencyPanel(), 2);
    return group;
}

QGroupBox *MainWindow::createBmsSummaryGroup()
{
    auto *group = new QGroupBox(QStringLiteral("BMS 数据汇总"), this);
    auto *layout = new QGridLayout(group);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);

    const QList<QPair<QString, QString>> items = {
        {QStringLiteral("电池包电压"), formatNumber(dashboardData_.bms.packVoltage, 1, QStringLiteral("V"))},
        {QStringLiteral("电池包电流"), formatNumber(dashboardData_.bms.packCurrent, 1, QStringLiteral("A"))},
        {QStringLiteral("剩余容量 SOC"), formatNumber(dashboardData_.bms.soc, 1, QStringLiteral("%"))},
        {QStringLiteral("健康度 SOH"), formatNumber(dashboardData_.bms.soh, 1, QStringLiteral("%"))},
        {QStringLiteral("累计充电量"), formatNumber(dashboardData_.bms.chargedEnergy, 1, QStringLiteral("kWh"))},
        {QStringLiteral("累计放电量"), formatNumber(dashboardData_.bms.dischargedEnergy, 1, QStringLiteral("kWh"))},
        {QStringLiteral("电量变化值"), formatNumber(dashboardData_.bms.energyDelta, 1, QStringLiteral("kWh"))},
        {QStringLiteral("单体最高电压"), formatNumber(dashboardData_.bms.maxCellVoltage, 3, QStringLiteral("V"))},
        {QStringLiteral("单体最低电压"), formatNumber(dashboardData_.bms.minCellVoltage, 3, QStringLiteral("V"))},
        {QStringLiteral("单体最高温度"), formatNumber(dashboardData_.bms.maxCellTemperature, 1, QStringLiteral("℃"))},
        {QStringLiteral("单体最低温度"), formatNumber(dashboardData_.bms.minCellTemperature, 1, QStringLiteral("℃"))},
        {QStringLiteral("充电限流"), formatNumber(dashboardData_.bms.chargeCurrentLimit, 1, QStringLiteral("A"))},
        {QStringLiteral("放电限流"), formatNumber(dashboardData_.bms.dischargeCurrentLimit, 1, QStringLiteral("A"))}
    };

    const int columns = 7;
    for (int index = 0; index < items.size(); ++index) {
        layout->addWidget(createMetricCard(items.at(index).first, items.at(index).second, QStringLiteral("#255f95")),
                          index / columns,
                          index % columns);
    }

    for (int column = 0; column < columns; ++column) {
        layout->setColumnStretch(column, 1);
    }

    return group;
}

QGroupBox *MainWindow::createDcdcSummaryGroup()
{
    auto *group = new QGroupBox(QStringLiteral("DCDC 数据汇总"), this);
    auto *layout = new QGridLayout(group);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);

    const QList<QPair<QString, QString>> items = {
        {QStringLiteral("高压侧电压"), formatNumber(dashboardData_.dcdc.hvVoltage, 1, QStringLiteral("V"))},
        {QStringLiteral("高压侧电流"), formatNumber(dashboardData_.dcdc.hvCurrent, 1, QStringLiteral("A"))},
        {QStringLiteral("低压侧电流"), formatNumber(dashboardData_.dcdc.lvCurrent, 1, QStringLiteral("A"))},
        {QStringLiteral("板上温度"), formatNumber(dashboardData_.dcdc.boardTemperature, 1, QStringLiteral("℃"))},
        {QStringLiteral("充电限流值"), formatNumber(dashboardData_.dcdc.chargeCurrentLimit, 1, QStringLiteral("A"))},
        {QStringLiteral("放电限流值"), formatNumber(dashboardData_.dcdc.dischargeCurrentLimit, 1, QStringLiteral("A"))},
        {QStringLiteral("充电开始电压"), formatNumber(dashboardData_.dcdc.chargeStartVoltage, 1, QStringLiteral("V"))},
        {QStringLiteral("充电截止电压"), formatNumber(dashboardData_.dcdc.chargeStopVoltage, 1, QStringLiteral("V"))},
        {QStringLiteral("充电目标电压"), formatNumber(dashboardData_.dcdc.chargeTargetVoltage, 1, QStringLiteral("V"))},
        {QStringLiteral("放电开始电压"), formatNumber(dashboardData_.dcdc.dischargeStartVoltage, 1, QStringLiteral("V"))},
        {QStringLiteral("放电截止电压"), formatNumber(dashboardData_.dcdc.dischargeStopVoltage, 1, QStringLiteral("V"))},
        {QStringLiteral("放电目标电压"), formatNumber(dashboardData_.dcdc.dischargeTargetVoltage, 1, QStringLiteral("V"))}
    };

    const int columns = 6;
    for (int index = 0; index < items.size(); ++index) {
        layout->addWidget(createMetricCard(items.at(index).first, items.at(index).second, QStringLiteral("#a45a13")),
                          index / columns,
                          index % columns);
    }

    for (int column = 0; column < columns; ++column) {
        layout->setColumnStretch(column, 1);
    }

    return group;
}

QTableWidget *MainWindow::createVoltageTemperatureTable()
{
    auto *table = new QTableWidget(dashboardData_.cells.size(), 4, this);
    table->setHorizontalHeaderLabels({
        QStringLiteral("编号"),
        QStringLiteral("单体电压 (V)"),
        QStringLiteral("单体温度 (℃)"),
        QStringLiteral("状态")
    });
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setShowGrid(true);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->verticalHeader()->setDefaultSectionSize(28);

    for (int row = 0; row < dashboardData_.cells.size(); ++row) {
        const CellRecord &cell = dashboardData_.cells.at(row);

        auto *idItem = new QTableWidgetItem(cell.id);
        auto *voltageItem = new QTableWidgetItem(QString::number(cell.voltage, 'f', 3));
        auto *temperatureItem = new QTableWidgetItem(QString::number(cell.temperature, 'f', 1));
        auto *statusItem = new QTableWidgetItem(cell.status);

        idItem->setTextAlignment(Qt::AlignCenter);
        voltageItem->setTextAlignment(Qt::AlignCenter);
        temperatureItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setTextAlignment(Qt::AlignCenter);

        if (cell.status == QStringLiteral("偏高")) {
            statusItem->setForeground(QColor(QStringLiteral("#b54708")));
        } else if (cell.status == QStringLiteral("偏低")) {
            statusItem->setForeground(QColor(QStringLiteral("#b42318")));
        } else {
            statusItem->setForeground(QColor(QStringLiteral("#1f8f5f")));
        }

        table->setItem(row, 0, idItem);
        table->setItem(row, 1, voltageItem);
        table->setItem(row, 2, temperatureItem);
        table->setItem(row, 3, statusItem);
    }

    return table;
}

QWidget *MainWindow::createStatisticsPanel()
{
    const CellStatistics stats = calculateCellStatistics();

    auto *group = new QGroupBox(QStringLiteral("统计信息"), this);
    auto *layout = new QGridLayout(group);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);

    const QList<QPair<QString, QString>> items = {
        {QStringLiteral("最高单体电压"), formatNumber(stats.maxVoltage, 3, QStringLiteral("V"))},
        {QStringLiteral("最低单体电压"), formatNumber(stats.minVoltage, 3, QStringLiteral("V"))},
        {QStringLiteral("单体压差"), formatNumber(stats.voltageDiff, 3, QStringLiteral("V"))},
        {QStringLiteral("最高温度"), formatNumber(stats.maxTemperature, 1, QStringLiteral("℃"))},
        {QStringLiteral("最低温度"), formatNumber(stats.minTemperature, 1, QStringLiteral("℃"))},
        {QStringLiteral("温差"), formatNumber(stats.temperatureDiff, 1, QStringLiteral("℃"))}
    };

    for (int index = 0; index < items.size(); ++index) {
        layout->addWidget(createMetricCard(items.at(index).first, items.at(index).second, QStringLiteral("#255f95")),
                          index / 2,
                          index % 2);
    }

    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    return group;
}

QWidget *MainWindow::createRuntimeChartPanel()
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *toolbar = new QWidget(container);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(8);

    auto *titleLabel = new QLabel(QStringLiteral("关键运行量趋势"), toolbar);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; color: #29415d;"));

    auto *buttonGroup = new QButtonGroup(toolbar);
    buttonGroup->setExclusive(true);
    const QStringList ranges = {
        QStringLiteral("1 min"),
        QStringLiteral("5 min"),
        QStringLiteral("30 min"),
        QStringLiteral("1 h"),
        QStringLiteral("全部")
    };

    toolbarLayout->addWidget(titleLabel);
    toolbarLayout->addStretch();

    for (int index = 0; index < ranges.size(); ++index) {
        auto *button = new QPushButton(ranges.at(index), toolbar);
        button->setCheckable(true);
        button->setMinimumWidth(58);
        if (index == 0) {
            button->setChecked(true);
        }
        buttonGroup->addButton(button, index);
        toolbarLayout->addWidget(button);
    }

    connect(buttonGroup,
            QOverload<int>::of(&QButtonGroup::idClicked),
            this,
            [this, ranges](int index) {
                statusBar()->showMessage(QStringLiteral("当前为静态假曲线，时间范围切换到 %1").arg(ranges.at(index)),
                                         2000);
            });

    auto *chart = new QChart();
    chart->setBackgroundVisible(false);
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    auto *hvVoltageSeries = new QLineSeries(chart);
    hvVoltageSeries->setName(QStringLiteral("高压侧电压"));
    hvVoltageSeries->setColor(QColor(QStringLiteral("#1d4f91")));

    auto *lvVoltageSeries = new QLineSeries(chart);
    lvVoltageSeries->setName(QStringLiteral("低压侧电压"));
    lvVoltageSeries->setColor(QColor(QStringLiteral("#2a9d8f")));

    auto *lvCurrentSeries = new QLineSeries(chart);
    lvCurrentSeries->setName(QStringLiteral("低压侧电流"));
    lvCurrentSeries->setColor(QColor(QStringLiteral("#e76f51")));

    auto *chargeLimitSeries = new QLineSeries(chart);
    chargeLimitSeries->setName(QStringLiteral("充电限流值"));
    chargeLimitSeries->setColor(QColor(QStringLiteral("#9c6644")));

    auto *dischargeLimitSeries = new QLineSeries(chart);
    dischargeLimitSeries->setName(QStringLiteral("放电限流值"));
    dischargeLimitSeries->setColor(QColor(QStringLiteral("#7f5539")));

    auto *temperatureSeries = new QLineSeries(chart);
    temperatureSeries->setName(QStringLiteral("板上温度"));
    temperatureSeries->setColor(QColor(QStringLiteral("#8d3da8")));

    for (const RuntimeSample &sample : dashboardData_.runtimeSamples) {
        hvVoltageSeries->append(sample.minute, sample.hvVoltage);
        lvVoltageSeries->append(sample.minute, sample.lvVoltage);
        lvCurrentSeries->append(sample.minute, sample.lvCurrent);
        chargeLimitSeries->append(sample.minute, sample.chargeCurrentLimit);
        dischargeLimitSeries->append(sample.minute, sample.dischargeCurrentLimit);
        temperatureSeries->append(sample.minute, sample.boardTemperature);
    }

    chart->addSeries(hvVoltageSeries);
    chart->addSeries(lvVoltageSeries);
    chart->addSeries(lvCurrentSeries);
    chart->addSeries(chargeLimitSeries);
    chart->addSeries(dischargeLimitSeries);
    chart->addSeries(temperatureSeries);

    auto *axisX = new QValueAxis(chart);
    axisX->setRange(0.0, 55.0);
    axisX->setTickCount(12);
    axisX->setLabelFormat(QStringLiteral("%.0f"));
    axisX->setTitleText(QStringLiteral("时间 (min)"));

    auto *axisYLeft = new QValueAxis(chart);
    axisYLeft->setRange(0.0, 800.0);
    axisYLeft->setLabelFormat(QStringLiteral("%.0f"));
    axisYLeft->setTitleText(QStringLiteral("电压 / 电流"));

    auto *axisYRight = new QValueAxis(chart);
    axisYRight->setRange(0.0, 60.0);
    axisYRight->setLabelFormat(QStringLiteral("%.0f"));
    axisYRight->setTitleText(QStringLiteral("温度 (℃)"));

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisYLeft, Qt::AlignLeft);
    chart->addAxis(axisYRight, Qt::AlignRight);

    for (QLineSeries *series : {hvVoltageSeries, lvVoltageSeries, lvCurrentSeries, chargeLimitSeries, dischargeLimitSeries}) {
        series->attachAxis(axisX);
        series->attachAxis(axisYLeft);
    }

    temperatureSeries->attachAxis(axisX);
    temperatureSeries->attachAxis(axisYRight);

    auto *chartView = new QChartView(chart, container);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(360);
    chartView->setStyleSheet(QStringLiteral("background-color: #ffffff; border: 1px solid #d7e0ea; border-radius: 6px;"));

    layout->addWidget(toolbar);
    layout->addWidget(chartView, 1);
    return container;
}

QWidget *MainWindow::createCellVoltageChartPanel()
{
    auto *group = new QGroupBox(QStringLiteral("单体电压柱状图"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(8, 8, 8, 8);

    auto *barSet = new QBarSet(QStringLiteral("单体电压"));
    QStringList categories;
    double minVoltage = std::numeric_limits<double>::max();
    double maxVoltage = std::numeric_limits<double>::lowest();

    for (const CellRecord &cell : dashboardData_.cells) {
        *barSet << cell.voltage;
        categories << cell.id;
        minVoltage = qMin(minVoltage, cell.voltage);
        maxVoltage = qMax(maxVoltage, cell.voltage);
    }

    barSet->setColor(QColor(QStringLiteral("#2a6fb0")));
    auto *series = new QBarSeries(group);
    series->append(barSet);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->legend()->setVisible(false);
    chart->setBackgroundVisible(false);
    chart->setMargins(QMargins(8, 8, 8, 8));

    auto *axisX = new QBarCategoryAxis(chart);
    axisX->append(categories);

    auto *axisY = new QValueAxis(chart);
    axisY->setRange(minVoltage - 0.02, maxVoltage + 0.02);
    axisY->setLabelFormat(QStringLiteral("%.3f"));
    axisY->setTitleText(QStringLiteral("电压 (V)"));

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    auto *chartView = new QChartView(chart, group);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(240);
    chartView->setStyleSheet(QStringLiteral("background-color: #ffffff; border: 1px solid #d7e0ea; border-radius: 6px;"));

    layout->addWidget(chartView);
    return group;
}

QWidget *MainWindow::createConsistencyPanel()
{
    const CellStatistics stats = calculateCellStatistics();

    auto *group = new QGroupBox(QStringLiteral("一致性状态判断"), this);
    auto *layout = new QGridLayout(group);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);

    layout->addWidget(createMetricCard(QStringLiteral("最高电压"),
                                       formatNumber(stats.maxVoltage, 3, QStringLiteral("V")),
                                       QStringLiteral("#255f95")),
                      0,
                      0);
    layout->addWidget(createMetricCard(QStringLiteral("最低电压"),
                                       formatNumber(stats.minVoltage, 3, QStringLiteral("V")),
                                       QStringLiteral("#255f95")),
                      0,
                      1);
    layout->addWidget(createMetricCard(QStringLiteral("平均电压"),
                                       formatNumber(stats.averageVoltage, 3, QStringLiteral("V")),
                                       QStringLiteral("#255f95")),
                      1,
                      0);
    layout->addWidget(createMetricCard(QStringLiteral("压差"),
                                       formatNumber(stats.voltageDiff, 3, QStringLiteral("V")),
                                       QStringLiteral("#255f95")),
                      1,
                      1);
    layout->addWidget(createMetricCard(QStringLiteral("状态"),
                                       stats.consistencyStatus,
                                       stats.consistencyStatus.contains(QStringLiteral("正常"))
                                           ? QStringLiteral("#1f8f5f")
                                           : QStringLiteral("#b54708")),
                      2,
                      0,
                      1,
                      2);

    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    return group;
}

QWidget *MainWindow::createMetricCard(const QString &title,
                                      const QString &value,
                                      const QString &accentColor) const
{
    auto *card = new QFrame();
    card->setObjectName(QStringLiteral("metricCard"));
    card->setMinimumHeight(54);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(4);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("cardTitle"));
    titleLabel->setWordWrap(true);

    auto *valueLabel = new QLabel(value, card);
    valueLabel->setObjectName(QStringLiteral("cardValue"));
    valueLabel->setWordWrap(true);

    if (!accentColor.isEmpty()) {
        valueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 15px; font-weight: 700;").arg(accentColor));
    }

    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    return card;
}

QLabel *MainWindow::createStatusBadge(const QString &channel,
                                      const QString &state,
                                      const QString &color) const
{
    auto *label = new QLabel(QStringLiteral("%1  %2").arg(channel, state));
    label->setObjectName(QStringLiteral("statusBadge"));
    label->setStyleSheet(QStringLiteral("background-color: %1;").arg(color));
    return label;
}

QString MainWindow::formatNumber(double value, int decimals, const QString &unit) const
{
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', decimals), unit);
}

MainWindow::CellStatistics MainWindow::calculateCellStatistics() const
{
    CellStatistics stats;
    if (dashboardData_.cells.isEmpty()) {
        return stats;
    }

    double voltageTotal = 0.0;
    double maxVoltage = std::numeric_limits<double>::lowest();
    double minVoltage = std::numeric_limits<double>::max();
    double maxTemperature = std::numeric_limits<double>::lowest();
    double minTemperature = std::numeric_limits<double>::max();

    for (const CellRecord &cell : dashboardData_.cells) {
        voltageTotal += cell.voltage;
        maxVoltage = qMax(maxVoltage, cell.voltage);
        minVoltage = qMin(minVoltage, cell.voltage);
        maxTemperature = qMax(maxTemperature, cell.temperature);
        minTemperature = qMin(minTemperature, cell.temperature);
    }

    stats.maxVoltage = maxVoltage;
    stats.minVoltage = minVoltage;
    stats.averageVoltage = voltageTotal / static_cast<double>(dashboardData_.cells.size());
    stats.voltageDiff = maxVoltage - minVoltage;
    stats.maxTemperature = maxTemperature;
    stats.minTemperature = minTemperature;
    stats.temperatureDiff = maxTemperature - minTemperature;
    stats.consistencyStatus = stats.voltageDiff <= 0.12
                                  ? QStringLiteral("正常（一致性良好）")
                                  : QStringLiteral("需关注（压差偏大）");
    return stats;
}

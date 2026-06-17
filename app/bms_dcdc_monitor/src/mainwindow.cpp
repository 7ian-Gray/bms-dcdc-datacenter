#include "mainwindow.h"

#include "MockDataGenerator.h"
#include "communication/CanSessionController.h"
#include "pages/CanMonitorPage.h"

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
#include <QScrollArea>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <limits>

namespace
{
struct RuntimeMetricDefinition
{
    QString title;
    QString unit;
    QColor color;
    double (*accessor)(const RuntimeSample &sample);
};

double runtimeHvVoltage(const RuntimeSample &sample) { return sample.hvVoltage; }
double runtimeLvVoltage(const RuntimeSample &sample) { return sample.lvVoltage; }
double runtimeHvCurrent(const RuntimeSample &sample) { return sample.hvCurrent; }
double runtimeLvCurrent(const RuntimeSample &sample) { return sample.lvCurrent; }
double runtimeChargeLimit(const RuntimeSample &sample) { return sample.chargeCurrentLimit; }
double runtimeDischargeLimit(const RuntimeSample &sample) { return sample.dischargeCurrentLimit; }
double runtimeBoardTemperature(const RuntimeSample &sample) { return sample.boardTemperature; }

const QVector<RuntimeMetricDefinition> &runtimeMetricDefinitions()
{
    static const QVector<RuntimeMetricDefinition> definitions = {
        {QStringLiteral("高压侧电压"), QStringLiteral("V"), QColor(QStringLiteral("#1d4f91")), runtimeHvVoltage},
        {QStringLiteral("低压侧电压"), QStringLiteral("V"), QColor(QStringLiteral("#2a9d8f")), runtimeLvVoltage},
        {QStringLiteral("高压侧电流"), QStringLiteral("A"), QColor(QStringLiteral("#355070")), runtimeHvCurrent},
        {QStringLiteral("低压侧电流"), QStringLiteral("A"), QColor(QStringLiteral("#e76f51")), runtimeLvCurrent},
        {QStringLiteral("充电限流值"), QStringLiteral("A"), QColor(QStringLiteral("#9c6644")), runtimeChargeLimit},
        {QStringLiteral("放电限流值"), QStringLiteral("A"), QColor(QStringLiteral("#7f5539")), runtimeDischargeLimit},
        {QStringLiteral("板上温度"), QStringLiteral("℃"), QColor(QStringLiteral("#8d3da8")), runtimeBoardTemperature}
    };

    return definitions;
}

QChartView *createRuntimeChartView(const QVector<RuntimeSample> &samples,
                                   const QList<int> &metricIndexes,
                                   bool singleAxis,
                                   QWidget *parent)
{
    auto *chart = new QChart();
    chart->setBackgroundVisible(false);
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    auto *axisX = new QValueAxis(chart);
    axisX->setRange(0.0, samples.isEmpty() ? 60.0 : samples.last().minute);
    axisX->setTickCount(samples.size() > 2 ? samples.size() : 6);
    axisX->setLabelFormat(QStringLiteral("%.0f"));
    axisX->setTitleText(QStringLiteral("时间 (min)"));
    chart->addAxis(axisX, Qt::AlignBottom);

    auto *leftAxis = new QValueAxis(chart);
    auto *rightAxis = new QValueAxis(chart);

    double leftMin = std::numeric_limits<double>::max();
    double leftMax = std::numeric_limits<double>::lowest();
    double rightMin = std::numeric_limits<double>::max();
    double rightMax = std::numeric_limits<double>::lowest();
    bool rightAxisUsed = false;

    const auto &definitions = runtimeMetricDefinitions();

    for (int metricIndex : metricIndexes) {
        const RuntimeMetricDefinition &definition = definitions.at(metricIndex);
        auto *series = new QLineSeries(chart);
        series->setName(definition.title);
        series->setColor(definition.color);

        double localMin = std::numeric_limits<double>::max();
        double localMax = std::numeric_limits<double>::lowest();

        for (const RuntimeSample &sample : samples) {
            const double value = definition.accessor(sample);
            series->append(sample.minute, value);
            localMin = qMin(localMin, value);
            localMax = qMax(localMax, value);
        }

        chart->addSeries(series);
        series->attachAxis(axisX);

        const bool useRightAxis = !singleAxis && metricIndex == 6;
        if (useRightAxis) {
            rightAxisUsed = true;
            rightMin = qMin(rightMin, localMin);
            rightMax = qMax(rightMax, localMax);
        } else {
            leftMin = qMin(leftMin, localMin);
            leftMax = qMax(leftMax, localMax);
        }
    }

    if (leftMin == std::numeric_limits<double>::max()) {
        leftMin = 0.0;
        leftMax = 1.0;
    }

    const double leftPadding = qMax((leftMax - leftMin) * 0.15, 1.0);
    leftAxis->setRange(leftMin - leftPadding, leftMax + leftPadding);
    leftAxis->setLabelFormat(QStringLiteral("%.1f"));
    leftAxis->setTitleText(singleAxis ? QStringLiteral("数值") : QStringLiteral("电压 / 电流"));
    chart->addAxis(leftAxis, Qt::AlignLeft);

    if (rightAxisUsed) {
        const double rightPadding = qMax((rightMax - rightMin) * 0.2, 1.0);
        rightAxis->setRange(rightMin - rightPadding, rightMax + rightPadding);
        rightAxis->setLabelFormat(QStringLiteral("%.1f"));
        rightAxis->setTitleText(QStringLiteral("温度 (℃)"));
        chart->addAxis(rightAxis, Qt::AlignRight);
    }

    for (QObject *object : chart->series()) {
        auto *series = qobject_cast<QLineSeries *>(object);
        if (series == nullptr) {
            continue;
        }

        const int metricIndex = metricIndexes.at(chart->series().indexOf(series));
        const bool useRightAxis = rightAxisUsed && !singleAxis && metricIndex == 6;
        series->attachAxis(useRightAxis ? rightAxis : leftAxis);
    }

    auto *chartView = new QChartView(chart, parent);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setStyleSheet(QStringLiteral("background-color: #ffffff; border: 1px solid #d7e0ea; border-radius: 6px;"));
    return chartView;
}

QChartView *createCellVoltageBarChartView(const QVector<CellRecord> &cells,
                                          QWidget *parent,
                                          int minimumWidth,
                                          bool compact)
{
    auto *barSet = new QBarSet(QStringLiteral("单体电压"));
    QStringList categories;

    double minVoltage = std::numeric_limits<double>::max();
    double maxVoltage = std::numeric_limits<double>::lowest();

    for (const CellRecord &cell : cells) {
        *barSet << cell.voltage;
        categories << cell.id;
        minVoltage = qMin(minVoltage, cell.voltage);
        maxVoltage = qMax(maxVoltage, cell.voltage);
    }

    barSet->setColor(QColor(QStringLiteral("#2a6fb0")));
    auto *series = new QBarSeries(parent);
    series->append(barSet);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->legend()->setVisible(false);
    chart->setBackgroundVisible(false);
    chart->setMargins(QMargins(8, 8, 8, 8));

    auto *axisX = new QBarCategoryAxis(chart);
    axisX->append(categories);
    axisX->setLabelsAngle(compact ? -35 : -55);
    QFont labelFont;
    labelFont.setPointSize(compact ? 8 : 9);
    axisX->setLabelsFont(labelFont);

    auto *axisY = new QValueAxis(chart);
    axisY->setRange(minVoltage - 0.02, maxVoltage + 0.02);
    axisY->setLabelFormat(QStringLiteral("%.3f"));
    axisY->setTitleText(QStringLiteral("电压 (V)"));

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    auto *chartView = new QChartView(chart, parent);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(compact ? 180 : 220);
    chartView->setMinimumWidth(minimumWidth);
    chartView->setStyleSheet(QStringLiteral("background-color: #ffffff; border: 1px solid #d7e0ea; border-radius: 6px;"));
    return chartView;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("BMS-DCDC 能量回收系统数据监控平台"));
    resize(1260, 810);
    setMinimumSize(1100, 720);
    loadMockData();
    setupUi();
    initializeCommunication();
    updateCurrentTime();
    switchRuntimeMetricChart(0);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateCurrentTime);
    timer->start(1000);
}

MainWindow::~MainWindow()
{
    shutdownCommunication();
}

void MainWindow::initializeCommunication()
{
    qRegisterMetaType<CanFrame>("CanFrame");
    qRegisterMetaType<CanFrameDirection>("CanFrameDirection");

    canSessionController_ = new CanSessionController(this);

    if (canMonitorPage_ != nullptr) {
        canMonitorPage_->setPlatformCapabilityText(
            canSessionController_->platformCapabilityText());
        canMonitorPage_->setConnectionState(canSessionController_->state());
        canMonitorPage_->setSourceMode(canSessionController_->sourceMode());

        connect(canMonitorPage_,
                &CanMonitorPage::openDeviceRequested,
                canSessionController_,
                &CanSessionController::openDevice);
        connect(canMonitorPage_,
                &CanMonitorPage::startChannelRequested,
                canSessionController_,
                &CanSessionController::startChannel);
        connect(canMonitorPage_,
                &CanMonitorPage::stopChannelRequested,
                canSessionController_,
                &CanSessionController::stopChannel);
        connect(canMonitorPage_,
                &CanMonitorPage::closeDeviceRequested,
                canSessionController_,
                &CanSessionController::closeDevice);
        connect(canMonitorPage_,
                &CanMonitorPage::sendFrameRequested,
                canSessionController_,
                &CanSessionController::sendFrame);

        connect(canSessionController_,
                &CanSessionController::stateChanged,
                canMonitorPage_,
                &CanMonitorPage::setConnectionState);
        connect(canSessionController_,
                &CanSessionController::sourceModeChanged,
                canMonitorPage_,
                &CanMonitorPage::setSourceMode);
        connect(canSessionController_,
                &CanSessionController::sessionResetRequested,
                canMonitorPage_,
                &CanMonitorPage::resetSession);
        connect(canSessionController_,
                &CanSessionController::frameReceived,
                canMonitorPage_,
                &CanMonitorPage::appendFrame);
        connect(canSessionController_,
                &CanSessionController::frameTransmitted,
                canMonitorPage_,
                &CanMonitorPage::appendFrame);
        connect(canSessionController_,
                &CanSessionController::frameTransmitted,
                canMonitorPage_,
                [this](const CanFrame &) {
                    canMonitorPage_->setSendResult(QStringLiteral("发送成功"), true);
                });
        connect(canSessionController_,
                &CanSessionController::errorOccurred,
                canMonitorPage_,
                [this](const QString &message) {
                    canMonitorPage_->setLastError(message);
                    canMonitorPage_->setSendResult(message, false);
                });
    }

    connect(canSessionController_,
            &CanSessionController::frameReceived,
            this,
            &MainWindow::onCanFrameReceived);
    connect(canSessionController_,
            &CanSessionController::stateChanged,
            this,
            [this](CanConnectionState state) {
                const bool online = state == CanConnectionState::ChannelStarted;
                setCommunicationBadge(canStatusLabel_,
                                      QStringLiteral("CAN"),
                                      online,
                                      online ? QStringLiteral("CAN 通道运行中")
                                             : QStringLiteral("CAN 通道未启动"));
            });
    connect(canSessionController_,
            &CanSessionController::errorOccurred,
            this,
            [this](const QString &message) {
                setCommunicationBadge(canStatusLabel_,
                                      QStringLiteral("CAN"),
                                      false,
                                      message);
                statusBar()->showMessage(message, 5000);
            });

    setCommunicationBadge(canStatusLabel_,
                          QStringLiteral("CAN"),
                          false,
                          canSessionController_->platformCapabilityText());
}

void MainWindow::shutdownCommunication()
{
    if (canSessionController_ != nullptr) {
        canSessionController_->shutdown();
    }
}

void MainWindow::onCanFrameReceived(const CanFrame &frame)
{
    const BmsCanParser::UpdateFlags updates =
        bmsParser_.parse(frame, dashboardData_.bms);

    if (updates == BmsCanParser::NoUpdate) {
        return;
    }

    setCommunicationBadge(canStatusLabel_,
                          QStringLiteral("CAN"),
                          true,
                          QStringLiteral("已收到 BMS CAN 报文"));
    refreshBmsSummary();

    RuntimeSample sample;
    sample.minute = dashboardData_.runtimeSamples.isEmpty()
                        ? 0.0
                        : dashboardData_.runtimeSamples.last().minute + 0.02;
    sample.hvVoltage = dashboardData_.bms.packVoltage;
    sample.hvCurrent = dashboardData_.bms.packCurrent;
    sample.chargeCurrentLimit = dashboardData_.bms.chargeCurrentLimit;
    sample.dischargeCurrentLimit = dashboardData_.bms.dischargeCurrentLimit;

    dashboardData_.runtimeSamples.append(sample);
    while (dashboardData_.runtimeSamples.size() > 3000) {
        dashboardData_.runtimeSamples.removeFirst();
    }
}

void MainWindow::refreshBmsSummary()
{
    const BmsData &bms = dashboardData_.bms;

    setMetricValue(QStringLiteral("电池包电压"),
                   formatNumber(bms.packVoltage, 1, QStringLiteral("V")));
    setMetricValue(QStringLiteral("电池包电流"),
                   formatNumber(bms.packCurrent, 1, QStringLiteral("A")));
    setMetricValue(QStringLiteral("剩余容量 SOC"),
                   formatNumber(bms.soc, 1, QStringLiteral("%")));
    setMetricValue(QStringLiteral("健康度 SOH"),
                   formatNumber(bms.soh, 1, QStringLiteral("%")));
    setMetricValue(QStringLiteral("单体最高电压"),
                   formatNumber(bms.maxCellVoltage, 3, QStringLiteral("V")));
    setMetricValue(QStringLiteral("单体最低电压"),
                   formatNumber(bms.minCellVoltage, 3, QStringLiteral("V")));
    setMetricValue(QStringLiteral("单体最高温度"),
                   formatNumber(bms.maxCellTemperature, 1, QStringLiteral("℃")));
    setMetricValue(QStringLiteral("单体最低温度"),
                   formatNumber(bms.minCellTemperature, 1, QStringLiteral("℃")));
    setMetricValue(QStringLiteral("充电限流"),
                   formatNumber(bms.chargeCurrentLimit, 1, QStringLiteral("A")));
    setMetricValue(QStringLiteral("放电限流"),
                   formatNumber(bms.dischargeCurrentLimit, 1, QStringLiteral("A")));
}

void MainWindow::loadMockData()
{
    dashboardData_ = MockDataGenerator::createDashboardData();
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("dashboardRoot"));
    setupMainLayoutWithScrollArea(central);
    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("Stage 1: 1080p 适配版仪表盘已加载"), 4000);

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
        QFrame#runtimeCard {
            background-color: #ffffff;
            border: 1px solid #d7e0ea;
            border-radius: 8px;
        }
        QLabel#cardTitle {
            color: #5c6c7e;
            font-size: 10px;
            font-weight: 500;
        }
        QLabel#cardValue {
            color: #16324f;
            font-size: 14px;
            font-weight: 700;
        }
        QLabel#statusBadge {
            color: #ffffff;
            border-radius: 12px;
            padding: 4px 10px;
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
            padding: 7px 12px;
            font-weight: 700;
        }
        QPushButton:hover {
            background-color: #255f95;
        }
        QPushButton:checked {
            background-color: #173b63;
        }
        QSplitter::handle {
            background-color: #c8d6e5;
        }
        QSplitter::handle:hover {
            background-color: #0078d4;
        }
        QSplitter::handle:horizontal {
            width: 6px;
        }
        QSplitter::handle:vertical {
            height: 6px;
        }
    )"));
}

void MainWindow::setupMainLayoutWithScrollArea(QWidget *centralWidget)
{
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    auto *topBarWidget = setupTopBar();
    topBarWidget->setFixedHeight(64);
    mainLayout->addWidget(topBarWidget, 0);

    pageTabWidget_ = new QTabWidget(centralWidget);
    pageTabWidget_->setDocumentMode(true);
    pageTabWidget_->setTabPosition(QTabWidget::North);
    pageTabWidget_->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: 1px solid #c6d0dc; border-radius: 8px; background-color: #dde4ec; }"
        "QTabBar::tab { background-color: #e8eef5; color: #29415d; padding: 8px 18px; margin-right: 4px; border-top-left-radius: 6px; border-top-right-radius: 6px; font-weight: 700; }"
        "QTabBar::tab:selected { background-color: #ffffff; color: #173b63; }"));

    pageTabWidget_->addTab(createOverviewPage(), QStringLiteral("系统总览"));
    canMonitorPage_ = new CanMonitorPage(pageTabWidget_);
    pageTabWidget_->addTab(canMonitorPage_, QStringLiteral("CAN 通信监视"));
    mainLayout->addWidget(pageTabWidget_, 1);
}

QWidget *MainWindow::createOverviewPage()
{
    auto *page = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto *scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *dashboardContentWidget = new QWidget(scrollArea);
    dashboardContentWidget->setMinimumWidth(1040);
    dashboardContentWidget->setMinimumHeight(780);

    auto *dashboardLayout = new QVBoxLayout(dashboardContentWidget);
    dashboardLayout->setContentsMargins(0, 0, 2, 0);
    dashboardLayout->setSpacing(8);

    dashboardLayout->addWidget(createDashboardSplitterLayout(), 1);
    scrollArea->setWidget(dashboardContentWidget);

    pageLayout->addWidget(scrollArea, 1);
    return page;
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
    bar->setMinimumHeight(64);

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(10);

    auto *iconBlock = new QFrame(bar);
    iconBlock->setFixedSize(18, 18);
    iconBlock->setStyleSheet(QStringLiteral("background-color: #2a6fb0; border-radius: 4px;"));

    auto *titleLabel = new QLabel(QStringLiteral("BMS-DCDC 能量回收系统数据监控平台"), bar);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 21px; font-weight: 800; color: #1a2c42;"));

    auto *titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(10);
    titleLayout->addWidget(iconBlock);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto *titleContainer = new QWidget(bar);
    titleContainer->setLayout(titleLayout);

    canStatusLabel_ = createStatusBadge(QStringLiteral("CAN"), QStringLiteral("Offline"), QStringLiteral("#777777"));
    currentTimeLabel = new QLabel(bar);
    currentTimeLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; color: #29415d;"));

    auto *exportButton = new QPushButton(QStringLiteral("导出数据"), bar);
    exportButton->setMinimumWidth(104);
    connect(exportButton, &QPushButton::clicked, this, [this]() {
        statusBar()->showMessage(QStringLiteral("导出功能将在 CSV 模块接入后启用"), 3000);
    });

    layout->addWidget(titleContainer, 1);
    layout->addWidget(canStatusLabel_);
    layout->addWidget(currentTimeLabel);
    layout->addWidget(exportButton);

    return bar;
}

QSplitter *MainWindow::createDashboardSplitterLayout()
{
    auto *verticalSplitter = new QSplitter(Qt::Vertical, this);
    auto *topSplitter = createTopSplitter();
    auto *bottomSplitter = createBottomSplitter();

    verticalSplitter->setChildrenCollapsible(false);
    verticalSplitter->setHandleWidth(8);
    verticalSplitter->addWidget(topSplitter);
    verticalSplitter->addWidget(bottomSplitter);

    configureSplitterRatios(verticalSplitter, topSplitter, bottomSplitter);
    return verticalSplitter;
}

QSplitter *MainWindow::createTopSplitter()
{
    auto *topSplitter = new QSplitter(Qt::Horizontal, this);
    topSplitter->setChildrenCollapsible(false);
    topSplitter->setHandleWidth(8);
    topSplitter->addWidget(setupTopLeftSummaryArea());
    topSplitter->addWidget(setupTopRightTableArea());
    return topSplitter;
}

QSplitter *MainWindow::createBottomSplitter()
{
    auto *bottomSplitter = new QSplitter(Qt::Horizontal, this);
    bottomSplitter->setChildrenCollapsible(false);
    bottomSplitter->setHandleWidth(8);
    bottomSplitter->addWidget(setupBottomLeftChartArea());
    bottomSplitter->addWidget(setupBottomRightVoltageArea());
    return bottomSplitter;
}

QSplitter *MainWindow::createSummaryInnerSplitter()
{
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(6);
    splitter->addWidget(createBmsSummaryGroup());
    splitter->addWidget(createDcdcSummaryGroup());
    return splitter;
}

QSplitter *MainWindow::createTableStatsSplitter()
{
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(6);
    auto *tableContainer = new QWidget(splitter);
    tableContainer->setMinimumWidth(420);
    tableContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *tableLayout = new QVBoxLayout(tableContainer);
    tableLayout->setContentsMargins(4, 4, 4, 4);
    tableLayout->setSpacing(2);
    tableLayout->addWidget(createVoltageTemperatureTable());

    auto *statisticsPanel = createStatisticsPanel();
    statisticsPanel->setMinimumWidth(220);
    statisticsPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    splitter->addWidget(tableContainer);
    splitter->addWidget(statisticsPanel);
    return splitter;
}

QSplitter *MainWindow::createRuntimeInnerSplitter()
{
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(6);

    runtimeChartStack_ = new QStackedWidget(splitter);
    runtimeChartStack_->setMinimumHeight(300);
    runtimeChartStack_->addWidget(createOverviewRuntimeChart());
    runtimeChartStack_->addWidget(createSingleRuntimeChartPage());

    runtimeCardsStack_ = new QStackedWidget(splitter);
    runtimeCardsStack_->setMinimumHeight(90);
    runtimeCardsStack_->setMaximumHeight(220);

    auto *overviewCardsPlaceholder = new QWidget(runtimeCardsStack_);
    auto *overviewCardsLayout = new QVBoxLayout(overviewCardsPlaceholder);
    overviewCardsLayout->setContentsMargins(0, 0, 0, 0);
    auto *overviewInfo = new QLabel(QStringLiteral("总览模式下显示综合趋势图。切换到单图卡片模式后，可在下方选择单个运行量。"),
                                    overviewCardsPlaceholder);
    overviewInfo->setWordWrap(true);
    overviewInfo->setStyleSheet(QStringLiteral(
        "background-color: #ffffff; border: 1px solid #d7e0ea; border-radius: 6px; padding: 10px; color: #4e5d6c;"));
    overviewCardsLayout->addWidget(overviewInfo);

    runtimeCardsStack_->addWidget(overviewCardsPlaceholder);
    runtimeCardsStack_->addWidget(createRuntimeMetricCards());

    splitter->addWidget(runtimeChartStack_);
    splitter->addWidget(runtimeCardsStack_);
    return splitter;
}

QSplitter *MainWindow::createVoltageInnerSplitter()
{
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(6);
    splitter->addWidget(createCellVoltageOverviewChart());
    splitter->addWidget(createGroupedVoltageCardsScrollArea());
    splitter->addWidget(createCompactConsistencyPanel());
    return splitter;
}

void MainWindow::configureSplitterRatios(QSplitter *verticalSplitter,
                                         QSplitter *topSplitter,
                                         QSplitter *bottomSplitter)
{
    verticalSplitter->setStretchFactor(0, 35);
    verticalSplitter->setStretchFactor(1, 65);
    topSplitter->setStretchFactor(0, 58);
    topSplitter->setStretchFactor(1, 42);
    bottomSplitter->setStretchFactor(0, 58);
    bottomSplitter->setStretchFactor(1, 42);

    verticalSplitter->setSizes({280, 520});
    topSplitter->setSizes({660, 480});
    bottomSplitter->setSizes({660, 480});
}

void MainWindow::configureInnerSplitterRatios(QSplitter *summarySplitter,
                                              QSplitter *tableStatsSplitter,
                                              QSplitter *runtimeSplitter,
                                              QSplitter *voltageSplitter)
{
    if (summarySplitter != nullptr) {
        summarySplitter->setStretchFactor(0, 50);
        summarySplitter->setStretchFactor(1, 50);
        summarySplitter->setSizes({150, 150});
    }

    if (tableStatsSplitter != nullptr) {
        tableStatsSplitter->setStretchFactor(0, 68);
        tableStatsSplitter->setStretchFactor(1, 32);
        tableStatsSplitter->setSizes({470, 220});
    }

    if (runtimeSplitter != nullptr) {
        runtimeSplitter->setStretchFactor(0, 75);
        runtimeSplitter->setStretchFactor(1, 25);
        runtimeSplitter->setSizes({390, 130});
    }

    if (voltageSplitter != nullptr) {
        voltageSplitter->setStretchFactor(0, 35);
        voltageSplitter->setStretchFactor(1, 40);
        voltageSplitter->setStretchFactor(2, 25);
        voltageSplitter->setSizes({210, 240, 150});
    }
}

QGroupBox *MainWindow::setupTopLeftSummaryArea()
{
    auto *group = new QGroupBox(QStringLiteral("核心数据汇总卡片区"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    auto *splitter = createSummaryInnerSplitter();
    layout->addWidget(splitter);
    configureInnerSplitterRatios(splitter, nullptr, nullptr, nullptr);
    group->setMinimumSize(560, 260);
    return group;
}

QGroupBox *MainWindow::setupTopRightTableArea()
{
    auto *group = new QGroupBox(QStringLiteral("电压温度列表与统计信息"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    auto *splitter = createTableStatsSplitter();
    layout->addWidget(splitter);
    configureInnerSplitterRatios(nullptr, splitter, nullptr, nullptr);
    group->setMinimumSize(420, 260);
    return group;
}

QGroupBox *MainWindow::setupBottomLeftChartArea()
{
    auto *group = new QGroupBox(QStringLiteral("运行数据图卡片区"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    layout->addWidget(setupRuntimeChartToolbar());
    auto *splitter = createRuntimeInnerSplitter();
    layout->addWidget(splitter, 1);
    configureInnerSplitterRatios(nullptr, nullptr, splitter, nullptr);
    group->setMinimumSize(560, 480);
    return group;
}

QGroupBox *MainWindow::setupBottomRightVoltageArea()
{
    auto *group = new QGroupBox(QStringLiteral("单体电压图表区"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    auto *scrollArea = new QScrollArea(group);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(createBottomRightScrollContent());

    layout->addWidget(scrollArea, 1);
    group->setMinimumSize(420, 480);
    return group;
}

QWidget *MainWindow::setupRuntimeChartToolbar()
{
    auto *toolbar = new QWidget(this);
    auto *layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *titleLabel = new QLabel(QStringLiteral("关键运行量趋势"), toolbar);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; color: #29415d;"));
    layout->addWidget(titleLabel);

    auto *modeGroup = new QButtonGroup(toolbar);
    modeGroup->setExclusive(true);

    auto *overviewButton = new QPushButton(QStringLiteral("总览"), toolbar);
    overviewButton->setCheckable(true);
    overviewButton->setChecked(true);
    modeGroup->addButton(overviewButton, 0);

    auto *singleButton = new QPushButton(QStringLiteral("单图卡片"), toolbar);
    singleButton->setCheckable(true);
    modeGroup->addButton(singleButton, 1);

    layout->addSpacing(8);
    layout->addWidget(overviewButton);
    layout->addWidget(singleButton);
    layout->addStretch();

    auto *rangeGroup = new QButtonGroup(toolbar);
    rangeGroup->setExclusive(true);
    const QStringList ranges = {
        QStringLiteral("1 min"),
        QStringLiteral("5 min"),
        QStringLiteral("30 min"),
        QStringLiteral("1 h"),
        QStringLiteral("全部")
    };

    for (int index = 0; index < ranges.size(); ++index) {
        auto *button = new QPushButton(ranges.at(index), toolbar);
        button->setCheckable(true);
        button->setMinimumWidth(54);
        if (index == 0) {
            button->setChecked(true);
        }
        rangeGroup->addButton(button, index);
        layout->addWidget(button);
    }

    connect(modeGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int index) {
        if (runtimeChartStack_ != nullptr) {
            runtimeChartStack_->setCurrentIndex(index);
            if (runtimeCardsStack_ != nullptr) {
                runtimeCardsStack_->setCurrentIndex(index);
            }
            statusBar()->showMessage(index == 0
                                         ? QStringLiteral("已切换到运行数据总览模式")
                                         : QStringLiteral("已切换到运行数据单图卡片模式"),
                                     2000);
        }
    });

    connect(rangeGroup,
            QOverload<int>::of(&QButtonGroup::idClicked),
            this,
            [this, ranges](int index) {
                statusBar()->showMessage(QStringLiteral("当前为静态假曲线，时间范围切换到 %1").arg(ranges.at(index)),
                                         2000);
            });

    return toolbar;
}

QGroupBox *MainWindow::createBmsSummaryGroup()
{
    auto *group = new QGroupBox(QStringLiteral("BMS 数据汇总"), this);
    auto *layout = new QGridLayout(group);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(6);

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
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(6);

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
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setDefaultSectionSize(26);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table->setMinimumHeight(260);

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
    group->setMinimumWidth(220);
    group->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *layout = new QGridLayout(group);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(6);

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
    layout->setSpacing(8);
    auto *splitter = createRuntimeInnerSplitter();
    layout->addWidget(splitter, 1);
    configureInnerSplitterRatios(nullptr, nullptr, splitter, nullptr);
    return container;
}

QWidget *MainWindow::createOverviewRuntimeChart()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *chartView = createRuntimeChartView(dashboardData_.runtimeSamples, {0, 1, 2, 3, 4, 5, 6}, false, page);
    chartView->setMinimumHeight(360);
    chartView->setMaximumHeight(430);
    layout->addWidget(chartView, 1);
    return page;
}

QWidget *MainWindow::createSingleRuntimeChartPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    singleRuntimeChartView_ = createRuntimeChartView(dashboardData_.runtimeSamples, {0}, true, page);
    singleRuntimeChartView_->setMinimumHeight(360);
    singleRuntimeChartView_->setMaximumHeight(430);

    layout->addWidget(singleRuntimeChartView_, 1);
    layout->addWidget(createRuntimeMetricCards(), 0);
    return page;
}

QWidget *MainWindow::createRuntimeMetricCards()
{
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setMinimumHeight(104);
    scrollArea->setMaximumHeight(120);

    auto *content = new QWidget(scrollArea);
    auto *layout = new QHBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    runtimeMetricButtonGroup_ = new QButtonGroup(scrollArea);
    runtimeMetricButtonGroup_->setExclusive(true);

    const auto &definitions = runtimeMetricDefinitions();
    const RuntimeSample &latestSample = dashboardData_.runtimeSamples.last();

    for (int index = 0; index < definitions.size(); ++index) {
        const RuntimeMetricDefinition &definition = definitions.at(index);
        const QString buttonText = QStringLiteral("%1\n%2 %3")
                                       .arg(definition.title,
                                            QString::number(definition.accessor(latestSample), 'f', definition.unit == QStringLiteral("℃") ? 1 : 1),
                                            definition.unit);

        auto *button = new QPushButton(buttonText, content);
        button->setCheckable(true);
        button->setMinimumSize(132, 82);
        button->setMaximumHeight(86);
        button->setStyleSheet(QStringLiteral(
            "QPushButton { text-align: left; padding: 8px 10px; line-height: 1.35; font-size: 12px; }"
            "QPushButton:checked { background-color: #173b63; border: 2px solid #8fc4ff; }"));
        if (index == 0) {
            button->setChecked(true);
        }

        runtimeMetricButtonGroup_->addButton(button, index);
        layout->addWidget(button);
    }

    layout->addStretch();
    scrollArea->setWidget(content);

    connect(runtimeMetricButtonGroup_,
            QOverload<int>::of(&QButtonGroup::idClicked),
            this,
            &MainWindow::switchRuntimeMetricChart);

    return scrollArea;
}

void MainWindow::switchRuntimeMetricChart(int metricIndex)
{
    currentRuntimeMetricIndex_ = metricIndex;

    if (singleRuntimeChartView_ == nullptr) {
        return;
    }

    auto *newChartView = createRuntimeChartView(dashboardData_.runtimeSamples, {metricIndex}, true, singleRuntimeChartView_->parentWidget());
    newChartView->setMinimumHeight(singleRuntimeChartView_->minimumHeight());

    auto *layout = qobject_cast<QVBoxLayout *>(singleRuntimeChartView_->parentWidget()->layout());
    if (layout == nullptr) {
        delete newChartView;
        return;
    }

    layout->replaceWidget(singleRuntimeChartView_, newChartView);
    singleRuntimeChartView_->deleteLater();
    singleRuntimeChartView_ = newChartView;

    statusBar()->showMessage(QStringLiteral("单图模式已切换到 %1").arg(runtimeMetricDefinitions().at(metricIndex).title),
                             1800);
}

QWidget *MainWindow::createCellVoltageChartPanel()
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(createCellVoltageOverviewChart(), 2);
    layout->addWidget(createGroupedVoltageCardsScrollArea(), 2);
    return container;
}

QWidget *MainWindow::createBottomRightScrollContent()
{
    auto *content = new QWidget(this);
    content->setMinimumWidth(380);

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto *splitter = createVoltageInnerSplitter();
    layout->addWidget(splitter);
    configureInnerSplitterRatios(nullptr, nullptr, nullptr, splitter);
    return content;
}

QWidget *MainWindow::createCellVoltageOverviewChart()
{
    auto *group = new QGroupBox(QStringLiteral("单体电压总览图"), this);
    group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(6, 6, 6, 6);
    auto *chartView = createCellVoltageBarChartView(dashboardData_.cells, group, 0, false);
    chartView->setMinimumHeight(190);
    chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(chartView);
    group->setMinimumHeight(205);
    return group;
}

QWidget *MainWindow::createGroupedCellVoltageCards()
{
    auto *group = new QGroupBox(QStringLiteral("分组单体图表滚动区"), this);
    group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *outerLayout = new QVBoxLayout(group);
    outerLayout->setContentsMargins(6, 6, 6, 6);

    auto *scrollArea = new QScrollArea(group);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setMinimumHeight(220);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *content = new QWidget(scrollArea);
    auto *layout = new QHBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    const int groupSize = 8;
    for (int start = 0; start < dashboardData_.cells.size(); start += groupSize) {
        const int end = qMin(start + groupSize, dashboardData_.cells.size());
        QVector<CellRecord> groupCells;
        for (int index = start; index < end; ++index) {
            groupCells.append(dashboardData_.cells.at(index));
        }

        auto *card = new QFrame(content);
        card->setObjectName(QStringLiteral("runtimeCard"));
        card->setMinimumWidth(280);
        card->setMaximumWidth(320);
        card->setMinimumHeight(190);
        card->setMaximumHeight(220);

        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(6, 6, 6, 6);
        cardLayout->setSpacing(4);

        auto *title = new QLabel(
            QStringLiteral("Cell%1-%2")
                .arg(start + 1, 2, 10, QLatin1Char('0'))
                .arg(end, 2, 10, QLatin1Char('0')),
            card);
        title->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 700; color: #29415d;"));

        cardLayout->addWidget(title);
        auto *chartView = createCellVoltageBarChartView(groupCells, card, 260, true);
        chartView->setMinimumHeight(150);
        chartView->setMaximumHeight(170);
        cardLayout->addWidget(chartView);
        layout->addWidget(card);
    }

    layout->addStretch();
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
    return group;
}

QWidget *MainWindow::createConsistencyPanel()
{
    return createCompactConsistencyPanel();
}

QWidget *MainWindow::createGroupedVoltageCardsScrollArea()
{
    auto *group = createGroupedCellVoltageCards();
    group->setMinimumHeight(220);
    return group;
}

QWidget *MainWindow::createCompactConsistencyPanel()
{
    const CellStatistics stats = calculateCellStatistics();

    auto *group = new QGroupBox(QStringLiteral("一致性状态判断"), this);
    group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *layout = new QGridLayout(group);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(5);

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
    group->setMinimumHeight(132);
    return group;
}

QWidget *MainWindow::createMetricCard(const QString &title,
                                      const QString &value,
                                      const QString &accentColor) const
{
    auto *card = new QFrame();
    card->setObjectName(QStringLiteral("metricCard"));
    card->setMinimumHeight(42);
    card->setMaximumHeight(52);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(3);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("cardTitle"));
    titleLabel->setWordWrap(true);

    auto *valueLabel = new QLabel(value, card);
    valueLabel->setObjectName(QStringLiteral("cardValue"));
    valueLabel->setWordWrap(true);

    if (!accentColor.isEmpty()) {
        valueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 14px; font-weight: 700;").arg(accentColor));
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

void MainWindow::setMetricValue(const QString &metricTitle, const QString &value)
{
    const auto cards = findChildren<QFrame *>();

    for (QFrame *card : cards) {
        if (card->objectName() != QStringLiteral("metricCard") ||
            card->property("metricTitle").toString() != metricTitle) {
            continue;
        }

        const auto labels = card->findChildren<QLabel *>();
        for (QLabel *label : labels) {
            if (label->objectName() == QStringLiteral("cardValue")) {
                label->setText(value);
                return;
            }
        }
    }
}

void MainWindow::setCommunicationBadge(QLabel *badge,
                                       const QString &channel,
                                       bool online,
                                       const QString &detail)
{
    if (badge == nullptr) {
        return;
    }

    badge->setText(QStringLiteral("%1  %2")
                       .arg(channel,
                            online ? QStringLiteral("Online")
                                   : QStringLiteral("Offline")));
    badge->setStyleSheet(
        QStringLiteral("background-color: %1;")
            .arg(online ? QStringLiteral("#1f8f5f")
                        : QStringLiteral("#777777")));
    if (!detail.isEmpty()) {
        badge->setToolTip(detail);
    }
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

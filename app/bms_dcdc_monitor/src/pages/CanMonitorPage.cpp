#include "pages/CanMonitorPage.h"

#include "communication/CanFrameInputParser.h"
#include "models/CanFrameTableModel.h"

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

CanMonitorPage::CanMonitorPage(QWidget *parent)
    : QWidget(parent),
      model_(new CanFrameTableModel(this)),
      filterProxyModel_(new CanFrameFilterProxyModel(this)),
      tableView_(new QTableView(this)),
      statisticsTimer_(new QTimer(this))
{
    filterProxyModel_->setSourceModel(model_);
    filterProxyModel_->setDynamicSortFilter(true);

    setupUi();
    updatePauseButton();
    updateSourceStatusLabel();
    updateCachedFrameCount();
    updateDisplayedFrameCount();
    updateStatistics();
    updateCanIdFilterState();

    statisticsTimer_->setInterval(250);
    connect(statisticsTimer_, &QTimer::timeout,
            this, &CanMonitorPage::updateStatistics);
    statisticsTimer_->start();
}

void CanMonitorPage::setDataSourceStatus(const QString &sourceName,
                                         const QString &message,
                                         bool online)
{
    sourceName_ = sourceName;
    sourceStatusMessage_ = message;
    sourceOnline_ = online;
    updateSourceStatusLabel();
}

void CanMonitorPage::clearFrames()
{
    model_->clear();
    updateCachedFrameCount();
    updateDisplayedFrameCount();
}

void CanMonitorPage::resetSession()
{
    model_->clear();
    totalRxCount_ = 0;
    totalTxCount_ = 0;
    updateCachedFrameCount();
    updateDisplayedFrameCount();
    updateStatistics();
}

void CanMonitorPage::setPaused(bool paused)
{
    model_->setPaused(paused);
    updatePauseButton();
    updateSourceStatusLabel();
}

bool CanMonitorPage::isPaused() const
{
    return model_->isPaused();
}

void CanMonitorPage::setConnectionState(CanConnectionState state)
{
    connectionState_ = state;
    switch (state) {
    case CanConnectionState::Disconnected:
        sourceStatusMessage_ = QStringLiteral("未连接");
        sourceOnline_ = false;
        break;
    case CanConnectionState::DeviceOpened:
        sourceStatusMessage_ = QStringLiteral("设备已打开");
        sourceOnline_ = false;
        break;
    case CanConnectionState::ChannelStarted:
        sourceStatusMessage_ = QStringLiteral("通道运行中");
        sourceOnline_ = true;
        break;
    case CanConnectionState::Unsupported:
        sourceStatusMessage_ = QStringLiteral("当前平台不支持硬件驱动");
        sourceOnline_ = false;
        break;
    case CanConnectionState::Error:
        sourceStatusMessage_ = QStringLiteral("错误");
        sourceOnline_ = false;
        break;
    default:
        sourceStatusMessage_ = QStringLiteral("状态转换中");
        sourceOnline_ = false;
        break;
    }
    updateSourceStatusLabel();
    updateConnectionControls();
    updateRawSendControls();
}

void CanMonitorPage::setSourceMode(CanSourceMode mode)
{
    const int index = mode == CanSourceMode::Hardware ? 1 : 0;
    if (sourceModeComboBox_ != nullptr && sourceModeComboBox_->currentIndex() != index) {
        sourceModeComboBox_->setCurrentIndex(index);
    }

    sourceName_ = mode == CanSourceMode::Hardware
                      ? QStringLiteral("Hardware")
                      : QStringLiteral("Mock");
    updateSourceStatusLabel();
}

void CanMonitorPage::setPlatformCapabilityText(const QString &text)
{
    if (platformCapabilityValueLabel_ != nullptr) {
        platformCapabilityValueLabel_->setText(text);
        platformCapabilityValueLabel_->setToolTip(text);
    }
}

void CanMonitorPage::setLastError(const QString &message)
{
    if (lastErrorValueLabel_ != nullptr) {
        lastErrorValueLabel_->setText(message.isEmpty() ? QStringLiteral("-") : message);
        lastErrorValueLabel_->setToolTip(message);
    }
}

void CanMonitorPage::setSendResult(const QString &message, bool success)
{
    if (sendResultValueLabel_ == nullptr) {
        return;
    }

    sendResultValueLabel_->setText(message.isEmpty() ? QStringLiteral("-") : message);
    sendResultValueLabel_->setStyleSheet(
        QStringLiteral("color: %1; font-weight: 700;")
            .arg(success ? QStringLiteral("#1f8f5f") : QStringLiteral("#b54708")));
    sendResultValueLabel_->setToolTip(message);
}

void CanMonitorPage::appendFrame(const CanFrame &frame)
{
    if (frame.direction == CanFrameDirection::Tx) {
        ++totalTxCount_;
    } else {
        ++totalRxCount_;
    }

    model_->appendFrame(frame);
    updateCachedFrameCount();
}

void CanMonitorPage::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

    rootLayout->addWidget(createConnectionPanel());
    rootLayout->addWidget(createRawSendPanel());
    rootLayout->addWidget(createMonitorToolbar());

    tableView_->setModel(filterProxyModel_);
    tableView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    tableView_->setWordWrap(false);
    tableView_->setCornerButtonEnabled(false);
    tableView_->setShowGrid(false);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->horizontalHeader()->setSectionsMovable(true);
    tableView_->horizontalHeader()->setStretchLastSection(false);
    tableView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tableView_->horizontalHeader()->setSectionResizeMode(CanFrameTableModel::DataColumn,
                                                         QHeaderView::Stretch);
    tableView_->setStyleSheet(QStringLiteral(
        "QTableView { background-color: #ffffff; border: 1px solid #d7e0ea; border-radius: 8px; gridline-color: #e6ebf1; alternate-background-color: #f3f6fa; }"
        "QHeaderView::section { background-color: #e8eef5; color: #29415d; padding: 6px; border: none; border-right: 1px solid #d7e0ea; border-bottom: 1px solid #d7e0ea; font-weight: 700; }"));
    tableView_->horizontalHeader()->resizeSection(CanFrameTableModel::SequenceColumn, 72);
    tableView_->horizontalHeader()->resizeSection(CanFrameTableModel::TimeColumn, 200);
    tableView_->horizontalHeader()->resizeSection(CanFrameTableModel::DirectionColumn, 72);
    tableView_->horizontalHeader()->resizeSection(CanFrameTableModel::ChannelColumn, 72);
    tableView_->horizontalHeader()->resizeSection(CanFrameTableModel::CanIdColumn, 110);
    tableView_->horizontalHeader()->resizeSection(CanFrameTableModel::FrameFormatColumn, 88);
    tableView_->horizontalHeader()->resizeSection(CanFrameTableModel::FrameTypeColumn, 88);
    tableView_->horizontalHeader()->resizeSection(CanFrameTableModel::DlcColumn, 72);
    tableView_->setMinimumHeight(420);

    connect(filterProxyModel_,
            &QAbstractItemModel::rowsAboutToBeInserted,
            this,
            [this]() {
                pendingScrollToBottom_ = isViewingLatest();
            });

    connect(filterProxyModel_,
            &QAbstractItemModel::rowsInserted,
            this,
            [this]() {
                updateDisplayedFrameCount();
                if (pendingScrollToBottom_) {
                    QTimer::singleShot(0, this, [this]() {
                        tableView_->scrollToBottom();
                    });
                }
            });

    connect(filterProxyModel_,
            &QAbstractItemModel::rowsRemoved,
            this,
            [this]() {
                updateCachedFrameCount();
                updateDisplayedFrameCount();
            });

    connect(filterProxyModel_,
            &QAbstractItemModel::modelReset,
            this,
            [this]() {
                updateCachedFrameCount();
                updateDisplayedFrameCount();
            });

    connect(tableView_->verticalScrollBar(),
            &QScrollBar::valueChanged,
            this,
            [this](int) {
                pendingScrollToBottom_ = isViewingLatest();
            });

    rootLayout->addWidget(tableView_, 1);
    updateConnectionControls();
    updateRawSendControls();
}

QWidget *CanMonitorPage::createConnectionPanel()
{
    auto *group = new QGroupBox(QStringLiteral("CAN 设备连接"), this);
    auto *layout = new QGridLayout(group);
    layout->setContentsMargins(10, 16, 10, 10);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);

    sourceModeComboBox_ = new QComboBox(group);
    sourceModeComboBox_->addItem(QStringLiteral("Mock"), static_cast<int>(CanSourceMode::Mock));
    sourceModeComboBox_->addItem(QStringLiteral("Hardware"), static_cast<int>(CanSourceMode::Hardware));

    driverTypeComboBox_ = new QComboBox(group);
    driverTypeComboBox_->addItem(QStringLiteral("Mock"), static_cast<int>(CanDriverType::Mock));
    driverTypeComboBox_->addItem(QStringLiteral("ZLG ControlCAN"), static_cast<int>(CanDriverType::ZlgControlCan));

    connect(sourceModeComboBox_,
            &QComboBox::currentIndexChanged,
            this,
            [this](int index) {
                driverTypeComboBox_->setCurrentIndex(index);
            });
    connect(driverTypeComboBox_,
            &QComboBox::currentIndexChanged,
            this,
            [this](int index) {
                sourceModeComboBox_->setCurrentIndex(index == 1 ? 1 : 0);
            });

    deviceTypeSpinBox_ = new QSpinBox(group);
    deviceTypeSpinBox_->setRange(0, 100);
    deviceTypeSpinBox_->setValue(4);

    deviceIndexSpinBox_ = new QSpinBox(group);
    deviceIndexSpinBox_->setRange(0, 16);

    connectionChannelSpinBox_ = new QSpinBox(group);
    connectionChannelSpinBox_->setRange(0, 1);

    bitrateComboBox_ = new QComboBox(group);
    bitrateComboBox_->addItem(QStringLiteral("125000"), 125000);
    bitrateComboBox_->addItem(QStringLiteral("250000"), 250000);
    bitrateComboBox_->addItem(QStringLiteral("500000"), 500000);
    bitrateComboBox_->addItem(QStringLiteral("1000000"), 1000000);
    bitrateComboBox_->setCurrentIndex(2);

    workModeComboBox_ = new QComboBox(group);
    workModeComboBox_->addItem(QStringLiteral("正常"), false);
    workModeComboBox_->addItem(QStringLiteral("只听"), true);

    openDeviceButton_ = new QPushButton(QStringLiteral("打开设备"), group);
    startChannelButton_ = new QPushButton(QStringLiteral("启动通道"), group);
    stopChannelButton_ = new QPushButton(QStringLiteral("停止通道"), group);
    closeDeviceButton_ = new QPushButton(QStringLiteral("关闭设备"), group);

    connect(openDeviceButton_, &QPushButton::clicked, this, [this]() {
        emit openDeviceRequested(currentConnectionConfig());
    });
    connect(startChannelButton_, &QPushButton::clicked,
            this, &CanMonitorPage::startChannelRequested);
    connect(stopChannelButton_, &QPushButton::clicked,
            this, &CanMonitorPage::stopChannelRequested);
    connect(closeDeviceButton_, &QPushButton::clicked,
            this, &CanMonitorPage::closeDeviceRequested);

    connectionStateValueLabel_ = new QLabel(group);
    platformCapabilityValueLabel_ = new QLabel(QStringLiteral("-"), group);
    lastErrorValueLabel_ = new QLabel(QStringLiteral("-"), group);
    lastErrorValueLabel_->setStyleSheet(QStringLiteral("color: #b54708; font-weight: 700;"));

    layout->addWidget(new QLabel(QStringLiteral("数据源模式"), group), 0, 0);
    layout->addWidget(sourceModeComboBox_, 0, 1);
    layout->addWidget(new QLabel(QStringLiteral("驱动类型"), group), 0, 2);
    layout->addWidget(driverTypeComboBox_, 0, 3);
    layout->addWidget(new QLabel(QStringLiteral("设备类型"), group), 0, 4);
    layout->addWidget(deviceTypeSpinBox_, 0, 5);
    layout->addWidget(new QLabel(QStringLiteral("设备索引"), group), 0, 6);
    layout->addWidget(deviceIndexSpinBox_, 0, 7);

    layout->addWidget(new QLabel(QStringLiteral("CAN 通道"), group), 1, 0);
    layout->addWidget(connectionChannelSpinBox_, 1, 1);
    layout->addWidget(new QLabel(QStringLiteral("波特率"), group), 1, 2);
    layout->addWidget(bitrateComboBox_, 1, 3);
    layout->addWidget(new QLabel(QStringLiteral("工作模式"), group), 1, 4);
    layout->addWidget(workModeComboBox_, 1, 5);
    layout->addWidget(openDeviceButton_, 1, 6);
    layout->addWidget(startChannelButton_, 1, 7);
    layout->addWidget(stopChannelButton_, 1, 8);
    layout->addWidget(closeDeviceButton_, 1, 9);

    layout->addWidget(new QLabel(QStringLiteral("当前状态"), group), 2, 0);
    layout->addWidget(connectionStateValueLabel_, 2, 1, 1, 2);
    layout->addWidget(new QLabel(QStringLiteral("平台能力"), group), 2, 3);
    layout->addWidget(platformCapabilityValueLabel_, 2, 4, 1, 3);
    layout->addWidget(new QLabel(QStringLiteral("最近错误"), group), 2, 7);
    layout->addWidget(lastErrorValueLabel_, 2, 8, 1, 2);

    return group;
}

QWidget *CanMonitorPage::createRawSendPanel()
{
    auto *group = new QGroupBox(QStringLiteral("原始帧发送"), this);
    auto *layout = new QGridLayout(group);
    layout->setContentsMargins(10, 16, 10, 10);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);

    sendChannelSpinBox_ = new QSpinBox(group);
    sendChannelSpinBox_->setRange(0, 1);
    sendCanIdLineEdit_ = new QLineEdit(QStringLiteral("18E10101"), group);

    sendFrameFormatComboBox_ = new QComboBox(group);
    sendFrameFormatComboBox_->addItem(QStringLiteral("扩展帧"), true);
    sendFrameFormatComboBox_->addItem(QStringLiteral("标准帧"), false);

    sendFrameTypeComboBox_ = new QComboBox(group);
    sendFrameTypeComboBox_->addItem(QStringLiteral("数据帧"), false);
    sendFrameTypeComboBox_->addItem(QStringLiteral("远程帧"), true);

    sendDataLineEdit_ = new QLineEdit(QStringLiteral("01 02 03 04 05 06 07 08"), group);
    sendDlcValueLabel_ = new QLabel(QStringLiteral("8"), group);
    sendValidationValueLabel_ = new QLabel(QStringLiteral("-"), group);
    sendResultValueLabel_ = new QLabel(QStringLiteral("-"), group);

    sendFrameButton_ = new QPushButton(QStringLiteral("单次发送"), group);
    clearSendInputButton_ = new QPushButton(QStringLiteral("清空输入"), group);

    connect(sendFrameButton_, &QPushButton::clicked, this, [this]() {
        const CanFrameInputParseResult result =
            CanFrameInputParser::parse({
                sendCanIdLineEdit_->text(),
                sendDataLineEdit_->text(),
                sendChannelSpinBox_->value(),
                sendFrameFormatComboBox_->currentData().toBool(),
                sendFrameTypeComboBox_->currentData().toBool()
            });
        if (!result.ok) {
            setSendResult(result.errorMessage, false);
            validateRawFrameInput();
            return;
        }

        emit sendFrameRequested(result.frame);
    });

    connect(clearSendInputButton_, &QPushButton::clicked, this, [this]() {
        sendCanIdLineEdit_->clear();
        sendDataLineEdit_->clear();
        setSendResult(QStringLiteral("-"), true);
        validateRawFrameInput();
    });

    const auto validate = [this]() {
        validateRawFrameInput();
    };
    connect(sendCanIdLineEdit_, &QLineEdit::textChanged, this, validate);
    connect(sendDataLineEdit_, &QLineEdit::textChanged, this, validate);
    connect(sendFrameFormatComboBox_, &QComboBox::currentIndexChanged, this, validate);
    connect(sendFrameTypeComboBox_, &QComboBox::currentIndexChanged, this, validate);
    connect(sendChannelSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this, validate);

    layout->addWidget(new QLabel(QStringLiteral("通道"), group), 0, 0);
    layout->addWidget(sendChannelSpinBox_, 0, 1);
    layout->addWidget(new QLabel(QStringLiteral("CAN ID"), group), 0, 2);
    layout->addWidget(sendCanIdLineEdit_, 0, 3);
    layout->addWidget(new QLabel(QStringLiteral("帧格式"), group), 0, 4);
    layout->addWidget(sendFrameFormatComboBox_, 0, 5);
    layout->addWidget(new QLabel(QStringLiteral("帧类型"), group), 0, 6);
    layout->addWidget(sendFrameTypeComboBox_, 0, 7);
    layout->addWidget(new QLabel(QStringLiteral("DLC"), group), 0, 8);
    layout->addWidget(sendDlcValueLabel_, 0, 9);

    layout->addWidget(new QLabel(QStringLiteral("Data"), group), 1, 0);
    layout->addWidget(sendDataLineEdit_, 1, 1, 1, 5);
    layout->addWidget(sendFrameButton_, 1, 6);
    layout->addWidget(clearSendInputButton_, 1, 7);
    layout->addWidget(new QLabel(QStringLiteral("校验提示"), group), 2, 0);
    layout->addWidget(sendValidationValueLabel_, 2, 1, 1, 4);
    layout->addWidget(new QLabel(QStringLiteral("最近发送结果"), group), 2, 5);
    layout->addWidget(sendResultValueLabel_, 2, 6, 1, 4);

    validateRawFrameInput();
    return group;
}

QWidget *CanMonitorPage::createMonitorToolbar()
{
    auto *toolbarFrame = new QFrame(this);
    toolbarFrame->setStyleSheet(QStringLiteral("QFrame { background-color: #f7f9fc; border: 1px solid #d7e0ea; border-radius: 8px; }"));

    auto *toolbarLayout = new QHBoxLayout(toolbarFrame);
    toolbarLayout->setContentsMargins(12, 10, 12, 10);
    toolbarLayout->setSpacing(8);

    auto *sourceTitleLabel = new QLabel(QStringLiteral("数据源状态"), toolbarFrame);
    sourceTitleLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #29415d;"));
    sourceStatusValueLabel_ = new QLabel(toolbarFrame);
    updateSourceStatusLabel();

    pauseButton_ = new QPushButton(toolbarFrame);
    connect(pauseButton_, &QPushButton::clicked, this, [this]() {
        setPaused(!isPaused());
    });

    auto *clearButton = new QPushButton(QStringLiteral("清空缓存"), toolbarFrame);
    clearButton->setToolTip(QStringLiteral("只清空当前表格缓存，累计 RX/TX 总数保留"));
    connect(clearButton, &QPushButton::clicked,
            this, &CanMonitorPage::clearFrames);

    auto *canIdFilterLabel = new QLabel(QStringLiteral("CAN ID"), toolbarFrame);
    canIdFilterLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #29415d;"));
    canIdFilterLineEdit_ = new QLineEdit(toolbarFrame);
    canIdFilterLineEdit_->setClearButtonEnabled(true);
    canIdFilterLineEdit_->setPlaceholderText(QStringLiteral("支持 18E1 / 0x18E10101"));
    connect(canIdFilterLineEdit_, &QLineEdit::textChanged, this, [this](const QString &text) {
        filterProxyModel_->setCanIdFilterText(text);
        updateCanIdFilterState();
        updateDisplayedFrameCount();
    });

    auto *directionFilterLabel = new QLabel(QStringLiteral("方向"), toolbarFrame);
    directionFilterLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #29415d;"));
    directionFilterComboBox_ = new QComboBox(toolbarFrame);
    directionFilterComboBox_->addItem(QStringLiteral("全部"), static_cast<int>(CanFrameFilterProxyModel::DirectionFilter::All));
    directionFilterComboBox_->addItem(QStringLiteral("RX"), static_cast<int>(CanFrameFilterProxyModel::DirectionFilter::RxOnly));
    directionFilterComboBox_->addItem(QStringLiteral("TX"), static_cast<int>(CanFrameFilterProxyModel::DirectionFilter::TxOnly));
    connect(directionFilterComboBox_,
            &QComboBox::currentIndexChanged,
            this,
            [this](int index) {
                const auto filter = static_cast<CanFrameFilterProxyModel::DirectionFilter>(
                    directionFilterComboBox_->itemData(index).toInt());
                filterProxyModel_->setDirectionFilter(filter);
                updateDisplayedFrameCount();
            });

    auto *maximumFrameCountLabel = new QLabel(QStringLiteral("缓存上限"), toolbarFrame);
    maximumFrameCountLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #29415d;"));
    maximumFrameCountSpinBox_ = new QSpinBox(toolbarFrame);
    maximumFrameCountSpinBox_->setRange(100, 100000);
    maximumFrameCountSpinBox_->setSingleStep(500);
    maximumFrameCountSpinBox_->setValue(model_->maximumFrameCount());
    connect(maximumFrameCountSpinBox_,
            qOverload<int>(&QSpinBox::valueChanged),
            model_,
            &CanFrameTableModel::setMaximumFrameCount);
    connect(maximumFrameCountSpinBox_,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) {
                updateCachedFrameCount();
                updateDisplayedFrameCount();
            });

    auto *cachedFrameCountTitleLabel = new QLabel(QStringLiteral("缓存帧数"), toolbarFrame);
    cachedFrameCountTitleLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #29415d;"));
    cachedFrameCountValueLabel_ = new QLabel(QStringLiteral("0"), toolbarFrame);

    auto *displayedFrameCountTitleLabel = new QLabel(QStringLiteral("过滤显示"), toolbarFrame);
    displayedFrameCountTitleLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #29415d;"));
    displayedFrameCountValueLabel_ = new QLabel(QStringLiteral("0"), toolbarFrame);

    auto *totalRxCountTitleLabel = new QLabel(QStringLiteral("总 RX"), toolbarFrame);
    totalRxCountTitleLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #29415d;"));
    totalRxCountValueLabel_ = new QLabel(QStringLiteral("0"), toolbarFrame);

    auto *totalTxCountTitleLabel = new QLabel(QStringLiteral("总 TX"), toolbarFrame);
    totalTxCountTitleLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #29415d;"));
    totalTxCountValueLabel_ = new QLabel(QStringLiteral("0"), toolbarFrame);

    const QString valueLabelStyle =
        QStringLiteral("color: #16324f; font-weight: 700; min-width: 72px;");
    cachedFrameCountValueLabel_->setStyleSheet(valueLabelStyle);
    displayedFrameCountValueLabel_->setStyleSheet(valueLabelStyle);
    totalRxCountValueLabel_->setStyleSheet(valueLabelStyle);
    totalTxCountValueLabel_->setStyleSheet(valueLabelStyle);

    toolbarLayout->addWidget(sourceTitleLabel);
    toolbarLayout->addWidget(sourceStatusValueLabel_);
    toolbarLayout->addSpacing(8);
    toolbarLayout->addWidget(pauseButton_);
    toolbarLayout->addWidget(clearButton);
    toolbarLayout->addSpacing(8);
    toolbarLayout->addWidget(canIdFilterLabel);
    toolbarLayout->addWidget(canIdFilterLineEdit_, 1);
    toolbarLayout->addWidget(directionFilterLabel);
    toolbarLayout->addWidget(directionFilterComboBox_);
    toolbarLayout->addWidget(maximumFrameCountLabel);
    toolbarLayout->addWidget(maximumFrameCountSpinBox_);
    toolbarLayout->addSpacing(8);
    toolbarLayout->addWidget(cachedFrameCountTitleLabel);
    toolbarLayout->addWidget(cachedFrameCountValueLabel_);
    toolbarLayout->addWidget(displayedFrameCountTitleLabel);
    toolbarLayout->addWidget(displayedFrameCountValueLabel_);
    toolbarLayout->addWidget(totalRxCountTitleLabel);
    toolbarLayout->addWidget(totalRxCountValueLabel_);
    toolbarLayout->addWidget(totalTxCountTitleLabel);
    toolbarLayout->addWidget(totalTxCountValueLabel_);

    return toolbarFrame;
}

CanConnectionConfig CanMonitorPage::currentConnectionConfig() const
{
    CanConnectionConfig config;
    config.driverType = driverTypeComboBox_->currentData().toInt() == static_cast<int>(CanDriverType::ZlgControlCan)
                            ? CanDriverType::ZlgControlCan
                            : CanDriverType::Mock;
    config.deviceType = deviceTypeSpinBox_->value();
    config.deviceIndex = deviceIndexSpinBox_->value();
    config.channel = connectionChannelSpinBox_->value();
    config.bitrate = bitrateComboBox_->currentData().toInt();
    config.listenOnly = workModeComboBox_->currentData().toBool();
    return config;
}

void CanMonitorPage::validateRawFrameInput()
{
    if (sendValidationValueLabel_ == nullptr) {
        return;
    }

    const CanFrameInputParseResult result =
        CanFrameInputParser::parse({
            sendCanIdLineEdit_->text(),
            sendDataLineEdit_->text(),
            sendChannelSpinBox_->value(),
            sendFrameFormatComboBox_->currentData().toBool(),
            sendFrameTypeComboBox_->currentData().toBool()
        });

    sendDlcValueLabel_->setText(result.ok
                                   ? QString::number(result.frame.payload.size())
                                   : QStringLiteral("-"));
    sendValidationValueLabel_->setText(result.ok ? QStringLiteral("输入有效")
                                                 : result.errorMessage);
    sendValidationValueLabel_->setStyleSheet(
        QStringLiteral("color: %1; font-weight: 700;")
            .arg(result.ok ? QStringLiteral("#1f8f5f") : QStringLiteral("#b54708")));
    updateRawSendControls();
}

void CanMonitorPage::updateConnectionControls()
{
    if (connectionStateValueLabel_ == nullptr) {
        return;
    }

    QString stateText;
    switch (connectionState_) {
    case CanConnectionState::Disconnected:
        stateText = QStringLiteral("未连接");
        break;
    case CanConnectionState::Opening:
        stateText = QStringLiteral("打开中");
        break;
    case CanConnectionState::DeviceOpened:
        stateText = QStringLiteral("设备已打开");
        break;
    case CanConnectionState::Starting:
        stateText = QStringLiteral("启动中");
        break;
    case CanConnectionState::ChannelStarted:
        stateText = QStringLiteral("通道运行中");
        break;
    case CanConnectionState::Stopping:
        stateText = QStringLiteral("停止中");
        break;
    case CanConnectionState::Closing:
        stateText = QStringLiteral("关闭中");
        break;
    case CanConnectionState::Error:
        stateText = QStringLiteral("错误");
        break;
    case CanConnectionState::Unsupported:
        stateText = QStringLiteral("当前平台不支持硬件驱动");
        break;
    }
    connectionStateValueLabel_->setText(stateText);

    const bool transitioning =
        connectionState_ == CanConnectionState::Opening ||
        connectionState_ == CanConnectionState::Starting ||
        connectionState_ == CanConnectionState::Stopping ||
        connectionState_ == CanConnectionState::Closing;
    const bool disconnected = connectionState_ == CanConnectionState::Disconnected;
    const bool opened = connectionState_ == CanConnectionState::DeviceOpened;
    const bool started = connectionState_ == CanConnectionState::ChannelStarted;
    const bool errorLike = connectionState_ == CanConnectionState::Error ||
                           connectionState_ == CanConnectionState::Unsupported;

    openDeviceButton_->setEnabled(disconnected && !transitioning);
    startChannelButton_->setEnabled(opened && !transitioning);
    stopChannelButton_->setEnabled(started && !transitioning);
    closeDeviceButton_->setEnabled((opened || started || errorLike) && !transitioning);

    const bool configEditable = disconnected || errorLike;
    sourceModeComboBox_->setEnabled(configEditable);
    driverTypeComboBox_->setEnabled(configEditable);
    deviceTypeSpinBox_->setEnabled(configEditable);
    deviceIndexSpinBox_->setEnabled(configEditable);
    connectionChannelSpinBox_->setEnabled(configEditable);
    bitrateComboBox_->setEnabled(configEditable);
    workModeComboBox_->setEnabled(configEditable);
}

void CanMonitorPage::updateRawSendControls()
{
    if (sendFrameButton_ == nullptr) {
        return;
    }

    const CanFrameInputParseResult result =
        CanFrameInputParser::parse({
            sendCanIdLineEdit_->text(),
            sendDataLineEdit_->text(),
            sendChannelSpinBox_->value(),
            sendFrameFormatComboBox_->currentData().toBool(),
            sendFrameTypeComboBox_->currentData().toBool()
        });
    sendFrameButton_->setEnabled(connectionState_ == CanConnectionState::ChannelStarted &&
                                 result.ok);
}

void CanMonitorPage::updatePauseButton()
{
    if (pauseButton_ == nullptr) {
        return;
    }

    pauseButton_->setText(isPaused()
                              ? QStringLiteral("继续")
                              : QStringLiteral("暂停"));
}

void CanMonitorPage::updateSourceStatusLabel()
{
    if (sourceStatusValueLabel_ == nullptr) {
        return;
    }

    const QString statusText = isPaused()
                                   ? QStringLiteral("%1 已暂停")
                                         .arg(sourceName_)
                                   : QStringLiteral("%1 %2")
                                         .arg(sourceName_, sourceStatusMessage_);

    sourceStatusValueLabel_->setText(statusText);
    sourceStatusValueLabel_->setStyleSheet(
        QStringLiteral("color: #ffffff; border-radius: 10px; padding: 4px 10px; font-weight: 700; background-color: %1;")
            .arg(isPaused() ? QStringLiteral("#b26b00")
                            : (sourceOnline_ ? QStringLiteral("#1f8f5f")
                                             : QStringLiteral("#6b7280"))));
    sourceStatusValueLabel_->setToolTip(statusText);
}

void CanMonitorPage::updateCachedFrameCount()
{
    if (cachedFrameCountValueLabel_ == nullptr) {
        return;
    }

    cachedFrameCountValueLabel_->setText(QString::number(model_->rowCount()));
}

void CanMonitorPage::updateDisplayedFrameCount()
{
    if (displayedFrameCountValueLabel_ == nullptr) {
        return;
    }

    displayedFrameCountValueLabel_->setText(QString::number(filterProxyModel_->rowCount()));
}

void CanMonitorPage::updateStatistics()
{
    if (totalRxCountValueLabel_ != nullptr) {
        totalRxCountValueLabel_->setText(QString::number(totalRxCount_));
    }

    if (totalTxCountValueLabel_ != nullptr) {
        totalTxCountValueLabel_->setText(QString::number(totalTxCount_));
    }
}

void CanMonitorPage::updateCanIdFilterState()
{
    if (canIdFilterLineEdit_ == nullptr) {
        return;
    }

    if (filterProxyModel_->isCanIdFilterValid() ||
        canIdFilterLineEdit_->text().trimmed().isEmpty()) {
        canIdFilterLineEdit_->setStyleSheet(QString());
        canIdFilterLineEdit_->setToolTip(QStringLiteral("支持完整或部分十六进制 CAN ID"));
        return;
    }

    canIdFilterLineEdit_->setStyleSheet(
        QStringLiteral("QLineEdit { border: 1px solid #c0392b; border-radius: 4px; padding: 2px 6px; }"));
    canIdFilterLineEdit_->setToolTip(QStringLiteral("过滤值需为十六进制文本，例如 18E1 或 0x18E10101"));
}

bool CanMonitorPage::isViewingLatest() const
{
    if (tableView_ == nullptr || tableView_->verticalScrollBar() == nullptr) {
        return true;
    }

    const QScrollBar *scrollBar = tableView_->verticalScrollBar();
    return scrollBar->value() >= scrollBar->maximum() - 2;
}

#include "pages/CanMonitorPage.h"

#include "models/CanFrameTableModel.h"

#include <QAbstractItemModel>
#include <QComboBox>
#include <QFrame>
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
    tableView_->setMinimumHeight(520);

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

    rootLayout->addWidget(toolbarFrame);
    rootLayout->addWidget(tableView_, 1);
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

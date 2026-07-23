#include "pages/BmsCommandPage.h"

#include "models/BmsCommandAuditModel.h"
#include "widgets/BmsCommandAuditDetailWidget.h"

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{
constexpr int PlaceholderColumnCount = 3;
const QString EmptyValue = QStringLiteral("-");

QString formatCanId(quint32 canId)
{
    return QStringLiteral("0x") + QString::number(canId, 16).toUpper().rightJustified(8, QLatin1Char('0'));
}

QString formatFrameFormat(bool extended)
{
    return extended ? QStringLiteral("扩展帧") : QStringLiteral("标准帧");
}

QString formatPayload(const QByteArray &data)
{
    if (data.isEmpty()) {
        return EmptyValue;
    }
    return QString::fromLatin1(data.toHex(' ')).toUpper();
}

QString formatSafetyLevel(BmsSafetyLevel level)
{
    switch (level) {
    case BmsSafetyLevel::DemoOnly:
        return QStringLiteral("Demo Only");
    case BmsSafetyLevel::Low:
        return QStringLiteral("低");
    case BmsSafetyLevel::Medium:
        return QStringLiteral("中");
    case BmsSafetyLevel::High:
        return QStringLiteral("高");
    case BmsSafetyLevel::Critical:
        return QStringLiteral("关键");
    }
    return EmptyValue;
}

QString formatSeverity(ValidationSeverity severity)
{
    switch (severity) {
    case ValidationSeverity::Info:
        return QStringLiteral("信息");
    case ValidationSeverity::Warning:
        return QStringLiteral("警告");
    case ValidationSeverity::Error:
        return QStringLiteral("错误");
    }
    return EmptyValue;
}

// "Demo voltage [V]（0 ～ 1000，必填）"
QString buildParameterLabel(const BmsParameterDefinition &parameter)
{
    QString label = parameter.displayName.isEmpty() ? parameter.parameterId : parameter.displayName;
    if (!parameter.unit.isEmpty()) {
        label += QStringLiteral(" [%1]").arg(parameter.unit);
    }

    QStringList notes;
    if (parameter.minimum.has_value() || parameter.maximum.has_value()) {
        const QString minimumText = parameter.minimum.has_value()
                                        ? QString::number(*parameter.minimum)
                                        : QStringLiteral("不限");
        const QString maximumText = parameter.maximum.has_value()
                                        ? QString::number(*parameter.maximum)
                                        : QStringLiteral("不限");
        notes << QStringLiteral("%1 ～ %2").arg(minimumText, maximumText);
    }
    notes << (parameter.required ? QStringLiteral("必填") : QStringLiteral("选填"));
    label += QStringLiteral("（%1）").arg(notes.join(QStringLiteral("，")));
    return label;
}

QLabel *createValueLabel(QWidget *parent, const QString &objectName)
{
    auto *label = new QLabel(EmptyValue, parent);
    label->setObjectName(objectName);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}
} // namespace

BmsCommandPage::BmsCommandPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();

    // Populating the combo triggers the first parameter-form build.
    const QList<BmsCommandDefinition> commands = BmsCommandCatalog::demoCommands();
    for (const BmsCommandDefinition &command : commands) {
        commandComboBox_->addItem(command.displayName, command.commandId);
    }

    connect(commandComboBox_,
            &QComboBox::currentIndexChanged,
            this,
            [this](int) { onCommandSelectionChanged(); });

    onCommandSelectionChanged();

    // From here on, real user-driven selection changes may emit audit events.
    initializingPage_ = false;
}

void BmsCommandPage::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

    auto *titleLabel = new QLabel(QStringLiteral("BMS 指令下发（Demo / Mock 模式）"), this);
    titleLabel->setObjectName(QStringLiteral("bmsCommandPageTitle"));
    titleLabel->setStyleSheet(QStringLiteral("color: #173b63; font-size: 16px; font-weight: 700;"));
    rootLayout->addWidget(titleLabel, 0);

    rootLayout->addWidget(createSafetyBanner(), 0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(createLeftPane());
    splitter->addWidget(createRightPane());
    splitter->setStretchFactor(0, 45);
    splitter->setStretchFactor(1, 55);
    rootLayout->addWidget(splitter, 1);
}

QWidget *BmsCommandPage::createSafetyBanner()
{
    safetyBannerLabel_ = new QLabel(
        QStringLiteral("当前仅支持 Demo 指令预览、确认，以及 Mock CAN 单次发送。\n"
                       "未加载真实厂商 BMS 指令，不支持真实 CAN 硬件发送。"),
        this);
    safetyBannerLabel_->setObjectName(QStringLiteral("bmsCommandSafetyBanner"));
    safetyBannerLabel_->setWordWrap(true);
    safetyBannerLabel_->setStyleSheet(QStringLiteral(
        "background-color: #fff4d6; color: #8a5a00; border: 1px solid #e0b34d;"
        " border-radius: 6px; padding: 8px; font-weight: 700;"));
    return safetyBannerLabel_;
}

QWidget *BmsCommandPage::createCommandSelectionPanel()
{
    auto *group = new QGroupBox(QStringLiteral("指令选择"), this);
    auto *layout = new QFormLayout(group);
    layout->setContentsMargins(10, 16, 10, 10);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    commandComboBox_ = new QComboBox(group);
    commandComboBox_->setObjectName(QStringLiteral("bmsCommandComboBox"));
    layout->addRow(QStringLiteral("演示指令"), commandComboBox_);

    return group;
}

QGroupBox *BmsCommandPage::createCommandInfoPanel()
{
    auto *group = new QGroupBox(QStringLiteral("指令基本信息"), this);
    auto *layout = new QFormLayout(group);
    layout->setContentsMargins(10, 16, 10, 10);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    commandIdValueLabel_ = createValueLabel(group, QStringLiteral("commandIdValue"));
    commandDescriptionValueLabel_ = createValueLabel(group, QStringLiteral("commandDescriptionValue"));
    commandSafetyLevelValueLabel_ = createValueLabel(group, QStringLiteral("commandSafetyLevelValue"));
    commandDemoOnlyValueLabel_ = createValueLabel(group, QStringLiteral("commandDemoOnlyValue"));
    commandCanIdValueLabel_ = createValueLabel(group, QStringLiteral("commandCanIdValue"));
    commandChannelValueLabel_ = createValueLabel(group, QStringLiteral("commandChannelValue"));
    commandFrameFormatValueLabel_ = createValueLabel(group, QStringLiteral("commandFrameFormatValue"));
    commandDlcValueLabel_ = createValueLabel(group, QStringLiteral("commandDlcValue"));

    layout->addRow(QStringLiteral("指令 ID"), commandIdValueLabel_);
    layout->addRow(QStringLiteral("指令描述"), commandDescriptionValueLabel_);
    layout->addRow(QStringLiteral("安全等级"), commandSafetyLevelValueLabel_);
    layout->addRow(QStringLiteral("Demo Only"), commandDemoOnlyValueLabel_);
    layout->addRow(QStringLiteral("CAN ID"), commandCanIdValueLabel_);
    layout->addRow(QStringLiteral("通道"), commandChannelValueLabel_);
    layout->addRow(QStringLiteral("帧格式"), commandFrameFormatValueLabel_);
    layout->addRow(QStringLiteral("DLC"), commandDlcValueLabel_);

    return group;
}

QGroupBox *BmsCommandPage::createParameterPanel()
{
    parameterGroup_ = new QGroupBox(QStringLiteral("指令参数"), this);
    parameterFormLayout_ = new QFormLayout(parameterGroup_);
    parameterFormLayout_->setContentsMargins(10, 16, 10, 10);
    parameterFormLayout_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return parameterGroup_;
}

QWidget *BmsCommandPage::createLeftPane()
{
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget(scrollArea);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 8, 0);
    layout->setSpacing(10);

    layout->addWidget(createCommandSelectionPanel());
    layout->addWidget(createCommandInfoPanel());
    layout->addWidget(createParameterPanel());

    previewCommandButton_ = new QPushButton(QStringLiteral("生成 CAN 帧预览"), content);
    previewCommandButton_->setObjectName(QStringLiteral("previewCommandButton"));
    connect(previewCommandButton_, &QPushButton::clicked, this, &BmsCommandPage::generatePreview);
    layout->addWidget(previewCommandButton_);

    layout->addStretch(1);
    scrollArea->setWidget(content);
    return scrollArea;
}

QGroupBox *BmsCommandPage::createValidationPanel()
{
    auto *group = new QGroupBox(QStringLiteral("校验结果"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 16, 10, 10);
    layout->setSpacing(8);

    validationStatusLabel_ = new QLabel(QStringLiteral("等待生成预览"), group);
    validationStatusLabel_->setObjectName(QStringLiteral("validationStatusLabel"));
    validationStatusLabel_->setWordWrap(true);
    layout->addWidget(validationStatusLabel_);

    validationIssuesList_ = new QListWidget(group);
    validationIssuesList_->setObjectName(QStringLiteral("validationIssuesList"));
    validationIssuesList_->setMinimumHeight(80);
    layout->addWidget(validationIssuesList_);

    return group;
}

QGroupBox *BmsCommandPage::createPreviewPanel()
{
    auto *group = new QGroupBox(QStringLiteral("CAN 帧预览"), this);
    auto *layout = new QFormLayout(group);
    layout->setContentsMargins(10, 16, 10, 10);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    previewCommandIdValueLabel_ = createValueLabel(group, QStringLiteral("previewCommandIdValue"));
    previewCanIdValueLabel_ = createValueLabel(group, QStringLiteral("previewCanIdValue"));
    previewChannelValueLabel_ = createValueLabel(group, QStringLiteral("previewChannelValue"));
    previewFrameFormatValueLabel_ = createValueLabel(group, QStringLiteral("previewFrameFormatValue"));
    previewDlcValueLabel_ = createValueLabel(group, QStringLiteral("previewDlcValue"));
    previewPayloadValueLabel_ = createValueLabel(group, QStringLiteral("previewPayloadValue"));
    previewFingerprintValueLabel_ = createValueLabel(group, QStringLiteral("previewFingerprintValue"));
    previewEncodedAtValueLabel_ = createValueLabel(group, QStringLiteral("previewEncodedAtValue"));

    previewPayloadValueLabel_->setStyleSheet(QStringLiteral("font-family: monospace; font-weight: 700;"));
    previewFingerprintValueLabel_->setStyleSheet(QStringLiteral("font-family: monospace;"));

    layout->addRow(QStringLiteral("指令 ID"), previewCommandIdValueLabel_);
    layout->addRow(QStringLiteral("CAN ID"), previewCanIdValueLabel_);
    layout->addRow(QStringLiteral("通道"), previewChannelValueLabel_);
    layout->addRow(QStringLiteral("帧格式"), previewFrameFormatValueLabel_);
    layout->addRow(QStringLiteral("DLC"), previewDlcValueLabel_);
    layout->addRow(QStringLiteral("Payload"), previewPayloadValueLabel_);
    layout->addRow(QStringLiteral("Fingerprint"), previewFingerprintValueLabel_);
    layout->addRow(QStringLiteral("编码时间 (UTC)"), previewEncodedAtValueLabel_);

    return group;
}

QGroupBox *BmsCommandPage::createEncodedParameterPanel()
{
    auto *group = new QGroupBox(QStringLiteral("参数快照"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 16, 10, 10);

    encodedParameterTable_ = new QTableWidget(0, PlaceholderColumnCount, group);
    encodedParameterTable_->setObjectName(QStringLiteral("encodedParameterTable"));
    encodedParameterTable_->setHorizontalHeaderLabels(
        {QStringLiteral("参数 ID"), QStringLiteral("工程值"), QStringLiteral("原始值")});
    encodedParameterTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    encodedParameterTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    encodedParameterTable_->verticalHeader()->setVisible(false);
    encodedParameterTable_->horizontalHeader()->setStretchLastSection(true);
    encodedParameterTable_->setMinimumHeight(120);
    layout->addWidget(encodedParameterTable_);

    return group;
}

QGroupBox *BmsCommandPage::createConfirmationPanel()
{
    auto *group = new QGroupBox(QStringLiteral("预览确认"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 16, 10, 10);
    layout->setSpacing(8);

    confirmationWarningLabel_ = new QLabel(
        QStringLiteral("此操作只确认当前预览快照，不会发送 CAN 帧，也不构成真实设备发送授权。"),
        group);
    confirmationWarningLabel_->setObjectName(QStringLiteral("confirmationWarningLabel"));
    confirmationWarningLabel_->setWordWrap(true);
    confirmationWarningLabel_->setStyleSheet(QStringLiteral("color: #8a5a00; font-weight: 700;"));
    layout->addWidget(confirmationWarningLabel_);

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    confirmationRevisionValueLabel_ = createValueLabel(group, QStringLiteral("confirmationRevisionValue"));
    confirmationCodeValueLabel_ = createValueLabel(group, QStringLiteral("confirmationCodeValue"));
    confirmationCodeValueLabel_->setStyleSheet(
        QStringLiteral("font-family: monospace; font-size: 15px; font-weight: 700;"));
    confirmedFingerprintValueLabel_ = createValueLabel(group, QStringLiteral("confirmedFingerprintValue"));
    confirmedFingerprintValueLabel_->setStyleSheet(QStringLiteral("font-family: monospace;"));
    confirmedAtValueLabel_ = createValueLabel(group, QStringLiteral("confirmedAtValue"));

    confirmationAcknowledgementCheckBox_ = new QCheckBox(
        QStringLiteral("我已核对指令 ID、工程参数、CAN ID、Payload 和 Fingerprint"), group);
    confirmationAcknowledgementCheckBox_->setObjectName(
        QStringLiteral("confirmationAcknowledgementCheckBox"));

    confirmationCodeLineEdit_ = new QLineEdit(group);
    confirmationCodeLineEdit_->setObjectName(QStringLiteral("confirmationCodeLineEdit"));
    confirmationCodeLineEdit_->setPlaceholderText(QStringLiteral("请输入上方 8 位核对码"));
    confirmationCodeLineEdit_->setMaxLength(8);

    form->addRow(QStringLiteral("预览版本"), confirmationRevisionValueLabel_);
    form->addRow(QStringLiteral("核对码"), confirmationCodeValueLabel_);
    form->addRow(QStringLiteral("输入核对码"), confirmationCodeLineEdit_);
    form->addRow(QStringLiteral("已确认 Fingerprint"), confirmedFingerprintValueLabel_);
    form->addRow(QStringLiteral("确认时间 (UTC)"), confirmedAtValueLabel_);
    layout->addLayout(form);

    layout->addWidget(confirmationAcknowledgementCheckBox_);

    confirmPreviewButton_ = new QPushButton(QStringLiteral("确认当前预览"), group);
    confirmPreviewButton_->setObjectName(QStringLiteral("confirmPreviewButton"));
    connect(confirmPreviewButton_, &QPushButton::clicked, this, &BmsCommandPage::confirmCurrentPreview);
    layout->addWidget(confirmPreviewButton_);

    confirmationStatusLabel_ = new QLabel(QStringLiteral("等待生成预览"), group);
    confirmationStatusLabel_->setObjectName(QStringLiteral("confirmationStatusLabel"));
    confirmationStatusLabel_->setWordWrap(true);
    layout->addWidget(confirmationStatusLabel_);

    return group;
}

QGroupBox *BmsCommandPage::createMockDispatchPanel()
{
    auto *group = new QGroupBox(QStringLiteral("Mock CAN 单次发送"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 16, 10, 10);
    layout->setSpacing(8);

    auto *warning = new QLabel(
        QStringLiteral("仅允许发送已确认的 Demo 快照到当前 Mock CAN 通道。\n"
                       "每个确认版本最多成功发送一次；真实硬件发送未启用。"),
        group);
    warning->setWordWrap(true);
    warning->setStyleSheet(QStringLiteral("color: #8a5a00; font-weight: 700;"));
    layout->addWidget(warning);

    mockDispatchAvailabilityLabel_ = new QLabel(QStringLiteral("Mock 通道状态未知"), group);
    mockDispatchAvailabilityLabel_->setObjectName(QStringLiteral("mockDispatchAvailabilityLabel"));
    mockDispatchAvailabilityLabel_->setWordWrap(true);
    layout->addWidget(mockDispatchAvailabilityLabel_);

    sendCommandButton_ = new QPushButton(QStringLiteral("发送 Demo 到 Mock CAN（单次）"), group);
    sendCommandButton_->setObjectName(QStringLiteral("sendCommandButton"));
    sendCommandButton_->setEnabled(false);
    // The application-wide stylesheet paints every QPushButton in the active blue
    // and defines no disabled state, so a disabled control would still look
    // clickable. Give it an explicitly inactive appearance.
    sendCommandButton_->setStyleSheet(QStringLiteral(
        "QPushButton:disabled { background-color: #d5dbe3; color: #78859a;"
        " border: 1px solid #b9c4d0; }"));
    connect(sendCommandButton_, &QPushButton::clicked, this, &BmsCommandPage::requestMockDispatch);
    layout->addWidget(sendCommandButton_);

    mockDispatchStatusLabel_ = new QLabel(QStringLiteral("等待确认预览"), group);
    mockDispatchStatusLabel_->setObjectName(QStringLiteral("mockDispatchStatusLabel"));
    mockDispatchStatusLabel_->setWordWrap(true);
    layout->addWidget(mockDispatchStatusLabel_);

    return group;
}

QWidget *BmsCommandPage::createRightPane()
{
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget(scrollArea);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(10);

    layout->addWidget(createValidationPanel());
    layout->addWidget(createPreviewPanel());
    layout->addWidget(createConfirmationPanel());
    layout->addWidget(createEncodedParameterPanel());
    layout->addWidget(createMockDispatchPanel());
    layout->addWidget(createAuditPanel());

    layout->addStretch(1);
    scrollArea->setWidget(content);
    return scrollArea;
}

QGroupBox *BmsCommandPage::createAuditPanel()
{
    auto *group = new QGroupBox(QStringLiteral("BMS 指令审计记录（当前进程）"), this);
    group->setObjectName(QStringLiteral("bmsCommandAuditPanel"));
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 16, 10, 10);
    layout->setSpacing(8);

    bmsCommandAuditCountLabel_ = new QLabel(QStringLiteral("记录数量：0"), group);
    bmsCommandAuditCountLabel_->setObjectName(QStringLiteral("bmsCommandAuditCountLabel"));
    layout->addWidget(bmsCommandAuditCountLabel_);

    auto *storageWarning = new QLabel(
        QStringLiteral("提示：记录仅保存在当前应用进程内。\n"
                       "未持久化到文件，不构成合规审计、日志签名或防篡改证明。"),
        group);
    storageWarning->setObjectName(QStringLiteral("bmsCommandAuditStorageWarning"));
    storageWarning->setWordWrap(true);
    storageWarning->setStyleSheet(QStringLiteral("color: #8a5a00; font-weight: 700;"));
    layout->addWidget(storageWarning);

    auto *splitter = new QSplitter(Qt::Vertical, group);
    splitter->setObjectName(QStringLiteral("bmsCommandAuditSplitter"));
    splitter->setChildrenCollapsible(false);

    bmsCommandAuditTableView_ = new QTableView(splitter);
    bmsCommandAuditTableView_->setObjectName(QStringLiteral("bmsCommandAuditTableView"));
    bmsCommandAuditTableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    bmsCommandAuditTableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    bmsCommandAuditTableView_->setSortingEnabled(false);
    bmsCommandAuditTableView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    bmsCommandAuditTableView_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    bmsCommandAuditTableView_->verticalHeader()->setVisible(false);
    bmsCommandAuditTableView_->setMinimumHeight(160);
    splitter->addWidget(bmsCommandAuditTableView_);

    bmsCommandAuditDetailWidget_ = new BmsCommandAuditDetailWidget(splitter);
    bmsCommandAuditDetailWidget_->setMinimumHeight(240);
    splitter->addWidget(bmsCommandAuditDetailWidget_);

    // The table stays on top and keeps a larger share of the space.
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter);

    return group;
}

void BmsCommandPage::disconnectAuditModelConnections()
{
    // Precise teardown using the saved handles; never a broad
    // disconnect(model, nullptr, this, nullptr) that could drop unrelated links.
    QObject::disconnect(auditRowsInsertedConnection_);
    QObject::disconnect(auditModelResetConnection_);
    QObject::disconnect(auditCurrentRowConnection_);
    QObject::disconnect(auditModelDestroyedConnection_);
    auditRowsInsertedConnection_ = QMetaObject::Connection();
    auditModelResetConnection_ = QMetaObject::Connection();
    auditCurrentRowConnection_ = QMetaObject::Connection();
    auditModelDestroyedConnection_ = QMetaObject::Connection();
}

void BmsCommandPage::configureAuditColumns()
{
    QAbstractItemModel *model = bmsCommandAuditTableView_->model();
    if (model == nullptr) {
        return;
    }

    QHeaderView *header = bmsCommandAuditTableView_->horizontalHeader();
    for (int column = 0; column < model->columnCount(); ++column) {
        QHeaderView::ResizeMode mode = QHeaderView::ResizeToContents;
        if (column == BmsCommandAuditModel::FingerprintColumn) {
            mode = QHeaderView::Interactive;
        } else if (column == BmsCommandAuditModel::DetailsColumn) {
            mode = QHeaderView::Stretch;
        }
        header->setSectionResizeMode(column, mode);
    }
}

void BmsCommandPage::updateAuditCount()
{
    if (bmsCommandAuditCountLabel_ == nullptr) {
        return;
    }
    const QAbstractItemModel *model = bmsCommandAuditTableView_->model();
    const int count = model == nullptr ? 0 : model->rowCount();
    bmsCommandAuditCountLabel_->setText(QStringLiteral("记录数量：%1").arg(count));
}

void BmsCommandPage::selectAuditRow(int row)
{
    QAbstractItemModel *model = bmsCommandAuditTableView_->model();
    if (model == nullptr || row < 0 || row >= model->rowCount()) {
        return;
    }

    const QModelIndex index = model->index(row, BmsCommandAuditModel::SequenceColumn);
    // setCurrentIndex drives the current-row signal, which refreshes the detail.
    bmsCommandAuditTableView_->selectionModel()->setCurrentIndex(
        index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    bmsCommandAuditTableView_->scrollTo(index);
}

void BmsCommandPage::showAuditRecordFor(const QModelIndex &current)
{
    if (bmsCommandAuditDetailWidget_ == nullptr) {
        return;
    }

    const QAbstractItemModel *model = bmsCommandAuditTableView_->model();
    if (!current.isValid() || model == nullptr) {
        bmsCommandAuditDetailWidget_->clearRecord();
        return;
    }

    // Only the record role is trusted; a row whose UserRole is missing or of the
    // wrong type clears the detail rather than showing stale content.
    const QModelIndex recordIndex = model->index(current.row(), BmsCommandAuditModel::SequenceColumn);
    const QVariant value = recordIndex.data(Qt::UserRole);
    if (!value.canConvert<BmsCommandAuditRecord>()) {
        bmsCommandAuditDetailWidget_->clearRecord();
        return;
    }

    bmsCommandAuditDetailWidget_->setRecord(value.value<BmsCommandAuditRecord>());
}

void BmsCommandPage::onAuditCurrentRowChanged(const QModelIndex &current, const QModelIndex &)
{
    showAuditRecordFor(current);
}

void BmsCommandPage::onAuditRowsInserted(const QModelIndex &, int first, int last)
{
    updateAuditCount();

    // Decide tail-following from the selection as it was before this insertion:
    // no selection, or a selection on the row just before the new block, means the
    // user was watching the latest record and should keep watching it.
    const QModelIndex current = bmsCommandAuditTableView_->selectionModel()->currentIndex();
    const bool followLatest = !current.isValid() || current.row() == first - 1;

    if (followLatest) {
        selectAuditRow(last);
    }
    // Otherwise the user is inspecting an older record: leave their selection,
    // detail, and scroll position untouched.
}

void BmsCommandPage::onAuditModelReset()
{
    updateAuditCount();

    QAbstractItemModel *model = bmsCommandAuditTableView_->model();
    if (model != nullptr && model->rowCount() > 0) {
        selectAuditRow(model->rowCount() - 1);
    } else if (bmsCommandAuditDetailWidget_ != nullptr) {
        bmsCommandAuditDetailWidget_->clearRecord();
    }
}

void BmsCommandPage::onAuditModelDestroyed()
{
    // The table view observes the model separately, but the detail viewer contains
    // an independent value copy and must be cleared explicitly when the source
    // model disappears.
    auditModel_.clear();

    // Connections whose sender was the destroyed model are already unusable.
    // Reset the saved handles so a later setAuditModel() starts from a clean state.
    auditRowsInsertedConnection_ = QMetaObject::Connection();
    auditModelResetConnection_ = QMetaObject::Connection();
    auditCurrentRowConnection_ = QMetaObject::Connection();
    auditModelDestroyedConnection_ = QMetaObject::Connection();

    if (bmsCommandAuditCountLabel_ != nullptr) {
        bmsCommandAuditCountLabel_->setText(QStringLiteral("记录数量：0"));
    }

    if (bmsCommandAuditDetailWidget_ != nullptr) {
        bmsCommandAuditDetailWidget_->clearRecord();
    }
}

void BmsCommandPage::setAuditModel(QAbstractItemModel *model)
{
    if (bmsCommandAuditTableView_ == nullptr) {
        return;
    }

    // Safe to call repeatedly: tear down the previous model's links first.
    disconnectAuditModelConnections();
    if (bmsCommandAuditDetailWidget_ != nullptr) {
        bmsCommandAuditDetailWidget_->clearRecord();
    }

    // Display only; the page does not own the model and must not append or delete.
    auditModel_ = model;
    bmsCommandAuditTableView_->setModel(model);

    if (model != nullptr) {
        configureAuditColumns();

        auditRowsInsertedConnection_ =
            connect(model, &QAbstractItemModel::rowsInserted, this,
                    &BmsCommandPage::onAuditRowsInserted);
        auditModelResetConnection_ =
            connect(model, &QAbstractItemModel::modelReset, this,
                    &BmsCommandPage::onAuditModelReset);
        // If the external model is destroyed out from under the page, drop the
        // stale count and the detail's independent value copy.
        auditModelDestroyedConnection_ =
            connect(model, &QObject::destroyed, this,
                    &BmsCommandPage::onAuditModelDestroyed);

        if (QItemSelectionModel *selectionModel = bmsCommandAuditTableView_->selectionModel()) {
            auditCurrentRowConnection_ =
                connect(selectionModel, &QItemSelectionModel::currentRowChanged, this,
                        &BmsCommandPage::onAuditCurrentRowChanged);
        }
    }

    updateAuditCount();

    // An already-populated model opens on its newest record; an empty one shows
    // the placeholder detail.
    if (model != nullptr && model->rowCount() > 0) {
        selectAuditRow(model->rowCount() - 1);
    } else if (bmsCommandAuditDetailWidget_ != nullptr) {
        bmsCommandAuditDetailWidget_->clearRecord();
    }
}

std::optional<BmsCommandConfirmationSnapshot> BmsCommandPage::currentAuditSnapshot() const
{
    if (confirmationGate_.hasConfirmedSnapshot()) {
        return confirmationGate_.confirmedSnapshot();
    }
    if (confirmationGate_.hasStagedSnapshot()) {
        return confirmationGate_.stagedSnapshot();
    }
    return std::nullopt;
}

void BmsCommandPage::emitAuditRecord(BmsCommandAuditEventType type,
                                     BmsCommandAuditOutcome outcome,
                                     const BmsCommandConfirmationSnapshot *snapshot,
                                     const QString &code,
                                     const QString &message)
{
    emit auditRecordGenerated(makeBmsCommandAuditRecord(
        type, outcome, snapshot, QDateTime::currentDateTimeUtc(), code, message));
}

bool BmsCommandPage::currentDefinition(BmsCommandDefinition *definition) const
{
    if (commandComboBox_ == nullptr || commandComboBox_->currentIndex() < 0) {
        return false;
    }
    return BmsCommandCatalog::findDemoCommand(commandComboBox_->currentData().toString(), definition);
}

void BmsCommandPage::onCommandSelectionChanged()
{
    BmsCommandDefinition definition;
    if (!currentDefinition(&definition)) {
        return;
    }

    updateCommandInfo(definition);
    rebuildParameterForm(definition);

    clearPreview();
    validationIssuesList_->clear();
    validationStatusLabel_->setText(QStringLiteral("等待生成预览"));
    validationStatusLabel_->setStyleSheet(QString());

    // A real command switch drops any preview/confirmation of the previous command;
    // record that only when an actual snapshot existed and not during construction.
    const std::optional<BmsCommandConfirmationSnapshot> previousSnapshot = currentAuditSnapshot();
    if (!initializingPage_ && previousSnapshot.has_value()) {
        emitAuditRecord(BmsCommandAuditEventType::PreviewInvalidated,
                        BmsCommandAuditOutcome::Info,
                        &previousSnapshot.value(),
                        QStringLiteral("CommandChanged"),
                        QStringLiteral("指令切换导致原预览和确认失效"));
    }

    // Switching commands must not leave the previous command's confirmation behind.
    confirmationGate_.invalidate();
    currentRevision_ = 0;
    clearConfirmation(QStringLiteral("等待生成预览"));
}

void BmsCommandPage::updateCommandInfo(const BmsCommandDefinition &definition)
{
    commandIdValueLabel_->setText(definition.commandId);
    commandDescriptionValueLabel_->setText(definition.description);
    commandSafetyLevelValueLabel_->setText(formatSafetyLevel(definition.safetyLevel));
    commandDemoOnlyValueLabel_->setText(definition.demoOnly ? QStringLiteral("是") : QStringLiteral("否"));
    commandCanIdValueLabel_->setText(formatCanId(definition.canId));
    commandChannelValueLabel_->setText(QString::number(definition.channel));
    commandFrameFormatValueLabel_->setText(formatFrameFormat(definition.extended));
    commandDlcValueLabel_->setText(QString::number(definition.dlc));
}

void BmsCommandPage::rebuildParameterForm(const BmsCommandDefinition &definition)
{
    // Creating controls emits their change signals; those must not be mistaken
    // for the user editing a parameter.
    rebuildingParameterForm_ = true;
    parameterControls_.clear();
    while (parameterFormLayout_->rowCount() > 0) {
        parameterFormLayout_->removeRow(0);
    }

    for (const BmsParameterDefinition &parameter : definition.parameters) {
        const QString objectName = QStringLiteral("bmsParameter_%1").arg(parameter.parameterId);
        QWidget *control = nullptr;

        switch (parameter.valueType) {
        case BmsValueType::Boolean: {
            auto *checkBox = new QCheckBox(parameterGroup_);
            checkBox->setObjectName(objectName);
            connect(checkBox, &QCheckBox::toggled, this, [this](bool) { onParameterEdited(); });
            control = checkBox;
            break;
        }
        case BmsValueType::Enum: {
            auto *comboBox = new QComboBox(parameterGroup_);
            comboBox->setObjectName(objectName);
            for (auto it = parameter.enumValues.cbegin(); it != parameter.enumValues.cend(); ++it) {
                comboBox->addItem(it.value(), QVariant::fromValue(it.key()));
            }
            connect(comboBox, &QComboBox::currentIndexChanged, this, [this](int) {
                onParameterEdited();
            });
            control = comboBox;
            break;
        }
        case BmsValueType::UnsignedInteger:
        case BmsValueType::SignedInteger:
        case BmsValueType::Physical: {
            auto *lineEdit = new QLineEdit(parameterGroup_);
            lineEdit->setObjectName(objectName);
            // Intentionally permissive: empty text must reach the protocol
            // validator so it can report the issue instead of being blocked here.
            auto *validator = new QDoubleValidator(lineEdit);
            validator->setNotation(QDoubleValidator::StandardNotation);
            lineEdit->setValidator(validator);
            connect(lineEdit, &QLineEdit::textChanged, this, [this](const QString &) {
                onParameterEdited();
            });
            control = lineEdit;
            break;
        }
        }

        if (control == nullptr) {
            continue;
        }

        parameterFormLayout_->addRow(buildParameterLabel(parameter), control);
        parameterControls_.append({parameter.parameterId, parameter.valueType, control});
    }

    rebuildingParameterForm_ = false;
}

void BmsCommandPage::onParameterEdited()
{
    if (rebuildingParameterForm_) {
        return;
    }

    // Nothing has been invalidated on the very first edit, so the message must
    // not claim a confirmation was lost.
    const std::optional<BmsCommandConfirmationSnapshot> previousSnapshot = currentAuditSnapshot();
    const bool hadConfirmationContext = previousSnapshot.has_value();

    // Only an actual existing preview/confirmation is worth an invalidation record;
    // the first edit on a fresh form has nothing to invalidate.
    if (hadConfirmationContext) {
        emitAuditRecord(BmsCommandAuditEventType::PreviewInvalidated,
                        BmsCommandAuditOutcome::Info,
                        &previousSnapshot.value(),
                        QStringLiteral("ParameterChanged"),
                        QStringLiteral("参数修改导致原预览和确认失效"));
    }

    // Any edit detaches the displayed preview from the form, so both the preview
    // and any confirmation bound to it must go.
    confirmationGate_.invalidate();
    currentRevision_ = 0;
    clearPreview();
    validationIssuesList_->clear();
    validationStatusLabel_->setText(QStringLiteral("参数已修改，请重新生成预览"));
    validationStatusLabel_->setStyleSheet(QStringLiteral("color: #8a5a00; font-weight: 700;"));
    clearConfirmation(hadConfirmationContext
                          ? QStringLiteral("确认已失效，请重新生成并核对预览")
                          : QStringLiteral("参数已修改，请生成预览"));
}

BmsCommandRequest BmsCommandPage::buildRequest(const BmsCommandDefinition &definition) const
{
    BmsCommandRequest request;
    request.commandId = definition.commandId;

    for (const ParameterControl &control : parameterControls_) {
        switch (control.valueType) {
        case BmsValueType::Boolean:
            if (auto *checkBox = qobject_cast<QCheckBox *>(control.widget)) {
                request.engineeringParameters.insert(control.parameterId, checkBox->isChecked());
            }
            break;
        case BmsValueType::Enum:
            if (auto *comboBox = qobject_cast<QComboBox *>(control.widget)) {
                request.engineeringParameters.insert(control.parameterId, comboBox->currentData());
            }
            break;
        case BmsValueType::UnsignedInteger:
        case BmsValueType::SignedInteger:
        case BmsValueType::Physical:
            if (auto *lineEdit = qobject_cast<QLineEdit *>(control.widget)) {
                // The raw text is handed over as-is; BmsCommandEncoder owns all
                // scale/offset/range conversion and reports non-numeric input.
                request.engineeringParameters.insert(control.parameterId, lineEdit->text().trimmed());
            }
            break;
        }
    }

    return request;
}

void BmsCommandPage::generatePreview()
{
    BmsCommandDefinition definition;
    if (!currentDefinition(&definition)) {
        return;
    }

    const BmsCommandRequest request = buildRequest(definition);
    const BmsCommandEncodeResult result = BmsCommandEncoder().encode(definition, request);

    showValidationIssues(result.validation);

    // A preview click captures whatever snapshot is currently in force; the two
    // outcomes below invalidate it with different reasons, never both at once.
    const std::optional<BmsCommandConfirmationSnapshot> previousSnapshot = currentAuditSnapshot();

    if (!result.ok()) {
        if (previousSnapshot.has_value()) {
            emitAuditRecord(BmsCommandAuditEventType::PreviewInvalidated,
                            BmsCommandAuditOutcome::Info,
                            &previousSnapshot.value(),
                            QStringLiteral("PreviewValidationFailed"),
                            QStringLiteral("新预览校验失败，原预览上下文已失效"));
        }
        // Never leave a stale successful preview visible next to a failed run.
        clearPreview();
        validationStatusLabel_->setText(QStringLiteral("校验失败"));
        validationStatusLabel_->setStyleSheet(QStringLiteral("color: #b54708; font-weight: 700;"));
        confirmationGate_.invalidate();
        currentRevision_ = 0;
        clearConfirmation(QStringLiteral("确认已失效，请重新生成并核对预览"));
        return;
    }

    // A successful re-preview supersedes the previous snapshot before the new one
    // is staged, so the trail shows PreviewInvalidated(old) then PreviewStaged(new).
    if (previousSnapshot.has_value()) {
        emitAuditRecord(BmsCommandAuditEventType::PreviewInvalidated,
                        BmsCommandAuditOutcome::Info,
                        &previousSnapshot.value(),
                        QStringLiteral("PreviewRegenerated"),
                        QStringLiteral("用户重新生成预览，原确认上下文失效"));
    }

    showPreview(result.command);
    validationStatusLabel_->setText(QStringLiteral("预览生成成功"));
    validationStatusLabel_->setStyleSheet(QStringLiteral("color: #1f8f5f; font-weight: 700;"));

    // Every successful preview stages a new revision, so a previous confirmation
    // never carries over - not even when the encoded bytes are identical.
    const BmsCommandConfirmationResult staging = confirmationGate_.stage(result.command);
    if (!staging.accepted) {
        currentRevision_ = 0;
        clearConfirmation(QStringLiteral("内部快照错误：%1").arg(staging.message));
        return;
    }

    const BmsCommandConfirmationSnapshot staged = *confirmationGate_.stagedSnapshot();
    emitAuditRecord(BmsCommandAuditEventType::PreviewStaged,
                    BmsCommandAuditOutcome::Info,
                    &staged,
                    QStringLiteral("PreviewStaged"),
                    QStringLiteral("已生成新的 Demo 指令预览快照"));

    showStagedConfirmation(staged);
}

void BmsCommandPage::showStagedConfirmation(const BmsCommandConfirmationSnapshot &snapshot)
{
    currentRevision_ = snapshot.revision;
    confirmationRevisionValueLabel_->setText(QString::number(snapshot.revision));
    confirmationCodeValueLabel_->setText(snapshot.confirmationCode);
    confirmedFingerprintValueLabel_->setText(EmptyValue);
    confirmedAtValueLabel_->setText(EmptyValue);
    confirmationCodeLineEdit_->clear();
    confirmationAcknowledgementCheckBox_->setChecked(false);
    setConfirmationStatus(QStringLiteral("等待确认当前预览"), false);
    // A new revision is never carried by a previous dispatch result.
    resetDispatchState();
}

void BmsCommandPage::confirmCurrentPreview()
{
    const BmsCommandConfirmationResult result =
        confirmationGate_.confirm(currentRevision_,
                                  confirmationCodeLineEdit_->text(),
                                  confirmationAcknowledgementCheckBox_->isChecked());

    if (!result.accepted) {
        // The staged preview stays intact so the user can correct the input.
        QString message = result.message;
        if (result.code == QStringLiteral("AcknowledgementRequired")) {
            message = QStringLiteral("请勾选核对声明");
        } else if (result.code == QStringLiteral("ConfirmationCodeMismatch")) {
            message = QStringLiteral("核对码不匹配");
        } else if (result.code == QStringLiteral("RevisionMismatch")) {
            message = QStringLiteral("预览已失效，请重新生成预览");
        } else if (result.code == QStringLiteral("NoStagedSnapshot")) {
            message = QStringLiteral("当前没有可确认的预览");
        }
        setConfirmationStatus(message, false);

        // The reason code and a readable message are audited; the user-entered
        // confirmation code is never captured. When a staged snapshot exists it is
        // attached, otherwise the record carries revision 0 and an empty command.
        const std::optional<BmsCommandConfirmationSnapshot> staged =
            confirmationGate_.stagedSnapshot();
        emitAuditRecord(BmsCommandAuditEventType::ConfirmationFailed,
                        BmsCommandAuditOutcome::Failure,
                        staged.has_value() ? &staged.value() : nullptr,
                        result.code,
                        message);
        return;
    }

    const auto snapshot = confirmationGate_.confirmedSnapshot();
    confirmedFingerprintValueLabel_->setText(snapshot->command.fingerprint);
    confirmedAtValueLabel_->setText(snapshot->confirmedAtUtc.toString(Qt::ISODate));
    confirmationRevisionValueLabel_->setText(QString::number(snapshot->revision));
    setConfirmationStatus(QStringLiteral("当前预览快照已确认"), true);
    updateSendButtonState();

    emitAuditRecord(BmsCommandAuditEventType::ConfirmationSucceeded,
                    BmsCommandAuditOutcome::Success,
                    &snapshot.value(),
                    QStringLiteral("ConfirmationAccepted"),
                    QStringLiteral("用户已确认当前预览快照"));
}

void BmsCommandPage::clearConfirmation(const QString &statusMessage)
{
    confirmationRevisionValueLabel_->setText(EmptyValue);
    confirmationCodeValueLabel_->setText(EmptyValue);
    confirmedFingerprintValueLabel_->setText(EmptyValue);
    confirmedAtValueLabel_->setText(EmptyValue);
    confirmationCodeLineEdit_->clear();
    confirmationAcknowledgementCheckBox_->setChecked(false);
    setConfirmationStatus(statusMessage, false);
    // Losing the confirmation always revokes dispatch eligibility.
    resetDispatchState();
}

void BmsCommandPage::setConfirmationStatus(const QString &message, bool success)
{
    confirmationStatusLabel_->setText(message);
    confirmationStatusLabel_->setStyleSheet(
        success ? QStringLiteral("color: #1f8f5f; font-weight: 700;")
                : QStringLiteral("color: #8a5a00; font-weight: 700;"));
}

void BmsCommandPage::setMockDispatchStatus(const QString &message, bool success)
{
    mockDispatchStatusLabel_->setText(message);
    mockDispatchStatusLabel_->setStyleSheet(
        success ? QStringLiteral("color: #1f8f5f; font-weight: 700;")
                : QStringLiteral("color: #8a5a00; font-weight: 700;"));
}

void BmsCommandPage::resetDispatchState()
{
    dispatchInFlight_ = false;
    dispatchedRevision_ = 0;
    updateSendButtonState();
}

void BmsCommandPage::updateSendButtonState()
{
    const auto confirmed = confirmationGate_.confirmedSnapshot();

    // Every condition is required; the reason shown names the first one missing.
    QString reason;
    bool enabled = false;
    if (!confirmed.has_value()) {
        reason = QStringLiteral("请先确认当前预览");
    } else if (dispatchInFlight_) {
        reason = QStringLiteral("正在提交 Mock 单次发送请求");
    } else if (dispatchedRevision_ == confirmed->revision) {
        reason = QStringLiteral("该确认版本已发送过，请重新生成并确认预览");
    } else if (!mockDispatchAvailable_) {
        reason = mockDispatchAvailabilityLabel_->text();
    } else if (mockDispatchChannel_ != static_cast<int>(confirmed->command.channel)) {
        reason = QStringLiteral("指令通道 %1 与当前 Mock 通道 %2 不一致")
                     .arg(confirmed->command.channel)
                     .arg(mockDispatchChannel_);
    } else {
        enabled = true;
    }

    sendCommandButton_->setEnabled(enabled);
    sendCommandButton_->setToolTip(
        enabled ? QStringLiteral("发送已确认的 Demo 快照到 Mock CAN 通道 %1").arg(mockDispatchChannel_)
                : reason);

    if (!enabled && !dispatchInFlight_ && dispatchedRevision_ == 0) {
        setMockDispatchStatus(reason, false);
    }
}

void BmsCommandPage::setMockDispatchAvailability(bool available, int channel, const QString &reason)
{
    mockDispatchAvailable_ = available;
    mockDispatchChannel_ = channel;
    mockDispatchAvailabilityLabel_->setText(reason);
    mockDispatchAvailabilityLabel_->setStyleSheet(
        available ? QStringLiteral("color: #1f8f5f; font-weight: 700;")
                  : QStringLiteral("color: #8a5a00; font-weight: 700;"));
    updateSendButtonState();
}

void BmsCommandPage::requestMockDispatch()
{
    const auto confirmed = confirmationGate_.confirmedSnapshot();
    // Re-check instead of trusting the button state alone.
    if (!confirmed.has_value() || dispatchInFlight_ || !mockDispatchAvailable_ ||
        dispatchedRevision_ == confirmed->revision ||
        mockDispatchChannel_ != static_cast<int>(confirmed->command.channel)) {
        updateSendButtonState();
        return;
    }

    dispatchInFlight_ = true;
    updateSendButtonState();
    setMockDispatchStatus(QStringLiteral("正在提交 Mock 单次发送请求"), false);
    emit mockDispatchRequested(*confirmed);
}

void BmsCommandPage::handleMockDispatchSucceeded(quint64 revision,
                                                 const QString &fingerprint,
                                                 const QDateTime &transmittedAtUtc)
{
    const auto confirmed = confirmationGate_.confirmedSnapshot();
    if (!confirmed.has_value() || confirmed->revision != revision ||
        confirmed->command.fingerprint != fingerprint) {
        return;
    }

    dispatchInFlight_ = false;
    dispatchedRevision_ = revision;
    updateSendButtonState();
    setMockDispatchStatus(
        QStringLiteral("Demo 指令已通过 Mock CAN 单次发送，并已记录到 CAN 通信监视页面（%1）")
            .arg(transmittedAtUtc.toString(Qt::ISODate)),
        true);
}

void BmsCommandPage::handleMockDispatchFailed(quint64 revision,
                                              const QString &code,
                                              const QString &message)
{
    const auto confirmed = confirmationGate_.confirmedSnapshot();
    if (!confirmed.has_value() || confirmed->revision != revision) {
        return;
    }

    dispatchInFlight_ = false;
    if (code == QStringLiteral("SnapshotAlreadyConsumed")) {
        // Keep the button disabled for this revision; a new preview is required.
        dispatchedRevision_ = revision;
    }
    updateSendButtonState();
    setMockDispatchStatus(QStringLiteral("发送失败（%1）：%2").arg(code, message), false);
}

void BmsCommandPage::showValidationIssues(const ValidationResult &validation)
{
    validationIssuesList_->clear();
    for (const ValidationIssue &issue : validation.issues) {
        const QString parameterText = issue.parameterId.isEmpty()
                                          ? QStringLiteral("(指令级)")
                                          : issue.parameterId;
        validationIssuesList_->addItem(QStringLiteral("[%1] %2 / %3：%4")
                                           .arg(formatSeverity(issue.severity),
                                                issue.code,
                                                parameterText,
                                                issue.message));
    }
}

void BmsCommandPage::showPreview(const EncodedBmsCommand &command)
{
    previewCommandIdValueLabel_->setText(command.commandId);
    previewCanIdValueLabel_->setText(formatCanId(command.canId));
    previewChannelValueLabel_->setText(QString::number(command.channel));
    previewFrameFormatValueLabel_->setText(formatFrameFormat(command.extended));
    previewDlcValueLabel_->setText(QString::number(command.dlc));
    previewPayloadValueLabel_->setText(formatPayload(command.data));
    previewFingerprintValueLabel_->setText(command.fingerprint);
    previewEncodedAtValueLabel_->setText(command.encodedAtUtc.toString(Qt::ISODate));

    // Values come straight from the encoder snapshot; the page never recomputes raw values.
    encodedParameterTable_->setRowCount(0);
    for (auto it = command.engineeringParameters.cbegin();
         it != command.engineeringParameters.cend();
         ++it) {
        const int row = encodedParameterTable_->rowCount();
        encodedParameterTable_->insertRow(row);
        encodedParameterTable_->setItem(row, 0, new QTableWidgetItem(it.key()));
        encodedParameterTable_->setItem(row, 1, new QTableWidgetItem(it.value().toString()));
        encodedParameterTable_->setItem(
            row, 2, new QTableWidgetItem(command.rawParameters.value(it.key()).toString()));
    }
}

void BmsCommandPage::clearPreview()
{
    previewCommandIdValueLabel_->setText(EmptyValue);
    previewCanIdValueLabel_->setText(EmptyValue);
    previewChannelValueLabel_->setText(EmptyValue);
    previewFrameFormatValueLabel_->setText(EmptyValue);
    previewDlcValueLabel_->setText(EmptyValue);
    previewPayloadValueLabel_->setText(EmptyValue);
    previewFingerprintValueLabel_->setText(EmptyValue);
    previewEncodedAtValueLabel_->setText(EmptyValue);
    encodedParameterTable_->setRowCount(0);
}

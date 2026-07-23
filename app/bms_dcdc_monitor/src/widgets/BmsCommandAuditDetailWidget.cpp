#include "widgets/BmsCommandAuditDetailWidget.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
const QString EmptyValue = QStringLiteral("-");

QLabel *createValueLabel(QWidget *parent, const QString &objectName)
{
    auto *label = new QLabel(EmptyValue, parent);
    label->setObjectName(objectName);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

QTableWidget *createParameterTable(QWidget *parent, const QString &objectName)
{
    auto *table = new QTableWidget(0, 3, parent);
    table->setObjectName(objectName);
    table->setHorizontalHeaderLabels(
        {QStringLiteral("参数 ID"), QStringLiteral("类型"), QStringLiteral("值")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSortingEnabled(false);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setMinimumHeight(90);
    return table;
}
} // namespace

BmsCommandAuditDetailWidget::BmsCommandAuditDetailWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("bmsCommandAuditDetailWidget"));
    setupUi();
    renderEmpty();
}

bool BmsCommandAuditDetailWidget::hasRecord() const
{
    return currentRecord_.has_value();
}

void BmsCommandAuditDetailWidget::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(10);

    stateLabel_ = new QLabel(this);
    stateLabel_->setObjectName(QStringLiteral("bmsAuditDetailStateLabel"));
    stateLabel_->setWordWrap(true);
    stateLabel_->setStyleSheet(QStringLiteral("font-weight: 700;"));
    rootLayout->addWidget(stateLabel_);

    // Audit event group.
    auto *eventGroup = new QGroupBox(QStringLiteral("审计事件"), this);
    eventGroup->setObjectName(QStringLiteral("bmsAuditDetailEventGroup"));
    auto *eventForm = new QFormLayout(eventGroup);
    eventForm->setContentsMargins(10, 16, 10, 10);
    eventForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    sequenceValue_ = createValueLabel(eventGroup, QStringLiteral("bmsAuditDetailSequenceValue"));
    eventAtValue_ = createValueLabel(eventGroup, QStringLiteral("bmsAuditDetailEventAtValue"));
    eventTypeValue_ = createValueLabel(eventGroup, QStringLiteral("bmsAuditDetailEventTypeValue"));
    outcomeValue_ = createValueLabel(eventGroup, QStringLiteral("bmsAuditDetailOutcomeValue"));
    modeValue_ = createValueLabel(eventGroup, QStringLiteral("bmsAuditDetailModeValue"));
    codeValue_ = createValueLabel(eventGroup, QStringLiteral("bmsAuditDetailCodeValue"));
    messageValue_ = createValueLabel(eventGroup, QStringLiteral("bmsAuditDetailMessageValue"));

    eventForm->addRow(QStringLiteral("序号"), sequenceValue_);
    eventForm->addRow(QStringLiteral("事件时间 UTC"), eventAtValue_);
    eventForm->addRow(QStringLiteral("事件类型"), eventTypeValue_);
    eventForm->addRow(QStringLiteral("结果"), outcomeValue_);
    eventForm->addRow(QStringLiteral("模式"), modeValue_);
    eventForm->addRow(QStringLiteral("错误码"), codeValue_);
    eventForm->addRow(QStringLiteral("说明"), messageValue_);
    rootLayout->addWidget(eventGroup);

    // Command and CAN snapshot group.
    auto *commandGroup = new QGroupBox(QStringLiteral("指令与 CAN 快照"), this);
    commandGroup->setObjectName(QStringLiteral("bmsAuditDetailCommandGroup"));
    auto *commandForm = new QFormLayout(commandGroup);
    commandForm->setContentsMargins(10, 16, 10, 10);
    commandForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    contextValue_ = createValueLabel(commandGroup, QStringLiteral("bmsAuditDetailContextValue"));
    revisionValue_ = createValueLabel(commandGroup, QStringLiteral("bmsAuditDetailRevisionValue"));
    commandIdValue_ = createValueLabel(commandGroup, QStringLiteral("bmsAuditDetailCommandIdValue"));
    safetyLevelValue_ =
        createValueLabel(commandGroup, QStringLiteral("bmsAuditDetailSafetyLevelValue"));
    channelValue_ = createValueLabel(commandGroup, QStringLiteral("bmsAuditDetailChannelValue"));
    canIdValue_ = createValueLabel(commandGroup, QStringLiteral("bmsAuditDetailCanIdValue"));
    frameFormatValue_ =
        createValueLabel(commandGroup, QStringLiteral("bmsAuditDetailFrameFormatValue"));
    dlcValue_ = createValueLabel(commandGroup, QStringLiteral("bmsAuditDetailDlcValue"));
    payloadValue_ = createValueLabel(commandGroup, QStringLiteral("bmsAuditDetailPayloadValue"));
    payloadValue_->setStyleSheet(QStringLiteral("font-family: monospace;"));
    fingerprintValue_ =
        createValueLabel(commandGroup, QStringLiteral("bmsAuditDetailFingerprintValue"));
    fingerprintValue_->setStyleSheet(QStringLiteral("font-family: monospace;"));
    // The detail view always shows the whole fingerprint, so keyboard selection of
    // the full 64-character value is available too.
    fingerprintValue_->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                               Qt::TextSelectableByKeyboard);

    commandForm->addRow(QStringLiteral("指令上下文"), contextValue_);
    commandForm->addRow(QStringLiteral("版本 revision"), revisionValue_);
    commandForm->addRow(QStringLiteral("指令 ID"), commandIdValue_);
    commandForm->addRow(QStringLiteral("安全等级"), safetyLevelValue_);
    commandForm->addRow(QStringLiteral("通道"), channelValue_);
    commandForm->addRow(QStringLiteral("CAN ID"), canIdValue_);
    commandForm->addRow(QStringLiteral("帧格式"), frameFormatValue_);
    commandForm->addRow(QStringLiteral("DLC"), dlcValue_);
    commandForm->addRow(QStringLiteral("Payload"), payloadValue_);
    commandForm->addRow(QStringLiteral("完整 Fingerprint"), fingerprintValue_);
    rootLayout->addWidget(commandGroup);

    // Lifecycle timestamp group.
    auto *timeGroup = new QGroupBox(QStringLiteral("生命周期时间（UTC）"), this);
    timeGroup->setObjectName(QStringLiteral("bmsAuditDetailTimeGroup"));
    auto *timeForm = new QFormLayout(timeGroup);
    timeForm->setContentsMargins(10, 16, 10, 10);
    timeForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    eventAtUtcValue_ = createValueLabel(timeGroup, QStringLiteral("bmsAuditDetailEventAtUtcValue"));
    stagedAtUtcValue_ =
        createValueLabel(timeGroup, QStringLiteral("bmsAuditDetailStagedAtUtcValue"));
    confirmedAtUtcValue_ =
        createValueLabel(timeGroup, QStringLiteral("bmsAuditDetailConfirmedAtUtcValue"));
    transmittedAtUtcValue_ =
        createValueLabel(timeGroup, QStringLiteral("bmsAuditDetailTransmittedAtUtcValue"));

    timeForm->addRow(QStringLiteral("事件时间"), eventAtUtcValue_);
    timeForm->addRow(QStringLiteral("预览暂存时间"), stagedAtUtcValue_);
    timeForm->addRow(QStringLiteral("确认时间"), confirmedAtUtcValue_);
    timeForm->addRow(QStringLiteral("发送时间"), transmittedAtUtcValue_);
    rootLayout->addWidget(timeGroup);

    // Parameter snapshots.
    auto *engineeringGroup = new QGroupBox(QStringLiteral("工程参数"), this);
    engineeringGroup->setObjectName(QStringLiteral("bmsAuditDetailEngineeringGroup"));
    auto *engineeringLayout = new QVBoxLayout(engineeringGroup);
    engineeringLayout->setContentsMargins(10, 16, 10, 10);
    engineeringTable_ =
        createParameterTable(engineeringGroup, QStringLiteral("bmsAuditDetailEngineeringTable"));
    engineeringLayout->addWidget(engineeringTable_);
    rootLayout->addWidget(engineeringGroup);

    auto *rawGroup = new QGroupBox(QStringLiteral("原始参数"), this);
    rawGroup->setObjectName(QStringLiteral("bmsAuditDetailRawGroup"));
    auto *rawLayout = new QVBoxLayout(rawGroup);
    rawLayout->setContentsMargins(10, 16, 10, 10);
    rawTable_ = createParameterTable(rawGroup, QStringLiteral("bmsAuditDetailRawTable"));
    rawLayout->addWidget(rawTable_);
    rootLayout->addWidget(rawGroup);

    rootLayout->addStretch(1);
}

void BmsCommandAuditDetailWidget::setRecord(const BmsCommandAuditRecord &record)
{
    // Independent value copy: later external edits to the source record must not
    // change what is displayed here.
    currentRecord_ = record;
    renderRecord(record);
}

void BmsCommandAuditDetailWidget::clearRecord()
{
    currentRecord_.reset();
    renderEmpty();
}

void BmsCommandAuditDetailWidget::renderEmpty()
{
    stateLabel_->setText(QStringLiteral("请选择一条审计记录查看完整详情"));

    for (QLabel *label : {sequenceValue_, eventAtValue_, eventTypeValue_, outcomeValue_, modeValue_,
                          codeValue_, messageValue_, contextValue_, revisionValue_, commandIdValue_,
                          safetyLevelValue_, channelValue_, canIdValue_, frameFormatValue_,
                          dlcValue_, payloadValue_, fingerprintValue_, eventAtUtcValue_,
                          stagedAtUtcValue_, confirmedAtUtcValue_, transmittedAtUtcValue_}) {
        label->setText(EmptyValue);
    }

    engineeringTable_->setRowCount(0);
    rawTable_->setRowCount(0);
}

void BmsCommandAuditDetailWidget::renderRecord(const BmsCommandAuditRecord &record)
{
    // The command-context rule is identical to the summary table's, so the detail
    // view never contradicts the row it was opened from.
    const bool hasCommandContext = record.revision > 0 && !record.commandId.isEmpty();

    QString state = QStringLiteral("正在查看审计记录 #%1").arg(record.sequence);
    if (!hasCommandContext) {
        state += QStringLiteral("（该事件没有关联的指令快照）");
    }
    stateLabel_->setText(state);

    // Event group.
    sequenceValue_->setText(QString::number(record.sequence));
    eventAtValue_->setText(timestampText(record.eventAtUtc));
    eventTypeValue_->setText(bmsCommandAuditEventText(record.eventType));
    outcomeValue_->setText(bmsCommandAuditOutcomeText(record.outcome));
    modeValue_->setText(bmsCommandAuditModeText(record.mode));
    codeValue_->setText(record.code.isEmpty() ? EmptyValue : record.code);
    messageValue_->setText(record.message.isEmpty() ? EmptyValue : record.message);

    // Command and CAN group.
    contextValue_->setText(hasCommandContext ? QStringLiteral("有指令上下文")
                                             : QStringLiteral("无指令上下文"));
    if (hasCommandContext) {
        revisionValue_->setText(QString::number(record.revision));
        commandIdValue_->setText(record.commandId);
        safetyLevelValue_->setText(safetyLevelText(record.safetyLevel));
        channelValue_->setText(QString::number(record.channel));
        canIdValue_->setText(bmsCommandAuditCanIdText(record.canId));
        frameFormatValue_->setText(frameFormatText(record.extended));
        dlcValue_->setText(QString::number(record.dlc));
        payloadValue_->setText(bmsCommandAuditPayloadText(record.payload));
        fingerprintValue_->setText(record.fingerprint.isEmpty() ? EmptyValue : record.fingerprint);
    } else {
        // A record with no staged command carries the struct's zero defaults; the
        // detail view refuses to present those as real command attributes.
        for (QLabel *label : {revisionValue_, commandIdValue_, safetyLevelValue_, channelValue_,
                              canIdValue_, frameFormatValue_, dlcValue_, payloadValue_,
                              fingerprintValue_}) {
            label->setText(EmptyValue);
        }
    }

    // Lifecycle time group.
    eventAtUtcValue_->setText(timestampText(record.eventAtUtc));
    stagedAtUtcValue_->setText(timestampText(record.stagedAtUtc));
    confirmedAtUtcValue_->setText(timestampText(record.confirmedAtUtc));
    transmittedAtUtcValue_->setText(timestampText(record.transmittedAtUtc));

    populateParameterTable(engineeringTable_, record.engineeringParameters);
    populateParameterTable(rawTable_, record.rawParameters);
}

void BmsCommandAuditDetailWidget::populateParameterTable(QTableWidget *table,
                                                         const QVariantMap &parameters)
{
    table->setRowCount(0);

    // Explicit lexicographic ordering, not a reliance on the map's internal order.
    QStringList keys = parameters.keys();
    std::sort(keys.begin(), keys.end());

    for (const QString &key : keys) {
        const QVariant value = parameters.value(key);
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(key));
        table->setItem(row, 1, new QTableWidgetItem(variantTypeText(value)));
        table->setItem(row, 2, new QTableWidgetItem(variantValueText(value)));
    }
}

QString BmsCommandAuditDetailWidget::timestampText(const QDateTime &timestamp)
{
    if (!timestamp.isValid()) {
        return EmptyValue;
    }
    return timestamp.toUTC().toString(Qt::ISODateWithMs);
}

QString BmsCommandAuditDetailWidget::safetyLevelText(BmsSafetyLevel level)
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

QString BmsCommandAuditDetailWidget::frameFormatText(bool extended)
{
    return extended ? QStringLiteral("扩展帧") : QStringLiteral("标准帧");
}

QString BmsCommandAuditDetailWidget::variantTypeText(const QVariant &value)
{
    switch (value.typeId()) {
    case QMetaType::Bool:
        return QStringLiteral("bool");
    case QMetaType::Int:
        return QStringLiteral("int");
    case QMetaType::UInt:
        return QStringLiteral("uint");
    case QMetaType::LongLong:
        return QStringLiteral("qlonglong");
    case QMetaType::ULongLong:
        return QStringLiteral("qulonglong");
    case QMetaType::Float:
    case QMetaType::Double:
        return QStringLiteral("double");
    case QMetaType::QString:
        return QStringLiteral("QString");
    case QMetaType::QByteArray:
        return QStringLiteral("QByteArray");
    default:
        break;
    }
    return QString::fromLatin1(value.metaType().name());
}

QString BmsCommandAuditDetailWidget::variantValueText(const QVariant &value)
{
    if (!value.isValid() || value.isNull()) {
        return EmptyValue;
    }

    switch (value.typeId()) {
    case QMetaType::Bool:
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QMetaType::QByteArray: {
        const QByteArray bytes = value.toByteArray();
        if (bytes.isEmpty()) {
            return QStringLiteral("（空）");
        }
        return QString::fromLatin1(bytes.toHex(' ')).toUpper();
    }
    case QMetaType::Float:
    case QMetaType::Double:
        // Locale-independent so a Chinese or English decimal setting cannot change it.
        return QString::number(value.toDouble(), 'g', 15);
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return value.toString();
    case QMetaType::QString: {
        const QString text = value.toString();
        return text.isEmpty() ? QStringLiteral("\"\"") : text;
    }
    default:
        break;
    }

    const QString fallback = value.toString();
    return fallback.isEmpty() ? EmptyValue : fallback;
}

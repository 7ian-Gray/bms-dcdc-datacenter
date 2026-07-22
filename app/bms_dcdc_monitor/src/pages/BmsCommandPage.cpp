#include "pages/BmsCommandPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
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
}

void BmsCommandPage::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

    auto *titleLabel = new QLabel(QStringLiteral("BMS 指令下发（预览模式）"), this);
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
        QStringLiteral("当前仅提供 Demo/Mock 指令配置、校验与 CAN 帧预览。\n"
                       "未加载真实厂商 BMS 指令，不支持向真实设备发送。"),
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
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

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
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

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
    parameterFormLayout_->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
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
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

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
    layout->addWidget(createEncodedParameterPanel());

    // Preview-only build: the control is shown so the workflow is visible, but it
    // is never enabled, never connected, and the page exposes no send signal.
    sendCommandButton_ = new QPushButton(QStringLiteral("发送指令（未启用）"), content);
    sendCommandButton_->setObjectName(QStringLiteral("sendCommandButton"));
    sendCommandButton_->setEnabled(false);
    sendCommandButton_->setToolTip(
        QStringLiteral("当前版本仅支持 Demo 指令配置与 CAN 帧预览，不支持真实发送"));
    layout->addWidget(sendCommandButton_);

    layout->addStretch(1);
    scrollArea->setWidget(content);
    return scrollArea;
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
            control = checkBox;
            break;
        }
        case BmsValueType::Enum: {
            auto *comboBox = new QComboBox(parameterGroup_);
            comboBox->setObjectName(objectName);
            for (auto it = parameter.enumValues.cbegin(); it != parameter.enumValues.cend(); ++it) {
                comboBox->addItem(it.value(), QVariant::fromValue(it.key()));
            }
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

    if (!result.ok()) {
        // Never leave a stale successful preview visible next to a failed run.
        clearPreview();
        validationStatusLabel_->setText(QStringLiteral("校验失败"));
        validationStatusLabel_->setStyleSheet(QStringLiteral("color: #b54708; font-weight: 700;"));
        return;
    }

    showPreview(result.command);
    validationStatusLabel_->setText(QStringLiteral("预览生成成功"));
    validationStatusLabel_->setStyleSheet(QStringLiteral("color: #1f8f5f; font-weight: 700;"));
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

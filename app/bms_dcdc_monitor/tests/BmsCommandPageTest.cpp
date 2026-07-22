#include "pages/BmsCommandPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QtTest/QtTest>

class BmsCommandPageTest : public QObject
{
    Q_OBJECT

private slots:
    void initializesInDemoMockMode();
    void buildsExpectedDemoPreview();
    void invalidInputClearsPreviousPreview();
    void repeatedPreviewIsDeterministic();
    void commandSelectionRebuildsParameterForm();
    void successfulPreviewRequiresConfirmation();
    void confirmsCurrentPreviewWithAcknowledgementAndCode();
    void rejectsWrongConfirmationCode();
    void editingParameterInvalidatesConfirmationAndPreview();
    void repeatedPreviewRequiresNewConfirmation();
    void initialParameterEntryDoesNotClaimConfirmationInvalidation();
    void sendDisabledUntilConfirmedAndMockReady();
    void confirmedSnapshotEnablesMatchingMockSend();
    void channelMismatchKeepsSendDisabled();
    void clickEmitsConfirmedSnapshotOnlyOnce();
    void dispatchSuccessConsumesCurrentRevision();
    void dispatchFailureAllowsSafeRetry();
    void parameterEditClearsDispatchEligibility();

private:
    // Marks the Mock channel as ready on channel 0, matching the demo command.
    static void makeMockReady(BmsCommandPage *page, int channel = 0);
    // Fills the form, previews, then supplies the displayed code and
    // acknowledgement so the current snapshot ends up confirmed.
    static void previewAndConfirm(BmsCommandPage *page);
    // Fills the demo parameter form with a known-valid set of engineering values.
    static void fillDemoParameters(BmsCommandPage *page,
                                   const QString &voltage,
                                   const QString &current,
                                   bool enabled,
                                   qint64 mode);
    static QString labelText(BmsCommandPage *page, const QString &objectName);
};

void BmsCommandPageTest::fillDemoParameters(BmsCommandPage *page,
                                            const QString &voltage,
                                            const QString &current,
                                            bool enabled,
                                            qint64 mode)
{
    auto *voltageEdit = page->findChild<QLineEdit *>(QStringLiteral("bmsParameter_demo_voltage_v"));
    auto *currentEdit = page->findChild<QLineEdit *>(QStringLiteral("bmsParameter_demo_current_a"));
    auto *enableBox = page->findChild<QCheckBox *>(QStringLiteral("bmsParameter_demo_enable"));
    auto *modeCombo = page->findChild<QComboBox *>(QStringLiteral("bmsParameter_demo_mode"));

    QVERIFY(voltageEdit != nullptr);
    QVERIFY(currentEdit != nullptr);
    QVERIFY(enableBox != nullptr);
    QVERIFY(modeCombo != nullptr);

    voltageEdit->setText(voltage);
    currentEdit->setText(current);
    enableBox->setChecked(enabled);

    const int modeIndex = modeCombo->findData(QVariant::fromValue(mode));
    QVERIFY(modeIndex >= 0);
    modeCombo->setCurrentIndex(modeIndex);
}

QString BmsCommandPageTest::labelText(BmsCommandPage *page, const QString &objectName)
{
    auto *label = page->findChild<QLabel *>(objectName);
    return label == nullptr ? QString() : label->text();
}

void BmsCommandPageTest::initializesInDemoMockMode()
{
    BmsCommandPage page;

    auto *commandCombo = page.findChild<QComboBox *>(QStringLiteral("bmsCommandComboBox"));
    QVERIFY(commandCombo != nullptr);
    QVERIFY(commandCombo->count() >= 1);

    auto *title = page.findChild<QLabel *>(QStringLiteral("bmsCommandPageTitle"));
    QVERIFY(title != nullptr);
    QCOMPARE(title->text(), QStringLiteral("BMS 指令下发（Demo / Mock 模式）"));

    auto *banner = page.findChild<QLabel *>(QStringLiteral("bmsCommandSafetyBanner"));
    QVERIFY(banner != nullptr);
    QVERIFY(!banner->text().isEmpty());

    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    QVERIFY(sendButton != nullptr);
    QVERIFY(!sendButton->isEnabled());
    // The app-wide stylesheet paints all buttons active blue and defines no
    // disabled state, so the button must carry its own inactive styling.
    QVERIFY(sendButton->styleSheet().contains(QStringLiteral("disabled")));

    QCOMPARE(labelText(&page, QStringLiteral("commandDemoOnlyValue")), QStringLiteral("是"));
    QCOMPARE(labelText(&page, QStringLiteral("commandSafetyLevelValue")), QStringLiteral("Demo Only"));

    // The page may only ask for a confirmed snapshot to be dispatched; it must
    // never be able to push a raw frame or a hardware/periodic send.
    QStringList signalSignatures;
    const QMetaObject *meta = page.metaObject();
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        const QMetaMethod method = meta->method(i);
        if (method.methodType() == QMetaMethod::Signal) {
            signalSignatures << QString::fromLatin1(method.methodSignature());
        }
    }

    QVERIFY2(signalSignatures.contains(
                 QStringLiteral("mockDispatchRequested(BmsCommandConfirmationSnapshot)")),
             qPrintable(signalSignatures.join(QStringLiteral(", "))));
    for (const QString &signature : signalSignatures) {
        QVERIFY2(!signature.startsWith(QStringLiteral("sendFrameRequested")), qPrintable(signature));
        QVERIFY2(!signature.startsWith(QStringLiteral("hardwareSendRequested")), qPrintable(signature));
        QVERIFY2(!signature.startsWith(QStringLiteral("periodicSendRequested")), qPrintable(signature));
        QVERIFY2(!signature.contains(QStringLiteral("CanFrame")), qPrintable(signature));
    }
}

void BmsCommandPageTest::buildsExpectedDemoPreview()
{
    BmsCommandPage page;
    fillDemoParameters(&page, QStringLiteral("1000.0"), QStringLiteral("-12.3"), true, 2);

    auto *previewButton = page.findChild<QPushButton *>(QStringLiteral("previewCommandButton"));
    QVERIFY(previewButton != nullptr);
    previewButton->click();

    QCOMPARE(labelText(&page, QStringLiteral("previewCanIdValue")), QStringLiteral("0x18E50101"));
    QCOMPARE(labelText(&page, QStringLiteral("previewChannelValue")), QStringLiteral("0"));
    QCOMPARE(labelText(&page, QStringLiteral("previewFrameFormatValue")), QStringLiteral("扩展帧"));
    QCOMPARE(labelText(&page, QStringLiteral("previewDlcValue")), QStringLiteral("8"));
    QCOMPARE(labelText(&page, QStringLiteral("previewPayloadValue")),
             QStringLiteral("10 27 FF 85 05 00 00 00"));

    // Only stability and format are asserted; the hash inputs may grow later.
    const QString fingerprint = labelText(&page, QStringLiteral("previewFingerprintValue"));
    QCOMPARE(fingerprint.size(), 64);
    QVERIFY(!fingerprint.isEmpty());

    auto *status = page.findChild<QLabel *>(QStringLiteral("validationStatusLabel"));
    QVERIFY(status != nullptr);
    QCOMPARE(status->text(), QStringLiteral("预览生成成功"));

    auto *table = page.findChild<QTableWidget *>(QStringLiteral("encodedParameterTable"));
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 4);

    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    QVERIFY(sendButton != nullptr);
    QVERIFY(!sendButton->isEnabled());
}

void BmsCommandPageTest::invalidInputClearsPreviousPreview()
{
    BmsCommandPage page;
    auto *previewButton = page.findChild<QPushButton *>(QStringLiteral("previewCommandButton"));
    QVERIFY(previewButton != nullptr);

    fillDemoParameters(&page, QStringLiteral("1000.0"), QStringLiteral("-12.3"), true, 2);
    previewButton->click();
    QCOMPARE(labelText(&page, QStringLiteral("previewPayloadValue")),
             QStringLiteral("10 27 FF 85 05 00 00 00"));

    auto *voltageEdit = page.findChild<QLineEdit *>(QStringLiteral("bmsParameter_demo_voltage_v"));
    QVERIFY(voltageEdit != nullptr);
    voltageEdit->clear();
    previewButton->click();

    auto *status = page.findChild<QLabel *>(QStringLiteral("validationStatusLabel"));
    QVERIFY(status != nullptr);
    QCOMPARE(status->text(), QStringLiteral("校验失败"));

    auto *issues = page.findChild<QListWidget *>(QStringLiteral("validationIssuesList"));
    QVERIFY(issues != nullptr);
    bool foundInvalidNumber = false;
    for (int i = 0; i < issues->count(); ++i) {
        if (issues->item(i)->text().contains(QStringLiteral("InvalidNumber"))) {
            foundInvalidNumber = true;
            break;
        }
    }
    QVERIFY(foundInvalidNumber);

    QCOMPARE(labelText(&page, QStringLiteral("previewPayloadValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("previewFingerprintValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("previewCanIdValue")), QStringLiteral("-"));

    auto *table = page.findChild<QTableWidget *>(QStringLiteral("encodedParameterTable"));
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 0);

    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    QVERIFY(sendButton != nullptr);
    QVERIFY(!sendButton->isEnabled());
}

void BmsCommandPageTest::repeatedPreviewIsDeterministic()
{
    BmsCommandPage page;
    auto *previewButton = page.findChild<QPushButton *>(QStringLiteral("previewCommandButton"));
    QVERIFY(previewButton != nullptr);

    fillDemoParameters(&page, QStringLiteral("100.0"), QStringLiteral("-10.0"), true, 1);

    previewButton->click();
    const QString firstPayload = labelText(&page, QStringLiteral("previewPayloadValue"));
    const QString firstFingerprint = labelText(&page, QStringLiteral("previewFingerprintValue"));

    previewButton->click();
    const QString secondPayload = labelText(&page, QStringLiteral("previewPayloadValue"));
    const QString secondFingerprint = labelText(&page, QStringLiteral("previewFingerprintValue"));

    QVERIFY(!firstPayload.isEmpty());
    QCOMPARE(firstPayload, secondPayload);
    QCOMPARE(firstFingerprint, secondFingerprint);

    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    QVERIFY(sendButton != nullptr);
    QVERIFY(!sendButton->isEnabled());
}

void BmsCommandPageTest::commandSelectionRebuildsParameterForm()
{
    BmsCommandPage page;

    // Every parameter of the selected definition must have a matching control.
    BmsCommandDefinition definition;
    auto *commandCombo = page.findChild<QComboBox *>(QStringLiteral("bmsCommandComboBox"));
    QVERIFY(commandCombo != nullptr);
    QVERIFY(BmsCommandCatalog::findDemoCommand(commandCombo->currentData().toString(), &definition));

    for (const BmsParameterDefinition &parameter : definition.parameters) {
        const QString objectName = QStringLiteral("bmsParameter_%1").arg(parameter.parameterId);
        QVERIFY2(page.findChild<QWidget *>(objectName) != nullptr, qPrintable(objectName));
    }

    // A selection change must rebuild the form: QFormLayout::removeRow() deletes
    // the row's widgets, so the original control is destroyed and replaced.
    QPointer<QLineEdit> originalVoltageEdit =
        page.findChild<QLineEdit *>(QStringLiteral("bmsParameter_demo_voltage_v"));
    QVERIFY(!originalVoltageEdit.isNull());

    // Clearing the selection first, because assigning the current index again
    // would be a no-op and would never emit currentIndexChanged.
    commandCombo->setCurrentIndex(-1);
    commandCombo->setCurrentIndex(0);

    QVERIFY(originalVoltageEdit.isNull());

    const QList<QLineEdit *> voltageEdits =
        page.findChildren<QLineEdit *>(QStringLiteral("bmsParameter_demo_voltage_v"));
    QCOMPARE(voltageEdits.size(), 1);
}

void BmsCommandPageTest::previewAndConfirm(BmsCommandPage *page)
{
    fillDemoParameters(page, QStringLiteral("1000.0"), QStringLiteral("-12.3"), true, 2);
    page->findChild<QPushButton *>(QStringLiteral("previewCommandButton"))->click();

    auto *ack = page->findChild<QCheckBox *>(QStringLiteral("confirmationAcknowledgementCheckBox"));
    auto *codeEdit = page->findChild<QLineEdit *>(QStringLiteral("confirmationCodeLineEdit"));
    QVERIFY(ack != nullptr);
    QVERIFY(codeEdit != nullptr);

    ack->setChecked(true);
    codeEdit->setText(labelText(page, QStringLiteral("confirmationCodeValue")));
    page->findChild<QPushButton *>(QStringLiteral("confirmPreviewButton"))->click();
}

void BmsCommandPageTest::successfulPreviewRequiresConfirmation()
{
    BmsCommandPage page;
    fillDemoParameters(&page, QStringLiteral("1000.0"), QStringLiteral("-12.3"), true, 2);
    page.findChild<QPushButton *>(QStringLiteral("previewCommandButton"))->click();

    QCOMPARE(labelText(&page, QStringLiteral("confirmationStatusLabel")),
             QStringLiteral("等待确认当前预览"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmationRevisionValue")), QStringLiteral("1"));

    const QString code = labelText(&page, QStringLiteral("confirmationCodeValue"));
    QCOMPARE(code.size(), 8);
    QCOMPARE(code, labelText(&page, QStringLiteral("previewFingerprintValue")).right(8).toUpper());

    // Nothing is confirmed until the user acts.
    QCOMPARE(labelText(&page, QStringLiteral("confirmedFingerprintValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmedAtValue")), QStringLiteral("-"));

    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    QVERIFY(!sendButton->isEnabled());
}

void BmsCommandPageTest::confirmsCurrentPreviewWithAcknowledgementAndCode()
{
    BmsCommandPage page;
    const auto previewButton = page.findChild<QPushButton *>(QStringLiteral("previewCommandButton"));
    QVERIFY(previewButton != nullptr);

    fillDemoParameters(&page, QStringLiteral("1000.0"), QStringLiteral("-12.3"), true, 2);
    previewButton->click();
    const QString previewFingerprint = labelText(&page, QStringLiteral("previewFingerprintValue"));

    // Acknowledgement alone is not enough.
    auto *confirmButton = page.findChild<QPushButton *>(QStringLiteral("confirmPreviewButton"));
    QVERIFY(confirmButton != nullptr);
    confirmButton->click();
    QCOMPARE(labelText(&page, QStringLiteral("confirmationStatusLabel")),
             QStringLiteral("请勾选核对声明"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmedFingerprintValue")), QStringLiteral("-"));

    auto *ack = page.findChild<QCheckBox *>(QStringLiteral("confirmationAcknowledgementCheckBox"));
    auto *codeEdit = page.findChild<QLineEdit *>(QStringLiteral("confirmationCodeLineEdit"));
    ack->setChecked(true);
    codeEdit->setText(labelText(&page, QStringLiteral("confirmationCodeValue")));
    confirmButton->click();

    QCOMPARE(labelText(&page, QStringLiteral("confirmationStatusLabel")),
             QStringLiteral("当前预览快照已确认"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmedFingerprintValue")), previewFingerprint);
    QVERIFY(!labelText(&page, QStringLiteral("confirmedAtValue")).isEmpty());
    QVERIFY(labelText(&page, QStringLiteral("confirmedAtValue")) != QStringLiteral("-"));

    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    QVERIFY(!sendButton->isEnabled());
}

void BmsCommandPageTest::rejectsWrongConfirmationCode()
{
    BmsCommandPage page;
    fillDemoParameters(&page, QStringLiteral("1000.0"), QStringLiteral("-12.3"), true, 2);
    page.findChild<QPushButton *>(QStringLiteral("previewCommandButton"))->click();

    page.findChild<QCheckBox *>(QStringLiteral("confirmationAcknowledgementCheckBox"))
        ->setChecked(true);
    page.findChild<QLineEdit *>(QStringLiteral("confirmationCodeLineEdit"))
        ->setText(QStringLiteral("00000000"));
    page.findChild<QPushButton *>(QStringLiteral("confirmPreviewButton"))->click();

    QCOMPARE(labelText(&page, QStringLiteral("confirmationStatusLabel")),
             QStringLiteral("核对码不匹配"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmedFingerprintValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmedAtValue")), QStringLiteral("-"));

    // The staged preview survives so the code can be corrected.
    QCOMPARE(labelText(&page, QStringLiteral("confirmationRevisionValue")), QStringLiteral("1"));
    QVERIFY(!page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"))->isEnabled());
}

void BmsCommandPageTest::editingParameterInvalidatesConfirmationAndPreview()
{
    BmsCommandPage page;
    previewAndConfirm(&page);
    QCOMPARE(labelText(&page, QStringLiteral("confirmationStatusLabel")),
             QStringLiteral("当前预览快照已确认"));

    auto *currentEdit = page.findChild<QLineEdit *>(QStringLiteral("bmsParameter_demo_current_a"));
    QVERIFY(currentEdit != nullptr);
    currentEdit->setText(QStringLiteral("-20.0"));

    QCOMPARE(labelText(&page, QStringLiteral("confirmationStatusLabel")),
             QStringLiteral("确认已失效，请重新生成并核对预览"));
    QCOMPARE(labelText(&page, QStringLiteral("validationStatusLabel")),
             QStringLiteral("参数已修改，请重新生成预览"));

    QCOMPARE(labelText(&page, QStringLiteral("previewCanIdValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("previewPayloadValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("previewFingerprintValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmedFingerprintValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmationCodeValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmationRevisionValue")), QStringLiteral("-"));

    auto *table = page.findChild<QTableWidget *>(QStringLiteral("encodedParameterTable"));
    QCOMPARE(table->rowCount(), 0);

    QVERIFY(!page.findChild<QCheckBox *>(QStringLiteral("confirmationAcknowledgementCheckBox"))
                 ->isChecked());
    QVERIFY(page.findChild<QLineEdit *>(QStringLiteral("confirmationCodeLineEdit"))->text().isEmpty());
    QVERIFY(!page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"))->isEnabled());
}

void BmsCommandPageTest::repeatedPreviewRequiresNewConfirmation()
{
    BmsCommandPage page;
    previewAndConfirm(&page);
    const QString firstFingerprint = labelText(&page, QStringLiteral("previewFingerprintValue"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmationRevisionValue")), QStringLiteral("1"));

    // Same parameters, so the encoded content and fingerprint are identical.
    page.findChild<QPushButton *>(QStringLiteral("previewCommandButton"))->click();

    QCOMPARE(labelText(&page, QStringLiteral("previewFingerprintValue")), firstFingerprint);
    QCOMPARE(labelText(&page, QStringLiteral("confirmationRevisionValue")), QStringLiteral("2"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmationStatusLabel")),
             QStringLiteral("等待确认当前预览"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmedFingerprintValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmedAtValue")), QStringLiteral("-"));
    QVERIFY(!page.findChild<QCheckBox *>(QStringLiteral("confirmationAcknowledgementCheckBox"))
                 ->isChecked());
    QVERIFY(!page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"))->isEnabled());
}

void BmsCommandPageTest::initialParameterEntryDoesNotClaimConfirmationInvalidation()
{
    BmsCommandPage page;

    // Typing into a fresh form, with no preview ever generated.
    auto *voltageEdit = page.findChild<QLineEdit *>(QStringLiteral("bmsParameter_demo_voltage_v"));
    QVERIFY(voltageEdit != nullptr);
    voltageEdit->setText(QStringLiteral("500.0"));

    const QString status = labelText(&page, QStringLiteral("confirmationStatusLabel"));
    QCOMPARE(status, QStringLiteral("参数已修改，请生成预览"));
    QVERIFY(!status.contains(QStringLiteral("确认已失效")));

    QCOMPARE(labelText(&page, QStringLiteral("previewCanIdValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("previewPayloadValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("previewFingerprintValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmationRevisionValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmationCodeValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmedFingerprintValue")), QStringLiteral("-"));

    QVERIFY(!page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"))->isEnabled());
}

void BmsCommandPageTest::makeMockReady(BmsCommandPage *page, int channel)
{
    page->setMockDispatchAvailability(
        true, channel, QStringLiteral("Mock CAN 通道 %1 已就绪").arg(channel));
}

void BmsCommandPageTest::sendDisabledUntilConfirmedAndMockReady()
{
    BmsCommandPage page;
    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    QVERIFY(sendButton != nullptr);

    // Nothing previewed yet, even with a ready Mock channel.
    makeMockReady(&page);
    QVERIFY(!sendButton->isEnabled());

    // Staged but not confirmed.
    fillDemoParameters(&page, QStringLiteral("1000.0"), QStringLiteral("-12.3"), true, 2);
    page.findChild<QPushButton *>(QStringLiteral("previewCommandButton"))->click();
    makeMockReady(&page);
    QVERIFY(!sendButton->isEnabled());

    // Confirmed but the Mock channel is not available.
    auto *ack = page.findChild<QCheckBox *>(QStringLiteral("confirmationAcknowledgementCheckBox"));
    ack->setChecked(true);
    page.findChild<QLineEdit *>(QStringLiteral("confirmationCodeLineEdit"))
        ->setText(labelText(&page, QStringLiteral("confirmationCodeValue")));
    page.findChild<QPushButton *>(QStringLiteral("confirmPreviewButton"))->click();

    page.setMockDispatchAvailability(
        false, -1, QStringLiteral("请先在 CAN 通信监视页面打开并启动 Mock 通道"));
    QVERIFY(!sendButton->isEnabled());
}

void BmsCommandPageTest::confirmedSnapshotEnablesMatchingMockSend()
{
    BmsCommandPage page;
    previewAndConfirm(&page);

    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    QVERIFY(!sendButton->isEnabled());

    makeMockReady(&page, 0);
    QVERIFY(sendButton->isEnabled());
}

void BmsCommandPageTest::channelMismatchKeepsSendDisabled()
{
    BmsCommandPage page;
    previewAndConfirm(&page);

    // The demo command targets channel 0.
    makeMockReady(&page, 1);

    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    QVERIFY(!sendButton->isEnabled());
    QVERIFY(labelText(&page, QStringLiteral("mockDispatchStatusLabel"))
                .contains(QStringLiteral("通道")));
}

void BmsCommandPageTest::clickEmitsConfirmedSnapshotOnlyOnce()
{
    BmsCommandPage page;
    previewAndConfirm(&page);
    makeMockReady(&page, 0);

    QSignalSpy dispatchSpy(&page, &BmsCommandPage::mockDispatchRequested);
    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    QVERIFY(sendButton->isEnabled());

    sendButton->click();

    QCOMPARE(dispatchSpy.count(), 1);
    const auto snapshot =
        qvariant_cast<BmsCommandConfirmationSnapshot>(dispatchSpy.at(0).at(0));
    QCOMPARE(snapshot.revision, 1ull);
    QCOMPARE(snapshot.command.fingerprint,
             labelText(&page, QStringLiteral("previewFingerprintValue")));
    QVERIFY(snapshot.confirmedAtUtc.isValid());
    QCOMPARE(snapshot.confirmationCode, labelText(&page, QStringLiteral("confirmationCodeValue")));

    // In-flight: the button is disabled immediately and a second click is inert.
    QVERIFY(!sendButton->isEnabled());
    sendButton->click();
    QCOMPARE(dispatchSpy.count(), 1);
}

void BmsCommandPageTest::dispatchSuccessConsumesCurrentRevision()
{
    BmsCommandPage page;
    previewAndConfirm(&page);
    makeMockReady(&page, 0);

    QSignalSpy dispatchSpy(&page, &BmsCommandPage::mockDispatchRequested);
    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    sendButton->click();
    QCOMPARE(dispatchSpy.count(), 1);

    const QString fingerprint = labelText(&page, QStringLiteral("previewFingerprintValue"));
    page.handleMockDispatchSucceeded(1, fingerprint, QDateTime::currentDateTimeUtc());

    QVERIFY(labelText(&page, QStringLiteral("mockDispatchStatusLabel"))
                .contains(QStringLiteral("已通过 Mock CAN 单次发送")));
    QVERIFY(!sendButton->isEnabled());

    // The audit fields survive for review.
    QCOMPARE(labelText(&page, QStringLiteral("confirmedFingerprintValue")), fingerprint);
    QVERIFY(labelText(&page, QStringLiteral("confirmedAtValue")) != QStringLiteral("-"));

    // A further click cannot resend the consumed revision.
    sendButton->click();
    QCOMPARE(dispatchSpy.count(), 1);
}

void BmsCommandPageTest::dispatchFailureAllowsSafeRetry()
{
    BmsCommandPage page;
    previewAndConfirm(&page);
    makeMockReady(&page, 0);

    QSignalSpy dispatchSpy(&page, &BmsCommandPage::mockDispatchRequested);
    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    sendButton->click();
    QCOMPARE(dispatchSpy.count(), 1);

    // The session refused; nothing was consumed.
    page.handleMockDispatchFailed(1,
                                  QStringLiteral("SessionRejected"),
                                  QStringLiteral("通道未启动，不能发送 CAN 帧"));

    QVERIFY(labelText(&page, QStringLiteral("mockDispatchStatusLabel"))
                .contains(QStringLiteral("SessionRejected")));
    // The confirmation survives and the same snapshot may be retried.
    QCOMPARE(labelText(&page, QStringLiteral("confirmedFingerprintValue")).size(), 64);
    QVERIFY(sendButton->isEnabled());
    // No automatic retry happened.
    QCOMPARE(dispatchSpy.count(), 1);

    sendButton->click();
    QCOMPARE(dispatchSpy.count(), 2);
}

void BmsCommandPageTest::parameterEditClearsDispatchEligibility()
{
    BmsCommandPage page;
    previewAndConfirm(&page);
    makeMockReady(&page, 0);

    auto *sendButton = page.findChild<QPushButton *>(QStringLiteral("sendCommandButton"));
    QVERIFY(sendButton->isEnabled());

    QSignalSpy dispatchSpy(&page, &BmsCommandPage::mockDispatchRequested);
    page.findChild<QLineEdit *>(QStringLiteral("bmsParameter_demo_current_a"))
        ->setText(QStringLiteral("-20.0"));

    QVERIFY(!sendButton->isEnabled());
    QCOMPARE(labelText(&page, QStringLiteral("confirmationStatusLabel")),
             QStringLiteral("确认已失效，请重新生成并核对预览"));
    QCOMPARE(labelText(&page, QStringLiteral("previewPayloadValue")), QStringLiteral("-"));
    QCOMPARE(labelText(&page, QStringLiteral("confirmedFingerprintValue")), QStringLiteral("-"));

    sendButton->click();
    QCOMPARE(dispatchSpy.count(), 0);
}

QTEST_MAIN(BmsCommandPageTest)

#include "BmsCommandPageTest.moc"

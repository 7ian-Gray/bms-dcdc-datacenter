#include "widgets/BmsCommandAuditDetailWidget.h"

#include "audit/BmsCommandAudit.h"

#include <QByteArray>
#include <QDateTime>
#include <QLabel>
#include <QTableWidget>
#include <QVariant>
#include <QtTest/QtTest>

class BmsCommandAuditDetailWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void startsEmptyWithPlaceholders();
    void rendersCompleteRecord();
    void rendersEmptyContextWithoutFakeCanTarget();
    void parameterTablesAreSortedAndReadOnly();
    void recordIsCopiedByValue();
    void clearRecordResetsEveryField();

private:
    static QString sampleFingerprint();
    static BmsCommandAuditRecord fullRecord();
    static QString labelText(const BmsCommandAuditDetailWidget &widget, const QString &objectName);
};

QString BmsCommandAuditDetailWidgetTest::sampleFingerprint()
{
    return QStringLiteral("0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF");
}

BmsCommandAuditRecord BmsCommandAuditDetailWidgetTest::fullRecord()
{
    BmsCommandAuditRecord record;
    record.sequence = 12;
    record.eventType = BmsCommandAuditEventType::DispatchSucceeded;
    record.outcome = BmsCommandAuditOutcome::Success;
    record.mode = BmsCommandAuditMode::DemoMock;
    record.revision = 3;
    record.commandId = QStringLiteral("demo_mock_bms_command");
    record.safetyLevel = BmsSafetyLevel::DemoOnly;
    record.canId = 0x18E50101u;
    record.channel = 0;
    record.extended = true;
    record.dlc = 8;
    record.payload = QByteArray::fromHex("1027FF8505000000");
    record.fingerprint = sampleFingerprint();
    record.eventAtUtc = QDateTime::currentDateTimeUtc();
    record.stagedAtUtc = record.eventAtUtc.addSecs(-10);
    record.confirmedAtUtc = record.eventAtUtc.addSecs(-5);
    record.transmittedAtUtc = record.eventAtUtc;
    record.code = QStringLiteral("MockDispatchSucceeded");
    record.message = QStringLiteral("Demo 指令已通过 Mock CAN 单次发送");
    record.engineeringParameters.insert(QStringLiteral("demo_voltage_v"), 1000.0);
    record.rawParameters.insert(QStringLiteral("demo_voltage_v"), 10000);
    return record;
}

QString BmsCommandAuditDetailWidgetTest::labelText(const BmsCommandAuditDetailWidget &widget,
                                                   const QString &objectName)
{
    auto *label = widget.findChild<QLabel *>(objectName);
    return label == nullptr ? QString() : label->text();
}

void BmsCommandAuditDetailWidgetTest::startsEmptyWithPlaceholders()
{
    BmsCommandAuditDetailWidget widget;

    QVERIFY(!widget.hasRecord());
    QVERIFY(labelText(widget, QStringLiteral("bmsAuditDetailStateLabel"))
                .contains(QStringLiteral("请选择")));

    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailSequenceValue")), QStringLiteral("-"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailRevisionValue")), QStringLiteral("-"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailCommandIdValue")), QStringLiteral("-"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailChannelValue")), QStringLiteral("-"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailCanIdValue")), QStringLiteral("-"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailPayloadValue")), QStringLiteral("-"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailFingerprintValue")),
             QStringLiteral("-"));

    auto *engineering = widget.findChild<QTableWidget *>(
        QStringLiteral("bmsAuditDetailEngineeringTable"));
    auto *raw = widget.findChild<QTableWidget *>(QStringLiteral("bmsAuditDetailRawTable"));
    QVERIFY(engineering != nullptr);
    QVERIFY(raw != nullptr);
    QCOMPARE(engineering->rowCount(), 0);
    QCOMPARE(raw->rowCount(), 0);
}

void BmsCommandAuditDetailWidgetTest::rendersCompleteRecord()
{
    BmsCommandAuditDetailWidget widget;
    widget.setRecord(fullRecord());

    QVERIFY(widget.hasRecord());
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailSequenceValue")), QStringLiteral("12"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailEventTypeValue")),
             QStringLiteral("发送成功"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailOutcomeValue")),
             QStringLiteral("成功"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailModeValue")),
             QStringLiteral("Demo / Mock"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailContextValue")),
             QStringLiteral("有指令上下文"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailRevisionValue")), QStringLiteral("3"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailCommandIdValue")),
             QStringLiteral("demo_mock_bms_command"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailSafetyLevelValue")),
             QStringLiteral("Demo Only"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailChannelValue")), QStringLiteral("0"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailCanIdValue")),
             QStringLiteral("0x18E50101"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailFrameFormatValue")),
             QStringLiteral("扩展帧"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailDlcValue")), QStringLiteral("8"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailPayloadValue")),
             QStringLiteral("10 27 FF 85 05 00 00 00"));

    // The detail view shows the full fingerprint, never the abbreviated form.
    const QString fingerprint = labelText(widget, QStringLiteral("bmsAuditDetailFingerprintValue"));
    QCOMPARE(fingerprint, sampleFingerprint());
    QVERIFY(!fingerprint.contains(QStringLiteral("…")));

    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailCodeValue")),
             QStringLiteral("MockDispatchSucceeded"));
    QVERIFY(!labelText(widget, QStringLiteral("bmsAuditDetailMessageValue")).isEmpty());

    // Every lifecycle timestamp is populated and UTC (ISO with 'Z').
    for (const QString &name : {QStringLiteral("bmsAuditDetailEventAtUtcValue"),
                                QStringLiteral("bmsAuditDetailStagedAtUtcValue"),
                                QStringLiteral("bmsAuditDetailConfirmedAtUtcValue"),
                                QStringLiteral("bmsAuditDetailTransmittedAtUtcValue")}) {
        const QString text = labelText(widget, name);
        QVERIFY2(text != QStringLiteral("-"), qPrintable(name));
        QVERIFY2(text.endsWith(QStringLiteral("Z")), qPrintable(text));
    }
}

void BmsCommandAuditDetailWidgetTest::rendersEmptyContextWithoutFakeCanTarget()
{
    // A confirmation failure with no staged snapshot carries the struct defaults.
    const BmsCommandAuditRecord record = makeBmsCommandAuditRecord(
        BmsCommandAuditEventType::ConfirmationFailed, BmsCommandAuditOutcome::Failure, nullptr,
        QDateTime::currentDateTimeUtc(), QStringLiteral("NoStagedSnapshot"),
        QStringLiteral("当前没有可确认的预览"));

    BmsCommandAuditDetailWidget widget;
    widget.setRecord(record);

    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailContextValue")),
             QStringLiteral("无指令上下文"));

    for (const QString &name : {QStringLiteral("bmsAuditDetailRevisionValue"),
                                QStringLiteral("bmsAuditDetailCommandIdValue"),
                                QStringLiteral("bmsAuditDetailSafetyLevelValue"),
                                QStringLiteral("bmsAuditDetailChannelValue"),
                                QStringLiteral("bmsAuditDetailCanIdValue"),
                                QStringLiteral("bmsAuditDetailFrameFormatValue"),
                                QStringLiteral("bmsAuditDetailDlcValue"),
                                QStringLiteral("bmsAuditDetailPayloadValue"),
                                QStringLiteral("bmsAuditDetailFingerprintValue")}) {
        QCOMPARE(labelText(widget, name), QStringLiteral("-"));
    }

    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailEventTypeValue")),
             QStringLiteral("确认失败"));
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailCodeValue")),
             QStringLiteral("NoStagedSnapshot"));
    QVERIFY(!labelText(widget, QStringLiteral("bmsAuditDetailMessageValue")).isEmpty());
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailMessageValue")),
             QStringLiteral("当前没有可确认的预览"));
}

void BmsCommandAuditDetailWidgetTest::parameterTablesAreSortedAndReadOnly()
{
    BmsCommandAuditRecord record = fullRecord();
    record.engineeringParameters.clear();
    record.rawParameters.clear();

    // Inserted out of order; the widget must sort by key ascending.
    record.engineeringParameters.insert(QStringLiteral("z_parameter"), QStringLiteral("hello"));
    record.engineeringParameters.insert(QStringLiteral("a_parameter"), true);
    record.engineeringParameters.insert(QStringLiteral("m_parameter"), 3.5);

    record.rawParameters.insert(QStringLiteral("z_parameter"), QString());
    record.rawParameters.insert(QStringLiteral("a_parameter"),
                                QByteArray::fromHex("1027FF8505000000"));
    record.rawParameters.insert(QStringLiteral("m_parameter"), QByteArray());

    BmsCommandAuditDetailWidget widget;
    widget.setRecord(record);

    auto *engineering = widget.findChild<QTableWidget *>(
        QStringLiteral("bmsAuditDetailEngineeringTable"));
    auto *raw = widget.findChild<QTableWidget *>(QStringLiteral("bmsAuditDetailRawTable"));
    QVERIFY(engineering != nullptr);
    QVERIFY(raw != nullptr);

    QCOMPARE(engineering->rowCount(), 3);
    QCOMPARE(engineering->item(0, 0)->text(), QStringLiteral("a_parameter"));
    QCOMPARE(engineering->item(1, 0)->text(), QStringLiteral("m_parameter"));
    QCOMPARE(engineering->item(2, 0)->text(), QStringLiteral("z_parameter"));

    // bool / double / QString formatting.
    QCOMPARE(engineering->item(0, 2)->text(), QStringLiteral("true"));
    QCOMPARE(engineering->item(1, 2)->text(), QStringLiteral("3.5"));
    QCOMPARE(engineering->item(2, 2)->text(), QStringLiteral("hello"));

    QCOMPARE(raw->rowCount(), 3);
    QCOMPARE(raw->item(0, 0)->text(), QStringLiteral("a_parameter"));
    QCOMPARE(raw->item(1, 0)->text(), QStringLiteral("m_parameter"));
    QCOMPARE(raw->item(2, 0)->text(), QStringLiteral("z_parameter"));

    // QByteArray (uppercase, spaced), empty QByteArray, empty QString formatting.
    QCOMPARE(raw->item(0, 2)->text(), QStringLiteral("10 27 FF 85 05 00 00 00"));
    QCOMPARE(raw->item(1, 2)->text(), QStringLiteral("（空）"));
    QCOMPARE(raw->item(2, 2)->text(), QStringLiteral("\"\""));

    for (QTableWidget *table : {engineering, raw}) {
        QCOMPARE(table->editTriggers(), QAbstractItemView::NoEditTriggers);
        QVERIFY(!table->isSortingEnabled());
        QCOMPARE(table->selectionBehavior(), QAbstractItemView::SelectRows);
    }
}

void BmsCommandAuditDetailWidgetTest::recordIsCopiedByValue()
{
    BmsCommandAuditRecord record = fullRecord();
    const QString originalFingerprint = record.fingerprint;
    const QString originalPayload = QStringLiteral("10 27 FF 85 05 00 00 00");
    const QString originalMessage = record.message;

    BmsCommandAuditDetailWidget widget;
    widget.setRecord(record);

    // Mutate the source after display; the widget's value copy must be unaffected.
    record.fingerprint = QStringLiteral("MUTATED");
    record.payload = QByteArray("\xAA\xBB", 2);
    record.message = QStringLiteral("changed");
    record.engineeringParameters.clear();
    record.rawParameters.clear();

    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailFingerprintValue")),
             originalFingerprint);
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailPayloadValue")), originalPayload);
    QCOMPARE(labelText(widget, QStringLiteral("bmsAuditDetailMessageValue")), originalMessage);

    auto *engineering = widget.findChild<QTableWidget *>(
        QStringLiteral("bmsAuditDetailEngineeringTable"));
    QVERIFY(engineering != nullptr);
    QCOMPARE(engineering->rowCount(), 1);
    QCOMPARE(engineering->item(0, 0)->text(), QStringLiteral("demo_voltage_v"));
}

void BmsCommandAuditDetailWidgetTest::clearRecordResetsEveryField()
{
    BmsCommandAuditDetailWidget widget;
    widget.setRecord(fullRecord());
    QVERIFY(widget.hasRecord());

    widget.clearRecord();

    QVERIFY(!widget.hasRecord());
    QVERIFY(labelText(widget, QStringLiteral("bmsAuditDetailStateLabel"))
                .contains(QStringLiteral("请选择")));

    for (const QString &name : {QStringLiteral("bmsAuditDetailSequenceValue"),
                                QStringLiteral("bmsAuditDetailEventTypeValue"),
                                QStringLiteral("bmsAuditDetailRevisionValue"),
                                QStringLiteral("bmsAuditDetailCommandIdValue"),
                                QStringLiteral("bmsAuditDetailCanIdValue"),
                                QStringLiteral("bmsAuditDetailPayloadValue"),
                                QStringLiteral("bmsAuditDetailFingerprintValue"),
                                QStringLiteral("bmsAuditDetailTransmittedAtUtcValue")}) {
        QCOMPARE(labelText(widget, name), QStringLiteral("-"));
    }

    auto *engineering = widget.findChild<QTableWidget *>(
        QStringLiteral("bmsAuditDetailEngineeringTable"));
    auto *raw = widget.findChild<QTableWidget *>(QStringLiteral("bmsAuditDetailRawTable"));
    QCOMPARE(engineering->rowCount(), 0);
    QCOMPARE(raw->rowCount(), 0);
}

QTEST_MAIN(BmsCommandAuditDetailWidgetTest)

#include "BmsCommandAuditDetailWidgetTest.moc"

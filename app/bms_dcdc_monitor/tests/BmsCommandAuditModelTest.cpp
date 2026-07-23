#include "models/BmsCommandAuditModel.h"

#include "audit/BmsCommandAudit.h"
#include "communication/BmsCommandConfirmationGate.h"
#include "protocol/BmsCommand.h"

#include <QtTest/QtTest>

class BmsCommandAuditModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void appendAssignsMonotonicSequence();
    void recordPreservesSnapshotByValue();
    void exposesExpectedDisplayAndTooltipData();
    void rejectsInvalidEventTimestamp();
    void modelIsReadOnly();

private:
    static BmsCommandConfirmationSnapshot confirmedSnapshot(BmsCommandConfirmationGate *gate);
    static QString sampleFingerprint();
};

void BmsCommandAuditModelTest::initTestCase()
{
    qRegisterMetaType<BmsCommandAuditRecord>("BmsCommandAuditRecord");
}

BmsCommandConfirmationSnapshot
BmsCommandAuditModelTest::confirmedSnapshot(BmsCommandConfirmationGate *gate)
{
    BmsCommandDefinition definition;
    const bool found =
        BmsCommandCatalog::findDemoCommand(QStringLiteral("demo_mock_bms_command"), &definition);
    Q_ASSERT(found);

    BmsCommandRequest request;
    request.commandId = definition.commandId;
    request.engineeringParameters.insert(QStringLiteral("demo_voltage_v"), 1000.0);
    request.engineeringParameters.insert(QStringLiteral("demo_current_a"), -12.3);
    request.engineeringParameters.insert(QStringLiteral("demo_enable"), true);
    request.engineeringParameters.insert(QStringLiteral("demo_mode"), 2);

    const BmsCommandEncodeResult result = BmsCommandEncoder().encode(definition, request);
    Q_ASSERT(result.ok());

    const BmsCommandConfirmationResult staged = gate->stage(result.command);
    Q_ASSERT(staged.accepted);
    const auto snapshot = gate->stagedSnapshot();
    const BmsCommandConfirmationResult confirmed =
        gate->confirm(snapshot->revision, snapshot->confirmationCode, true);
    Q_ASSERT(confirmed.accepted);
    return *gate->confirmedSnapshot();
}

QString BmsCommandAuditModelTest::sampleFingerprint()
{
    // A deterministic 64-character hex value so the abbreviation is exact.
    return QStringLiteral("0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF");
}

void BmsCommandAuditModelTest::appendAssignsMonotonicSequence()
{
    BmsCommandAuditModel model;
    const QDateTime when = QDateTime::currentDateTimeUtc();

    for (int i = 0; i < 3; ++i) {
        BmsCommandAuditRecord record = makeBmsCommandAuditRecord(
            BmsCommandAuditEventType::PreviewStaged, BmsCommandAuditOutcome::Info, nullptr, when,
            QStringLiteral("Code%1").arg(i));
        // Producer-set sequence numbers must be ignored by the model.
        record.sequence = 9000 + i;
        QVERIFY(model.appendRecord(record));
    }

    QCOMPARE(model.recordCount(), 3);
    QCOMPARE(model.rowCount(), 3);
    for (int row = 0; row < 3; ++row) {
        const auto stored = model.recordAt(row);
        QVERIFY(stored.has_value());
        QCOMPARE(stored->sequence, quint64(row + 1));
        // Insertion order is preserved.
        QCOMPARE(stored->code, QStringLiteral("Code%1").arg(row));
    }
}

void BmsCommandAuditModelTest::recordPreservesSnapshotByValue()
{
    BmsCommandAuditModel model;
    BmsCommandConfirmationGate gate;
    BmsCommandConfirmationSnapshot snapshot = confirmedSnapshot(&gate);

    const QString originalFingerprint = snapshot.command.fingerprint;
    const QByteArray originalPayload = snapshot.command.data;
    const QVariantMap originalEngineering = snapshot.command.engineeringParameters;
    const QVariantMap originalRaw = snapshot.command.rawParameters;

    const BmsCommandAuditRecord record = makeBmsCommandAuditRecord(
        BmsCommandAuditEventType::ConfirmationSucceeded, BmsCommandAuditOutcome::Success, &snapshot,
        QDateTime::currentDateTimeUtc());
    QVERIFY(model.appendRecord(record));

    // Mutate the source snapshot after the record was built; the stored record
    // must be an independent value copy.
    snapshot.command.fingerprint = QStringLiteral("MUTATED");
    snapshot.command.data = QByteArray("\xAA\xBB", 2);
    snapshot.command.engineeringParameters.insert(QStringLiteral("demo_voltage_v"), 0.0);
    snapshot.command.rawParameters.clear();

    const auto stored = model.recordAt(0);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->fingerprint, originalFingerprint);
    QCOMPARE(stored->payload, originalPayload);
    QCOMPARE(stored->engineeringParameters, originalEngineering);
    QCOMPARE(stored->rawParameters, originalRaw);
    QVERIFY(!originalPayload.isEmpty());
    QVERIFY(!originalEngineering.isEmpty());
}

void BmsCommandAuditModelTest::exposesExpectedDisplayAndTooltipData()
{
    BmsCommandAuditModel model;

    BmsCommandAuditRecord record;
    record.eventType = BmsCommandAuditEventType::DispatchSucceeded;
    record.outcome = BmsCommandAuditOutcome::Success;
    record.eventAtUtc = QDateTime::currentDateTimeUtc();
    record.mode = BmsCommandAuditMode::DemoMock;
    record.revision = 7;
    record.commandId = QStringLiteral("demo_mock_bms_command");
    record.canId = 0x18E50101u;
    record.channel = 0;
    record.payload = QByteArray::fromHex("1027FF8505000000");
    record.fingerprint = sampleFingerprint();
    record.code = QStringLiteral("MockDispatchSucceeded");
    record.message = QStringLiteral("Demo 指令已通过 Mock CAN 单次发送");
    QVERIFY(model.appendRecord(record));

    const auto text = [&](int column) {
        return model.data(model.index(0, column), Qt::DisplayRole).toString();
    };

    QCOMPARE(text(BmsCommandAuditModel::EventTypeColumn), QStringLiteral("发送成功"));
    QCOMPARE(text(BmsCommandAuditModel::OutcomeColumn), QStringLiteral("成功"));
    QCOMPARE(text(BmsCommandAuditModel::ModeColumn), QStringLiteral("Demo / Mock"));
    QCOMPARE(text(BmsCommandAuditModel::CanIdColumn), QStringLiteral("0x18E50101"));
    QCOMPARE(text(BmsCommandAuditModel::PayloadColumn),
             QStringLiteral("10 27 FF 85 05 00 00 00"));

    const QString expectedShort = sampleFingerprint().left(12) + QStringLiteral("…") +
                                  sampleFingerprint().right(8);
    QCOMPARE(text(BmsCommandAuditModel::FingerprintColumn), expectedShort);
    QCOMPARE(model.data(model.index(0, BmsCommandAuditModel::FingerprintColumn), Qt::ToolTipRole)
                 .toString(),
             sampleFingerprint());

    const QString details =
        model.data(model.index(0, BmsCommandAuditModel::DetailsColumn), Qt::DisplayRole).toString();
    QVERIFY(details.contains(QStringLiteral("MockDispatchSucceeded")));

    const auto userRecord = qvariant_cast<BmsCommandAuditRecord>(
        model.data(model.index(0, BmsCommandAuditModel::SequenceColumn), Qt::UserRole));
    QCOMPARE(userRecord.fingerprint, sampleFingerprint());
    QCOMPARE(userRecord.revision, quint64(7));
    QCOMPARE(userRecord.eventType, BmsCommandAuditEventType::DispatchSucceeded);
    QCOMPARE(userRecord.sequence, quint64(1));
}

void BmsCommandAuditModelTest::rejectsInvalidEventTimestamp()
{
    BmsCommandAuditModel model;

    BmsCommandAuditRecord invalid = makeBmsCommandAuditRecord(
        BmsCommandAuditEventType::PreviewStaged, BmsCommandAuditOutcome::Info, nullptr,
        QDateTime());
    QVERIFY(!invalid.eventAtUtc.isValid());
    QVERIFY(!model.appendRecord(invalid));
    QCOMPARE(model.rowCount(), 0);

    // The rejected record must not have consumed sequence 1.
    BmsCommandAuditRecord valid = makeBmsCommandAuditRecord(
        BmsCommandAuditEventType::PreviewStaged, BmsCommandAuditOutcome::Info, nullptr,
        QDateTime::currentDateTimeUtc());
    QVERIFY(model.appendRecord(valid));
    const auto stored = model.recordAt(0);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->sequence, quint64(1));
}

void BmsCommandAuditModelTest::modelIsReadOnly()
{
    BmsCommandAuditModel model;
    BmsCommandAuditRecord record = makeBmsCommandAuditRecord(
        BmsCommandAuditEventType::PreviewStaged, BmsCommandAuditOutcome::Info, nullptr,
        QDateTime::currentDateTimeUtc());
    QVERIFY(model.appendRecord(record));

    const QModelIndex index = model.index(0, BmsCommandAuditModel::DetailsColumn);
    QVERIFY(!(model.flags(index) & Qt::ItemIsEditable));

    // The base-class setData refuses because the subclass does not override it.
    QVERIFY(!model.setData(index, QStringLiteral("tampered"), Qt::EditRole));
    // No removeRows override is provided, so the default refuses.
    QVERIFY(!model.removeRows(0, 1));
    QCOMPARE(model.rowCount(), 1);
}

QTEST_MAIN(BmsCommandAuditModelTest)

#include "BmsCommandAuditModelTest.moc"

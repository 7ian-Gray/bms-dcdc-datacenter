#include "communication/BmsCommandConfirmationGate.h"

#include <QtTest/QtTest>

class BmsCommandConfirmationGateTest : public QObject
{
    Q_OBJECT

private slots:
    void stagesValidEncodedCommand();
    void rejectsInvalidSnapshot();
    void requiresAcknowledgementAndCorrectCode();
    void restagingInvalidatesConfirmation();
    void invalidationClearsSnapshots();

private:
    // Produces a real encoder output, so the gate is tested against the same
    // structure the page will hand it.
    static EncodedBmsCommand validCommand();
};

EncodedBmsCommand BmsCommandConfirmationGateTest::validCommand()
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
    return result.command;
}

void BmsCommandConfirmationGateTest::stagesValidEncodedCommand()
{
    BmsCommandConfirmationGate gate;
    QCOMPARE(gate.state(), BmsCommandConfirmationState::Empty);

    const EncodedBmsCommand command = validCommand();
    const BmsCommandConfirmationResult result = gate.stage(command);

    QVERIFY(result.accepted);
    QCOMPARE(gate.state(), BmsCommandConfirmationState::Staged);
    QVERIFY(gate.hasStagedSnapshot());
    QVERIFY(!gate.hasConfirmedSnapshot());

    const auto snapshot = gate.stagedSnapshot();
    QVERIFY(snapshot.has_value());
    QCOMPARE(snapshot->revision, 1ull);
    QCOMPARE(snapshot->confirmationCode, command.fingerprint.right(8).toUpper());
    QCOMPARE(snapshot->confirmationCode.size(), 8);
    QVERIFY(snapshot->stagedAtUtc.isValid());
    QVERIFY(snapshot->confirmedAtUtc.isNull());
    // The snapshot is a value copy of the encoded command.
    QCOMPARE(snapshot->command.fingerprint, command.fingerprint);
    QCOMPARE(snapshot->command.data, command.data);
}

void BmsCommandConfirmationGateTest::rejectsInvalidSnapshot()
{
    const EncodedBmsCommand base = validCommand();

    struct Mutation
    {
        const char *name;
        std::function<void(EncodedBmsCommand &)> apply;
    };

    const QList<Mutation> mutations = {
        {"empty commandId", [](EncodedBmsCommand &c) { c.commandId.clear(); }},
        {"short fingerprint", [](EncodedBmsCommand &c) { c.fingerprint = QStringLiteral("abc"); }},
        {"non-hex fingerprint",
         [](EncodedBmsCommand &c) { c.fingerprint = QString(64, QLatin1Char('z')); }},
        {"invalid encodedAtUtc", [](EncodedBmsCommand &c) { c.encodedAtUtc = QDateTime(); }},
        {"dlc out of range", [](EncodedBmsCommand &c) { c.dlc = 9; }},
        {"data size mismatch", [](EncodedBmsCommand &c) { c.data.chop(1); }},
        {"frame id mismatch", [](EncodedBmsCommand &c) { c.frame.id = c.canId + 1; }},
        {"frame channel mismatch",
         [](EncodedBmsCommand &c) { c.frame.channel = static_cast<quint8>(c.channel + 1); }},
        {"frame extended mismatch", [](EncodedBmsCommand &c) { c.frame.extended = !c.extended; }},
        {"remote frame", [](EncodedBmsCommand &c) { c.frame.remote = true; }},
        {"payload mismatch",
         [](EncodedBmsCommand &c) { c.frame.payload = QByteArray(c.dlc, '\xAA'); }},
        {"direction not Tx",
         [](EncodedBmsCommand &c) { c.frame.direction = CanFrameDirection::Rx; }},
    };

    for (const Mutation &mutation : mutations) {
        BmsCommandConfirmationGate gate;
        EncodedBmsCommand mutated = base;
        mutation.apply(mutated);

        const BmsCommandConfirmationResult result = gate.stage(mutated);
        QVERIFY2(!result.accepted, mutation.name);
        QVERIFY2(result.code == QStringLiteral("InvalidSnapshot"), mutation.name);
        QVERIFY2(!gate.hasStagedSnapshot(), mutation.name);
        QVERIFY2(gate.state() == BmsCommandConfirmationState::Empty, mutation.name);
    }

    // A rejected stage must not disturb an already staged snapshot.
    BmsCommandConfirmationGate gate;
    QVERIFY(gate.stage(base).accepted);
    EncodedBmsCommand broken = base;
    broken.commandId.clear();
    QVERIFY(!gate.stage(broken).accepted);
    QVERIFY(gate.hasStagedSnapshot());
    QCOMPARE(gate.stagedSnapshot()->revision, 1ull);
    QCOMPARE(gate.state(), BmsCommandConfirmationState::Staged);
}

void BmsCommandConfirmationGateTest::requiresAcknowledgementAndCorrectCode()
{
    BmsCommandConfirmationGate gate;
    const EncodedBmsCommand command = validCommand();
    QVERIFY(gate.stage(command).accepted);

    const quint64 revision = gate.stagedSnapshot()->revision;
    const QString code = gate.stagedSnapshot()->confirmationCode;

    // No acknowledgement.
    BmsCommandConfirmationResult result = gate.confirm(revision, code, false);
    QVERIFY(!result.accepted);
    QCOMPARE(result.code, QStringLiteral("AcknowledgementRequired"));
    QVERIFY(!gate.hasConfirmedSnapshot());

    // Wrong code.
    result = gate.confirm(revision, QStringLiteral("00000000"), true);
    QVERIFY(!result.accepted);
    QCOMPARE(result.code, QStringLiteral("ConfirmationCodeMismatch"));
    QVERIFY(!gate.hasConfirmedSnapshot());

    // Stale revision.
    result = gate.confirm(revision + 1, code, true);
    QVERIFY(!result.accepted);
    QCOMPARE(result.code, QStringLiteral("RevisionMismatch"));
    QVERIFY(!gate.hasConfirmedSnapshot());

    // Failures must leave the staged snapshot intact so the user can retry.
    QVERIFY(gate.hasStagedSnapshot());
    QCOMPARE(gate.stagedSnapshot()->revision, revision);

    // Correct code, tolerating surrounding whitespace and lower case.
    result = gate.confirm(revision, QStringLiteral("  ") + code.toLower() + QStringLiteral(" "), true);
    QVERIFY(result.accepted);
    QCOMPARE(gate.state(), BmsCommandConfirmationState::Confirmed);
    QVERIFY(gate.hasConfirmedSnapshot());
    QVERIFY(gate.confirmedSnapshot()->confirmedAtUtc.isValid());
    QCOMPARE(gate.confirmedSnapshot()->revision, revision);
    QCOMPARE(gate.confirmedSnapshot()->command.fingerprint, command.fingerprint);

    // Confirming without a staged snapshot at all.
    BmsCommandConfirmationGate emptyGate;
    result = emptyGate.confirm(1, QStringLiteral("ABCDEF12"), true);
    QVERIFY(!result.accepted);
    QCOMPARE(result.code, QStringLiteral("NoStagedSnapshot"));
}

void BmsCommandConfirmationGateTest::restagingInvalidatesConfirmation()
{
    BmsCommandConfirmationGate gate;
    const EncodedBmsCommand command = validCommand();

    QVERIFY(gate.stage(command).accepted);
    const QString code = gate.stagedSnapshot()->confirmationCode;
    QVERIFY(gate.confirm(1, code, true).accepted);
    QVERIFY(gate.hasConfirmedSnapshot());

    // Re-staging byte-identical content must still demand a new confirmation.
    QVERIFY(gate.stage(command).accepted);
    QCOMPARE(gate.stagedSnapshot()->revision, 2ull);
    QCOMPARE(gate.state(), BmsCommandConfirmationState::Staged);
    QVERIFY(!gate.hasConfirmedSnapshot());
    QCOMPARE(gate.stagedSnapshot()->confirmationCode, code);

    // The confirmation for the previous revision must no longer be accepted.
    const BmsCommandConfirmationResult stale = gate.confirm(1, code, true);
    QVERIFY(!stale.accepted);
    QCOMPARE(stale.code, QStringLiteral("RevisionMismatch"));
}

void BmsCommandConfirmationGateTest::invalidationClearsSnapshots()
{
    BmsCommandConfirmationGate gate;
    const EncodedBmsCommand command = validCommand();

    QVERIFY(gate.stage(command).accepted);
    QVERIFY(gate.confirm(1, gate.stagedSnapshot()->confirmationCode, true).accepted);
    QVERIFY(gate.hasConfirmedSnapshot());

    gate.invalidate();

    QCOMPARE(gate.state(), BmsCommandConfirmationState::Invalidated);
    QVERIFY(!gate.hasStagedSnapshot());
    QVERIFY(!gate.hasConfirmedSnapshot());
    QVERIFY(!gate.stagedSnapshot().has_value());
    QVERIFY(!gate.confirmedSnapshot().has_value());

    // Nothing can be confirmed after invalidation until a new preview is staged.
    const BmsCommandConfirmationResult result = gate.confirm(1, QStringLiteral("ABCDEF12"), true);
    QVERIFY(!result.accepted);
    QCOMPARE(result.code, QStringLiteral("NoStagedSnapshot"));
}

QTEST_MAIN(BmsCommandConfirmationGateTest)

#include "BmsCommandConfirmationGateTest.moc"

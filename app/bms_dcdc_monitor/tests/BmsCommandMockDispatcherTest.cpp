#include "communication/BmsCommandMockDispatcher.h"

#include "communication/BmsCommandConfirmationGate.h"
#include "communication/CanSessionController.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class BmsCommandMockDispatcherTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void rejectsSnapshotWithoutConfirmation();
    void rejectsNonDemoSnapshot();
    void rejectsWhenMockChannelNotStarted();
    void rejectsHardwareAndListenOnlyModes();
    void rejectsChannelMismatch();
    void dispatchesConfirmedSnapshotOnce();
    void rejectsReplayOfConsumedSnapshot();
    void newRevisionOfSameContentCanDispatchOnce();

private:
    static EncodedBmsCommand encodeDemoCommand(quint8 channel = 0);
    // Stages and confirms, returning the confirmed snapshot.
    static BmsCommandConfirmationSnapshot confirmedSnapshot(BmsCommandConfirmationGate *gate,
                                                            quint8 channel = 0);
    static void startMockChannel(CanSessionController *session,
                                 int channel = 0,
                                 bool listenOnly = false);
    static QString failureCode(const QSignalSpy &spy, int index = 0);
};

void BmsCommandMockDispatcherTest::initTestCase()
{
    qRegisterMetaType<CanFrame>("CanFrame");
    qRegisterMetaType<CanConnectionState>("CanConnectionState");
    qRegisterMetaType<CanSourceMode>("CanSourceMode");
    qRegisterMetaType<BmsCommandConfirmationSnapshot>("BmsCommandConfirmationSnapshot");
}

EncodedBmsCommand BmsCommandMockDispatcherTest::encodeDemoCommand(quint8 channel)
{
    BmsCommandDefinition definition;
    const bool found =
        BmsCommandCatalog::findDemoCommand(QStringLiteral("demo_mock_bms_command"), &definition);
    Q_ASSERT(found);
    definition.channel = channel;

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

BmsCommandConfirmationSnapshot
BmsCommandMockDispatcherTest::confirmedSnapshot(BmsCommandConfirmationGate *gate, quint8 channel)
{
    const EncodedBmsCommand command = encodeDemoCommand(channel);
    const BmsCommandConfirmationResult staged = gate->stage(command);
    Q_ASSERT(staged.accepted);

    const auto snapshot = gate->stagedSnapshot();
    const BmsCommandConfirmationResult confirmed =
        gate->confirm(snapshot->revision, snapshot->confirmationCode, true);
    Q_ASSERT(confirmed.accepted);
    return *gate->confirmedSnapshot();
}

void BmsCommandMockDispatcherTest::startMockChannel(CanSessionController *session,
                                                    int channel,
                                                    bool listenOnly)
{
    CanConnectionConfig config;
    config.driverType = CanDriverType::Mock;
    config.channel = channel;
    config.listenOnly = listenOnly;
    session->openDevice(config);
    session->startChannel();
}

QString BmsCommandMockDispatcherTest::failureCode(const QSignalSpy &spy, int index)
{
    return spy.at(index).at(1).toString();
}

void BmsCommandMockDispatcherTest::rejectsSnapshotWithoutConfirmation()
{
    CanSessionController session;
    startMockChannel(&session);
    BmsCommandMockDispatcher dispatcher(&session);

    BmsCommandConfirmationGate gate;
    QVERIFY(gate.stage(encodeDemoCommand()).accepted);
    const BmsCommandConfirmationSnapshot staged = *gate.stagedSnapshot();
    QVERIFY(staged.confirmedAtUtc.isNull());

    QSignalSpy transmittedSpy(&session, &CanSessionController::frameTransmitted);
    QSignalSpy failedSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchFailed);
    QSignalSpy succeededSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchSucceeded);

    dispatcher.dispatch(staged);

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failureCode(failedSpy), QStringLiteral("SnapshotNotConfirmed"));
    QCOMPARE(transmittedSpy.count(), 0);
    QCOMPARE(succeededSpy.count(), 0);
}

void BmsCommandMockDispatcherTest::rejectsNonDemoSnapshot()
{
    CanSessionController session;
    startMockChannel(&session);
    BmsCommandMockDispatcher dispatcher(&session);

    BmsCommandConfirmationGate gate;
    BmsCommandConfirmationSnapshot snapshot = confirmedSnapshot(&gate);
    snapshot.command.safetyLevel = BmsSafetyLevel::High;

    QSignalSpy transmittedSpy(&session, &CanSessionController::frameTransmitted);
    QSignalSpy failedSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchFailed);

    dispatcher.dispatch(snapshot);

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failureCode(failedSpy), QStringLiteral("NonDemoCommand"));
    QCOMPARE(transmittedSpy.count(), 0);
}

void BmsCommandMockDispatcherTest::rejectsWhenMockChannelNotStarted()
{
    // Device never opened.
    {
        CanSessionController session;
        BmsCommandMockDispatcher dispatcher(&session);
        BmsCommandConfirmationGate gate;

        QSignalSpy transmittedSpy(&session, &CanSessionController::frameTransmitted);
        QSignalSpy failedSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchFailed);

        dispatcher.dispatch(confirmedSnapshot(&gate));

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failureCode(failedSpy), QStringLiteral("MockChannelNotReady"));
        QCOMPARE(transmittedSpy.count(), 0);
    }

    // Device opened but channel not started.
    {
        CanSessionController session;
        CanConnectionConfig config;
        config.driverType = CanDriverType::Mock;
        session.openDevice(config);

        BmsCommandMockDispatcher dispatcher(&session);
        BmsCommandConfirmationGate gate;

        QSignalSpy transmittedSpy(&session, &CanSessionController::frameTransmitted);
        QSignalSpy failedSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchFailed);

        dispatcher.dispatch(confirmedSnapshot(&gate));

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failureCode(failedSpy), QStringLiteral("MockChannelNotReady"));
        QCOMPARE(transmittedSpy.count(), 0);
        QVERIFY(!dispatcher.isAvailable());
        QCOMPARE(dispatcher.availableChannel(), -1);
    }
}

void BmsCommandMockDispatcherTest::rejectsHardwareAndListenOnlyModes()
{
    // Hardware driver.
    {
        CanSessionController session;
        CanConnectionConfig config;
        config.driverType = CanDriverType::ZlgControlCan;
        session.openDevice(config);

        BmsCommandMockDispatcher dispatcher(&session);
        BmsCommandConfirmationGate gate;

        QSignalSpy transmittedSpy(&session, &CanSessionController::frameTransmitted);
        QSignalSpy failedSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchFailed);

        dispatcher.dispatch(confirmedSnapshot(&gate));

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failureCode(failedSpy), QStringLiteral("HardwareModeRejected"));
        QCOMPARE(transmittedSpy.count(), 0);
    }

    // Listen-only Mock channel.
    {
        CanSessionController session;
        startMockChannel(&session, 0, true);

        BmsCommandMockDispatcher dispatcher(&session);
        BmsCommandConfirmationGate gate;

        QSignalSpy transmittedSpy(&session, &CanSessionController::frameTransmitted);
        QSignalSpy failedSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchFailed);

        dispatcher.dispatch(confirmedSnapshot(&gate));

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failureCode(failedSpy), QStringLiteral("ListenOnlyRejected"));
        QCOMPARE(transmittedSpy.count(), 0);
        QVERIFY(!dispatcher.isAvailable());
    }
}

void BmsCommandMockDispatcherTest::rejectsChannelMismatch()
{
    CanSessionController session;
    startMockChannel(&session, 0);
    BmsCommandMockDispatcher dispatcher(&session);
    QVERIFY(dispatcher.isAvailable());
    QCOMPARE(dispatcher.availableChannel(), 0);

    // Snapshot targets channel 1 while the session runs channel 0; the snapshot
    // stays internally consistent because the encoder stamps the frame channel.
    BmsCommandConfirmationGate gate;
    const BmsCommandConfirmationSnapshot snapshot = confirmedSnapshot(&gate, 1);
    QCOMPARE(snapshot.command.channel, quint8(1));
    QCOMPARE(snapshot.command.frame.channel, quint8(1));

    QSignalSpy transmittedSpy(&session, &CanSessionController::frameTransmitted);
    QSignalSpy failedSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchFailed);

    dispatcher.dispatch(snapshot);

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failureCode(failedSpy), QStringLiteral("ChannelMismatch"));
    QCOMPARE(transmittedSpy.count(), 0);
}

void BmsCommandMockDispatcherTest::dispatchesConfirmedSnapshotOnce()
{
    CanSessionController session;
    startMockChannel(&session, 0);
    BmsCommandMockDispatcher dispatcher(&session);

    BmsCommandConfirmationGate gate;
    const BmsCommandConfirmationSnapshot snapshot = confirmedSnapshot(&gate);

    QSignalSpy transmittedSpy(&session, &CanSessionController::frameTransmitted);
    QSignalSpy succeededSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchSucceeded);
    QSignalSpy failedSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchFailed);

    dispatcher.dispatch(snapshot);

    QCOMPARE(transmittedSpy.count(), 1);
    QCOMPARE(succeededSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);

    const CanFrame sent = qvariant_cast<CanFrame>(transmittedSpy.at(0).at(0));
    QCOMPARE(sent.id, snapshot.command.canId);
    QCOMPARE(sent.channel, snapshot.command.channel);
    QCOMPARE(sent.extended, snapshot.command.extended);
    QCOMPARE(sent.remote, false);
    QCOMPARE(sent.payload, snapshot.command.data);
    QCOMPARE(sent.direction, CanFrameDirection::Tx);
    QVERIFY(sent.timestampUs > 0);

    QCOMPARE(succeededSpy.at(0).at(0).toULongLong(), snapshot.revision);
    QCOMPARE(succeededSpy.at(0).at(1).toString(), snapshot.command.fingerprint);
    QVERIFY(succeededSpy.at(0).at(2).toDateTime().isValid());
}

void BmsCommandMockDispatcherTest::rejectsReplayOfConsumedSnapshot()
{
    CanSessionController session;
    startMockChannel(&session, 0);
    BmsCommandMockDispatcher dispatcher(&session);

    BmsCommandConfirmationGate gate;
    const BmsCommandConfirmationSnapshot snapshot = confirmedSnapshot(&gate);

    dispatcher.dispatch(snapshot);

    QSignalSpy transmittedSpy(&session, &CanSessionController::frameTransmitted);
    QSignalSpy failedSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchFailed);
    QSignalSpy succeededSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchSucceeded);

    dispatcher.dispatch(snapshot);

    QCOMPARE(transmittedSpy.count(), 0);
    QCOMPARE(succeededSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failureCode(failedSpy), QStringLiteral("SnapshotAlreadyConsumed"));

    // Stopping and restarting the channel must not revive the consumed snapshot.
    session.stopChannel();
    session.startChannel();
    failedSpy.clear();
    dispatcher.dispatch(snapshot);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failureCode(failedSpy), QStringLiteral("SnapshotAlreadyConsumed"));
    QCOMPARE(transmittedSpy.count(), 0);
}

void BmsCommandMockDispatcherTest::newRevisionOfSameContentCanDispatchOnce()
{
    CanSessionController session;
    startMockChannel(&session, 0);
    BmsCommandMockDispatcher dispatcher(&session);

    QSignalSpy transmittedSpy(&session, &CanSessionController::frameTransmitted);
    QSignalSpy succeededSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchSucceeded);
    QSignalSpy failedSpy(&dispatcher, &BmsCommandMockDispatcher::dispatchFailed);

    BmsCommandConfirmationGate gate;
    const BmsCommandConfirmationSnapshot first = confirmedSnapshot(&gate);
    dispatcher.dispatch(first);

    // Same content re-staged and re-confirmed: identical fingerprint, new revision.
    const BmsCommandConfirmationSnapshot second = confirmedSnapshot(&gate);
    QCOMPARE(second.command.fingerprint, first.command.fingerprint);
    QVERIFY(second.revision != first.revision);
    dispatcher.dispatch(second);

    QCOMPARE(transmittedSpy.count(), 2);
    QCOMPARE(succeededSpy.count(), 2);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(succeededSpy.at(0).at(0).toULongLong(), first.revision);
    QCOMPARE(succeededSpy.at(1).at(0).toULongLong(), second.revision);

    // Each revision is consumed exactly once.
    dispatcher.dispatch(second);
    QCOMPARE(transmittedSpy.count(), 2);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failureCode(failedSpy), QStringLiteral("SnapshotAlreadyConsumed"));
}

QTEST_MAIN(BmsCommandMockDispatcherTest)

#include "BmsCommandMockDispatcherTest.moc"

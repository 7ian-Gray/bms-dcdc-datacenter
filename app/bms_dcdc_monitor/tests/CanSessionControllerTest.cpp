#include "communication/CanSessionController.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class CanSessionControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void initialStateIsDisconnected();
    void mockOpenMovesToDeviceOpened();
    void mockStartMovesToChannelStarted();
    void mockFrameReceivedWhileRunning();
    void cannotSendBeforeStart();
    void sendsTxAfterStart();
    void txDirectionIsSet();
    void cannotSendAfterStop();
    void closeReturnsToDisconnected();
    void shutdownStopsMockTimer();
    void repeatedOpenIsRejected();
    void hardwareMovesToUnsupported();
    void unsupportedCanCloseAndRecover();
    void oversizePayloadIsRejected();
    void remotePayloadIsRejected();
    void standardIdOverflowIsRejected();
    void extendedIdOverflowIsRejected();
    void illegalStateTransitions();

private:
    static CanConnectionConfig mockConfig();
    static CanConnectionConfig hardwareConfig();
    static CanFrame txFrame();
};

CanConnectionConfig CanSessionControllerTest::mockConfig()
{
    CanConnectionConfig config;
    config.driverType = CanDriverType::Mock;
    return config;
}

CanConnectionConfig CanSessionControllerTest::hardwareConfig()
{
    CanConnectionConfig config;
    config.driverType = CanDriverType::ZlgControlCan;
    return config;
}

CanFrame CanSessionControllerTest::txFrame()
{
    CanFrame frame;
    frame.id = 0x18E10101u;
    frame.extended = true;
    frame.remote = false;
    frame.channel = 0;
    frame.payload = QByteArray::fromHex("0102030405060708");
    frame.direction = CanFrameDirection::Tx;
    return frame;
}

void CanSessionControllerTest::initialStateIsDisconnected()
{
    CanSessionController controller;
    QCOMPARE(controller.state(), CanConnectionState::Disconnected);
    QCOMPARE(controller.sourceMode(), CanSourceMode::Mock);
    QVERIFY(!controller.isHardwareSupported());
}

void CanSessionControllerTest::mockOpenMovesToDeviceOpened()
{
    CanSessionController controller;
    controller.openDevice(mockConfig());
    QCOMPARE(controller.state(), CanConnectionState::DeviceOpened);
    QCOMPARE(controller.sourceMode(), CanSourceMode::Mock);
}

void CanSessionControllerTest::mockStartMovesToChannelStarted()
{
    CanSessionController controller;
    QSignalSpy stateSpy(&controller, &CanSessionController::stateChanged);
    controller.openDevice(mockConfig());
    controller.startChannel();
    QCOMPARE(controller.state(), CanConnectionState::ChannelStarted);
    QVERIFY(stateSpy.count() >= 3); // Opening, DeviceOpened, Starting, ChannelStarted
}

void CanSessionControllerTest::mockFrameReceivedWhileRunning()
{
    CanSessionController controller;
    QSignalSpy receivedSpy(&controller, &CanSessionController::frameReceived);
    controller.openDevice(mockConfig());
    controller.startChannel();
    QTRY_VERIFY(receivedSpy.count() > 0);

    const CanFrame frame = qvariant_cast<CanFrame>(receivedSpy.first().at(0));
    QCOMPARE(frame.direction, CanFrameDirection::Rx);
    QVERIFY(frame.timestampUs > 0);
}

void CanSessionControllerTest::cannotSendBeforeStart()
{
    CanSessionController controller;
    QSignalSpy transmittedSpy(&controller, &CanSessionController::frameTransmitted);
    controller.openDevice(mockConfig());
    controller.sendFrame(txFrame());
    QCOMPARE(transmittedSpy.count(), 0);
    QCOMPARE(controller.state(), CanConnectionState::Error);
}

void CanSessionControllerTest::sendsTxAfterStart()
{
    CanSessionController controller;
    QSignalSpy transmittedSpy(&controller, &CanSessionController::frameTransmitted);
    controller.openDevice(mockConfig());
    controller.startChannel();
    controller.sendFrame(txFrame());
    QCOMPARE(transmittedSpy.count(), 1);

    const CanFrame frame = qvariant_cast<CanFrame>(transmittedSpy.takeFirst().at(0));
    QCOMPARE(frame.direction, CanFrameDirection::Tx);
    QVERIFY(frame.timestampUs > 0);
}

void CanSessionControllerTest::txDirectionIsSet()
{
    CanSessionController controller;
    QSignalSpy transmittedSpy(&controller, &CanSessionController::frameTransmitted);
    controller.openDevice(mockConfig());
    controller.startChannel();

    CanFrame input = txFrame();
    input.direction = CanFrameDirection::Rx; // input is Rx
    controller.sendFrame(input);

    QCOMPARE(transmittedSpy.count(), 1);
    const CanFrame frame = qvariant_cast<CanFrame>(transmittedSpy.takeFirst().at(0));
    QCOMPARE(frame.direction, CanFrameDirection::Tx); // output is forced to Tx
}

void CanSessionControllerTest::cannotSendAfterStop()
{
    CanSessionController controller;
    QSignalSpy transmittedSpy(&controller, &CanSessionController::frameTransmitted);
    controller.openDevice(mockConfig());
    controller.startChannel();
    controller.stopChannel();
    controller.sendFrame(txFrame());
    QCOMPARE(transmittedSpy.count(), 0);
    QCOMPARE(controller.state(), CanConnectionState::Error);
}

void CanSessionControllerTest::closeReturnsToDisconnected()
{
    CanSessionController controller;
    controller.openDevice(mockConfig());
    controller.startChannel();
    controller.closeDevice();
    QCOMPARE(controller.state(), CanConnectionState::Disconnected);
}

void CanSessionControllerTest::shutdownStopsMockTimer()
{
    CanSessionController controller;
    QSignalSpy receivedSpy(&controller, &CanSessionController::frameReceived);
    controller.openDevice(mockConfig());
    controller.startChannel();
    QTRY_VERIFY(receivedSpy.count() > 0);
    controller.shutdown();

    const int countAfterShutdown = receivedSpy.count();
    QTest::qWait(250);
    QCOMPARE(receivedSpy.count(), countAfterShutdown);
}

void CanSessionControllerTest::repeatedOpenIsRejected()
{
    CanSessionController controller;
    QSignalSpy errorSpy(&controller, &CanSessionController::errorOccurred);
    controller.openDevice(mockConfig());
    controller.openDevice(mockConfig());
    QCOMPARE(controller.state(), CanConnectionState::Error);
    QVERIFY(errorSpy.count() > 0);
}

void CanSessionControllerTest::hardwareMovesToUnsupported()
{
    CanSessionController controller;
    controller.openDevice(hardwareConfig());
    QCOMPARE(controller.state(), CanConnectionState::Unsupported);
    QVERIFY(!controller.isHardwareSupported());
}

void CanSessionControllerTest::unsupportedCanCloseAndRecover()
{
    CanSessionController controller;
    controller.openDevice(hardwareConfig());
    QCOMPARE(controller.state(), CanConnectionState::Unsupported);

    controller.closeDevice();
    QCOMPARE(controller.state(), CanConnectionState::Disconnected);

    // Can re-open with Mock after recovering
    controller.openDevice(mockConfig());
    QCOMPARE(controller.state(), CanConnectionState::DeviceOpened);
}

void CanSessionControllerTest::oversizePayloadIsRejected()
{
    CanSessionController controller;
    QSignalSpy errorSpy(&controller, &CanSessionController::errorOccurred);
    controller.openDevice(mockConfig());
    controller.startChannel();

    CanFrame frame = txFrame();
    frame.payload = QByteArray(9, '\x01');
    controller.sendFrame(frame);

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.first().at(0).toString().contains(QStringLiteral("8 字节")));
}

void CanSessionControllerTest::remotePayloadIsRejected()
{
    CanSessionController controller;
    QSignalSpy errorSpy(&controller, &CanSessionController::errorOccurred);
    controller.openDevice(mockConfig());
    controller.startChannel();

    CanFrame frame = txFrame();
    frame.remote = true;
    frame.payload = QByteArray::fromHex("01");
    controller.sendFrame(frame);

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.first().at(0).toString().contains(QStringLiteral("远程帧")));
}

void CanSessionControllerTest::standardIdOverflowIsRejected()
{
    CanSessionController controller;
    QSignalSpy errorSpy(&controller, &CanSessionController::errorOccurred);
    controller.openDevice(mockConfig());
    controller.startChannel();

    CanFrame frame = txFrame();
    frame.id = 0x800u;
    frame.extended = false;
    controller.sendFrame(frame);

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.first().at(0).toString().contains(QStringLiteral("0x7FF")));
}

void CanSessionControllerTest::extendedIdOverflowIsRejected()
{
    CanSessionController controller;
    QSignalSpy errorSpy(&controller, &CanSessionController::errorOccurred);
    controller.openDevice(mockConfig());
    controller.startChannel();

    CanFrame frame = txFrame();
    frame.id = 0x20000000u;
    frame.extended = true;
    controller.sendFrame(frame);

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.first().at(0).toString().contains(QStringLiteral("0x1FFFFFFF")));
}

void CanSessionControllerTest::illegalStateTransitions()
{
    // startChannel without openDevice
    {
        CanSessionController controller;
        controller.startChannel();
        QCOMPARE(controller.state(), CanConnectionState::Error);
    }

    // stopChannel without startChannel
    {
        CanSessionController controller;
        controller.openDevice(mockConfig());
        controller.stopChannel();
        QCOMPARE(controller.state(), CanConnectionState::Error);
    }

    // closeDevice when already disconnected
    {
        CanSessionController controller;
        QSignalSpy errorSpy(&controller, &CanSessionController::errorOccurred);
        controller.closeDevice();
        QCOMPARE(controller.state(), CanConnectionState::Error);
        QVERIFY(errorSpy.count() > 0);
    }
}

QTEST_MAIN(CanSessionControllerTest)

#include "CanSessionControllerTest.moc"

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
    void cannotSendBeforeStart();
    void sendsTxAfterStart();
    void cannotSendAfterStop();
    void closeReturnsToDisconnected();
    void repeatedOpenDoesNotCreateSecondSession();
    void switchingMockToHardwareResetsSession();
    void unsupportedHardwareMovesToUnsupportedWhenUnavailable();
    void shutdownStopsMockTimer();
    void failedSendDoesNotEmitTransmittedFrame();

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
}

void CanSessionControllerTest::mockOpenMovesToDeviceOpened()
{
    CanSessionController controller;
    controller.openDevice(mockConfig());
    QCOMPARE(controller.state(), CanConnectionState::DeviceOpened);
}

void CanSessionControllerTest::mockStartMovesToChannelStarted()
{
    CanSessionController controller;
    controller.openDevice(mockConfig());
    controller.startChannel();
    QCOMPARE(controller.state(), CanConnectionState::ChannelStarted);
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

void CanSessionControllerTest::repeatedOpenDoesNotCreateSecondSession()
{
    CanSessionController controller;
    QSignalSpy resetSpy(&controller, &CanSessionController::sessionResetRequested);
    controller.openDevice(mockConfig());
    controller.openDevice(mockConfig());
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(controller.state(), CanConnectionState::Error);
}

void CanSessionControllerTest::switchingMockToHardwareResetsSession()
{
    CanSessionController controller;
    QSignalSpy resetSpy(&controller, &CanSessionController::sessionResetRequested);
    controller.openDevice(mockConfig());
    controller.startChannel();
    controller.openDevice(hardwareConfig());
    QVERIFY(resetSpy.count() >= 2);
    QCOMPARE(controller.sourceMode(), CanSourceMode::Hardware);
}

void CanSessionControllerTest::unsupportedHardwareMovesToUnsupportedWhenUnavailable()
{
    CanSessionController controller;
    controller.openDevice(hardwareConfig());
    if (controller.isHardwareSupported()) {
        QSKIP("Hardware is supported in this build; unsupported path is not applicable.");
    }
    QCOMPARE(controller.state(), CanConnectionState::Unsupported);
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

void CanSessionControllerTest::failedSendDoesNotEmitTransmittedFrame()
{
    CanSessionController controller;
    QSignalSpy transmittedSpy(&controller, &CanSessionController::frameTransmitted);
    controller.sendFrame(txFrame());
    QCOMPARE(transmittedSpy.count(), 0);
}

QTEST_MAIN(CanSessionControllerTest)

#include "CanSessionControllerTest.moc"

#include "communication/ModbusRtuClient.h"
#include "communication/MockModbusTransport.h"
#include "protocol/ModbusRtuFrame.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class ModbusRtuClientTest : public QObject
{
    Q_OBJECT

private slots:
    void sendsReadHoldingRegistersRequest();
    void rejectsInvalidRequestRange();
    void rejectsWhenTransportClosed();
    void rejectsWhenWriteFails();
    void rejectsSecondRequestWhilePending();
    void emitsRegistersReadForValidResponse();
    void emitsFailureForExceptionResponse();
    void emitsFailureForCrcMismatch();
    void emitsFailureForWrongSlaveAddress();
    void clearsPendingOnCancel();
    void retriesOnTimeout();
    void failsAfterMaxRetries();
    void emitsRawFrameSent();
    void emitsRawFrameReceived();
    void noQSerialPortDependency();
};

void ModbusRtuClientTest::sendsReadHoldingRegistersRequest()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);

    const bool ok = client.readHoldingRegisters(0x11, 0x0010, 0x0002);
    QVERIFY(ok);
    QVERIFY(client.hasPendingRequest());

    const auto frames = transport.writtenFrames();
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.first(), QByteArray::fromHex("110300100002c75e"));
}

void ModbusRtuClientTest::rejectsInvalidRequestRange()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);

    QString error;
    const bool ok = client.readHoldingRegisters(0x00, 0x0000, 0x0001, &error);
    QVERIFY(!ok);
    QVERIFY(!error.isEmpty());
    QVERIFY(!client.hasPendingRequest());
    QCOMPARE(transport.writtenFrames().size(), 0);
}

void ModbusRtuClientTest::rejectsWhenTransportClosed()
{
    MockModbusTransport transport;
    transport.setOpen(false);
    ModbusRtuClient client(&transport);

    QString error;
    const bool ok = client.readHoldingRegisters(0x11, 0x0010, 0x0002, &error);
    QVERIFY(!ok);
    QVERIFY(error.contains("not open"));
    QVERIFY(!client.hasPendingRequest());
}

void ModbusRtuClientTest::rejectsWhenWriteFails()
{
    MockModbusTransport transport;
    transport.setWriteFailure(true);
    ModbusRtuClient client(&transport);

    QString error;
    const bool ok = client.readHoldingRegisters(0x11, 0x0010, 0x0002, &error);
    QVERIFY(!ok);
    QVERIFY(error.contains("Failed to write"));
    QVERIFY(!client.hasPendingRequest());
}

void ModbusRtuClientTest::rejectsSecondRequestWhilePending()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);

    const bool ok1 = client.readHoldingRegisters(0x11, 0x0010, 0x0002);
    QVERIFY(ok1);

    QString error;
    const bool ok2 = client.readHoldingRegisters(0x12, 0x0020, 0x0003, &error);
    QVERIFY(!ok2);
    QVERIFY(error.contains("already pending"));
}

void ModbusRtuClientTest::emitsRegistersReadForValidResponse()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);
    QSignalSpy spy(&client, &ModbusRtuClient::registersRead);

    client.readHoldingRegisters(0x11, 0x0010, 0x0002);

    // Valid response: slave=0x11, func=0x03, byteCount=4, data=[0x0064, 0x00C8], crc
    transport.injectFrame(QByteArray::fromHex("110304006400c8abbb"));

    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 0x11);
    QCOMPARE(args.at(1).toInt(), 0x0010);
    const auto registers = args.at(2).value<QVector<quint16>>();
    QCOMPARE(registers.size(), 2);
    QCOMPARE(registers.at(0), static_cast<quint16>(0x0064));
    QCOMPARE(registers.at(1), static_cast<quint16>(0x00C8));
    QVERIFY(!client.hasPendingRequest());
}

void ModbusRtuClientTest::emitsFailureForExceptionResponse()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);
    QSignalSpy spy(&client, &ModbusRtuClient::requestFailed);

    client.readHoldingRegisters(0x11, 0x0010, 0x0002);

    // Exception response: slave=0x11, func=0x83, exceptionCode=0x02, crc
    transport.injectFrame(QByteArray::fromHex("118302c134"));

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.takeFirst().at(0).toString().contains("Exception"));
    QVERIFY(!client.hasPendingRequest());
}

void ModbusRtuClientTest::emitsFailureForCrcMismatch()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);
    QSignalSpy spy(&client, &ModbusRtuClient::requestFailed);

    client.readHoldingRegisters(0x11, 0x0010, 0x0002);

    // Response with corrupted CRC
    transport.injectFrame(QByteArray::fromHex("110304006400c8abbc"));

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.takeFirst().at(0).toString().contains("CRC"));
    QVERIFY(!client.hasPendingRequest());
}

void ModbusRtuClientTest::emitsFailureForWrongSlaveAddress()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);
    QSignalSpy spy(&client, &ModbusRtuClient::requestFailed);

    client.readHoldingRegisters(0x11, 0x0010, 0x0002);

    // Response with wrong slave address (0x12 instead of 0x11)
    transport.injectFrame(QByteArray::fromHex("120304006400c8abbb"));

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.takeFirst().at(0).toString().contains("Slave address"));
    QVERIFY(!client.hasPendingRequest());
}

void ModbusRtuClientTest::clearsPendingOnCancel()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);

    client.readHoldingRegisters(0x11, 0x0010, 0x0002);
    QVERIFY(client.hasPendingRequest());

    client.cancelPendingRequest();
    QVERIFY(!client.hasPendingRequest());
}

void ModbusRtuClientTest::retriesOnTimeout()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);
    client.setTimeoutMs(10);
    client.setMaxRetries(2);

    QSignalSpy failSpy(&client, &ModbusRtuClient::requestFailed);

    client.readHoldingRegisters(0x11, 0x0010, 0x0002);
    QCOMPARE(transport.writtenFrames().size(), 1);

    // Wait for timeout
    QTest::qSleep(30);
    QCoreApplication::processEvents();

    // Should have retried
    QCOMPARE(transport.writtenFrames().size(), 2);
    QVERIFY(client.hasPendingRequest());
    QCOMPARE(failSpy.count(), 0);
}

void ModbusRtuClientTest::failsAfterMaxRetries()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);
    client.setTimeoutMs(50);
    client.setMaxRetries(1);

    QSignalSpy failSpy(&client, &ModbusRtuClient::requestFailed);

    client.readHoldingRegisters(0x11, 0x0010, 0x0002);
    QCOMPARE(transport.writtenFrames().size(), 1);

    // Wait for the failure signal with timeout
    QVERIFY(failSpy.wait(500));
    QCOMPARE(failSpy.count(), 1);
    QVERIFY(failSpy.takeFirst().at(0).toString().contains("timed out"));
    QVERIFY(!client.hasPendingRequest());
    QCOMPARE(transport.writtenFrames().size(), 2);
}

void ModbusRtuClientTest::emitsRawFrameSent()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);
    QSignalSpy spy(&client, &ModbusRtuClient::rawFrameSent);

    client.readHoldingRegisters(0x11, 0x0010, 0x0002);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toByteArray(), QByteArray::fromHex("110300100002c75e"));
}

void ModbusRtuClientTest::emitsRawFrameReceived()
{
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);
    QSignalSpy spy(&client, &ModbusRtuClient::rawFrameReceived);

    client.readHoldingRegisters(0x11, 0x0010, 0x0002);

    const QByteArray frame = QByteArray::fromHex("110304006400c8abbb");
    transport.injectFrame(frame);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toByteArray(), frame);
}

void ModbusRtuClientTest::noQSerialPortDependency()
{
    // This test verifies that the header doesn't include QSerialPort
    // If QSerialPort was included, this file would fail to compile
    // without Qt6::SerialPort linked
    MockModbusTransport transport;
    ModbusRtuClient client(&transport);
    QVERIFY(!client.hasPendingRequest());
}

QTEST_MAIN(ModbusRtuClientTest)

#include "ModbusRtuClientTest.moc"

#include "communication/DcdcModbusSession.h"
#include "communication/MockModbusTransport.h"
#include "protocol/ModbusRtuFrame.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class DcdcModbusSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultConfiguration();
    void startSendsInitialReadRequest();
    void stopCancelsPendingRequest();
    void emitsDcdcDataDecodedOnValidResponse();
    void decodesScaledValues();
    void emitsCommunicationErrorOnTimeout();
    void emitsCommunicationErrorOnCrcMismatch();
    void emitsCommunicationErrorOnModbusException();
    void skipsPollWhileRequestPending();
    void respectsCustomSlaveAddress();
    void respectsCustomReadRange();
    void respectsCustomPollInterval();
    void handlesTransportClosedGracefully();
    void restartAfterStop();
    void relaysRawFrames();
    void doesNotDependOnSerialPortOrUi();

private:
    static QByteArray appendCrc(const QByteArray &payload);
    static QByteArray buildReadHoldingRegistersResponse(quint8 slave,
                                                        const QVector<quint16> &registers);
    static QByteArray buildExceptionResponse(quint8 slave, quint8 exceptionCode);
};

void DcdcModbusSessionTest::initTestCase()
{
    qRegisterMetaType<DcdcRegisterDecodeResult>("DcdcRegisterDecodeResult");
}

QByteArray DcdcModbusSessionTest::appendCrc(const QByteArray &payload)
{
    const quint16 crc = ModbusRtuFrame::crc16(payload);
    QByteArray frame = payload;
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    return frame;
}

QByteArray DcdcModbusSessionTest::buildReadHoldingRegistersResponse(
    quint8 slave, const QVector<quint16> &registers)
{
    QByteArray payload;
    payload.append(static_cast<char>(slave));
    payload.append(static_cast<char>(0x03));
    payload.append(static_cast<char>(static_cast<quint8>(registers.size() * 2)));

    for (quint16 val : registers) {
        payload.append(static_cast<char>((val >> 8) & 0xFF));
        payload.append(static_cast<char>(val & 0xFF));
    }

    return appendCrc(payload);
}

QByteArray DcdcModbusSessionTest::buildExceptionResponse(quint8 slave, quint8 exceptionCode)
{
    QByteArray payload;
    payload.append(static_cast<char>(slave));
    payload.append(static_cast<char>(0x83));
    payload.append(static_cast<char>(exceptionCode));

    return appendCrc(payload);
}

void DcdcModbusSessionTest::defaultConfiguration()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);

    QVERIFY(!session.isRunning());
    QCOMPARE(session.pollIntervalMs(), 1000);
    QCOMPARE(session.slaveAddress(), static_cast<quint8>(1));
    QCOMPARE(session.startRegister(), DcdcModbusRegisterMap::DefaultStartRegister);
    QCOMPARE(session.registerCount(), DcdcModbusRegisterMap::DefaultRegisterCount);
}

void DcdcModbusSessionTest::startSendsInitialReadRequest()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(500);
    session.setMaxRetries(0);

    session.start();

    QVERIFY(session.isRunning());
    QCOMPARE(transport.writtenFrames().size(), 1);

    // Verify the frame is a valid 0x03 request for registers 101..144
    const QByteArray frame = transport.writtenFrames().first();
    QCOMPARE(static_cast<quint8>(frame.at(0)), static_cast<quint8>(1));   // slave
    QCOMPARE(static_cast<quint8>(frame.at(1)), static_cast<quint8>(0x03)); // func
    // start register = 101 = 0x0065
    QCOMPARE(static_cast<quint8>(frame.at(2)), static_cast<quint8>(0x00));
    QCOMPARE(static_cast<quint8>(frame.at(3)), static_cast<quint8>(0x65));
    // register count = 44 = 0x002C
    QCOMPARE(static_cast<quint8>(frame.at(4)), static_cast<quint8>(0x00));
    QCOMPARE(static_cast<quint8>(frame.at(5)), static_cast<quint8>(0x2C));

    session.stop();
}

void DcdcModbusSessionTest::stopCancelsPendingRequest()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(20);
    session.setMaxRetries(0);

    QSignalSpy errorSpy(&session, &DcdcModbusSession::communicationError);

    session.start();
    QCOMPARE(transport.writtenFrames().size(), 1);

    // Stop before timeout fires
    session.stop();
    QVERIFY(!session.isRunning());

    // Wait past timeout — should NOT emit error since we cancelled
    QTest::qSleep(100);
    QCoreApplication::processEvents();

    QCOMPARE(errorSpy.count(), 0);
}

void DcdcModbusSessionTest::emitsDcdcDataDecodedOnValidResponse()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(500);
    session.setMaxRetries(0);

    // Use direct signal connection instead of QSignalSpy to avoid metatype QVariant issues
    DcdcRegisterDecodeResult capturedResult;
    bool captured = false;
    QObject::connect(&session, &DcdcModbusSession::dcdcDataDecoded,
                     [&](const DcdcRegisterDecodeResult &result) {
                         capturedResult = result;
                         captured = true;
                     });

    QSignalSpy errorSpy(&session, &DcdcModbusSession::communicationError);

    session.start();
    QCOMPARE(transport.writtenFrames().size(), 1);

    // Build a valid response with 44 registers, all set to 100
    QVector<quint16> registers(44, 100);
    transport.injectFrame(buildReadHoldingRegistersResponse(0x01, registers));

    QVERIFY(captured);
    QVERIFY(capturedResult.ok);
    QCOMPARE(capturedResult.values.size(), 44);
    QVERIFY(capturedResult.errors.isEmpty());
    QCOMPARE(errorSpy.count(), 0);

    session.stop();
}

void DcdcModbusSessionTest::decodesScaledValues()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(500);
    session.setMaxRetries(0);

    DcdcRegisterDecodeResult capturedResult;
    bool captured = false;
    QObject::connect(&session, &DcdcModbusSession::dcdcDataDecoded,
                     [&](const DcdcRegisterDecodeResult &result) {
                         capturedResult = result;
                         captured = true;
                     });

    session.start();

    // Build a response with known raw values
    QVector<quint16> registers;
    registers.reserve(44);
    for (int i = 0; i < 44; ++i) {
        registers.append(static_cast<quint16>(100 + i));
    }
    transport.injectFrame(buildReadHoldingRegistersResponse(0x01, registers));

    QVERIFY(captured);
    QVERIFY(capturedResult.ok);
    QCOMPARE(capturedResult.values.size(), 44);

    // With scale=0.1 and offset=0.0, raw 100 -> scaled 10.0
    QCOMPARE(capturedResult.values.at(0).rawValue, static_cast<quint16>(100));
    QCOMPARE(capturedResult.values.at(0).scaledValue, 10.0);

    // raw 143 -> scaled 14.3
    QCOMPARE(capturedResult.values.at(43).rawValue, static_cast<quint16>(143));
    QVERIFY(qAbs(capturedResult.values.at(43).scaledValue - 14.3) < 0.001);

    session.stop();
}

void DcdcModbusSessionTest::emitsCommunicationErrorOnTimeout()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(20);
    session.setMaxRetries(0);

    QSignalSpy errorSpy(&session, &DcdcModbusSession::communicationError);

    session.start();
    QCOMPARE(transport.writtenFrames().size(), 1);

    // Don't inject any response — wait for timeout to fire
    QTest::qSleep(100);
    QCoreApplication::processEvents();

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.first().at(0).toString().contains("timed out"));

    session.stop();
}

void DcdcModbusSessionTest::emitsCommunicationErrorOnCrcMismatch()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(500);
    session.setMaxRetries(0);

    QSignalSpy errorSpy(&session, &DcdcModbusSession::communicationError);

    session.start();

    // Build a response with corrupted CRC
    QVector<quint16> registers(44, 100);
    QByteArray frame = buildReadHoldingRegistersResponse(0x01, registers);
    // Corrupt the last byte (CRC)
    frame[frame.size() - 1] ^= 0xFF;
    transport.injectFrame(frame);

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.first().at(0).toString().contains("CRC"));

    session.stop();
}

void DcdcModbusSessionTest::emitsCommunicationErrorOnModbusException()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(500);
    session.setMaxRetries(0);

    QSignalSpy errorSpy(&session, &DcdcModbusSession::communicationError);

    session.start();

    // Exception response: illegal data address
    transport.injectFrame(buildExceptionResponse(0x01, 0x02));

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.first().at(0).toString().contains("Exception"));

    session.stop();
}

void DcdcModbusSessionTest::skipsPollWhileRequestPending()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(200);
    session.setMaxRetries(0);
    session.setPollIntervalMs(50);

    session.start();
    QCOMPARE(transport.writtenFrames().size(), 1);

    // Don't inject any response, wait for multiple poll intervals
    QTest::qSleep(250);
    QCoreApplication::processEvents();

    // Should still be only 1 frame — no duplicate requests
    QCOMPARE(transport.writtenFrames().size(), 1);

    session.stop();
}

void DcdcModbusSessionTest::respectsCustomSlaveAddress()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setSlaveAddress(5);
    session.setRequestTimeoutMs(500);
    session.setMaxRetries(0);

    session.start();

    QCOMPARE(transport.writtenFrames().size(), 1);
    const QByteArray frame = transport.writtenFrames().first();
    QCOMPARE(static_cast<quint8>(frame.at(0)), static_cast<quint8>(5));

    session.stop();
}

void DcdcModbusSessionTest::respectsCustomReadRange()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setReadRange(200, 10);
    session.setRequestTimeoutMs(500);
    session.setMaxRetries(0);

    session.start();

    QCOMPARE(transport.writtenFrames().size(), 1);
    const QByteArray frame = transport.writtenFrames().first();
    // slave
    QCOMPARE(static_cast<quint8>(frame.at(0)), static_cast<quint8>(1));
    // func
    QCOMPARE(static_cast<quint8>(frame.at(1)), static_cast<quint8>(0x03));
    // start register = 200 = 0x00C8
    QCOMPARE(static_cast<quint8>(frame.at(2)), static_cast<quint8>(0x00));
    QCOMPARE(static_cast<quint8>(frame.at(3)), static_cast<quint8>(0xC8));
    // register count = 10 = 0x000A
    QCOMPARE(static_cast<quint8>(frame.at(4)), static_cast<quint8>(0x00));
    QCOMPARE(static_cast<quint8>(frame.at(5)), static_cast<quint8>(0x0A));

    session.stop();
}

void DcdcModbusSessionTest::respectsCustomPollInterval()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setPollIntervalMs(500);

    QCOMPARE(session.pollIntervalMs(), 500);

    // Verify clamping to minimum 1
    session.setPollIntervalMs(0);
    QCOMPARE(session.pollIntervalMs(), 1);
}

void DcdcModbusSessionTest::handlesTransportClosedGracefully()
{
    MockModbusTransport transport;
    transport.setOpen(false);
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(500);
    session.setMaxRetries(0);

    QSignalSpy errorSpy(&session, &DcdcModbusSession::communicationError);

    session.start();

    // readHoldingRegisters should fail because transport is closed
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.first().at(0).toString().contains("not open"));

    session.stop();
}

void DcdcModbusSessionTest::restartAfterStop()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(500);
    session.setMaxRetries(0);

    session.start();
    QVERIFY(session.isRunning());
    QCOMPARE(transport.writtenFrames().size(), 1);

    // Inject a response to clear pending state
    QVector<quint16> registers(44, 100);
    transport.injectFrame(buildReadHoldingRegistersResponse(0x01, registers));

    session.stop();
    QVERIFY(!session.isRunning());

    transport.clearWrittenFrames();
    session.start();
    QVERIFY(session.isRunning());
    QCOMPARE(transport.writtenFrames().size(), 1);

    session.stop();
}

void DcdcModbusSessionTest::relaysRawFrames()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(500);
    session.setMaxRetries(0);

    QSignalSpy sentSpy(&session, &DcdcModbusSession::rawFrameSent);
    QSignalSpy recvSpy(&session, &DcdcModbusSession::rawFrameReceived);

    session.start();

    QCOMPARE(sentSpy.count(), 1);
    QCOMPARE(transport.writtenFrames().size(), 1);
    QCOMPARE(sentSpy.first().at(0).toByteArray(), transport.writtenFrames().first());

    // Inject a response
    QVector<quint16> registers(44, 100);
    const QByteArray response = buildReadHoldingRegistersResponse(0x01, registers);
    transport.injectFrame(response);

    QCOMPARE(recvSpy.count(), 1);
    QCOMPARE(recvSpy.first().at(0).toByteArray(), response);

    session.stop();
}

void DcdcModbusSessionTest::doesNotDependOnSerialPortOrUi()
{
    // Compile-time guard: if DcdcModbusSession.h pulled in QSerialPort or QWidget,
    // this test file would fail to link without Qt6::SerialPort or Qt6::Widgets.
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    QVERIFY(!session.isRunning());
}

QTEST_MAIN(DcdcModbusSessionTest)

#include "DcdcModbusSessionTest.moc"

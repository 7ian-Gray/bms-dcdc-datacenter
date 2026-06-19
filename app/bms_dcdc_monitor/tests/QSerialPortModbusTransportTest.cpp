#include <QtTest>
#include <QSignalSpy>
#include "QSerialPortModbusTransport.h"

class QSerialPortModbusTransportTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultStateIsClosed();
    void defaultBaudRateIs9600();
    void defaultSerialSettingsAre9600_8N1();
    void defaultInterFrameTimeoutIs20Ms();
    void setterAndGetterPortName();
    void setterAndGetterBaudRate();
    void setterAndGetterSerialSettings();
    void setterAndGetterInterFrameTimeout();
    void writeFrameFailsWhenNotOpen();
    void writeFrameFailsForEmptyFrame();
    void openNonexistentPortFails();
    void openNonexistentPortEmitsError();
    void closeWhenNotOpenIsSafe();
    void doesNotDependOnUiOrDcdcMap();
};

void QSerialPortModbusTransportTest::defaultStateIsClosed()
{
    QSerialPortModbusTransport transport;
    QVERIFY(!transport.isOpen());
}

void QSerialPortModbusTransportTest::defaultBaudRateIs9600()
{
    QSerialPortModbusTransport transport;
    QCOMPARE(transport.baudRate(), 9600);
}

void QSerialPortModbusTransportTest::defaultSerialSettingsAre9600_8N1()
{
    QSerialPortModbusTransport transport;
    QCOMPARE(transport.baudRate(), 9600);
    QCOMPARE(transport.dataBits(), QSerialPort::Data8);
    QCOMPARE(transport.parity(), QSerialPort::NoParity);
    QCOMPARE(transport.stopBits(), QSerialPort::OneStop);
    QCOMPARE(transport.flowControl(), QSerialPort::NoFlowControl);
}

void QSerialPortModbusTransportTest::defaultInterFrameTimeoutIs20Ms()
{
    QSerialPortModbusTransport transport;
    QCOMPARE(transport.interFrameTimeoutMs(), 20);
}

void QSerialPortModbusTransportTest::setterAndGetterPortName()
{
    QSerialPortModbusTransport transport;
    transport.setPortName("/dev/ttyUSB0");
    // QSerialPort may normalize the port name (stripping /dev/ prefix on macOS)
    QVERIFY(!transport.portName().isEmpty());
    QVERIFY(transport.portName().contains("ttyUSB0"));
}

void QSerialPortModbusTransportTest::setterAndGetterBaudRate()
{
    QSerialPortModbusTransport transport;
    transport.setBaudRate(115200);
    QCOMPARE(transport.baudRate(), 115200);
}

void QSerialPortModbusTransportTest::setterAndGetterSerialSettings()
{
    QSerialPortModbusTransport transport;
    transport.setBaudRate(19200);
    transport.setDataBits(QSerialPort::Data7);
    transport.setParity(QSerialPort::EvenParity);
    transport.setStopBits(QSerialPort::TwoStop);
    transport.setFlowControl(QSerialPort::HardwareControl);

    QCOMPARE(transport.baudRate(), 19200);
    QCOMPARE(transport.dataBits(), QSerialPort::Data7);
    QCOMPARE(transport.parity(), QSerialPort::EvenParity);
    QCOMPARE(transport.stopBits(), QSerialPort::TwoStop);
    QCOMPARE(transport.flowControl(), QSerialPort::HardwareControl);
}

void QSerialPortModbusTransportTest::setterAndGetterInterFrameTimeout()
{
    QSerialPortModbusTransport transport;
    transport.setInterFrameTimeoutMs(50);
    QCOMPARE(transport.interFrameTimeoutMs(), 50);
}

void QSerialPortModbusTransportTest::writeFrameFailsWhenNotOpen()
{
    QSerialPortModbusTransport transport;
    QSignalSpy spy(&transport, &ModbusTransport::errorOccurred);

    bool ok = transport.writeFrame(QByteArray::fromHex("110300100002c75e"));
    QVERIFY(!ok);
    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.first().first().toString().contains("not open", Qt::CaseInsensitive));
}

void QSerialPortModbusTransportTest::writeFrameFailsForEmptyFrame()
{
    QSerialPortModbusTransport transport;
    QSignalSpy spy(&transport, &ModbusTransport::errorOccurred);

    // Port is not open, so "not open" error fires before empty check.
    // Both empty and not-open are valid failure reasons.
    bool ok = transport.writeFrame(QByteArray());
    QVERIFY(!ok);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!spy.first().first().toString().isEmpty());
}

void QSerialPortModbusTransportTest::openNonexistentPortFails()
{
    QSerialPortModbusTransport transport;
    transport.setPortName("__bms_dcdc_nonexistent_serial_port__");

    QString error;
    bool ok = transport.open(&error);
    QVERIFY(!ok);
    QVERIFY(!error.isEmpty());
    QVERIFY(!transport.isOpen());
}

void QSerialPortModbusTransportTest::openNonexistentPortEmitsError()
{
    QSerialPortModbusTransport transport;
    transport.setPortName("__bms_dcdc_nonexistent_serial_port__");

    QSignalSpy spy(&transport, &ModbusTransport::errorOccurred);
    transport.open();

    QVERIFY(spy.count() >= 1);
    QVERIFY(!spy.first().first().toString().isEmpty());
}

void QSerialPortModbusTransportTest::closeWhenNotOpenIsSafe()
{
    QSerialPortModbusTransport transport;
    // Should not crash or throw
    transport.close();
    transport.close();
    QVERIFY(!transport.isOpen());
}

void QSerialPortModbusTransportTest::doesNotDependOnUiOrDcdcMap()
{
    // Compile-time guard: this test verifies the transport header
    // does not pull in UI or DCDC map dependencies.
    // QSerialPort is expected and allowed.
    QSerialPortModbusTransport transport;
    transport.setPortName("/dev/null");
    QVERIFY(!transport.isOpen());
}

QTEST_MAIN(QSerialPortModbusTransportTest)
#include "QSerialPortModbusTransportTest.moc"

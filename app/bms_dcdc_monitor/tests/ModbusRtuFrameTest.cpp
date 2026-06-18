#include "protocol/ModbusRtuFrame.h"

#include <QtTest/QtTest>

class ModbusRtuFrameTest : public QObject
{
    Q_OBJECT

private slots:
    void crc16KnownVector();
    void buildsReadHoldingRegistersRequestGoldenVector();
    void rejectsInvalidSlaveAddressZero();
    void rejectsInvalidSlaveAddressAbove247();
    void rejectsZeroRegisterCount();
    void rejectsRegisterCountAbove125();
    void rejectsRegisterAddressOverflow();
    void decodesValidReadHoldingRegistersResponse();
    void rejectsWrongSlaveAddress();
    void rejectsWrongFunctionCode();
    void rejectsMismatchedByteCount();
    void rejectsCrcMismatch();
    void recognizesExceptionResponse();
    void waitsForPartialNormalResponse();
    void waitsForPartialExceptionResponse();
    void reportsConsumedBytesWithTrailingData();
};

void ModbusRtuFrameTest::crc16KnownVector()
{
    // Golden vector: slave=0x11, func=0x03, start=0x0010, count=0x0002
    // Expected frame: 11 03 00 10 00 02 C7 5E
    const QByteArray request = QByteArray::fromHex("110300100002");
    const quint16 crc = ModbusRtuFrame::crc16(request);

    QCOMPARE(crc, static_cast<quint16>(0x5EC7));

    // Verify CRC bytes are low-first, high-second
    const quint8 crcLow = crc & 0xFF;
    const quint8 crcHigh = (crc >> 8) & 0xFF;
    QCOMPARE(crcLow, static_cast<quint8>(0xC7));
    QCOMPARE(crcHigh, static_cast<quint8>(0x5E));
}

void ModbusRtuFrameTest::buildsReadHoldingRegistersRequestGoldenVector()
{
    const QByteArray frame = ModbusRtuFrame::buildReadHoldingRegistersRequest(
        0x11, 0x0010, 0x0002);

    QCOMPARE(frame, QByteArray::fromHex("110300100002c75e"));
    QCOMPARE(frame.size(), 8);

    // Verify CRC is valid
    QVERIFY(ModbusRtuFrame::hasValidCrc(frame));
}

void ModbusRtuFrameTest::rejectsInvalidSlaveAddressZero()
{
    QString error;
    const bool valid = ModbusRtuFrame::isValidReadHoldingRegistersRequest(
        0x00, 0x0000, 0x0001, &error);

    QVERIFY(!valid);
    QVERIFY(error.contains("1-247"));
}

void ModbusRtuFrameTest::rejectsInvalidSlaveAddressAbove247()
{
    QString error;
    const bool valid = ModbusRtuFrame::isValidReadHoldingRegistersRequest(
        0xF8, 0x0000, 0x0001, &error);

    QVERIFY(!valid);
    QVERIFY(error.contains("1-247"));
}

void ModbusRtuFrameTest::rejectsZeroRegisterCount()
{
    QString error;
    const bool valid = ModbusRtuFrame::isValidReadHoldingRegistersRequest(
        0x01, 0x0000, 0x0000, &error);

    QVERIFY(!valid);
    QVERIFY(error.contains("1-125"));
}

void ModbusRtuFrameTest::rejectsRegisterCountAbove125()
{
    QString error;
    const bool valid = ModbusRtuFrame::isValidReadHoldingRegistersRequest(
        0x01, 0x0000, 0x007E, &error);

    QVERIFY(!valid);
    QVERIFY(error.contains("1-125"));
}

void ModbusRtuFrameTest::rejectsRegisterAddressOverflow()
{
    QString error;
    const bool valid = ModbusRtuFrame::isValidReadHoldingRegistersRequest(
        0x01, 0xFFFE, 0x0003, &error);

    QVERIFY(!valid);
    QVERIFY(error.contains("overflow"));
}

void ModbusRtuFrameTest::decodesValidReadHoldingRegistersResponse()
{
    // Build request for reference
    const PendingModbusRequest pending = {
        .slaveAddress = 0x11,
        .functionCode = 0x03,
        .startRegister = 0x0010,
        .registerCount = 2
    };

    // Response: slave=0x11, func=0x03, byteCount=4, data=[0x0064, 0x00C8], crc
    // Data: 00 64 00 C8
    const QByteArray response = QByteArray::fromHex("110304006400c8abbb");

    const ModbusResponseValidation result =
        ModbusRtuFrame::validateReadHoldingRegistersResponse(response, pending);

    QCOMPARE(result.error, ModbusRtuError::None);
    QCOMPARE(result.registers.size(), 2);
    QCOMPARE(result.registers.at(0), static_cast<quint16>(0x0064));
    QCOMPARE(result.registers.at(1), static_cast<quint16>(0x00C8));
    QCOMPARE(result.consumedBytes, 9);
    QCOMPARE(result.exceptionCode, static_cast<quint8>(0));
}

void ModbusRtuFrameTest::rejectsWrongSlaveAddress()
{
    const PendingModbusRequest pending = {
        .slaveAddress = 0x11,
        .functionCode = 0x03,
        .startRegister = 0x0010,
        .registerCount = 2
    };

    // Response with wrong slave address (0x12 instead of 0x11)
    const QByteArray response = QByteArray::fromHex("120304006400c8fa24");

    const ModbusResponseValidation result =
        ModbusRtuFrame::validateReadHoldingRegistersResponse(response, pending);

    QCOMPARE(result.error, ModbusRtuError::ResponseMismatch);
    QVERIFY(result.message.contains("Slave address mismatch"));
}

void ModbusRtuFrameTest::rejectsWrongFunctionCode()
{
    const PendingModbusRequest pending = {
        .slaveAddress = 0x11,
        .functionCode = 0x03,
        .startRegister = 0x0010,
        .registerCount = 2
    };

    // Response with wrong function code (0x04 instead of 0x03)
    const QByteArray response = QByteArray::fromHex("110404006400c8fa24");

    const ModbusResponseValidation result =
        ModbusRtuFrame::validateReadHoldingRegistersResponse(response, pending);

    QCOMPARE(result.error, ModbusRtuError::ResponseMismatch);
    QVERIFY(result.message.contains("Function code mismatch"));
}

void ModbusRtuFrameTest::rejectsMismatchedByteCount()
{
    const PendingModbusRequest pending = {
        .slaveAddress = 0x11,
        .functionCode = 0x03,
        .startRegister = 0x0010,
        .registerCount = 2
    };

    // Response with byteCount=6 (should be 4 for 2 registers)
    const QByteArray response = QByteArray::fromHex("110306006400c80000fa24");

    const ModbusResponseValidation result =
        ModbusRtuFrame::validateReadHoldingRegistersResponse(response, pending);

    QCOMPARE(result.error, ModbusRtuError::ResponseMismatch);
    QVERIFY(result.message.contains("Byte count mismatch"));
}

void ModbusRtuFrameTest::rejectsCrcMismatch()
{
    const PendingModbusRequest pending = {
        .slaveAddress = 0x11,
        .functionCode = 0x03,
        .startRegister = 0x0010,
        .registerCount = 2
    };

    // Valid response but with corrupted CRC (last byte changed)
    const QByteArray response = QByteArray::fromHex("110304006400c8fa25");

    const ModbusResponseValidation result =
        ModbusRtuFrame::validateReadHoldingRegistersResponse(response, pending);

    QCOMPARE(result.error, ModbusRtuError::CrcMismatch);
    QVERIFY(result.message.contains("CRC mismatch"));
}

void ModbusRtuFrameTest::recognizesExceptionResponse()
{
    const PendingModbusRequest pending = {
        .slaveAddress = 0x11,
        .functionCode = 0x03,
        .startRegister = 0x0010,
        .registerCount = 2
    };

    // Exception response: slave=0x11, func=0x83, exceptionCode=0x02, crc
    const QByteArray response = QByteArray::fromHex("118302c134");

    const ModbusResponseValidation result =
        ModbusRtuFrame::validateReadHoldingRegistersResponse(response, pending);

    QCOMPARE(result.error, ModbusRtuError::ExceptionResponse);
    QCOMPARE(result.exceptionCode, static_cast<quint8>(0x02));
    QCOMPARE(result.consumedBytes, 5);
    QVERIFY(result.message.contains("Exception response"));
}

void ModbusRtuFrameTest::waitsForPartialNormalResponse()
{
    const PendingModbusRequest pending = {
        .slaveAddress = 0x11,
        .functionCode = 0x03,
        .startRegister = 0x0010,
        .registerCount = 2
    };

    // Partial response: only first 4 bytes (missing data and CRC)
    const QByteArray response = QByteArray::fromHex("1103040064");

    const ModbusResponseValidation result =
        ModbusRtuFrame::validateReadHoldingRegistersResponse(response, pending);

    QCOMPARE(result.error, ModbusRtuError::NeedMoreData);
    QCOMPARE(result.consumedBytes, 0);
}

void ModbusRtuFrameTest::waitsForPartialExceptionResponse()
{
    const PendingModbusRequest pending = {
        .slaveAddress = 0x11,
        .functionCode = 0x03,
        .startRegister = 0x0010,
        .registerCount = 2
    };

    // Partial exception: only 3 bytes (missing CRC)
    const QByteArray response = QByteArray::fromHex("118302");

    const ModbusResponseValidation result =
        ModbusRtuFrame::validateReadHoldingRegistersResponse(response, pending);

    QCOMPARE(result.error, ModbusRtuError::NeedMoreData);
    QCOMPARE(result.consumedBytes, 0);
}

void ModbusRtuFrameTest::reportsConsumedBytesWithTrailingData()
{
    const PendingModbusRequest pending = {
        .slaveAddress = 0x11,
        .functionCode = 0x03,
        .startRegister = 0x0010,
        .registerCount = 2
    };

    // Valid response followed by trailing garbage
    const QByteArray response = QByteArray::fromHex("110304006400c8abbbdeadbeef");

    const ModbusResponseValidation result =
        ModbusRtuFrame::validateReadHoldingRegistersResponse(response, pending);

    QCOMPARE(result.error, ModbusRtuError::None);
    QCOMPARE(result.consumedBytes, 9);
    QCOMPARE(result.registers.size(), 2);
    QCOMPARE(result.registers.at(0), static_cast<quint16>(0x0064));
    QCOMPARE(result.registers.at(1), static_cast<quint16>(0x00C8));

    // Verify that only the first 9 bytes are consumed, not the trailing data
    QVERIFY(result.consumedBytes < response.size());
}

QTEST_MAIN(ModbusRtuFrameTest)

#include "ModbusRtuFrameTest.moc"

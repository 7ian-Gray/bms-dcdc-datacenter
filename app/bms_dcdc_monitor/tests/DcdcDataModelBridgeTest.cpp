#include "communication/DcdcDataModelBridge.h"
#include "communication/DcdcModbusSession.h"
#include "communication/MockModbusTransport.h"
#include "protocol/ModbusRtuFrame.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class DcdcDataModelBridgeTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultConfiguration_hasZeroInitialDataAndDefaultMappings();
    void updatesDcdcDataFromValidDecodeResult();
    void mapsKnownRegistersToDcdcDataFields();
    void ignoresUnmappedRegisters();
    void partialRegisterUpdatePreservesPreviousValues();
    void decodeFailureDoesNotUpdateCurrentData();
    void decodeFailureEmitsBridgeError();
    void consecutiveValidUpdatesRefreshData();
    void customMappingOverridesDefaultMapping();
    void emptyMappingEmitsUpdatedDataWithoutChangingFields();
    void attachSessionRelaysDecodedDataFromDcdcModbusSession();
    void doesNotDependOnSerialPortOrUi();

private:
    static DcdcRegisterDecodeResult buildDecodeResult(
        const QVector<QPair<quint16, double>> &addressScaledPairs);
    static DcdcRegisterDecodeResult buildFailedDecodeResult(const QStringList &errors);
    static QByteArray appendCrc(const QByteArray &payload);
    static QByteArray buildReadHoldingRegistersResponse(quint8 slave,
                                                        const QVector<quint16> &registers);
};

void DcdcDataModelBridgeTest::initTestCase()
{
    qRegisterMetaType<DcdcData>("DcdcData");
    qRegisterMetaType<DcdcRegisterDecodeResult>("DcdcRegisterDecodeResult");
}

DcdcRegisterDecodeResult DcdcDataModelBridgeTest::buildDecodeResult(
    const QVector<QPair<quint16, double>> &addressScaledPairs)
{
    DcdcRegisterDecodeResult result;
    result.ok = true;
    for (const auto &pair : addressScaledPairs) {
        DcdcRegisterValue val;
        val.address = pair.first;
        val.key = QStringLiteral("register_%1").arg(pair.first);
        val.displayName = QStringLiteral("Register %1").arg(pair.first);
        val.rawValue = static_cast<quint16>(pair.second / 0.1); // inverse of default scale
        val.scaledValue = pair.second;
        result.values.append(val);
    }
    return result;
}

DcdcRegisterDecodeResult DcdcDataModelBridgeTest::buildFailedDecodeResult(
    const QStringList &errors)
{
    DcdcRegisterDecodeResult result;
    result.ok = false;
    result.errors = errors;
    return result;
}

QByteArray DcdcDataModelBridgeTest::appendCrc(const QByteArray &payload)
{
    const quint16 crc = ModbusRtuFrame::crc16(payload);
    QByteArray frame = payload;
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    return frame;
}

QByteArray DcdcDataModelBridgeTest::buildReadHoldingRegistersResponse(
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

void DcdcDataModelBridgeTest::defaultConfiguration_hasZeroInitialDataAndDefaultMappings()
{
    DcdcDataModelBridge bridge;

    const DcdcData data = bridge.currentData();
    QCOMPARE(data.hvVoltage, 0.0);
    QCOMPARE(data.hvCurrent, 0.0);
    QCOMPARE(data.lvVoltage, 0.0);
    QCOMPARE(data.lvCurrent, 0.0);
    QCOMPARE(data.boardTemperature, 0.0);
    QCOMPARE(data.chargeCurrentLimit, 0.0);
    QCOMPARE(data.dischargeCurrentLimit, 0.0);
    QCOMPARE(data.chargeStartVoltage, 0.0);
    QCOMPARE(data.chargeStopVoltage, 0.0);
    QCOMPARE(data.chargeTargetVoltage, 0.0);
    QCOMPARE(data.dischargeStartVoltage, 0.0);
    QCOMPARE(data.dischargeStopVoltage, 0.0);
    QCOMPARE(data.dischargeTargetVoltage, 0.0);

    const auto mappings = bridge.mappings();
    QCOMPARE(mappings.size(), 13);
    QCOMPARE(mappings.first().address, static_cast<quint16>(101));
    QCOMPARE(mappings.last().address, static_cast<quint16>(113));
}

void DcdcDataModelBridgeTest::updatesDcdcDataFromValidDecodeResult()
{
    DcdcDataModelBridge bridge;

    QSignalSpy spy(&bridge, &DcdcDataModelBridge::dcdcDataUpdated);

    // Build a result with register 101 = hvVoltage = 24.5
    auto result = buildDecodeResult({{101, 24.5}});
    bridge.onDcdcDataDecoded(result);

    QCOMPARE(spy.count(), 1);
    const DcdcData emitted = spy.first().first().value<DcdcData>();
    QCOMPARE(emitted.hvVoltage, 24.5);

    const DcdcData current = bridge.currentData();
    QCOMPARE(current.hvVoltage, 24.5);
}

void DcdcDataModelBridgeTest::mapsKnownRegistersToDcdcDataFields()
{
    DcdcDataModelBridge bridge;

    // Map all 13 registers
    auto result = buildDecodeResult({
        {101, 24.0},   // hvVoltage
        {102, 10.5},   // hvCurrent
        {103, 12.0},   // lvVoltage
        {104, 5.5},    // lvCurrent
        {105, 35.2},   // boardTemperature
        {106, 20.0},   // chargeCurrentLimit
        {107, 15.0},   // dischargeCurrentLimit
        {108, 22.0},   // chargeStartVoltage
        {109, 28.0},   // chargeStopVoltage
        {110, 26.0},   // chargeTargetVoltage
        {111, 44.0},   // dischargeStartVoltage
        {112, 40.0},   // dischargeStopVoltage
        {113, 42.0},   // dischargeTargetVoltage
    });

    bridge.onDcdcDataDecoded(result);

    const DcdcData data = bridge.currentData();
    QCOMPARE(data.hvVoltage, 24.0);
    QCOMPARE(data.hvCurrent, 10.5);
    QCOMPARE(data.lvVoltage, 12.0);
    QCOMPARE(data.lvCurrent, 5.5);
    QCOMPARE(data.boardTemperature, 35.2);
    QCOMPARE(data.chargeCurrentLimit, 20.0);
    QCOMPARE(data.dischargeCurrentLimit, 15.0);
    QCOMPARE(data.chargeStartVoltage, 22.0);
    QCOMPARE(data.chargeStopVoltage, 28.0);
    QCOMPARE(data.chargeTargetVoltage, 26.0);
    QCOMPARE(data.dischargeStartVoltage, 44.0);
    QCOMPARE(data.dischargeStopVoltage, 40.0);
    QCOMPARE(data.dischargeTargetVoltage, 42.0);
}

void DcdcDataModelBridgeTest::ignoresUnmappedRegisters()
{
    DcdcDataModelBridge bridge;

    // Register 999 is not in any mapping
    auto result = buildDecodeResult({{999, 99.9}});
    QSignalSpy spy(&bridge, &DcdcDataModelBridge::dcdcDataUpdated);

    bridge.onDcdcDataDecoded(result);

    QCOMPARE(spy.count(), 1);
    const DcdcData data = spy.first().first().value<DcdcData>();
    // All fields should remain at default 0.0
    QCOMPARE(data.hvVoltage, 0.0);
    QCOMPARE(data.hvCurrent, 0.0);
    QCOMPARE(data.lvVoltage, 0.0);
    QCOMPARE(data.lvCurrent, 0.0);
}

void DcdcDataModelBridgeTest::partialRegisterUpdatePreservesPreviousValues()
{
    DcdcDataModelBridge bridge;

    // First update: set hvVoltage and lvVoltage
    auto result1 = buildDecodeResult({{101, 24.0}, {103, 12.0}});
    bridge.onDcdcDataDecoded(result1);

    QCOMPARE(bridge.currentData().hvVoltage, 24.0);
    QCOMPARE(bridge.currentData().lvVoltage, 12.0);
    QCOMPARE(bridge.currentData().hvCurrent, 0.0);

    // Second update: only hvCurrent changes
    auto result2 = buildDecodeResult({{102, 10.5}});
    bridge.onDcdcDataDecoded(result2);

    const DcdcData data = bridge.currentData();
    QCOMPARE(data.hvVoltage, 24.0);   // preserved from first update
    QCOMPARE(data.lvVoltage, 12.0);   // preserved from first update
    QCOMPARE(data.hvCurrent, 10.5);   // updated
}

void DcdcDataModelBridgeTest::decodeFailureDoesNotUpdateCurrentData()
{
    DcdcDataModelBridge bridge;

    // Set some initial data
    auto validResult = buildDecodeResult({{101, 24.0}});
    bridge.onDcdcDataDecoded(validResult);
    QCOMPARE(bridge.currentData().hvVoltage, 24.0);

    // Now emit a failed decode
    auto failedResult = buildFailedDecodeResult({"CRC mismatch"});
    bridge.onDcdcDataDecoded(failedResult);

    // Data should remain unchanged
    QCOMPARE(bridge.currentData().hvVoltage, 24.0);
}

void DcdcDataModelBridgeTest::decodeFailureEmitsBridgeError()
{
    DcdcDataModelBridge bridge;

    QSignalSpy spy(&bridge, &DcdcDataModelBridge::bridgeError);

    auto failedResult = buildFailedDecodeResult({"CRC mismatch", "timeout"});
    bridge.onDcdcDataDecoded(failedResult);

    QCOMPARE(spy.count(), 1);
    const QString errorMsg = spy.first().first().toString();
    QVERIFY(errorMsg.contains("CRC mismatch"));
    QVERIFY(errorMsg.contains("timeout"));
}

void DcdcDataModelBridgeTest::consecutiveValidUpdatesRefreshData()
{
    DcdcDataModelBridge bridge;

    QSignalSpy spy(&bridge, &DcdcDataModelBridge::dcdcDataUpdated);

    for (int i = 1; i <= 5; ++i) {
        auto result = buildDecodeResult({{101, static_cast<double>(i) * 10.0}});
        bridge.onDcdcDataDecoded(result);
    }

    QCOMPARE(spy.count(), 5);
    QCOMPARE(bridge.currentData().hvVoltage, 50.0);
}

void DcdcDataModelBridgeTest::customMappingOverridesDefaultMapping()
{
    DcdcDataModelBridge bridge;

    // Remap address 101 to LvVoltage instead of HvVoltage
    QVector<DcdcDataModelBridge::RegisterMapping> customMappings = {
        {101, DcdcDataModelBridge::Field::LvVoltage},
    };
    bridge.setMappings(customMappings);

    auto result = buildDecodeResult({{101, 12.0}});
    bridge.onDcdcDataDecoded(result);

    const DcdcData data = bridge.currentData();
    QCOMPARE(data.lvVoltage, 12.0);
    QCOMPARE(data.hvVoltage, 0.0);  // 101 no longer maps to hvVoltage
}

void DcdcDataModelBridgeTest::emptyMappingEmitsUpdatedDataWithoutChangingFields()
{
    DcdcDataModelBridge bridge;
    bridge.setMappings({});

    QSignalSpy spy(&bridge, &DcdcDataModelBridge::dcdcDataUpdated);

    auto result = buildDecodeResult({{101, 24.0}});
    bridge.onDcdcDataDecoded(result);

    QCOMPARE(spy.count(), 1);
    const DcdcData data = spy.first().first().value<DcdcData>();
    QCOMPARE(data.hvVoltage, 0.0);  // unchanged because no mapping for 101
}

void DcdcDataModelBridgeTest::attachSessionRelaysDecodedDataFromDcdcModbusSession()
{
    MockModbusTransport transport;
    DcdcModbusSession session(&transport);
    session.setRequestTimeoutMs(500);
    session.setMaxRetries(0);

    DcdcDataModelBridge bridge;
    bridge.attachSession(&session);

    QSignalSpy spy(&bridge, &DcdcDataModelBridge::dcdcDataUpdated);

    session.start();

    // Build a response with 44 registers; register at index 0 = address 101, etc.
    // Set register 101 (index 0) to raw 240 -> scaled 24.0 (scale=0.1)
    QVector<quint16> registers(44, 0);
    registers[0] = 240;   // address 101 -> hvVoltage = 24.0
    registers[1] = 105;   // address 102 -> hvCurrent = 10.5
    registers[2] = 120;   // address 103 -> lvVoltage = 12.0

    transport.injectFrame(buildReadHoldingRegistersResponse(0x01, registers));

    QCOMPARE(spy.count(), 1);
    const DcdcData data = spy.first().first().value<DcdcData>();
    QCOMPARE(data.hvVoltage, 24.0);
    QCOMPARE(data.hvCurrent, 10.5);
    QCOMPARE(data.lvVoltage, 12.0);

    session.stop();
}

void DcdcDataModelBridgeTest::doesNotDependOnSerialPortOrUi()
{
    DcdcDataModelBridge bridge;
    DcdcData data = bridge.currentData();
    QCOMPARE(data.hvVoltage, 0.0);
}

QTEST_MAIN(DcdcDataModelBridgeTest)

#include "DcdcDataModelBridgeTest.moc"

#include <QtTest>
#include "DcdcModbusRegisterMap.h"

class DcdcModbusRegisterMapTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultReadRangeIs101To144();
    void defaultDefinitionsContain44Registers();
    void defaultDefinitionsAreContiguous();
    void defaultDefinitionsUseScale0p1();
    void validatesDefaultDefinitions();
    void rejectsEmptyDefinitions();
    void rejectsDuplicateAddresses();
    void rejectsEmptyKey();
    void rejectsZeroScale();
    void decodesUnsignedRawAndScaledValues();
    void decodesSignedRawAndScaledValues();
    void reportsMissingRegister();
    void allowsExtraRegistersInResponse();
    void decodesNonZeroResponseStartOffset();
    void preservesRawValue();
    void doesNotDependOnSerialPortOrUi();
};

void DcdcModbusRegisterMapTest::defaultReadRangeIs101To144()
{
    auto range = DcdcModbusRegisterMap::defaultReadRange();
    QCOMPARE(range.startRegister, static_cast<quint16>(101));
    QCOMPARE(range.registerCount, static_cast<quint16>(44));
}

void DcdcModbusRegisterMapTest::defaultDefinitionsContain44Registers()
{
    auto defs = DcdcModbusRegisterMap::defaultDefinitions();
    QCOMPARE(defs.size(), 44);
}

void DcdcModbusRegisterMapTest::defaultDefinitionsAreContiguous()
{
    auto defs = DcdcModbusRegisterMap::defaultDefinitions();

    for (int i = 0; i < defs.size(); ++i)
    {
        QCOMPARE(defs.at(i).address,
                 static_cast<quint16>(DcdcModbusRegisterMap::DefaultStartRegister + i));
    }
}

void DcdcModbusRegisterMapTest::defaultDefinitionsUseScale0p1()
{
    auto defs = DcdcModbusRegisterMap::defaultDefinitions();

    for (const auto &def : defs)
    {
        QCOMPARE(def.scale, 0.1);
    }
}

void DcdcModbusRegisterMapTest::validatesDefaultDefinitions()
{
    auto defs = DcdcModbusRegisterMap::defaultDefinitions();
    QStringList errors;
    bool ok = DcdcModbusRegisterMap::validateDefinitions(defs, &errors);
    QVERIFY(ok);
    QVERIFY(errors.isEmpty());
}

void DcdcModbusRegisterMapTest::rejectsEmptyDefinitions()
{
    QVector<DcdcRegisterDefinition> empty;
    QStringList errors;
    bool ok = DcdcModbusRegisterMap::validateDefinitions(empty, &errors);
    QVERIFY(!ok);
    QVERIFY(!errors.isEmpty());
    QVERIFY(errors.first().contains("empty", Qt::CaseInsensitive));
}

void DcdcModbusRegisterMapTest::rejectsDuplicateAddresses()
{
    QVector<DcdcRegisterDefinition> defs;

    DcdcRegisterDefinition def1;
    def1.address = 101;
    def1.key = "reg_a";
    def1.scale = 1.0;
    defs.append(def1);

    DcdcRegisterDefinition def2;
    def2.address = 101;
    def2.key = "reg_b";
    def2.scale = 1.0;
    defs.append(def2);

    QStringList errors;
    bool ok = DcdcModbusRegisterMap::validateDefinitions(defs, &errors);
    QVERIFY(!ok);
    QVERIFY(!errors.isEmpty());

    bool foundDuplicate = false;
    for (const auto &err : errors)
    {
        if (err.contains("Duplicate", Qt::CaseInsensitive) && err.contains("101"))
        {
            foundDuplicate = true;
            break;
        }
    }
    QVERIFY2(foundDuplicate, "Expected duplicate address error for address 101");
}

void DcdcModbusRegisterMapTest::rejectsEmptyKey()
{
    QVector<DcdcRegisterDefinition> defs;

    DcdcRegisterDefinition def;
    def.address = 101;
    def.key = "";
    def.scale = 1.0;
    defs.append(def);

    QStringList errors;
    bool ok = DcdcModbusRegisterMap::validateDefinitions(defs, &errors);
    QVERIFY(!ok);
    QVERIFY(!errors.isEmpty());
    QVERIFY(errors.first().contains("key", Qt::CaseInsensitive));
}

void DcdcModbusRegisterMapTest::rejectsZeroScale()
{
    QVector<DcdcRegisterDefinition> defs;

    DcdcRegisterDefinition def;
    def.address = 101;
    def.key = "zero_scale_reg";
    def.scale = 0.0;
    defs.append(def);

    QStringList errors;
    bool ok = DcdcModbusRegisterMap::validateDefinitions(defs, &errors);
    QVERIFY(!ok);
    QVERIFY(!errors.isEmpty());

    bool foundZeroScale = false;
    for (const auto &err : errors)
    {
        if (err.contains("zero scale", Qt::CaseInsensitive))
        {
            foundZeroScale = true;
            break;
        }
    }
    QVERIFY2(foundZeroScale, "Expected 'zero scale' error for scale=0.0");
}

void DcdcModbusRegisterMapTest::decodesUnsignedRawAndScaledValues()
{
    QVector<DcdcRegisterDefinition> defs;

    DcdcRegisterDefinition def;
    def.address = 101;
    def.key = "test_reg";
    def.displayName = "Test Register";
    def.unit = "V";
    def.scale = 0.1;
    def.offset = 0.0;
    def.valueType = DcdcRegisterValueType::Unsigned16;
    defs.append(def);

    QVector<quint16> registers = {1000};
    auto result = DcdcModbusRegisterMap::decodeHoldingRegisters(101, registers, defs);

    QVERIFY(result.ok);
    QCOMPARE(result.values.size(), 1);
    QCOMPARE(result.values.first().rawValue, static_cast<quint16>(1000));
    QCOMPARE(result.values.first().scaledValue, 100.0);
    QCOMPARE(result.values.first().key, QStringLiteral("test_reg"));
    QCOMPARE(result.values.first().displayName, QStringLiteral("Test Register"));
    QCOMPARE(result.values.first().unit, QStringLiteral("V"));
}

void DcdcModbusRegisterMapTest::decodesSignedRawAndScaledValues()
{
    QVector<DcdcRegisterDefinition> defs;

    DcdcRegisterDefinition def;
    def.address = 101;
    def.key = "signed_reg";
    def.displayName = "Signed Register";
    def.scale = 0.1;
    def.offset = 0.0;
    def.valueType = DcdcRegisterValueType::Signed16;
    defs.append(def);

    // 0xFF9C = 65436 unsigned = -100 signed
    QVector<quint16> registers = {0xFF9C};
    auto result = DcdcModbusRegisterMap::decodeHoldingRegisters(101, registers, defs);

    QVERIFY(result.ok);
    QCOMPARE(result.values.size(), 1);
    QCOMPARE(result.values.first().rawValue, static_cast<quint16>(0xFF9C));
    QCOMPARE(result.values.first().scaledValue, -10.0);
}

void DcdcModbusRegisterMapTest::reportsMissingRegister()
{
    QVector<DcdcRegisterDefinition> defs;

    DcdcRegisterDefinition def1;
    def1.address = 101;
    def1.key = "reg_101";
    def1.scale = 1.0;
    defs.append(def1);

    DcdcRegisterDefinition def2;
    def2.address = 102;
    def2.key = "reg_102";
    def2.scale = 1.0;
    defs.append(def2);

    // Response only contains register 101, register 102 is missing
    QVector<quint16> registers = {500};
    auto result = DcdcModbusRegisterMap::decodeHoldingRegisters(101, registers, defs);

    QVERIFY(!result.ok);
    QCOMPARE(result.values.size(), 1);   // register 101 decoded
    QCOMPARE(result.errors.size(), 1);   // register 102 missing
    QVERIFY(result.errors.first().contains("102"));
    QVERIFY(result.errors.first().contains("Missing"));
}

void DcdcModbusRegisterMapTest::allowsExtraRegistersInResponse()
{
    QVector<DcdcRegisterDefinition> defs;

    DcdcRegisterDefinition def;
    def.address = 101;
    def.key = "reg_101";
    def.scale = 1.0;
    defs.append(def);

    // Response contains registers 101, 102, 103 but we only define 101
    QVector<quint16> registers = {500, 600, 700};
    auto result = DcdcModbusRegisterMap::decodeHoldingRegisters(101, registers, defs);

    QVERIFY(result.ok);
    QCOMPARE(result.values.size(), 1);
    QCOMPARE(result.values.first().rawValue, static_cast<quint16>(500));
    QVERIFY(result.errors.isEmpty());
}

void DcdcModbusRegisterMapTest::decodesNonZeroResponseStartOffset()
{
    QVector<DcdcRegisterDefinition> defs;

    DcdcRegisterDefinition def;
    def.address = 103;
    def.key = "reg_103";
    def.scale = 0.1;
    defs.append(def);

    // Response starts at register 101, register 103 is at offset 2
    QVector<quint16> registers = {100, 200, 300};
    auto result = DcdcModbusRegisterMap::decodeHoldingRegisters(101, registers, defs);

    QVERIFY(result.ok);
    QCOMPARE(result.values.size(), 1);
    QCOMPARE(result.values.first().rawValue, static_cast<quint16>(300));
    QCOMPARE(result.values.first().scaledValue, 30.0);
}

void DcdcModbusRegisterMapTest::preservesRawValue()
{
    QVector<DcdcRegisterDefinition> defs;

    DcdcRegisterDefinition def;
    def.address = 101;
    def.key = "reg_101";
    def.scale = 0.1;
    defs.append(def);

    QVector<quint16> registers = {12345};
    auto result = DcdcModbusRegisterMap::decodeHoldingRegisters(101, registers, defs);

    QVERIFY(result.ok);
    QCOMPARE(result.values.first().rawValue, static_cast<quint16>(12345));
    QCOMPARE(result.values.first().scaledValue, 1234.5);
}

void DcdcModbusRegisterMapTest::doesNotDependOnSerialPortOrUi()
{
    // This test exists as a compile-time guard.
    // If this file transitively included QSerialPort or UI headers,
    // it would fail to compile because this test target does NOT
    // link Qt6::SerialPort or Qt6::Widgets.

    auto defs = DcdcModbusRegisterMap::defaultDefinitions();
    QCOMPARE(defs.size(), 44);

    auto range = DcdcModbusRegisterMap::defaultReadRange();
    QCOMPARE(range.startRegister, static_cast<quint16>(101));

    QVERIFY(DcdcModbusRegisterMap::containsAddress(defs, 101));
    QVERIFY(!DcdcModbusRegisterMap::containsAddress(defs, 200));
}

QTEST_MAIN(DcdcModbusRegisterMapTest)
#include "DcdcModbusRegisterMapTest.moc"

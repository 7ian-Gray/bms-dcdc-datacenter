#include "protocol/BmsCommand.h"

#include <QtTest/QtTest>

#include <limits>

class BmsCommandEncoderTest : public QObject
{
    Q_OBJECT

private slots:
    void encodesDemoCommandWithExpectedRawValuesAndBytes();
    void encodesMinimumBoundaryValues();
    void encodesMaximumBoundaryValues();
    void rejectsEngineeringValueAboveRange();
    void rejectsRawOverflow();
    void rejectsOverlappingBitFields();
    void rejectsInvalidEnumValue();
    void rejectsMissingRequiredParameter();
    void rejectsInvalidCanIdAndDlc();
    void encodedCommandIsIndependentSnapshot();
    void stableFingerprintForSameInput();
    void differentCanDataChangesFingerprint();
    void rejectsZeroScale();
    void rejectsZeroBitLength();
    void rejectsBitFieldExceedingDlc();
    void rejectsNegativeValueForUnsignedField();
    void rejectsNonFiniteEngineeringValue();
    void byteOrderChangesPayloadLayout();
    void rejectsBitLengthBeyondExactDoubleRange();
    void rejectsMissingRequestCommandId();
    void rejectsMismatchedRequestCommandId();
    void defaultEncodeResultIsNotOk();
    void differentChannelChangesFingerprint();

private:
    static BmsCommandDefinition singleParameterCommand(BmsParameterDefinition parameter, int dlc);
    static BmsCommandRequest singleParameterRequest(const QString &parameterId,
                                                    const QVariant &value);
    static BmsCommandDefinition demoCommand();
    static BmsCommandRequest demoRequest(double voltage, double current, bool enabled, int mode);
    static BmsCommandDefinition rawOverflowCommand();
    static bool hasIssue(const ValidationResult &result, const QString &code);
};

BmsCommandDefinition BmsCommandEncoderTest::demoCommand()
{
    BmsCommandDefinition definition;
    const bool found =
        BmsCommandCatalog::findDemoCommand(QStringLiteral("demo_mock_bms_command"), &definition);
    Q_ASSERT(found);
    return definition;
}

BmsCommandRequest BmsCommandEncoderTest::demoRequest(double voltage,
                                                     double current,
                                                     bool enabled,
                                                     int mode)
{
    BmsCommandRequest request;
    request.commandId = QStringLiteral("demo_mock_bms_command");
    request.engineeringParameters.insert(QStringLiteral("demo_voltage_v"), voltage);
    request.engineeringParameters.insert(QStringLiteral("demo_current_a"), current);
    request.engineeringParameters.insert(QStringLiteral("demo_enable"), enabled);
    request.engineeringParameters.insert(QStringLiteral("demo_mode"), mode);
    return request;
}

BmsCommandDefinition BmsCommandEncoderTest::rawOverflowCommand()
{
    BmsCommandDefinition definition;
    definition.commandId = QStringLiteral("demo_raw_overflow");
    definition.category = BmsCommandCategory::Demo;
    definition.safetyLevel = BmsSafetyLevel::DemoOnly;
    definition.demoOnly = true;
    definition.canId = 0x321u;
    definition.extended = false;
    definition.dlc = 1;

    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("too_wide_for_u8");
    parameter.valueType = BmsValueType::UnsignedInteger;
    parameter.minimum = 0.0;
    parameter.maximum = 1000.0;
    parameter.scale = 1.0;
    parameter.byteOffset = 0;
    parameter.bitOffset = 0;
    parameter.bitLength = 8;
    parameter.byteOrder = ByteOrder::LittleEndian;
    definition.parameters.append(parameter);
    return definition;
}

BmsCommandDefinition BmsCommandEncoderTest::singleParameterCommand(BmsParameterDefinition parameter,
                                                                   int dlc)
{
    BmsCommandDefinition definition;
    definition.commandId = QStringLiteral("demo_single_parameter");
    definition.category = BmsCommandCategory::Demo;
    definition.safetyLevel = BmsSafetyLevel::DemoOnly;
    definition.demoOnly = true;
    definition.canId = 0x321u;
    definition.extended = false;
    definition.dlc = dlc;
    definition.parameters.append(parameter);
    return definition;
}

BmsCommandRequest BmsCommandEncoderTest::singleParameterRequest(const QString &parameterId,
                                                                const QVariant &value)
{
    BmsCommandRequest request;
    request.commandId = QStringLiteral("demo_single_parameter");
    request.engineeringParameters.insert(parameterId, value);
    return request;
}

bool BmsCommandEncoderTest::hasIssue(const ValidationResult &result, const QString &code)
{
    for (const ValidationIssue &issue : result.issues) {
        if (issue.code == code) {
            return true;
        }
    }
    return false;
}

void BmsCommandEncoderTest::encodesDemoCommandWithExpectedRawValuesAndBytes()
{
    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(demoCommand(), demoRequest(1000.0, -12.3, true, 2));

    QVERIFY(result.ok());
    QCOMPARE(result.command.rawParameters.value(QStringLiteral("demo_voltage_v")).toULongLong(),
             10000ull);
    QCOMPARE(result.command.rawParameters.value(QStringLiteral("demo_current_a")).toLongLong(),
             -123ll);
    QCOMPARE(result.command.rawParameters.value(QStringLiteral("demo_enable")).toULongLong(), 1ull);
    QCOMPARE(result.command.rawParameters.value(QStringLiteral("demo_mode")).toULongLong(), 2ull);
    QCOMPARE(result.command.data, QByteArray::fromHex("1027ff8505000000"));
    QCOMPARE(result.command.frame.id, 0x18E50101u);
    QCOMPARE(result.command.frame.extended, true);
    QCOMPARE(result.command.frame.remote, false);
    QCOMPARE(result.command.frame.payload, QByteArray::fromHex("1027ff8505000000"));
    QCOMPARE(result.command.dlc, 8);
}

void BmsCommandEncoderTest::encodesMinimumBoundaryValues()
{
    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(demoCommand(), demoRequest(0.0, -500.0, false, 0));

    QVERIFY(result.ok());
    QCOMPARE(result.command.rawParameters.value(QStringLiteral("demo_voltage_v")).toULongLong(),
             0ull);
    QCOMPARE(result.command.rawParameters.value(QStringLiteral("demo_current_a")).toLongLong(),
             -5000ll);
    QCOMPARE(result.command.data, QByteArray::fromHex("0000ec7800000000"));
}

void BmsCommandEncoderTest::encodesMaximumBoundaryValues()
{
    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(demoCommand(), demoRequest(1000.0, 500.0, true, 3));

    QVERIFY(result.ok());
    QCOMPARE(result.command.rawParameters.value(QStringLiteral("demo_voltage_v")).toULongLong(),
             10000ull);
    QCOMPARE(result.command.rawParameters.value(QStringLiteral("demo_current_a")).toLongLong(),
             5000ll);
    QCOMPARE(result.command.data, QByteArray::fromHex("1027138807000000"));
}

void BmsCommandEncoderTest::rejectsEngineeringValueAboveRange()
{
    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(demoCommand(), demoRequest(1000.1, 0.0, true, 1));

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("EngineeringValueAboveMaximum")));
}

void BmsCommandEncoderTest::rejectsRawOverflow()
{
    BmsCommandRequest request;
    request.commandId = QStringLiteral("demo_raw_overflow");
    request.engineeringParameters.insert(QStringLiteral("too_wide_for_u8"), 300.0);

    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(rawOverflowCommand(), request);

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("RawValueOverflow")));
}

void BmsCommandEncoderTest::rejectsOverlappingBitFields()
{
    BmsCommandDefinition definition = demoCommand();
    BmsParameterDefinition overlap = definition.parameters.at(2);
    overlap.parameterId = QStringLiteral("demo_overlap");
    overlap.bitOffset = 2;
    definition.parameters.append(overlap);

    BmsCommandRequest request = demoRequest(100.0, 10.0, true, 1);
    request.engineeringParameters.insert(QStringLiteral("demo_overlap"), true);

    const BmsCommandEncodeResult result = BmsCommandEncoder().encode(definition, request);

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("BitFieldOverlap")));
}

void BmsCommandEncoderTest::rejectsInvalidEnumValue()
{
    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(demoCommand(), demoRequest(100.0, 10.0, true, 4));

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("InvalidEnum")));
}

void BmsCommandEncoderTest::rejectsMissingRequiredParameter()
{
    BmsCommandRequest request = demoRequest(100.0, 10.0, true, 1);
    request.engineeringParameters.remove(QStringLiteral("demo_current_a"));

    const BmsCommandEncodeResult result = BmsCommandEncoder().encode(demoCommand(), request);

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("MissingRequiredParameter")));
}

void BmsCommandEncoderTest::rejectsInvalidCanIdAndDlc()
{
    BmsCommandDefinition definition = demoCommand();
    definition.extended = false;
    definition.canId = 0x800u;
    definition.dlc = 9;

    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(definition, demoRequest(100.0, 10.0, true, 1));

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("InvalidCanId")));
    QVERIFY(hasIssue(result.validation, QStringLiteral("InvalidDlc")));
}

void BmsCommandEncoderTest::encodedCommandIsIndependentSnapshot()
{
    BmsCommandRequest request = demoRequest(123.4, -12.3, true, 2);
    const BmsCommandEncodeResult result = BmsCommandEncoder().encode(demoCommand(), request);
    QVERIFY(result.ok());

    request.engineeringParameters[QStringLiteral("demo_voltage_v")] = 0.0;
    request.engineeringParameters[QStringLiteral("demo_current_a")] = 0.0;

    QCOMPARE(result.command.engineeringParameters.value(QStringLiteral("demo_voltage_v")).toDouble(),
             123.4);
    QCOMPARE(result.command.engineeringParameters.value(QStringLiteral("demo_current_a")).toDouble(),
             -12.3);
    QCOMPARE(result.command.rawParameters.value(QStringLiteral("demo_voltage_v")).toULongLong(),
             1234ull);
    QCOMPARE(result.command.frame.payload, QByteArray::fromHex("d204ff8505000000"));
}

void BmsCommandEncoderTest::stableFingerprintForSameInput()
{
    const BmsCommandDefinition definition = demoCommand();
    const BmsCommandRequest request = demoRequest(100.0, -10.0, true, 1);

    const BmsCommandEncodeResult first = BmsCommandEncoder().encode(definition, request);
    const BmsCommandEncodeResult second = BmsCommandEncoder().encode(definition, request);

    QVERIFY(first.ok());
    QVERIFY(second.ok());
    QVERIFY(!first.command.fingerprint.isEmpty());
    QCOMPARE(first.command.fingerprint, second.command.fingerprint);
}

void BmsCommandEncoderTest::differentCanDataChangesFingerprint()
{
    const BmsCommandDefinition definition = demoCommand();

    const BmsCommandEncodeResult first =
        BmsCommandEncoder().encode(definition, demoRequest(100.0, -10.0, true, 1));
    const BmsCommandEncodeResult second =
        BmsCommandEncoder().encode(definition, demoRequest(100.1, -10.0, true, 1));

    QVERIFY(first.ok());
    QVERIFY(second.ok());
    QVERIFY(first.command.data != second.command.data);
    QVERIFY(first.command.fingerprint != second.command.fingerprint);
}

void BmsCommandEncoderTest::rejectsZeroScale()
{
    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("zero_scale");
    parameter.valueType = BmsValueType::UnsignedInteger;
    parameter.scale = 0.0;
    parameter.byteOffset = 0;
    parameter.bitOffset = 0;
    parameter.bitLength = 8;

    const BmsCommandRequest request =
        singleParameterRequest(QStringLiteral("zero_scale"), 1.0);

    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(singleParameterCommand(parameter, 1), request);

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("ZeroScale")));
}

void BmsCommandEncoderTest::rejectsZeroBitLength()
{
    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("zero_bits");
    parameter.valueType = BmsValueType::UnsignedInteger;
    parameter.scale = 1.0;
    parameter.byteOffset = 0;
    parameter.bitOffset = 0;
    parameter.bitLength = 0;

    const BmsCommandRequest request =
        singleParameterRequest(QStringLiteral("zero_bits"), 0.0);

    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(singleParameterCommand(parameter, 1), request);

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("InvalidBitLength")));
}

void BmsCommandEncoderTest::rejectsBitFieldExceedingDlc()
{
    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("exceeds_dlc");
    parameter.valueType = BmsValueType::UnsignedInteger;
    parameter.scale = 1.0;
    parameter.byteOffset = 1;
    parameter.bitOffset = 4;
    parameter.bitLength = 8;

    const BmsCommandRequest request =
        singleParameterRequest(QStringLiteral("exceeds_dlc"), 0.0);

    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(singleParameterCommand(parameter, 2), request);

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("BitFieldOutOfRange")));
}

void BmsCommandEncoderTest::rejectsNegativeValueForUnsignedField()
{
    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("unsigned_only");
    parameter.valueType = BmsValueType::UnsignedInteger;
    parameter.scale = 1.0;
    parameter.byteOffset = 0;
    parameter.bitOffset = 0;
    parameter.bitLength = 8;

    const BmsCommandRequest request =
        singleParameterRequest(QStringLiteral("unsigned_only"), -1.0);

    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(singleParameterCommand(parameter, 1), request);

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("RawValueOverflow")));
}

void BmsCommandEncoderTest::rejectsNonFiniteEngineeringValue()
{
    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("finite_only");
    parameter.valueType = BmsValueType::Physical;
    parameter.scale = 0.1;
    parameter.byteOffset = 0;
    parameter.bitOffset = 0;
    parameter.bitLength = 16;

    const BmsCommandDefinition definition = singleParameterCommand(parameter, 2);

    const BmsCommandRequest nanRequest = singleParameterRequest(
        QStringLiteral("finite_only"), std::numeric_limits<double>::quiet_NaN());
    const BmsCommandEncodeResult nanResult = BmsCommandEncoder().encode(definition, nanRequest);
    QVERIFY(!nanResult.ok());
    QVERIFY(hasIssue(nanResult.validation, QStringLiteral("InvalidNumber")));

    const BmsCommandRequest infRequest = singleParameterRequest(
        QStringLiteral("finite_only"), std::numeric_limits<double>::infinity());
    const BmsCommandEncodeResult infResult = BmsCommandEncoder().encode(definition, infRequest);
    QVERIFY(!infResult.ok());
    QVERIFY(hasIssue(infResult.validation, QStringLiteral("InvalidNumber")));
}

void BmsCommandEncoderTest::byteOrderChangesPayloadLayout()
{
    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("word_value");
    parameter.valueType = BmsValueType::UnsignedInteger;
    parameter.scale = 1.0;
    parameter.byteOffset = 0;
    parameter.bitOffset = 0;
    parameter.bitLength = 16;

    const BmsCommandRequest request =
        singleParameterRequest(QStringLiteral("word_value"), 0x1234);

    parameter.byteOrder = ByteOrder::LittleEndian;
    const BmsCommandEncodeResult littleEndian =
        BmsCommandEncoder().encode(singleParameterCommand(parameter, 2), request);
    QVERIFY(littleEndian.ok());
    QCOMPARE(littleEndian.command.data, QByteArray::fromHex("3412"));

    parameter.byteOrder = ByteOrder::BigEndian;
    const BmsCommandEncodeResult bigEndian =
        BmsCommandEncoder().encode(singleParameterCommand(parameter, 2), request);
    QVERIFY(bigEndian.ok());
    QCOMPARE(bigEndian.command.data, QByteArray::fromHex("1234"));
}

void BmsCommandEncoderTest::rejectsBitLengthBeyondExactDoubleRange()
{
    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("too_wide_for_double");
    parameter.valueType = BmsValueType::UnsignedInteger;
    parameter.scale = 1.0;
    parameter.byteOffset = 0;
    parameter.bitOffset = 0;
    parameter.bitLength = 54;

    const BmsCommandEncodeResult result =
        BmsCommandEncoder().encode(singleParameterCommand(parameter, 8),
                                   singleParameterRequest(QStringLiteral("too_wide_for_double"),
                                                          0.0));

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("InvalidBitLength")));
}

void BmsCommandEncoderTest::rejectsMissingRequestCommandId()
{
    BmsCommandRequest request = demoRequest(100.0, 10.0, true, 1);
    request.commandId.clear();

    const BmsCommandEncodeResult result = BmsCommandEncoder().encode(demoCommand(), request);

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("MissingRequestCommandId")));
}

void BmsCommandEncoderTest::rejectsMismatchedRequestCommandId()
{
    BmsCommandRequest request = demoRequest(100.0, 10.0, true, 1);
    request.commandId = QStringLiteral("some_other_command");

    const BmsCommandEncodeResult result = BmsCommandEncoder().encode(demoCommand(), request);

    QVERIFY(!result.ok());
    QVERIFY(hasIssue(result.validation, QStringLiteral("CommandIdMismatch")));
}

void BmsCommandEncoderTest::defaultEncodeResultIsNotOk()
{
    const BmsCommandEncodeResult result;

    QVERIFY(!result.ok());
}

void BmsCommandEncoderTest::differentChannelChangesFingerprint()
{
    BmsCommandDefinition definition = demoCommand();
    const BmsCommandRequest request = demoRequest(100.0, -10.0, true, 1);

    definition.channel = 0;
    const BmsCommandEncodeResult first = BmsCommandEncoder().encode(definition, request);

    definition.channel = 1;
    const BmsCommandEncodeResult second = BmsCommandEncoder().encode(definition, request);

    QVERIFY(first.ok());
    QVERIFY(second.ok());
    QCOMPARE(first.command.data, second.command.data);
    QVERIFY(first.command.fingerprint != second.command.fingerprint);
}

QTEST_MAIN(BmsCommandEncoderTest)

#include "BmsCommandEncoderTest.moc"

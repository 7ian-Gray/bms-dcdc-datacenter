#include "protocol/BmsCommand.h"

#include <QCryptographicHash>
#include <QMetaType>
#include <QSet>

#include <cmath>
#include <limits>

namespace
{
constexpr int ClassicCanMaxDlc = 8;
constexpr quint32 StandardCanMaxId = 0x7FFu;
constexpr quint32 ExtendedCanMaxId = 0x1FFFFFFFu;

struct RawValue
{
    bool signedValue = false;
    qint64 signedRaw = 0;
    quint64 encodedRaw = 0;
};

bool isSignedRaw(const BmsParameterDefinition &parameter)
{
    return parameter.valueType == BmsValueType::SignedInteger || parameter.signedRaw;
}

quint64 maxUnsignedRaw(int bitLength)
{
    if (bitLength >= 64) {
        return std::numeric_limits<quint64>::max();
    }
    return (quint64{1} << bitLength) - 1u;
}

qint64 minSignedRaw(int bitLength)
{
    if (bitLength >= 64) {
        return std::numeric_limits<qint64>::min();
    }
    return -(qint64{1} << (bitLength - 1));
}

qint64 maxSignedRaw(int bitLength)
{
    if (bitLength >= 64) {
        return std::numeric_limits<qint64>::max();
    }
    return (qint64{1} << (bitLength - 1)) - 1;
}

void appendError(ValidationResult *result,
                 const QString &code,
                 const QString &parameterId,
                 const QString &message)
{
    result->addIssue(ValidationSeverity::Error, code, parameterId, message);
}

bool readEngineeringDouble(const BmsParameterDefinition &parameter,
                           const QVariant &value,
                           double *engineeringValue,
                           ValidationResult *result)
{
    if (parameter.valueType == BmsValueType::Boolean) {
        const int typeId = value.metaType().id();
        if (typeId == QMetaType::Bool) {
            *engineeringValue = value.toBool() ? 1.0 : 0.0;
            return true;
        }

        bool ok = false;
        const double numericValue = value.toDouble(&ok);
        if (ok && (numericValue == 0.0 || numericValue == 1.0)) {
            *engineeringValue = numericValue;
            return true;
        }

        appendError(result,
                    QStringLiteral("InvalidBoolean"),
                    parameter.parameterId,
                    QStringLiteral("Boolean parameter must be true, false, 0, or 1"));
        return false;
    }

    if (parameter.valueType == BmsValueType::Enum) {
        if (value.metaType().id() == QMetaType::QString) {
            const QString text = value.toString();
            for (auto it = parameter.enumValues.cbegin(); it != parameter.enumValues.cend(); ++it) {
                if (QString::compare(text, it.value(), Qt::CaseInsensitive) == 0) {
                    *engineeringValue = static_cast<double>(it.key());
                    return true;
                }
            }
        }

        bool ok = false;
        const qint64 enumValue = value.toLongLong(&ok);
        if (ok) {
            *engineeringValue = static_cast<double>(enumValue);
            return true;
        }

        appendError(result,
                    QStringLiteral("InvalidEnum"),
                    parameter.parameterId,
                    QStringLiteral("Enum parameter value is not a known label or integer"));
        return false;
    }

    bool ok = false;
    const double numericValue = value.toDouble(&ok);
    if (!ok || !std::isfinite(numericValue)) {
        appendError(result,
                    QStringLiteral("InvalidNumber"),
                    parameter.parameterId,
                    QStringLiteral("Numeric parameter must be a finite number"));
        return false;
    }

    *engineeringValue = numericValue;
    return true;
}

bool convertToRawValue(const BmsParameterDefinition &parameter,
                       const QVariant &value,
                       RawValue *rawValue,
                       ValidationResult *result)
{
    double engineeringValue = 0.0;
    if (!readEngineeringDouble(parameter, value, &engineeringValue, result)) {
        return false;
    }

    if (parameter.minimum.has_value() && engineeringValue < *parameter.minimum) {
        appendError(result,
                    QStringLiteral("EngineeringValueBelowMinimum"),
                    parameter.parameterId,
                    QStringLiteral("Engineering value is below the allowed minimum"));
        return false;
    }

    if (parameter.maximum.has_value() && engineeringValue > *parameter.maximum) {
        appendError(result,
                    QStringLiteral("EngineeringValueAboveMaximum"),
                    parameter.parameterId,
                    QStringLiteral("Engineering value is above the allowed maximum"));
        return false;
    }

    if (qFuzzyIsNull(parameter.scale)) {
        appendError(result,
                    QStringLiteral("ZeroScale"),
                    parameter.parameterId,
                    QStringLiteral("Scale must not be zero"));
        return false;
    }

    const double roundedRaw = std::round((engineeringValue - parameter.offset) / parameter.scale);
    if (!std::isfinite(roundedRaw)) {
        appendError(result,
                    QStringLiteral("RawValueNotFinite"),
                    parameter.parameterId,
                    QStringLiteral("Raw value is not finite"));
        return false;
    }

    if (parameter.valueType == BmsValueType::Enum) {
        const qint64 enumRaw = static_cast<qint64>(roundedRaw);
        if (!parameter.enumValues.contains(enumRaw)) {
            appendError(result,
                        QStringLiteral("InvalidEnum"),
                        parameter.parameterId,
                        QStringLiteral("Enum parameter value is not defined"));
            return false;
        }
    }

    if (isSignedRaw(parameter)) {
        const double minRaw = static_cast<double>(minSignedRaw(parameter.bitLength));
        const double maxRaw = static_cast<double>(maxSignedRaw(parameter.bitLength));
        if (roundedRaw < minRaw || roundedRaw > maxRaw) {
            appendError(result,
                        QStringLiteral("RawValueOverflow"),
                        parameter.parameterId,
                        QStringLiteral("Signed raw value does not fit in the configured bit length"));
            return false;
        }

        rawValue->signedValue = true;
        rawValue->signedRaw = static_cast<qint64>(roundedRaw);
        if (rawValue->signedRaw < 0) {
            rawValue->encodedRaw = parameter.bitLength >= 64
                                       ? static_cast<quint64>(rawValue->signedRaw)
                                       : (quint64{1} << parameter.bitLength) +
                                             static_cast<quint64>(rawValue->signedRaw);
        } else {
            rawValue->encodedRaw = static_cast<quint64>(rawValue->signedRaw);
        }
        return true;
    }

    const double maxRaw = static_cast<double>(maxUnsignedRaw(parameter.bitLength));
    if (roundedRaw < 0.0 || roundedRaw > maxRaw) {
        appendError(result,
                    QStringLiteral("RawValueOverflow"),
                    parameter.parameterId,
                    QStringLiteral("Unsigned raw value does not fit in the configured bit length"));
        return false;
    }

    rawValue->signedValue = false;
    rawValue->encodedRaw = static_cast<quint64>(roundedRaw);
    rawValue->signedRaw = static_cast<qint64>(rawValue->encodedRaw);
    return true;
}

QList<int> fieldBitPositions(const BmsParameterDefinition &parameter, int dlc, bool *ok)
{
    QList<int> positions;
    *ok = false;

    if (dlc < 0 || dlc > ClassicCanMaxDlc ||
        parameter.byteOffset < 0 ||
        parameter.bitOffset < 0 ||
        parameter.bitOffset > 7 ||
        parameter.bitLength <= 0 ||
        parameter.bitLength > 64) {
        return positions;
    }

    const int totalBits = dlc * 8;
    const int startBit = parameter.byteOffset * 8 + parameter.bitOffset;
    if (startBit < 0 || startBit + parameter.bitLength > totalBits) {
        return positions;
    }

    for (int i = 0; i < parameter.bitLength; ++i) {
        int byteIndex = 0;
        int bitIndex = 0;
        if (parameter.byteOrder == ByteOrder::LittleEndian) {
            const int linearBit = startBit + i;
            byteIndex = linearBit / 8;
            bitIndex = linearBit % 8;
        } else {
            const int linearBit = startBit + i;
            byteIndex = linearBit / 8;
            bitIndex = 7 - (linearBit % 8);
        }

        if (byteIndex < 0 || byteIndex >= dlc || bitIndex < 0 || bitIndex > 7) {
            return positions;
        }
        positions.append(byteIndex * 8 + bitIndex);
    }

    *ok = true;
    return positions;
}

void setPayloadBit(QByteArray *data, int absoluteBit, bool set)
{
    const int byteIndex = absoluteBit / 8;
    const int bitIndex = absoluteBit % 8;
    auto byte = static_cast<quint8>(data->at(byteIndex));
    const quint8 mask = static_cast<quint8>(1u << bitIndex);
    byte = set ? static_cast<quint8>(byte | mask) : static_cast<quint8>(byte & ~mask);
    (*data)[byteIndex] = static_cast<char>(byte);
}

void encodeRawValue(const BmsParameterDefinition &parameter,
                    const RawValue &rawValue,
                    QByteArray *data)
{
    bool positionsOk = false;
    const QList<int> positions = fieldBitPositions(parameter, data->size(), &positionsOk);
    if (!positionsOk) {
        return;
    }

    for (int fieldIndex = 0; fieldIndex < parameter.bitLength; ++fieldIndex) {
        const int sourceBit = parameter.byteOrder == ByteOrder::LittleEndian
                                  ? fieldIndex
                                  : parameter.bitLength - 1 - fieldIndex;
        const bool bitSet = ((rawValue.encodedRaw >> sourceBit) & 0x1u) != 0;
        setPayloadBit(data, positions.at(fieldIndex), bitSet);
    }
}

const BmsParameterDefinition *findParameter(const BmsCommandDefinition &definition,
                                            const QString &parameterId)
{
    for (const BmsParameterDefinition &parameter : definition.parameters) {
        if (parameter.parameterId == parameterId) {
            return &parameter;
        }
    }
    return nullptr;
}

void addFingerprintText(QCryptographicHash *hash, const QString &text)
{
    static const QByteArray separator(1, '\0');
    hash->addData(text.toUtf8());
    hash->addData(separator);
}

QString buildFingerprint(const EncodedBmsCommand &command)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    addFingerprintText(&hash, command.commandId);
    addFingerprintText(&hash, QString::number(command.canId));
    addFingerprintText(&hash, command.extended ? QStringLiteral("extended")
                                               : QStringLiteral("standard"));
    addFingerprintText(&hash, QString::number(command.dlc));
    addFingerprintText(&hash, QString::fromLatin1(command.data.toHex()));
    for (auto it = command.rawParameters.cbegin(); it != command.rawParameters.cend(); ++it) {
        addFingerprintText(&hash, it.key());
        addFingerprintText(&hash, it.value().toString());
    }
    return QString::fromLatin1(hash.result().toHex());
}

BmsParameterDefinition demoVoltageParameter()
{
    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("demo_voltage_v");
    parameter.displayName = QStringLiteral("Demo voltage");
    parameter.unit = QStringLiteral("V");
    parameter.valueType = BmsValueType::UnsignedInteger;
    parameter.minimum = 0.0;
    parameter.maximum = 1000.0;
    parameter.scale = 0.1;
    parameter.offset = 0.0;
    parameter.byteOffset = 0;
    parameter.bitOffset = 0;
    parameter.bitLength = 16;
    parameter.byteOrder = ByteOrder::LittleEndian;
    return parameter;
}

BmsParameterDefinition demoCurrentParameter()
{
    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("demo_current_a");
    parameter.displayName = QStringLiteral("Demo current");
    parameter.unit = QStringLiteral("A");
    parameter.valueType = BmsValueType::SignedInteger;
    parameter.minimum = -500.0;
    parameter.maximum = 500.0;
    parameter.scale = 0.1;
    parameter.offset = 0.0;
    parameter.byteOffset = 2;
    parameter.bitOffset = 0;
    parameter.bitLength = 16;
    parameter.byteOrder = ByteOrder::BigEndian;
    return parameter;
}

BmsParameterDefinition demoEnableParameter()
{
    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("demo_enable");
    parameter.displayName = QStringLiteral("Demo enable");
    parameter.valueType = BmsValueType::Boolean;
    parameter.minimum = 0.0;
    parameter.maximum = 1.0;
    parameter.scale = 1.0;
    parameter.offset = 0.0;
    parameter.byteOffset = 4;
    parameter.bitOffset = 0;
    parameter.bitLength = 1;
    parameter.byteOrder = ByteOrder::LittleEndian;
    return parameter;
}

BmsParameterDefinition demoModeParameter()
{
    BmsParameterDefinition parameter;
    parameter.parameterId = QStringLiteral("demo_mode");
    parameter.displayName = QStringLiteral("Demo mode");
    parameter.valueType = BmsValueType::Enum;
    parameter.scale = 1.0;
    parameter.offset = 0.0;
    parameter.byteOffset = 4;
    parameter.bitOffset = 1;
    parameter.bitLength = 2;
    parameter.byteOrder = ByteOrder::LittleEndian;
    parameter.enumValues.insert(0, QStringLiteral("Off"));
    parameter.enumValues.insert(1, QStringLiteral("Charge"));
    parameter.enumValues.insert(2, QStringLiteral("Discharge"));
    parameter.enumValues.insert(3, QStringLiteral("Standby"));
    return parameter;
}
} // namespace

bool ValidationResult::hasErrors() const
{
    for (const ValidationIssue &issue : issues) {
        if (issue.severity == ValidationSeverity::Error) {
            return true;
        }
    }
    return false;
}

bool ValidationResult::isValid() const
{
    return !hasErrors();
}

void ValidationResult::addIssue(ValidationSeverity severity,
                                const QString &code,
                                const QString &parameterId,
                                const QString &message)
{
    ValidationIssue issue;
    issue.severity = severity;
    issue.code = code;
    issue.parameterId = parameterId;
    issue.message = message;
    issues.append(issue);
}

QList<BmsCommandDefinition> BmsCommandCatalog::demoCommands()
{
    BmsCommandDefinition command;
    command.commandId = QStringLiteral("demo_mock_bms_command");
    command.displayName = QStringLiteral("Demo/Mock BMS command");
    command.description =
        QStringLiteral("Demo-only command definition for command validation and CAN encoding tests.");
    command.category = BmsCommandCategory::Demo;
    command.safetyLevel = BmsSafetyLevel::DemoOnly;
    command.demoOnly = true;
    // Fictitious demo ID; not taken from any vendor protocol and must never be sent to real devices.
    command.canId = 0x18E50101u;
    command.extended = true;
    command.channel = 0;
    command.dlc = 8;
    command.parameters = {
        demoVoltageParameter(),
        demoCurrentParameter(),
        demoEnableParameter(),
        demoModeParameter(),
    };
    return {command};
}

bool BmsCommandCatalog::findDemoCommand(const QString &commandId,
                                        BmsCommandDefinition *definition)
{
    const QList<BmsCommandDefinition> commands = demoCommands();
    for (const BmsCommandDefinition &command : commands) {
        if (command.commandId == commandId) {
            if (definition != nullptr) {
                *definition = command;
            }
            return true;
        }
    }
    return false;
}

ValidationResult BmsCommandValidator::validate(const BmsCommandDefinition &definition,
                                               const BmsCommandRequest &request) const
{
    ValidationResult result;

    if (definition.commandId.isEmpty()) {
        appendError(&result,
                    QStringLiteral("MissingCommandId"),
                    QString(),
                    QStringLiteral("Command definition id must not be empty"));
    }

    if (!request.commandId.isEmpty() && request.commandId != definition.commandId) {
        appendError(&result,
                    QStringLiteral("CommandIdMismatch"),
                    QString(),
                    QStringLiteral("Request command id does not match the command definition"));
    }

    if (definition.dlc < 0 || definition.dlc > ClassicCanMaxDlc) {
        appendError(&result,
                    QStringLiteral("InvalidDlc"),
                    QString(),
                    QStringLiteral("Classic CAN DLC must be in the range 0..8"));
    }

    const quint32 maxCanId = definition.extended ? ExtendedCanMaxId : StandardCanMaxId;
    if (definition.canId > maxCanId) {
        appendError(&result,
                    QStringLiteral("InvalidCanId"),
                    QString(),
                    definition.extended
                        ? QStringLiteral("Extended CAN ID must be <= 0x1FFFFFFF")
                        : QStringLiteral("Standard CAN ID must be <= 0x7FF"));
    }

    QSet<QString> parameterIds;
    QSet<int> occupiedBits;
    for (const BmsParameterDefinition &parameter : definition.parameters) {
        if (parameter.parameterId.isEmpty()) {
            appendError(&result,
                        QStringLiteral("MissingParameterId"),
                        QString(),
                        QStringLiteral("Parameter id must not be empty"));
        } else if (parameterIds.contains(parameter.parameterId)) {
            appendError(&result,
                        QStringLiteral("DuplicateParameterId"),
                        parameter.parameterId,
                        QStringLiteral("Parameter id must be unique inside a command"));
        } else {
            parameterIds.insert(parameter.parameterId);
        }

        if (parameter.minimum.has_value() &&
            parameter.maximum.has_value() &&
            *parameter.minimum > *parameter.maximum) {
            appendError(&result,
                        QStringLiteral("InvalidEngineeringRange"),
                        parameter.parameterId,
                        QStringLiteral("Parameter minimum must be <= maximum"));
        }

        if (qFuzzyIsNull(parameter.scale)) {
            appendError(&result,
                        QStringLiteral("ZeroScale"),
                        parameter.parameterId,
                        QStringLiteral("Scale must not be zero"));
        }

        if (parameter.bitLength <= 0 || parameter.bitLength > 64) {
            appendError(&result,
                        QStringLiteral("InvalidBitLength"),
                        parameter.parameterId,
                        QStringLiteral("Bit length must be in the range 1..64"));
        }

        if (parameter.byteOffset < 0 || parameter.bitOffset < 0 || parameter.bitOffset > 7) {
            appendError(&result,
                        QStringLiteral("InvalidBitOffset"),
                        parameter.parameterId,
                        QStringLiteral("byteOffset must be >= 0 and bitOffset must be in 0..7"));
        }

        if (parameter.valueType == BmsValueType::Boolean && parameter.bitLength != 1) {
            appendError(&result,
                        QStringLiteral("InvalidBooleanBitLength"),
                        parameter.parameterId,
                        QStringLiteral("Boolean parameters must use a 1-bit field"));
        }

        if (parameter.valueType == BmsValueType::Enum && parameter.enumValues.isEmpty()) {
            appendError(&result,
                        QStringLiteral("MissingEnumValues"),
                        parameter.parameterId,
                        QStringLiteral("Enum parameters must define allowed values"));
        }

        if (definition.dlc >= 0 && definition.dlc <= ClassicCanMaxDlc) {
            bool positionsOk = false;
            const QList<int> positions = fieldBitPositions(parameter, definition.dlc, &positionsOk);
            if (!positionsOk) {
                appendError(&result,
                            QStringLiteral("BitFieldOutOfRange"),
                            parameter.parameterId,
                            QStringLiteral("Parameter bit field exceeds the CAN data area"));
            } else {
                for (const int bit : positions) {
                    if (occupiedBits.contains(bit)) {
                        appendError(&result,
                                    QStringLiteral("BitFieldOverlap"),
                                    parameter.parameterId,
                                    QStringLiteral("Parameter bit field overlaps another parameter"));
                        break;
                    }
                }
                for (const int bit : positions) {
                    occupiedBits.insert(bit);
                }
            }
        }

        if (parameter.required && !request.engineeringParameters.contains(parameter.parameterId)) {
            appendError(&result,
                        QStringLiteral("MissingRequiredParameter"),
                        parameter.parameterId,
                        QStringLiteral("Required parameter is missing"));
        }
    }

    for (auto it = request.engineeringParameters.cbegin();
         it != request.engineeringParameters.cend();
         ++it) {
        const BmsParameterDefinition *parameter = findParameter(definition, it.key());
        if (parameter == nullptr) {
            appendError(&result,
                        QStringLiteral("UnknownParameter"),
                        it.key(),
                        QStringLiteral("Request contains a parameter not defined by the command"));
            continue;
        }

        if (parameter->bitLength > 0 && parameter->bitLength <= 64) {
            RawValue rawValue;
            convertToRawValue(*parameter, it.value(), &rawValue, &result);
        }
    }

    return result;
}

bool BmsCommandEncodeResult::ok() const
{
    return validation.isValid();
}

BmsCommandEncodeResult BmsCommandEncoder::encode(const BmsCommandDefinition &definition,
                                                 const BmsCommandRequest &request) const
{
    BmsCommandEncodeResult result;
    const BmsCommandValidator validator;
    result.validation = validator.validate(definition, request);
    if (!result.validation.isValid()) {
        return result;
    }

    EncodedBmsCommand encoded;
    encoded.commandId = definition.commandId;
    encoded.engineeringParameters = request.engineeringParameters;
    encoded.canId = definition.canId;
    encoded.extended = definition.extended;
    encoded.channel = definition.channel;
    encoded.dlc = definition.dlc;
    encoded.data = QByteArray(definition.dlc, '\0');
    encoded.safetyLevel = definition.safetyLevel;
    encoded.encodedAtUtc = QDateTime::currentDateTimeUtc();

    for (const BmsParameterDefinition &parameter : definition.parameters) {
        if (!request.engineeringParameters.contains(parameter.parameterId)) {
            continue;
        }

        const QVariant engineeringValue = request.engineeringParameters.value(parameter.parameterId);
        RawValue rawValue;
        convertToRawValue(parameter, engineeringValue, &rawValue, &result.validation);
        if (!result.validation.isValid()) {
            return result;
        }

        encoded.rawParameters.insert(parameter.parameterId,
                                     rawValue.signedValue
                                         ? QVariant::fromValue(rawValue.signedRaw)
                                         : QVariant::fromValue(rawValue.encodedRaw));
        encodeRawValue(parameter, rawValue, &encoded.data);
    }

    encoded.frame.id = encoded.canId;
    encoded.frame.channel = encoded.channel;
    encoded.frame.extended = encoded.extended;
    encoded.frame.remote = false;
    encoded.frame.timestampUs =
        static_cast<quint64>(encoded.encodedAtUtc.toMSecsSinceEpoch()) * 1000u;
    encoded.frame.payload = encoded.data;
    encoded.frame.direction = CanFrameDirection::Tx;
    encoded.fingerprint = buildFingerprint(encoded);

    result.command = encoded;
    return result;
}

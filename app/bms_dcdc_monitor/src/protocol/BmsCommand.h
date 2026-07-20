#pragma once

#include "communication/CanFrame.h"

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QtGlobal>

#include <optional>

enum class BmsCommandCategory : quint8
{
    Demo = 0,
    Control,
    Limits,
    Maintenance,
    Diagnostics
};

enum class BmsSafetyLevel : quint8
{
    DemoOnly = 0,
    Low,
    Medium,
    High,
    Critical
};

enum class BmsValueType : quint8
{
    Boolean = 0,
    UnsignedInteger,
    SignedInteger,
    Enum,
    Physical
};

enum class ByteOrder : quint8
{
    LittleEndian = 0,
    BigEndian
};

enum class ValidationSeverity : quint8
{
    Info = 0,
    Warning,
    Error
};

struct BmsParameterDefinition
{
    QString parameterId;
    QString displayName;
    QString unit;
    BmsValueType valueType = BmsValueType::UnsignedInteger;
    bool required = true;
    std::optional<double> minimum;
    std::optional<double> maximum;
    double scale = 1.0;
    double offset = 0.0;
    int byteOffset = 0;
    // Little-endian bitOffset is LSB-based; big-endian bitOffset is MSB-based.
    int bitOffset = 0;
    int bitLength = 0;
    ByteOrder byteOrder = ByteOrder::LittleEndian;
    bool signedRaw = false;
    QMap<qint64, QString> enumValues;
};

struct BmsCommandDefinition
{
    QString commandId;
    QString displayName;
    QString description;
    BmsCommandCategory category = BmsCommandCategory::Demo;
    BmsSafetyLevel safetyLevel = BmsSafetyLevel::DemoOnly;
    bool demoOnly = true;
    quint32 canId = 0;
    bool extended = true;
    quint8 channel = 0;
    int dlc = 8;
    QList<BmsParameterDefinition> parameters;
};

struct BmsCommandRequest
{
    QString commandId;
    QMap<QString, QVariant> engineeringParameters;
};

struct EncodedBmsCommand
{
    QString commandId;
    QMap<QString, QVariant> engineeringParameters;
    QMap<QString, QVariant> rawParameters;
    quint32 canId = 0;
    bool extended = true;
    quint8 channel = 0;
    int dlc = 0;
    QByteArray data;
    BmsSafetyLevel safetyLevel = BmsSafetyLevel::DemoOnly;
    QDateTime encodedAtUtc;
    QString fingerprint;
    CanFrame frame;
};

struct ValidationIssue
{
    ValidationSeverity severity = ValidationSeverity::Error;
    QString code;
    QString parameterId;
    QString message;
};

struct ValidationResult
{
    QList<ValidationIssue> issues;

    bool hasErrors() const;
    bool isValid() const;
    void addIssue(ValidationSeverity severity,
                  const QString &code,
                  const QString &parameterId,
                  const QString &message);
};

class BmsCommandCatalog
{
public:
    static QList<BmsCommandDefinition> demoCommands();
    static bool findDemoCommand(const QString &commandId, BmsCommandDefinition *definition);
};

class BmsCommandValidator
{
public:
    ValidationResult validate(const BmsCommandDefinition &definition,
                              const BmsCommandRequest &request) const;
};

struct BmsCommandEncodeResult
{
    ValidationResult validation;
    EncodedBmsCommand command;

    bool ok() const;
};

class BmsCommandEncoder
{
public:
    BmsCommandEncodeResult encode(const BmsCommandDefinition &definition,
                                  const BmsCommandRequest &request) const;
};

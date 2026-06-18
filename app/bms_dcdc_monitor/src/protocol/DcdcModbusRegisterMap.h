#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

enum class DcdcRegisterValueType
{
    Unsigned16,
    Signed16
};

struct DcdcRegisterDefinition
{
    quint16 address = 0;
    QString key;
    QString displayName;
    QString unit;
    double scale = 1.0;
    double offset = 0.0;
    DcdcRegisterValueType valueType = DcdcRegisterValueType::Unsigned16;
};

struct DcdcRegisterValue
{
    quint16 address = 0;
    QString key;
    QString displayName;
    QString unit;
    quint16 rawValue = 0;
    double scaledValue = 0.0;
};

struct DcdcRegisterDecodeResult
{
    bool ok = false;
    QVector<DcdcRegisterValue> values;
    QStringList errors;
};

struct DcdcModbusReadRange
{
    quint16 startRegister = 0;
    quint16 registerCount = 0;
};

class DcdcModbusRegisterMap
{
public:
    static constexpr quint16 DefaultStartRegister = 101;
    static constexpr quint16 DefaultEndRegister = 144;
    static constexpr quint16 DefaultRegisterCount =
        DefaultEndRegister - DefaultStartRegister + 1;
    static constexpr double DefaultScale = 0.1;

    static DcdcModbusReadRange defaultReadRange();

    static QVector<DcdcRegisterDefinition> defaultDefinitions();

    static bool validateDefinitions(const QVector<DcdcRegisterDefinition> &definitions,
                                    QStringList *errors = nullptr);

    static DcdcRegisterDecodeResult decodeHoldingRegisters(
        quint16 responseStartRegister,
        const QVector<quint16> &registers,
        const QVector<DcdcRegisterDefinition> &definitions = defaultDefinitions());

    static bool containsAddress(const QVector<DcdcRegisterDefinition> &definitions,
                                quint16 address);

private:
    static double scaleRawValue(quint16 rawValue,
                                const DcdcRegisterDefinition &definition);
};

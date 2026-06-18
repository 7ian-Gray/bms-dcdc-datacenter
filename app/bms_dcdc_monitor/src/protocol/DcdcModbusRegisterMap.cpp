#include "DcdcModbusRegisterMap.h"

DcdcModbusReadRange DcdcModbusRegisterMap::defaultReadRange()
{
    DcdcModbusReadRange range;
    range.startRegister = DefaultStartRegister;
    range.registerCount = DefaultRegisterCount;
    return range;
}

QVector<DcdcRegisterDefinition> DcdcModbusRegisterMap::defaultDefinitions()
{
    QVector<DcdcRegisterDefinition> defs;
    defs.reserve(DefaultRegisterCount);

    for (quint16 addr = DefaultStartRegister; addr <= DefaultEndRegister; ++addr)
    {
        DcdcRegisterDefinition def;
        def.address = addr;
        def.key = QStringLiteral("register_%1").arg(addr);
        def.displayName = QStringLiteral("Register %1").arg(addr);
        def.unit = QString();
        def.scale = DefaultScale;
        def.offset = 0.0;
        def.valueType = DcdcRegisterValueType::Unsigned16;
        defs.append(def);
    }

    return defs;
}

bool DcdcModbusRegisterMap::validateDefinitions(
    const QVector<DcdcRegisterDefinition> &definitions,
    QStringList *errors)
{
    QStringList localErrors;

    if (definitions.isEmpty())
    {
        if (errors)
            *errors = QStringList{QStringLiteral("Definitions list is empty")};
        return false;
    }

    QHash<quint16, int> addressCount;

    for (int i = 0; i < definitions.size(); ++i)
    {
        const auto &def = definitions.at(i);

        if (def.key.isEmpty())
        {
            localErrors.append(
                QStringLiteral("Definition at index %1 has empty key").arg(i));
        }

        if (def.scale == 0.0)
        {
            localErrors.append(
                QStringLiteral("Definition '%1' has zero scale").arg(def.key));
        }

        addressCount[def.address]++;
    }

    for (auto it = addressCount.constBegin(); it != addressCount.constEnd(); ++it)
    {
        if (it.value() > 1)
        {
            localErrors.append(
                QStringLiteral("Duplicate address %1 found %2 times")
                    .arg(it.key())
                    .arg(it.value()));
        }
    }

    if (errors)
        *errors = localErrors;

    return localErrors.isEmpty();
}

DcdcRegisterDecodeResult DcdcModbusRegisterMap::decodeHoldingRegisters(
    quint16 responseStartRegister,
    const QVector<quint16> &registers,
    const QVector<DcdcRegisterDefinition> &definitions)
{
    DcdcRegisterDecodeResult result;

    if (!validateDefinitions(definitions, &result.errors))
    {
        result.ok = false;
        return result;
    }

    for (const auto &def : definitions)
    {
        quint16 offset = def.address - responseStartRegister;

        if (def.address < responseStartRegister ||
            offset >= static_cast<quint16>(registers.size()))
        {
            result.errors.append(
                QStringLiteral("Missing register %1 (%2)")
                    .arg(def.address)
                    .arg(def.key));
            continue;
        }

        DcdcRegisterValue val;
        val.address = def.address;
        val.key = def.key;
        val.displayName = def.displayName;
        val.unit = def.unit;
        val.rawValue = registers.at(offset);
        val.scaledValue = scaleRawValue(val.rawValue, def);
        result.values.append(val);
    }

    result.ok = result.errors.isEmpty();
    return result;
}

bool DcdcModbusRegisterMap::containsAddress(
    const QVector<DcdcRegisterDefinition> &definitions,
    quint16 address)
{
    for (const auto &def : definitions)
    {
        if (def.address == address)
            return true;
    }
    return false;
}

double DcdcModbusRegisterMap::scaleRawValue(
    quint16 rawValue,
    const DcdcRegisterDefinition &definition)
{
    double value;

    if (definition.valueType == DcdcRegisterValueType::Signed16)
    {
        value = static_cast<double>(static_cast<qint16>(rawValue));
    }
    else
    {
        value = static_cast<double>(rawValue);
    }

    return value * definition.scale + definition.offset;
}

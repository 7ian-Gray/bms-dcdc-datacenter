#include "DcdcDataModelBridge.h"
#include "DcdcModbusSession.h"

#include <QHash>

DcdcDataModelBridge::DcdcDataModelBridge(QObject *parent)
    : QObject(parent)
    , mappings_(defaultMappings())
{
}

DcdcDataModelBridge::DcdcDataModelBridge(DcdcModbusSession *session, QObject *parent)
    : QObject(parent)
    , mappings_(defaultMappings())
{
    attachSession(session);
}

DcdcData DcdcDataModelBridge::currentData() const
{
    return currentData_;
}

QVector<DcdcDataModelBridge::RegisterMapping> DcdcDataModelBridge::mappings() const
{
    return mappings_;
}

void DcdcDataModelBridge::setMappings(const QVector<RegisterMapping> &mappings)
{
    mappings_ = mappings;
}

// This default mapping is provisional and must be replaced after the actual DCDC Modbus
// register table is confirmed.
QVector<DcdcDataModelBridge::RegisterMapping> DcdcDataModelBridge::defaultMappings()
{
    return {
        {101, Field::HvVoltage},
        {102, Field::HvCurrent},
        {103, Field::LvVoltage},
        {104, Field::LvCurrent},
        {105, Field::BoardTemperature},
        {106, Field::ChargeCurrentLimit},
        {107, Field::DischargeCurrentLimit},
        {108, Field::ChargeStartVoltage},
        {109, Field::ChargeStopVoltage},
        {110, Field::ChargeTargetVoltage},
        {111, Field::DischargeStartVoltage},
        {112, Field::DischargeStopVoltage},
        {113, Field::DischargeTargetVoltage},
    };
}

void DcdcDataModelBridge::attachSession(DcdcModbusSession *session)
{
    if (attachedSession_ && attachedSession_ != session) {
        disconnect(attachedSession_, &DcdcModbusSession::dcdcDataDecoded,
                   this, &DcdcDataModelBridge::onDcdcDataDecoded);
    }

    attachedSession_ = session;

    if (session) {
        connect(session, &DcdcModbusSession::dcdcDataDecoded,
                this, &DcdcDataModelBridge::onDcdcDataDecoded);
    }
}

void DcdcDataModelBridge::onDcdcDataDecoded(DcdcRegisterDecodeResult result)
{
    if (!result.ok) {
        const QString errorMsg = result.errors.isEmpty()
            ? QStringLiteral("DCDC register decode failed with no error details")
            : result.errors.join("; ");
        emit bridgeError(errorMsg);
        return;
    }

    // Build address-to-scaledValue lookup from the decode result.
    QHash<quint16, double> addressValues;
    addressValues.reserve(result.values.size());
    for (const auto &val : result.values) {
        addressValues.insert(val.address, val.scaledValue);
    }

    // Start from current state; only overwrite fields present in this result.
    DcdcData nextData = currentData_;

    for (const auto &mapping : mappings_) {
        auto it = addressValues.find(mapping.address);
        if (it == addressValues.end()) {
            continue;
        }
        applyValueToData(nextData, mapping.address, *it);
    }

    currentData_ = nextData;
    emit dcdcDataUpdated(currentData_);
}

void DcdcDataModelBridge::applyValueToData(DcdcData &data,
                                            quint16 address,
                                            double scaledValue) const
{
    // Find the mapping for this address and write to the corresponding field.
    for (const auto &mapping : mappings_) {
        if (mapping.address != address) {
            continue;
        }

        switch (mapping.field) {
        case Field::HvVoltage:
            data.hvVoltage = scaledValue;
            break;
        case Field::HvCurrent:
            data.hvCurrent = scaledValue;
            break;
        case Field::LvVoltage:
            data.lvVoltage = scaledValue;
            break;
        case Field::LvCurrent:
            data.lvCurrent = scaledValue;
            break;
        case Field::BoardTemperature:
            data.boardTemperature = scaledValue;
            break;
        case Field::ChargeCurrentLimit:
            data.chargeCurrentLimit = scaledValue;
            break;
        case Field::DischargeCurrentLimit:
            data.dischargeCurrentLimit = scaledValue;
            break;
        case Field::ChargeStartVoltage:
            data.chargeStartVoltage = scaledValue;
            break;
        case Field::ChargeStopVoltage:
            data.chargeStopVoltage = scaledValue;
            break;
        case Field::ChargeTargetVoltage:
            data.chargeTargetVoltage = scaledValue;
            break;
        case Field::DischargeStartVoltage:
            data.dischargeStartVoltage = scaledValue;
            break;
        case Field::DischargeStopVoltage:
            data.dischargeStopVoltage = scaledValue;
            break;
        case Field::DischargeTargetVoltage:
            data.dischargeTargetVoltage = scaledValue;
            break;
        }

        // Found and applied; for a given address there's only one mapping entry.
        break;
    }
}

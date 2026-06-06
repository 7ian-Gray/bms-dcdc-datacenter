#include "BmsCanParser.h"

namespace
{
quint16 readU16Le(const QByteArray &data, int offset)
{
    const auto low = static_cast<quint8>(data.at(offset));
    const auto high = static_cast<quint8>(data.at(offset + 1));
    return static_cast<quint16>(low | (static_cast<quint16>(high) << 8));
}

qint16 readI16Le(const QByteArray &data, int offset)
{
    return static_cast<qint16>(readU16Le(data, offset));
}
} // namespace

BmsCanParser::UpdateFlags BmsCanParser::parse(const CanFrame &frame, BmsData &bmsData) const
{
    if (frame.remote || frame.payload.size() < 8) {
        return NoUpdate;
    }

    const quint32 messageGroup = frame.id & 0xFFFF0000u;

    switch (messageGroup) {
    case 0x18E10000u:
        bmsData.packVoltage = readU16Le(frame.payload, 0) * 0.1;
        bmsData.packCurrent = readI16Le(frame.payload, 2) * 0.1;
        bmsData.soc = readU16Le(frame.payload, 4) * 0.1;
        bmsData.soh = readU16Le(frame.payload, 6) * 0.1;
        return BasicInfoUpdated;

    case 0x18E20000u:
        bmsData.chargeCurrentLimit = readU16Le(frame.payload, 0) * 0.1;
        bmsData.dischargeCurrentLimit = readU16Le(frame.payload, 2) * 0.1;
        bmsData.chargeVoltageLimit = readU16Le(frame.payload, 4) * 0.1;
        bmsData.dischargeVoltageLimit = readU16Le(frame.payload, 6) * 0.1;
        return LimitsUpdated;

    case 0x18E30000u:
        bmsData.chargeAvailableEnergy = readU16Le(frame.payload, 0) * 0.1;
        bmsData.dischargeAvailableEnergy = readU16Le(frame.payload, 2) * 0.1;
        bmsData.statusWord = readU16Le(frame.payload, 4);
        bmsData.sop = readU16Le(frame.payload, 6) * 0.1;
        return StatusUpdated;

    case 0x18E40000u:
        bmsData.maxCellVoltage = readU16Le(frame.payload, 0) * 0.001;
        bmsData.minCellVoltage = readU16Le(frame.payload, 2) * 0.001;
        bmsData.maxCellTemperature = readI16Le(frame.payload, 4) * 0.1;
        bmsData.minCellTemperature = readI16Le(frame.payload, 6) * 0.1;
        return ExtremesUpdated;

    default:
        return NoUpdate;
    }
}

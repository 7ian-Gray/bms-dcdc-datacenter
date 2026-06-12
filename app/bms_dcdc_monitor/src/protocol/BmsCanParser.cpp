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

BmsCanParser::BmsCanParser(const BmsCanAddressConfig &addressConfig)
    : addressConfig_(addressConfig)
{
}

quint32 BmsCanParser::expectedId(quint8 messageType, const BmsCanAddressConfig &addressConfig)
{
    return 0x18000000u |
           (static_cast<quint32>(messageType) << 16) |
           (static_cast<quint32>(addressConfig.destinationAddress) << 8) |
           static_cast<quint32>(addressConfig.sourceAddress);
}

BmsParseResult BmsCanParser::parseFrame(const CanFrame &frame) const
{
    BmsParseResult result;

    if (!frame.extended) {
        result.status = BmsParseResult::Status::InvalidFrame;
        result.error = QStringLiteral("BMS CAN frame must be an extended CAN 2.0B frame");
        return result;
    }

    if (frame.remote) {
        result.status = BmsParseResult::Status::InvalidFrame;
        result.error = QStringLiteral("BMS CAN frame must not be a remote frame");
        return result;
    }

    if (frame.payload.size() != 8) {
        result.status = BmsParseResult::Status::InvalidFrame;
        result.error = QStringLiteral("BMS CAN payload must be exactly 8 bytes");
        return result;
    }

    const quint32 basicInfoId = expectedId(0xE1, addressConfig_);
    const quint32 limitsId = expectedId(0xE2, addressConfig_);
    const quint32 statusId = expectedId(0xE3, addressConfig_);
    const quint32 extremesId = expectedId(0xE4, addressConfig_);

    if (frame.id == basicInfoId) {
        result.data.packVoltage = readU16Le(frame.payload, 0) * 0.1;
        result.data.packCurrent = readI16Le(frame.payload, 2) * 0.1;
        result.data.soc = readU16Le(frame.payload, 4) * 0.1;
        result.data.soh = readU16Le(frame.payload, 6) * 0.1;
        result.updateFlags = BasicInfoUpdated;
        result.status = BmsParseResult::Status::Updated;
        return result;
    }

    if (frame.id == limitsId) {
        result.data.chargeCurrentLimit = readU16Le(frame.payload, 0) * 0.1;
        result.data.dischargeCurrentLimit = readU16Le(frame.payload, 2) * 0.1;
        result.data.chargeVoltageLimit = readU16Le(frame.payload, 4) * 0.1;
        result.data.dischargeVoltageLimit = readU16Le(frame.payload, 6) * 0.1;
        result.updateFlags = LimitsUpdated;
        result.status = BmsParseResult::Status::Updated;
        return result;
    }

    if (frame.id == statusId) {
        result.data.chargeAvailableEnergy = readU16Le(frame.payload, 0) * 0.1;
        result.data.dischargeAvailableEnergy = readU16Le(frame.payload, 2) * 0.1;
        result.data.statusWord = readU16Le(frame.payload, 4);
        result.data.sop = readU16Le(frame.payload, 6) * 0.1;
        result.updateFlags = StatusUpdated;
        result.status = BmsParseResult::Status::Updated;
        return result;
    }

    if (frame.id == extremesId) {
        result.data.maxCellVoltage = readU16Le(frame.payload, 0) * 0.001;
        result.data.minCellVoltage = readU16Le(frame.payload, 2) * 0.001;
        result.data.maxCellTemperature = readI16Le(frame.payload, 4) * 0.1;
        result.data.minCellTemperature = readI16Le(frame.payload, 6) * 0.1;
        result.updateFlags = ExtremesUpdated;
        result.status = BmsParseResult::Status::Updated;
        return result;
    }

    return result;
}

BmsCanParser::UpdateFlags BmsCanParser::parse(const CanFrame &frame, BmsData &bmsData) const
{
    const BmsParseResult result = parseFrame(frame);
    if (result.status != BmsParseResult::Status::Updated) {
        return NoUpdate;
    }

    const UpdateFlags updates = UpdateFlags::fromInt(result.updateFlags);
    if (updates.testFlag(BasicInfoUpdated)) {
        bmsData.packVoltage = result.data.packVoltage;
        bmsData.packCurrent = result.data.packCurrent;
        bmsData.soc = result.data.soc;
        bmsData.soh = result.data.soh;
    }

    if (updates.testFlag(LimitsUpdated)) {
        bmsData.chargeCurrentLimit = result.data.chargeCurrentLimit;
        bmsData.dischargeCurrentLimit = result.data.dischargeCurrentLimit;
        bmsData.chargeVoltageLimit = result.data.chargeVoltageLimit;
        bmsData.dischargeVoltageLimit = result.data.dischargeVoltageLimit;
    }

    if (updates.testFlag(StatusUpdated)) {
        bmsData.chargeAvailableEnergy = result.data.chargeAvailableEnergy;
        bmsData.dischargeAvailableEnergy = result.data.dischargeAvailableEnergy;
        bmsData.statusWord = result.data.statusWord;
        bmsData.sop = result.data.sop;
    }

    if (updates.testFlag(ExtremesUpdated)) {
        bmsData.maxCellVoltage = result.data.maxCellVoltage;
        bmsData.minCellVoltage = result.data.minCellVoltage;
        bmsData.maxCellTemperature = result.data.maxCellTemperature;
        bmsData.minCellTemperature = result.data.minCellTemperature;
    }

    return updates;
}

#pragma once

#include "CanFrame.h"
#include "DataModel.h"

#include <QFlags>
#include <QString>
#include <QtGlobal>

struct BmsCanAddressConfig
{
    quint8 destinationAddress = 0x01;
    quint8 sourceAddress = 0x01;
};

struct BmsParseResult
{
    enum class Status
    {
        Updated,
        NotMatched,
        InvalidFrame
    };

    Status status = Status::NotMatched;
    BmsData data;
    quint32 updateFlags = 0;
    QString error;
};

class BmsCanParser
{
public:
    enum UpdateFlag : quint32
    {
        NoUpdate = 0x00,
        BasicInfoUpdated = 0x01,
        LimitsUpdated = 0x02,
        StatusUpdated = 0x04,
        ExtremesUpdated = 0x08
    };
    Q_DECLARE_FLAGS(UpdateFlags, UpdateFlag)

    explicit BmsCanParser(const BmsCanAddressConfig &addressConfig = {});

    BmsParseResult parseFrame(const CanFrame &frame) const;
    UpdateFlags parse(const CanFrame &frame, BmsData &bmsData) const;

private:
    static quint32 expectedId(quint8 messageType, const BmsCanAddressConfig &addressConfig);

    BmsCanAddressConfig addressConfig_;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BmsCanParser::UpdateFlags)

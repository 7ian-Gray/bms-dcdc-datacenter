#pragma once

#include "CanFrame.h"
#include "DataModel.h"

#include <QFlags>
#include <QtGlobal>

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

    UpdateFlags parse(const CanFrame &frame, BmsData &bmsData) const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BmsCanParser::UpdateFlags)

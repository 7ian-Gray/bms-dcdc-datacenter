#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QtGlobal>

enum class CanFrameDirection : quint8
{
    Rx = 0,
    Tx = 1
};

struct CanFrame
{
    quint32 id = 0;
    quint8 channel = 0;
    bool extended = true;
    bool remote = false;
    CanFrameDirection direction = CanFrameDirection::Rx;
    quint64 timestampUs = 0;
    QByteArray payload;
};

Q_DECLARE_METATYPE(CanFrameDirection)
Q_DECLARE_METATYPE(CanFrame)

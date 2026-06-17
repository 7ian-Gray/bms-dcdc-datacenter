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
    quint64 timestampUs = 0;
    QByteArray payload;
    CanFrameDirection direction = CanFrameDirection::Rx;
};

Q_DECLARE_METATYPE(CanFrameDirection)
Q_DECLARE_METATYPE(CanFrame)

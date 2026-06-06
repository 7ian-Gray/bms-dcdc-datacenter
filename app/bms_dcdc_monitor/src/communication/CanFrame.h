#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QtGlobal>

struct CanFrame
{
    quint32 id = 0;
    quint8 channel = 0;
    bool extended = true;
    bool remote = false;
    quint64 timestampUs = 0;
    QByteArray payload;
};

Q_DECLARE_METATYPE(CanFrame)

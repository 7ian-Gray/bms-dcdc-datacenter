#pragma once

#include "communication/CanFrame.h"

#include <QString>

struct CanFrameInput
{
    QString canIdText;
    QString dataText;
    int channel = 0;
    bool extended = true;
    bool remote = false;
};

struct CanFrameInputParseResult
{
    bool ok = false;
    QString errorMessage;
    CanFrame frame;
};

class CanFrameInputParser
{
public:
    static CanFrameInputParseResult parse(const CanFrameInput &input);
    static bool parseCanId(const QString &text,
                           bool extended,
                           quint32 *canId,
                           QString *errorMessage);
    static bool parsePayload(const QString &text,
                             QByteArray *payload,
                             QString *errorMessage);
};

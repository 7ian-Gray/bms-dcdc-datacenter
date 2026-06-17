#include "communication/CanFrameInputParser.h"

namespace
{
QString normalizedHexText(const QString &text)
{
    QString normalized = text.trimmed();
    normalized.remove(QLatin1Char(' '));
    normalized.remove(QLatin1Char('\t'));
    normalized.remove(QLatin1Char('\n'));
    normalized.remove(QLatin1Char('\r'));

    if (normalized.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        normalized.remove(0, 2);
    }

    return normalized.toUpper();
}

bool isHexText(const QString &text)
{
    for (const QChar &character : text) {
        if (!character.isDigit() &&
            (character < QLatin1Char('A') || character > QLatin1Char('F'))) {
            return false;
        }
    }
    return true;
}
}

CanFrameInputParseResult CanFrameInputParser::parse(const CanFrameInput &input)
{
    CanFrameInputParseResult result;

    quint32 canId = 0;
    QString errorMessage;
    if (!parseCanId(input.canIdText, input.extended, &canId, &errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    QByteArray payload;
    if (!parsePayload(input.dataText, &payload, &errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    if (input.remote) {
        result.errorMessage = QStringLiteral("当前阶段仅支持经典 CAN 数据帧，暂不支持远程帧");
        return result;
    }

    result.frame.id = canId;
    result.frame.channel = static_cast<quint8>(qBound(0, input.channel, 255));
    result.frame.extended = input.extended;
    result.frame.remote = input.remote;
    result.frame.payload = input.remote ? QByteArray() : payload;
    result.frame.direction = CanFrameDirection::Tx;
    result.ok = true;
    return result;
}

bool CanFrameInputParser::parseCanId(const QString &text,
                                     bool extended,
                                     quint32 *canId,
                                     QString *errorMessage)
{
    const QString normalized = normalizedHexText(text);
    if (normalized.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("CAN ID 不能为空");
        }
        return false;
    }

    if (!isHexText(normalized)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("CAN ID 必须是十六进制");
        }
        return false;
    }

    bool ok = false;
    const quint64 parsedId = normalized.toULongLong(&ok, 16);
    if (!ok) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("CAN ID 解析失败");
        }
        return false;
    }

    const quint64 maxId = extended ? 0x1FFFFFFFULL : 0x7FFULL;
    if (parsedId > maxId) {
        if (errorMessage != nullptr) {
            *errorMessage = extended
                                ? QStringLiteral("扩展帧 CAN ID 必须在 0x00000000 到 0x1FFFFFFF 范围内")
                                : QStringLiteral("标准帧 CAN ID 必须在 0x000 到 0x7FF 范围内");
        }
        return false;
    }

    if (canId != nullptr) {
        *canId = static_cast<quint32>(parsedId);
    }
    return true;
}

bool CanFrameInputParser::parsePayload(const QString &text,
                                       QByteArray *payload,
                                       QString *errorMessage)
{
    const QString normalized = normalizedHexText(text);
    if (normalized.isEmpty()) {
        if (payload != nullptr) {
            payload->clear();
        }
        return true;
    }

    if (!isHexText(normalized)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Data 必须是十六进制");
        }
        return false;
    }

    if (normalized.size() % 2 != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Data 十六进制字符数必须为偶数");
        }
        return false;
    }

    if (normalized.size() / 2 > 8) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Classic CAN Data 不能超过 8 字节");
        }
        return false;
    }

    if (payload != nullptr) {
        *payload = QByteArray::fromHex(normalized.toLatin1());
    }
    return true;
}

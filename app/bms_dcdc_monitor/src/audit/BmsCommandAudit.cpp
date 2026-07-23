#include "audit/BmsCommandAudit.h"

namespace
{
constexpr int FingerprintPrefixLength = 12;
constexpr int FingerprintSuffixLength = 8;
} // namespace

BmsCommandAuditRecord makeBmsCommandAuditRecord(
    BmsCommandAuditEventType eventType,
    BmsCommandAuditOutcome outcome,
    const BmsCommandConfirmationSnapshot *snapshot,
    const QDateTime &eventAtUtc,
    const QString &code,
    const QString &message,
    const QDateTime &transmittedAtUtc)
{
    BmsCommandAuditRecord record;
    record.eventType = eventType;
    record.outcome = outcome;
    record.mode = BmsCommandAuditMode::DemoMock;
    record.eventAtUtc = eventAtUtc;
    record.code = code;
    record.message = message;
    record.transmittedAtUtc = transmittedAtUtc;

    if (snapshot != nullptr) {
        const EncodedBmsCommand &command = snapshot->command;
        record.revision = snapshot->revision;
        record.commandId = command.commandId;
        record.safetyLevel = command.safetyLevel;
        record.fingerprint = command.fingerprint;
        record.canId = command.canId;
        record.channel = command.channel;
        record.extended = command.extended;
        record.dlc = command.dlc;
        // Value copies: the audit record must not alias the live snapshot.
        record.payload = command.data;
        record.engineeringParameters = command.engineeringParameters;
        record.rawParameters = command.rawParameters;
        record.stagedAtUtc = snapshot->stagedAtUtc;
        record.confirmedAtUtc = snapshot->confirmedAtUtc;
        // The confirmation code is deliberately never copied into the trail.
    }

    return record;
}

QString bmsCommandAuditEventText(BmsCommandAuditEventType type)
{
    switch (type) {
    case BmsCommandAuditEventType::PreviewStaged:
        return QStringLiteral("预览生成");
    case BmsCommandAuditEventType::PreviewInvalidated:
        return QStringLiteral("预览失效");
    case BmsCommandAuditEventType::ConfirmationSucceeded:
        return QStringLiteral("确认成功");
    case BmsCommandAuditEventType::ConfirmationFailed:
        return QStringLiteral("确认失败");
    case BmsCommandAuditEventType::DispatchRequested:
        return QStringLiteral("发送请求");
    case BmsCommandAuditEventType::DispatchSucceeded:
        return QStringLiteral("发送成功");
    case BmsCommandAuditEventType::DispatchFailed:
        return QStringLiteral("发送失败");
    }
    return QString();
}

QString bmsCommandAuditOutcomeText(BmsCommandAuditOutcome outcome)
{
    switch (outcome) {
    case BmsCommandAuditOutcome::Info:
        return QStringLiteral("信息");
    case BmsCommandAuditOutcome::Success:
        return QStringLiteral("成功");
    case BmsCommandAuditOutcome::Failure:
        return QStringLiteral("失败");
    }
    return QString();
}

QString bmsCommandAuditModeText(BmsCommandAuditMode mode)
{
    switch (mode) {
    case BmsCommandAuditMode::DemoMock:
        return QStringLiteral("Demo / Mock");
    }
    return QString();
}

QString bmsCommandAuditPayloadText(const QByteArray &payload)
{
    if (payload.isEmpty()) {
        return QStringLiteral("-");
    }
    return QString::fromLatin1(payload.toHex(' ')).toUpper();
}

QString bmsCommandAuditCanIdText(quint32 canId)
{
    return QStringLiteral("0x") +
           QString::number(canId, 16).toUpper().rightJustified(8, QLatin1Char('0'));
}

QString bmsCommandAuditFingerprintShortText(const QString &fingerprint)
{
    if (fingerprint.isEmpty()) {
        return QStringLiteral("-");
    }
    if (fingerprint.size() <= FingerprintPrefixLength + FingerprintSuffixLength) {
        return fingerprint;
    }
    return fingerprint.left(FingerprintPrefixLength) + QStringLiteral("…") +
           fingerprint.right(FingerprintSuffixLength);
}

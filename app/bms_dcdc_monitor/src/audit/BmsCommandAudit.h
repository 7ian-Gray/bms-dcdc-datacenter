#pragma once

#include "communication/BmsCommandConfirmationGate.h"
#include "protocol/BmsCommand.h"

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVariantMap>
#include <QtGlobal>

// One entry in the Demo/Mock BMS command lifecycle audit trail.
//
// The trail is an in-process, append-only software-debugging aid. A record is a
// frozen value copy of one lifecycle event; it is not persisted, not signed, and
// makes no tamper-proof, compliance, authentication, or hardware-operation claim.
enum class BmsCommandAuditEventType : quint8
{
    PreviewStaged = 0,
    PreviewInvalidated,
    ConfirmationSucceeded,
    ConfirmationFailed,
    DispatchRequested,
    DispatchSucceeded,
    DispatchFailed
};

enum class BmsCommandAuditOutcome : quint8
{
    Info = 0,
    Success,
    Failure
};

// Only Demo/Mock is allowed in this PR. Real hardware modes are a separate,
// future extension and must not be pre-declared here.
enum class BmsCommandAuditMode : quint8
{
    DemoMock = 0
};

struct BmsCommandAuditRecord
{
    // Assigned by BmsCommandAuditModel on append; producers must leave it 0.
    quint64 sequence = 0;
    BmsCommandAuditEventType eventType = BmsCommandAuditEventType::PreviewStaged;
    BmsCommandAuditOutcome outcome = BmsCommandAuditOutcome::Info;
    BmsCommandAuditMode mode = BmsCommandAuditMode::DemoMock;
    QDateTime eventAtUtc;
    quint64 revision = 0;
    QString commandId;
    BmsSafetyLevel safetyLevel = BmsSafetyLevel::DemoOnly;
    QString fingerprint;
    quint32 canId = 0;
    quint8 channel = 0;
    bool extended = true;
    int dlc = 0;
    QByteArray payload;
    QVariantMap engineeringParameters;
    QVariantMap rawParameters;
    QDateTime stagedAtUtc;
    QDateTime confirmedAtUtc;
    QDateTime transmittedAtUtc;
    QString code;
    QString message;
};

Q_DECLARE_METATYPE(BmsCommandAuditRecord)

// Builds a record from one lifecycle event. When snapshot != nullptr it copies
// the frozen snapshot values (commandId, revision, fingerprint, CAN fields, and
// value copies of the parameter maps) but never the confirmation code. It does
// not re-encode the frame, recompute the fingerprint, validate provenance, or
// claim tamper protection. sequence stays 0 (the model assigns it) and
// eventAtUtc must be supplied by the caller at the moment the event occurred.
BmsCommandAuditRecord makeBmsCommandAuditRecord(
    BmsCommandAuditEventType eventType,
    BmsCommandAuditOutcome outcome,
    const BmsCommandConfirmationSnapshot *snapshot,
    const QDateTime &eventAtUtc,
    const QString &code = QString(),
    const QString &message = QString(),
    const QDateTime &transmittedAtUtc = QDateTime());

QString bmsCommandAuditEventText(BmsCommandAuditEventType type);
QString bmsCommandAuditOutcomeText(BmsCommandAuditOutcome outcome);
QString bmsCommandAuditModeText(BmsCommandAuditMode mode);
QString bmsCommandAuditPayloadText(const QByteArray &payload);
QString bmsCommandAuditCanIdText(quint32 canId);
// "前 12 位…后 8 位" abbreviation for table display; the tooltip shows the full value.
QString bmsCommandAuditFingerprintShortText(const QString &fingerprint);

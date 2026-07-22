#pragma once

#include "protocol/BmsCommand.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

#include <optional>

enum class BmsCommandConfirmationState : quint8
{
    Empty = 0,
    Staged,
    Confirmed,
    Invalidated
};

// A frozen value copy of one encoded preview, plus the bookkeeping needed to
// bind a user confirmation to that exact revision.
struct BmsCommandConfirmationSnapshot
{
    quint64 revision = 0;
    EncodedBmsCommand command;
    QString confirmationCode;
    QDateTime stagedAtUtc;
    QDateTime confirmedAtUtc;
};

// Registered so a confirmed snapshot can travel through Qt signals.
Q_DECLARE_METATYPE(BmsCommandConfirmationSnapshot)

struct BmsCommandConfirmationResult
{
    bool accepted = false;
    QString code;
    QString message;
};

// Records that the operator reviewed one specific preview snapshot. This is a
// review record only: it authorizes nothing, unlocks no transmission path, and
// makes no claim about hardware or vendor protocol validation. The confirmation
// code is a human cross-check aid derived from the fingerprint - not a password,
// signature, or authentication token.
//
// Structural validation is not provenance validation. The gate assumes its
// input comes from the trusted in-process encoder path; it does not recompute
// the fingerprint and does not provide authenticity or tamper protection.
class BmsCommandConfirmationGate
{
public:
    // Freezes command as the snapshot awaiting confirmation. Rejects internally
    // inconsistent input without disturbing any existing state; it does not
    // verify where the value came from.
    BmsCommandConfirmationResult stage(const EncodedBmsCommand &command);

    // Confirms the staged snapshot. expectedRevision must match the staged
    // revision, so a confirmation can never land on a newer preview.
    BmsCommandConfirmationResult confirm(quint64 expectedRevision,
                                         const QString &confirmationCode,
                                         bool acknowledged);

    void invalidate();

    BmsCommandConfirmationState state() const;
    bool hasStagedSnapshot() const;
    bool hasConfirmedSnapshot() const;

    std::optional<BmsCommandConfirmationSnapshot> stagedSnapshot() const;
    std::optional<BmsCommandConfirmationSnapshot> confirmedSnapshot() const;

    static QString confirmationCodeFor(const EncodedBmsCommand &command);

private:
    quint64 nextRevision_ = 1;
    BmsCommandConfirmationState state_ = BmsCommandConfirmationState::Empty;

    std::optional<BmsCommandConfirmationSnapshot> staged_;
    std::optional<BmsCommandConfirmationSnapshot> confirmed_;
};

#include "communication/BmsCommandConfirmationGate.h"

namespace
{
constexpr int FingerprintLength = 64;
constexpr int ConfirmationCodeLength = 8;
constexpr int ClassicCanMaxDlc = 8;

BmsCommandConfirmationResult makeResult(bool accepted, const QString &code, const QString &message)
{
    BmsCommandConfirmationResult result;
    result.accepted = accepted;
    result.code = code;
    result.message = message;
    return result;
}

bool isHexString(const QString &text)
{
    for (const QChar character : text) {
        if (!isxdigit(character.toLatin1())) {
            return false;
        }
    }
    return !text.isEmpty();
}

// The snapshot must be internally consistent before it can be staged.
// This check does not authenticate the snapshot's origin, recompute its
// fingerprint, or prove that it was produced by BmsCommandEncoder.
bool isStructurallyValid(const EncodedBmsCommand &command)
{
    if (command.commandId.isEmpty()) {
        return false;
    }
    if (command.fingerprint.size() != FingerprintLength || !isHexString(command.fingerprint)) {
        return false;
    }
    if (!command.encodedAtUtc.isValid()) {
        return false;
    }
    if (command.dlc < 0 || command.dlc > ClassicCanMaxDlc) {
        return false;
    }
    if (command.data.size() != command.dlc) {
        return false;
    }
    if (command.frame.id != command.canId) {
        return false;
    }
    if (command.frame.channel != command.channel) {
        return false;
    }
    if (command.frame.extended != command.extended) {
        return false;
    }
    if (command.frame.remote) {
        return false;
    }
    if (command.frame.payload != command.data) {
        return false;
    }
    if (command.frame.direction != CanFrameDirection::Tx) {
        return false;
    }
    return true;
}
} // namespace

QString BmsCommandConfirmationGate::confirmationCodeFor(const EncodedBmsCommand &command)
{
    return command.fingerprint.right(ConfirmationCodeLength).toUpper();
}

BmsCommandConfirmationResult BmsCommandConfirmationGate::stage(const EncodedBmsCommand &command)
{
    if (!isStructurallyValid(command)) {
        // Leave any existing staged/confirmed state untouched on rejection.
        return makeResult(false,
                          QStringLiteral("InvalidSnapshot"),
                          QStringLiteral("Encoded command snapshot is structurally invalid"));
    }

    BmsCommandConfirmationSnapshot snapshot;
    // A fresh revision every time, even for byte-identical content: the
    // confirmation binds to this presented preview, not to a content hash.
    snapshot.revision = nextRevision_++;
    snapshot.command = command;
    snapshot.confirmationCode = confirmationCodeFor(command);
    snapshot.stagedAtUtc = QDateTime::currentDateTimeUtc();

    staged_ = snapshot;
    confirmed_.reset();
    state_ = BmsCommandConfirmationState::Staged;

    return makeResult(true, QString(), QStringLiteral("Preview snapshot staged for confirmation"));
}

BmsCommandConfirmationResult BmsCommandConfirmationGate::confirm(quint64 expectedRevision,
                                                                 const QString &confirmationCode,
                                                                 bool acknowledged)
{
    if (!staged_.has_value()) {
        return makeResult(false,
                          QStringLiteral("NoStagedSnapshot"),
                          QStringLiteral("There is no preview snapshot awaiting confirmation"));
    }

    if (!acknowledged) {
        return makeResult(false,
                          QStringLiteral("AcknowledgementRequired"),
                          QStringLiteral("The review acknowledgement must be checked"));
    }

    if (expectedRevision != staged_->revision) {
        return makeResult(false,
                          QStringLiteral("RevisionMismatch"),
                          QStringLiteral("The preview changed since it was displayed"));
    }

    const QString normalizedCode = confirmationCode.trimmed().toUpper();
    if (normalizedCode != staged_->confirmationCode) {
        return makeResult(false,
                          QStringLiteral("ConfirmationCodeMismatch"),
                          QStringLiteral("The confirmation code does not match the preview"));
    }

    BmsCommandConfirmationSnapshot snapshot = *staged_;
    snapshot.confirmedAtUtc = QDateTime::currentDateTimeUtc();
    staged_ = snapshot;
    confirmed_ = snapshot;
    state_ = BmsCommandConfirmationState::Confirmed;

    return makeResult(true,
                      QString(),
                      QStringLiteral("Preview snapshot confirmed; sending remains disabled"));
}

void BmsCommandConfirmationGate::invalidate()
{
    staged_.reset();
    confirmed_.reset();
    state_ = BmsCommandConfirmationState::Invalidated;
}

BmsCommandConfirmationState BmsCommandConfirmationGate::state() const
{
    return state_;
}

bool BmsCommandConfirmationGate::hasStagedSnapshot() const
{
    return staged_.has_value();
}

bool BmsCommandConfirmationGate::hasConfirmedSnapshot() const
{
    return confirmed_.has_value();
}

std::optional<BmsCommandConfirmationSnapshot> BmsCommandConfirmationGate::stagedSnapshot() const
{
    return staged_;
}

std::optional<BmsCommandConfirmationSnapshot> BmsCommandConfirmationGate::confirmedSnapshot() const
{
    return confirmed_;
}

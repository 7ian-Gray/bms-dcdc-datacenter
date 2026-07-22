#include "communication/BmsCommandMockDispatcher.h"

#include "communication/CanSessionController.h"

#include <QTimeZone>

namespace
{
constexpr int FingerprintLength = 64;
constexpr int ConfirmationCodeLength = 8;
constexpr int ClassicCanMaxDlc = 8;

bool isHexString(const QString &text)
{
    for (const QChar character : text) {
        if (!isxdigit(character.toLatin1())) {
            return false;
        }
    }
    return !text.isEmpty();
}
} // namespace

BmsCommandMockDispatcher::BmsCommandMockDispatcher(CanSessionController *sessionController,
                                                   QObject *parent)
    : QObject(parent),
      sessionController_(sessionController)
{
    qRegisterMetaType<BmsCommandConfirmationSnapshot>("BmsCommandConfirmationSnapshot");

    if (sessionController_ != nullptr) {
        connect(sessionController_, &CanSessionController::stateChanged,
                this, &BmsCommandMockDispatcher::refreshAvailability);
        connect(sessionController_, &CanSessionController::sourceModeChanged,
                this, &BmsCommandMockDispatcher::refreshAvailability);
        connect(sessionController_, &CanSessionController::sessionResetRequested,
                this, &BmsCommandMockDispatcher::refreshAvailability);
        connect(sessionController_, &CanSessionController::frameTransmitted,
                this, &BmsCommandMockDispatcher::onFrameTransmitted);
        connect(sessionController_, &CanSessionController::errorOccurred,
                this, &BmsCommandMockDispatcher::onSessionError);
    }

    lastAvailable_ = isAvailable();
    lastChannel_ = availableChannel();
    lastReason_ = availabilityText();
}

bool BmsCommandMockDispatcher::isAvailable() const
{
    if (sessionController_ == nullptr) {
        return false;
    }

    const CanConnectionConfig config = sessionController_->config();
    return sessionController_->state() == CanConnectionState::ChannelStarted &&
           sessionController_->sourceMode() == CanSourceMode::Mock &&
           config.driverType == CanDriverType::Mock &&
           !config.listenOnly;
}

int BmsCommandMockDispatcher::availableChannel() const
{
    if (!isAvailable()) {
        return -1;
    }
    return sessionController_->config().channel;
}

QString BmsCommandMockDispatcher::availabilityText() const
{
    if (sessionController_ == nullptr) {
        return QStringLiteral("尚未连接 CAN 会话控制器");
    }

    const CanConnectionConfig config = sessionController_->config();
    if (sessionController_->sourceMode() != CanSourceMode::Mock ||
        config.driverType != CanDriverType::Mock) {
        return QStringLiteral("当前不是 Mock CAN 模式");
    }
    if (config.listenOnly) {
        return QStringLiteral("监听模式禁止发送");
    }
    if (sessionController_->state() != CanConnectionState::ChannelStarted) {
        return QStringLiteral("请先在 CAN 通信监视页面打开并启动 Mock 通道");
    }
    return QStringLiteral("Mock CAN 通道 %1 已就绪").arg(config.channel);
}

void BmsCommandMockDispatcher::refreshAvailability()
{
    const bool available = isAvailable();
    const int channel = availableChannel();
    const QString reason = availabilityText();

    // Only report genuine transitions, so the page is not churned on every
    // unrelated session signal.
    if (available == lastAvailable_ && channel == lastChannel_ && reason == lastReason_) {
        return;
    }

    lastAvailable_ = available;
    lastChannel_ = channel;
    lastReason_ = reason;
    emit availabilityChanged(available, channel, reason);
}

QString BmsCommandMockDispatcher::snapshotKey(const BmsCommandConfirmationSnapshot &snapshot)
{
    return QStringLiteral("%1|%2|%3|%4")
        .arg(snapshot.command.commandId)
        .arg(snapshot.revision)
        .arg(snapshot.confirmedAtUtc.toMSecsSinceEpoch())
        .arg(snapshot.command.fingerprint);
}

QString BmsCommandMockDispatcher::validateSnapshot(const BmsCommandConfirmationSnapshot &snapshot,
                                                   QString *message) const
{
    const EncodedBmsCommand &command = snapshot.command;

    if (snapshot.revision == 0 || !snapshot.stagedAtUtc.isValid()) {
        *message = QStringLiteral("快照未经过预览暂存");
        return QStringLiteral("InvalidSnapshot");
    }
    if (!snapshot.confirmedAtUtc.isValid()) {
        *message = QStringLiteral("快照尚未确认，无法发送");
        return QStringLiteral("SnapshotNotConfirmed");
    }
    if (snapshot.confirmedAtUtc < snapshot.stagedAtUtc) {
        *message = QStringLiteral("确认时间早于暂存时间");
        return QStringLiteral("InvalidSnapshot");
    }
    if (command.fingerprint.size() != FingerprintLength || !isHexString(command.fingerprint)) {
        *message = QStringLiteral("Fingerprint 格式不正确");
        return QStringLiteral("InvalidSnapshot");
    }
    if (snapshot.confirmationCode != command.fingerprint.right(ConfirmationCodeLength).toUpper()) {
        *message = QStringLiteral("核对码与 Fingerprint 不一致");
        return QStringLiteral("InvalidSnapshot");
    }
    if (command.safetyLevel != BmsSafetyLevel::DemoOnly) {
        *message = QStringLiteral("仅允许发送 Demo 指令");
        return QStringLiteral("NonDemoCommand");
    }
    if (command.commandId.isEmpty() ||
        command.dlc < 0 || command.dlc > ClassicCanMaxDlc ||
        command.data.size() != command.dlc ||
        command.frame.id != command.canId ||
        command.frame.channel != command.channel ||
        command.frame.extended != command.extended ||
        command.frame.remote ||
        command.frame.payload != command.data ||
        command.frame.direction != CanFrameDirection::Tx) {
        *message = QStringLiteral("快照内部字段不一致");
        return QStringLiteral("InvalidSnapshot");
    }

    return QString();
}

QString BmsCommandMockDispatcher::validateSessionForDispatch(QString *message) const
{
    if (sessionController_ == nullptr) {
        *message = QStringLiteral("尚未连接 CAN 会话控制器");
        return QStringLiteral("NoSessionController");
    }

    const CanConnectionConfig config = sessionController_->config();
    if (sessionController_->sourceMode() != CanSourceMode::Mock ||
        config.driverType != CanDriverType::Mock) {
        *message = QStringLiteral("当前不是 Mock CAN 模式，禁止发送");
        return QStringLiteral("HardwareModeRejected");
    }
    if (config.listenOnly) {
        *message = QStringLiteral("监听模式禁止发送");
        return QStringLiteral("ListenOnlyRejected");
    }
    if (sessionController_->state() != CanConnectionState::ChannelStarted) {
        *message = QStringLiteral("Mock 通道尚未启动");
        return QStringLiteral("MockChannelNotReady");
    }

    return QString();
}

void BmsCommandMockDispatcher::failDispatch(quint64 revision,
                                            const QString &code,
                                            const QString &message)
{
    emit dispatchFailed(revision, code, message);
}

void BmsCommandMockDispatcher::dispatch(const BmsCommandConfirmationSnapshot &snapshot)
{
    if (pending_.has_value()) {
        failDispatch(snapshot.revision,
                     QStringLiteral("DispatchBusy"),
                     QStringLiteral("已有一次发送正在处理"));
        return;
    }

    QString message;
    const QString snapshotError = validateSnapshot(snapshot, &message);
    if (!snapshotError.isEmpty()) {
        failDispatch(snapshot.revision, snapshotError, message);
        return;
    }

    const QString key = snapshotKey(snapshot);
    if (consumedKeys_.contains(key)) {
        failDispatch(snapshot.revision,
                     QStringLiteral("SnapshotAlreadyConsumed"),
                     QStringLiteral("该确认版本已发送过，请重新生成并确认预览"));
        return;
    }

    const QString sessionError = validateSessionForDispatch(&message);
    if (!sessionError.isEmpty()) {
        failDispatch(snapshot.revision, sessionError, message);
        return;
    }

    if (sessionController_->config().channel != snapshot.command.channel) {
        failDispatch(snapshot.revision,
                     QStringLiteral("ChannelMismatch"),
                     QStringLiteral("指令通道与当前 Mock 通道不一致"));
        return;
    }

    pending_ = snapshot;
    pendingKey_ = key;

    // The Mock session answers synchronously, so by the time this returns either
    // onFrameTransmitted() or onSessionError() has already cleared the pending slot.
    sessionController_->sendFrame(snapshot.command.frame);

    if (pending_.has_value()) {
        const quint64 revision = pending_->revision;
        pending_.reset();
        pendingKey_.clear();
        failDispatch(revision,
                     QStringLiteral("NoTransmissionAcknowledgement"),
                     QStringLiteral("未收到发送回执，快照未被消费"));
    }
}

void BmsCommandMockDispatcher::onFrameTransmitted(const CanFrame &frame)
{
    if (!pending_.has_value()) {
        return;
    }

    const CanFrame &expected = pending_->command.frame;
    // The session stamps its own timestamp, so everything but the timestamp must match.
    if (frame.id != expected.id ||
        frame.channel != expected.channel ||
        frame.extended != expected.extended ||
        frame.remote != expected.remote ||
        frame.payload != expected.payload ||
        frame.direction != CanFrameDirection::Tx) {
        const quint64 revision = pending_->revision;
        pending_.reset();
        pendingKey_.clear();
        failDispatch(revision,
                     QStringLiteral("TransmissionMismatch"),
                     QStringLiteral("发送回执与待发送帧不一致，快照未被消费"));
        return;
    }

    // Consume and clear before emitting, so a slot reacting to success cannot
    // re-enter dispatch() and send the same snapshot again.
    const quint64 revision = pending_->revision;
    const QString fingerprint = pending_->command.fingerprint;
    consumedKeys_.insert(pendingKey_);
    pending_.reset();
    pendingKey_.clear();

    const QDateTime transmittedAtUtc =
        QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(frame.timestampUs / 1000u),
                                       QTimeZone::UTC);

    emit dispatchSucceeded(revision, fingerprint, transmittedAtUtc);
}

void BmsCommandMockDispatcher::onSessionError(const QString &message)
{
    if (!pending_.has_value()) {
        return;
    }

    // Nothing is consumed: once the session recovers the same confirmed snapshot
    // may be submitted again.
    const quint64 revision = pending_->revision;
    pending_.reset();
    pendingKey_.clear();
    failDispatch(revision, QStringLiteral("SessionRejected"), message);
}

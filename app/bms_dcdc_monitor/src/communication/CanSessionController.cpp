#include "communication/CanSessionController.h"

#include "communication/MockCanSource.h"

#include <QDateTime>

CanSessionController::CanSessionController(QObject *parent)
    : QObject(parent),
      mockSource_(new MockCanSource(this))
{
    qRegisterMetaType<CanSourceMode>("CanSourceMode");
    qRegisterMetaType<CanDriverType>("CanDriverType");
    qRegisterMetaType<CanConnectionState>("CanConnectionState");
    qRegisterMetaType<CanConnectionConfig>("CanConnectionConfig");

    connect(mockSource_,
            &MockCanSource::frameGenerated,
            this,
            &CanSessionController::onMockFrameGenerated);
}

CanSessionController::~CanSessionController()
{
    shutdown();
}

CanConnectionState CanSessionController::state() const
{
    return state_;
}

CanSourceMode CanSessionController::sourceMode() const
{
    return sourceMode_;
}

CanConnectionConfig CanSessionController::config() const
{
    return config_;
}

bool CanSessionController::isHardwareSupported() const
{
    return false;
}

QString CanSessionController::platformCapabilityText() const
{
    return QStringLiteral("当前构建不支持 ZLG ControlCAN，Hardware 模式不可用");
}

void CanSessionController::openDevice(const CanConnectionConfig &config)
{
    if (state_ != CanConnectionState::Disconnected) {
        reportError(QStringLiteral("当前状态不允许重复打开设备"));
        return;
    }

    config_ = config;
    setSourceMode(config.driverType == CanDriverType::Mock
                      ? CanSourceMode::Mock
                      : CanSourceMode::Hardware);
    emit sessionResetRequested();

    if (config.driverType == CanDriverType::Mock) {
        setState(CanConnectionState::Opening);
        setState(CanConnectionState::DeviceOpened);
        return;
    }

    // Hardware not supported in this build
    setState(CanConnectionState::Unsupported);
    emit errorOccurred(QStringLiteral("当前平台不支持 ZLG ControlCAN 硬件驱动"));
}

void CanSessionController::startChannel()
{
    if (!ensureState(CanConnectionState::DeviceOpened, QStringLiteral("启动通道"))) {
        return;
    }

    setState(CanConnectionState::Starting);
    if (config_.driverType == CanDriverType::Mock) {
        mockSource_->start();
        setState(CanConnectionState::ChannelStarted);
        return;
    }

    reportError(QStringLiteral("当前平台不支持硬件通道启动"));
}

void CanSessionController::stopChannel()
{
    if (!ensureState(CanConnectionState::ChannelStarted, QStringLiteral("停止通道"))) {
        return;
    }

    setState(CanConnectionState::Stopping);
    if (config_.driverType == CanDriverType::Mock) {
        mockSource_->stop();
        setState(CanConnectionState::DeviceOpened);
        return;
    }

    reportError(QStringLiteral("当前平台不支持硬件通道停止"));
}

void CanSessionController::closeDevice()
{
    if (state_ == CanConnectionState::Disconnected) {
        reportError(QStringLiteral("当前状态不允许关闭设备"));
        return;
    }

    if (state_ == CanConnectionState::Opening ||
        state_ == CanConnectionState::Starting ||
        state_ == CanConnectionState::Stopping ||
        state_ == CanConnectionState::Closing) {
        reportError(QStringLiteral("状态转换中，暂时不能关闭设备"));
        return;
    }

    if (config_.driverType == CanDriverType::Mock) {
        setState(CanConnectionState::Closing);
        mockSource_->stop();
        setState(CanConnectionState::Disconnected);
        return;
    }

    // Hardware unsupported or in Unsupported state — just reset
    setState(CanConnectionState::Disconnected);
}

void CanSessionController::sendFrame(const CanFrame &frame)
{
    if (state_ != CanConnectionState::ChannelStarted) {
        reportError(QStringLiteral("通道未启动，不能发送 CAN 帧"));
        return;
    }

    if (frame.payload.size() > 8) {
        reportError(QStringLiteral("Classic CAN Data 不能超过 8 字节"));
        return;
    }

    if (frame.remote && !frame.payload.isEmpty()) {
        reportError(QStringLiteral("远程帧不能携带 Data payload"));
        return;
    }

    const quint32 maxId = frame.extended ? 0x1FFFFFFFu : 0x7FFu;
    if (frame.id > maxId) {
        reportError(frame.extended
                        ? QStringLiteral("扩展帧 CAN ID 必须在 0x00000000 到 0x1FFFFFFF 范围内")
                        : QStringLiteral("标准帧 CAN ID 必须在 0x000 到 0x7FF 范围内"));
        return;
    }

    if (config_.driverType == CanDriverType::Mock) {
        CanFrame transmittedFrame = frame;
        transmittedFrame.direction = CanFrameDirection::Tx;
        transmittedFrame.timestampUs =
            static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000u;
        emit frameTransmitted(transmittedFrame);
        return;
    }

    reportError(QStringLiteral("当前平台不支持硬件帧发送"));
}

void CanSessionController::shutdown()
{
    shutdownCurrentSession();
}

void CanSessionController::onMockFrameGenerated(const CanFrame &frame)
{
    if (sourceMode_ == CanSourceMode::Mock &&
        state_ == CanConnectionState::ChannelStarted) {
        emit frameReceived(frame);
    }
}

void CanSessionController::setState(CanConnectionState state)
{
    if (state_ == state) {
        return;
    }

    state_ = state;
    emit stateChanged(state_);
}

void CanSessionController::setSourceMode(CanSourceMode mode)
{
    if (sourceMode_ == mode) {
        return;
    }

    sourceMode_ = mode;
    emit sourceModeChanged(mode);
}

bool CanSessionController::ensureState(CanConnectionState expected,
                                       const QString &operation)
{
    if (state_ == expected) {
        return true;
    }

    reportError(QStringLiteral("当前状态不允许%1").arg(operation));
    return false;
}

void CanSessionController::reportError(const QString &message)
{
    if (state_ != CanConnectionState::Unsupported) {
        setState(CanConnectionState::Error);
    }
    emit errorOccurred(message);
}

void CanSessionController::shutdownCurrentSession()
{
    if (mockSource_ != nullptr) {
        mockSource_->stop();
    }

    setState(CanConnectionState::Disconnected);
}

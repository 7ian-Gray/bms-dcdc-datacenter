#include "communication/CanSessionController.h"

#include "communication/MockCanSource.h"

#ifdef BMS_HAS_CONTROLCAN
#include "communication/ControlCanWorker.h"
#include <QThread>
#endif

#include <QDateTime>
#include <QMetaObject>

CanSessionController::CanSessionController(QObject *parent)
    : QObject(parent),
      mockSource_(new MockCanSource(this))
{
    qRegisterMetaType<CanSourceMode>("CanSourceMode");
    qRegisterMetaType<CanDriverType>("CanDriverType");
    qRegisterMetaType<CanConnectionState>("CanConnectionState");
    qRegisterMetaType<CanConnectionConfig>("CanConnectionConfig");
    qRegisterMetaType<CanFrame>("CanFrame");

    connect(mockSource_,
            &MockCanSource::frameGenerated,
            this,
            &CanSessionController::onMockFrameGenerated);

#ifdef BMS_HAS_CONTROLCAN
    hardwareThread_ = new QThread(this);
    hardwareWorker_ = new ControlCanWorker();
    hardwareWorker_->moveToThread(hardwareThread_);

    connect(hardwareThread_,
            &QThread::finished,
            hardwareWorker_,
            &QObject::deleteLater);
    connect(hardwareWorker_,
            &ControlCanWorker::deviceOpened,
            this,
            &CanSessionController::onHardwareDeviceOpened);
    connect(hardwareWorker_,
            &ControlCanWorker::channelStarted,
            this,
            &CanSessionController::onHardwareChannelStarted);
    connect(hardwareWorker_,
            &ControlCanWorker::channelStopped,
            this,
            &CanSessionController::onHardwareChannelStopped);
    connect(hardwareWorker_,
            &ControlCanWorker::deviceClosed,
            this,
            &CanSessionController::onHardwareDeviceClosed);
    connect(hardwareWorker_,
            &ControlCanWorker::frameReceived,
            this,
            &CanSessionController::onHardwareFrameReceived);
    connect(hardwareWorker_,
            &ControlCanWorker::frameTransmitted,
            this,
            &CanSessionController::onHardwareFrameTransmitted);
    connect(hardwareWorker_,
            &ControlCanWorker::errorOccurred,
            this,
            &CanSessionController::onHardwareError);

    hardwareThread_->start();
#endif
}

CanSessionController::~CanSessionController()
{
    shutdown();

#ifdef BMS_HAS_CONTROLCAN
    if (hardwareThread_ != nullptr && hardwareThread_->isRunning()) {
        hardwareThread_->quit();
        hardwareThread_->wait();
    }
    hardwareWorker_ = nullptr;
#endif
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
#ifdef BMS_HAS_CONTROLCAN
    return true;
#else
    return false;
#endif
}

QString CanSessionController::platformCapabilityText() const
{
    return isHardwareSupported()
               ? QStringLiteral("当前构建支持 ZLG ControlCAN")
               : QStringLiteral("当前平台/构建不支持 ZLG ControlCAN，Hardware 模式不可用");
}

void CanSessionController::openDevice(const CanConnectionConfig &config)
{
    if (state_ != CanConnectionState::Disconnected) {
        if (config.driverType != config_.driverType) {
            shutdownCurrentSession();
        } else {
            reportError(QStringLiteral("当前状态不允许重复打开设备"));
            return;
        }
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

    if (!isHardwareSupported()) {
        setState(CanConnectionState::Unsupported);
        emit errorOccurred(QStringLiteral("当前平台不支持 ZLG ControlCAN 硬件驱动"));
        return;
    }

#ifdef BMS_HAS_CONTROLCAN
    setState(CanConnectionState::Opening);
    QMetaObject::invokeMethod(hardwareWorker_,
                              "openDevice",
                              Qt::QueuedConnection,
                              Q_ARG(quint32, static_cast<quint32>(config_.deviceType)),
                              Q_ARG(quint32, static_cast<quint32>(config_.deviceIndex)),
                              Q_ARG(quint32, static_cast<quint32>(bitrateKbps())),
                              Q_ARG(quint8, static_cast<quint8>(config_.channel)),
                              Q_ARG(bool, config_.listenOnly),
                              Q_ARG(QString, QString()));
#endif
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

#ifdef BMS_HAS_CONTROLCAN
    QMetaObject::invokeMethod(hardwareWorker_,
                              "startChannel",
                              Qt::QueuedConnection,
                              Q_ARG(quint8, static_cast<quint8>(config_.channel)));
#endif
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

#ifdef BMS_HAS_CONTROLCAN
    QMetaObject::invokeMethod(hardwareWorker_,
                              "stopChannel",
                              Qt::QueuedConnection,
                              Q_ARG(quint8, static_cast<quint8>(config_.channel)));
#endif
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

    if (!isHardwareSupported() || state_ == CanConnectionState::Unsupported) {
        setState(CanConnectionState::Disconnected);
        return;
    }

#ifdef BMS_HAS_CONTROLCAN
    setState(CanConnectionState::Closing);
    QMetaObject::invokeMethod(hardwareWorker_,
                              "stopAndClose",
                              Qt::QueuedConnection);
#endif
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

    if (config_.driverType == CanDriverType::Mock) {
        CanFrame transmittedFrame = frame;
        transmittedFrame.direction = CanFrameDirection::Tx;
        transmittedFrame.timestampUs =
            static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000u;
        emit frameTransmitted(transmittedFrame);
        return;
    }

#ifdef BMS_HAS_CONTROLCAN
    QMetaObject::invokeMethod(hardwareWorker_,
                              "sendFrame",
                              Qt::QueuedConnection,
                              Q_ARG(CanFrame, frame));
#endif
}

void CanSessionController::shutdown()
{
    shuttingDown_ = true;
    shutdownCurrentSession();
}

void CanSessionController::onMockFrameGenerated(const CanFrame &frame)
{
    if (sourceMode_ == CanSourceMode::Mock &&
        state_ == CanConnectionState::ChannelStarted) {
        emit frameReceived(frame);
    }
}

void CanSessionController::onHardwareDeviceOpened(const QString &)
{
    if (!shuttingDown_) {
        setState(CanConnectionState::DeviceOpened);
    }
}

void CanSessionController::onHardwareChannelStarted(const QString &)
{
    if (!shuttingDown_) {
        setState(CanConnectionState::ChannelStarted);
    }
}

void CanSessionController::onHardwareChannelStopped(const QString &)
{
    if (!shuttingDown_) {
        setState(CanConnectionState::DeviceOpened);
    }
}

void CanSessionController::onHardwareDeviceClosed(const QString &)
{
    if (!shuttingDown_) {
        setState(CanConnectionState::Disconnected);
    }
}

void CanSessionController::onHardwareFrameReceived(const CanFrame &frame)
{
    if (sourceMode_ == CanSourceMode::Hardware &&
        state_ == CanConnectionState::ChannelStarted) {
        emit frameReceived(frame);
    }
}

void CanSessionController::onHardwareFrameTransmitted(const CanFrame &frame)
{
    if (sourceMode_ == CanSourceMode::Hardware &&
        state_ == CanConnectionState::ChannelStarted) {
        emit frameTransmitted(frame);
    }
}

void CanSessionController::onHardwareError(const QString &message)
{
    if (shuttingDown_) {
        return;
    }

    setState(CanConnectionState::Error);
    emit errorOccurred(message);
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

#ifdef BMS_HAS_CONTROLCAN
    if (hardwareWorker_ != nullptr && hardwareThread_ != nullptr &&
        hardwareThread_->isRunning()) {
        QMetaObject::invokeMethod(hardwareWorker_,
                                  "stopAndClose",
                                  Qt::BlockingQueuedConnection);
    }
#endif

    setState(CanConnectionState::Disconnected);
}

int CanSessionController::bitrateKbps() const
{
    return config_.bitrate >= 10000 ? config_.bitrate / 1000 : config_.bitrate;
}

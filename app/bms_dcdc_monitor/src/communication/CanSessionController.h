#pragma once

#include "communication/CanFrame.h"

#include <QObject>
#include <QString>

class MockCanSource;

enum class CanSourceMode : quint8
{
    Mock = 0,
    Hardware = 1
};

enum class CanDriverType : quint8
{
    Mock = 0,
    ZlgControlCan = 1
};

enum class CanConnectionState : quint8
{
    Disconnected = 0,
    Opening,
    DeviceOpened,
    Starting,
    ChannelStarted,
    Stopping,
    Closing,
    Error,
    Unsupported
};

struct CanConnectionConfig
{
    CanDriverType driverType = CanDriverType::Mock;
    int deviceType = 4;
    int deviceIndex = 0;
    int channel = 0;
    int bitrate = 500000;
    bool listenOnly = false;
};

Q_DECLARE_METATYPE(CanSourceMode)
Q_DECLARE_METATYPE(CanDriverType)
Q_DECLARE_METATYPE(CanConnectionState)
Q_DECLARE_METATYPE(CanConnectionConfig)

class CanSessionController : public QObject
{
    Q_OBJECT

public:
    explicit CanSessionController(QObject *parent = nullptr);
    ~CanSessionController() override;

    CanConnectionState state() const;
    CanSourceMode sourceMode() const;
    CanConnectionConfig config() const;
    bool isHardwareSupported() const;
    QString platformCapabilityText() const;

public slots:
    void openDevice(const CanConnectionConfig &config);
    void startChannel();
    void stopChannel();
    void closeDevice();
    void sendFrame(const CanFrame &frame);
    void shutdown();

signals:
    void stateChanged(CanConnectionState state);
    void frameReceived(const CanFrame &frame);
    void frameTransmitted(const CanFrame &frame);
    void errorOccurred(const QString &message);
    void sourceModeChanged(CanSourceMode mode);
    void sessionResetRequested();

private slots:
    void onMockFrameGenerated(const CanFrame &frame);

private:
    void setState(CanConnectionState state);
    void setSourceMode(CanSourceMode mode);
    bool ensureState(CanConnectionState expected, const QString &operation);
    void reportError(const QString &message);
    void shutdownCurrentSession();

    MockCanSource *mockSource_ = nullptr;
    CanConnectionConfig config_;
    CanConnectionState state_ = CanConnectionState::Disconnected;
    CanSourceMode sourceMode_ = CanSourceMode::Mock;
};

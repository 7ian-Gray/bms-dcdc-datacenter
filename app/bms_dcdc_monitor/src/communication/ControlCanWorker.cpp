#include "ControlCanWorker.h"

#ifdef Q_OS_WIN

#include "ControlCAN.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibrary>
#include <QTimer>

#include <array>
#include <cstring>

namespace
{
bool baudTiming(quint32 baudRateKbps, UCHAR &timing0, UCHAR &timing1)
{
    switch (baudRateKbps) {
    case 10:   timing0 = 0x31; timing1 = 0x1C; return true;
    case 20:   timing0 = 0x18; timing1 = 0x1C; return true;
    case 40:   timing0 = 0x87; timing1 = 0xFF; return true;
    case 50:   timing0 = 0x09; timing1 = 0x1C; return true;
    case 80:   timing0 = 0x83; timing1 = 0xFF; return true;
    case 100:  timing0 = 0x04; timing1 = 0x1C; return true;
    case 125:  timing0 = 0x03; timing1 = 0x1C; return true;
    case 200:  timing0 = 0x81; timing1 = 0xFA; return true;
    case 250:  timing0 = 0x01; timing1 = 0x1C; return true;
    case 400:  timing0 = 0x80; timing1 = 0xFA; return true;
    case 500:  timing0 = 0x00; timing1 = 0x1C; return true;
    case 666:  timing0 = 0x80; timing1 = 0xB6; return true;
    case 800:  timing0 = 0x00; timing1 = 0x16; return true;
    case 1000: timing0 = 0x00; timing1 = 0x14; return true;
    case 33:   timing0 = 0x09; timing1 = 0x6F; return true;
    case 66:   timing0 = 0x04; timing1 = 0x6F; return true;
    case 83:   timing0 = 0x03; timing1 = 0x6F; return true;
    default:
        return false;
    }
}
} // namespace

class ControlCanWorker::Impl
{
public:
    using OpenDeviceFn = DWORD (WINAPI *)(DWORD, DWORD, DWORD);
    using CloseDeviceFn = DWORD (WINAPI *)(DWORD, DWORD);
    using InitCanFn = DWORD (WINAPI *)(DWORD, DWORD, DWORD, PVCI_INIT_CONFIG);
    using StartCanFn = DWORD (WINAPI *)(DWORD, DWORD, DWORD);
    using ResetCanFn = DWORD (WINAPI *)(DWORD, DWORD, DWORD);
    using TransmitFn = ULONG (WINAPI *)(DWORD, DWORD, DWORD, PVCI_CAN_OBJ, ULONG);
    using ReceiveFn = ULONG (WINAPI *)(DWORD, DWORD, DWORD, PVCI_CAN_OBJ, ULONG, INT);

    QLibrary library;
    OpenDeviceFn openDevice = nullptr;
    CloseDeviceFn closeDevice = nullptr;
    InitCanFn initCan = nullptr;
    StartCanFn startCan = nullptr;
    ResetCanFn resetCan = nullptr;
    TransmitFn transmit = nullptr;
    ReceiveFn receive = nullptr;

    quint32 deviceType = 0;
    quint32 deviceIndex = 0;
    bool connected = false;

    bool resolveAll()
    {
        openDevice = reinterpret_cast<OpenDeviceFn>(library.resolve("VCI_OpenDevice"));
        closeDevice = reinterpret_cast<CloseDeviceFn>(library.resolve("VCI_CloseDevice"));
        initCan = reinterpret_cast<InitCanFn>(library.resolve("VCI_InitCAN"));
        startCan = reinterpret_cast<StartCanFn>(library.resolve("VCI_StartCAN"));
        resetCan = reinterpret_cast<ResetCanFn>(library.resolve("VCI_ResetCAN"));
        transmit = reinterpret_cast<TransmitFn>(library.resolve("VCI_Transmit"));
        receive = reinterpret_cast<ReceiveFn>(library.resolve("VCI_Receive"));

        return openDevice && closeDevice && initCan && startCan &&
               resetCan && transmit && receive;
    }
};

ControlCanWorker::ControlCanWorker(QObject *parent)
    : QObject(parent),
      impl_(new Impl),
      pollTimer_(new QTimer(this))
{
    pollTimer_->setInterval(10);
    connect(pollTimer_, &QTimer::timeout,
            this, &ControlCanWorker::pollReceive);
}

ControlCanWorker::~ControlCanWorker()
{
    stopAndClose();
    delete impl_;
}

void ControlCanWorker::openDevice(quint32 deviceType,
                                  quint32 deviceIndex,
                                  quint32 baudRateKbps,
                                  const QString &libraryPath)
{
    stopAndClose();

    UCHAR timing0 = 0;
    UCHAR timing1 = 0;
    if (!baudTiming(baudRateKbps, timing0, timing1)) {
        emit errorOccurred(QStringLiteral("Unsupported ControlCAN baud rate: %1 Kbps")
                               .arg(baudRateKbps));
        return;
    }

    const QString resolvedPath =
        libraryPath.isEmpty()
            ? QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("ControlCAN.dll"))
            : libraryPath;

    impl_->library.setFileName(resolvedPath);
    if (!impl_->library.load()) {
        emit errorOccurred(QStringLiteral("Failed to load %1: %2")
                               .arg(resolvedPath, impl_->library.errorString()));
        return;
    }

    if (!impl_->resolveAll()) {
        emit errorOccurred(QStringLiteral("ControlCAN.dll is missing one or more required VCI symbols"));
        impl_->library.unload();
        return;
    }

    impl_->deviceType = deviceType;
    impl_->deviceIndex = deviceIndex;

    if (impl_->openDevice(deviceType, deviceIndex, 0) != STATUS_OK) {
        emit errorOccurred(QStringLiteral("VCI_OpenDevice failed"));
        impl_->library.unload();
        return;
    }

    VCI_INIT_CONFIG config{};
    config.AccCode = 0;
    config.AccMask = 0xFFFFFFFFu;
    config.Filter = 1;
    config.Timing0 = timing0;
    config.Timing1 = timing1;
    config.Mode = 0;

    for (DWORD channel = 0; channel < 2; ++channel) {
        if (impl_->initCan(deviceType, deviceIndex, channel, &config) != STATUS_OK) {
            emit errorOccurred(QStringLiteral("VCI_InitCAN failed on channel %1").arg(channel));
            stopAndClose();
            return;
        }

        if (impl_->startCan(deviceType, deviceIndex, channel) != STATUS_OK) {
            emit errorOccurred(QStringLiteral("VCI_StartCAN failed on channel %1").arg(channel));
            stopAndClose();
            return;
        }
    }

    impl_->connected = true;
    pollTimer_->start();
    emit connectionChanged(true,
                           QStringLiteral("ControlCAN device opened: type=%1 index=%2 baud=%3 Kbps")
                               .arg(deviceType)
                               .arg(deviceIndex)
                               .arg(baudRateKbps));
}

void ControlCanWorker::stopAndClose()
{
    pollTimer_->stop();

    if (impl_ == nullptr) {
        return;
    }

    if (impl_->connected && impl_->resetCan) {
        impl_->resetCan(impl_->deviceType, impl_->deviceIndex, 0);
        impl_->resetCan(impl_->deviceType, impl_->deviceIndex, 1);
    }

    if (impl_->connected && impl_->closeDevice) {
        impl_->closeDevice(impl_->deviceType, impl_->deviceIndex);
    }

    const bool wasConnected = impl_->connected;
    impl_->connected = false;

    if (impl_->library.isLoaded()) {
        impl_->library.unload();
    }

    if (wasConnected) {
        emit connectionChanged(false, QStringLiteral("ControlCAN device closed"));
    }
}

void ControlCanWorker::sendFrame(const CanFrame &frame)
{
    if (!impl_->connected || !impl_->transmit) {
        emit errorOccurred(QStringLiteral("Cannot transmit: ControlCAN device is not connected"));
        return;
    }

    if (frame.payload.size() > 8) {
        emit errorOccurred(QStringLiteral("Classic CAN payload cannot exceed 8 bytes"));
        return;
    }

    VCI_CAN_OBJ object{};
    object.ID = frame.id;
    object.RemoteFlag = frame.remote ? 1 : 0;
    object.ExternFlag = frame.extended ? 1 : 0;
    object.DataLen = static_cast<BYTE>(frame.payload.size());

    if (!frame.payload.isEmpty()) {
        std::memcpy(object.Data,
                    frame.payload.constData(),
                    static_cast<size_t>(frame.payload.size()));
    }

    const ULONG sent = impl_->transmit(impl_->deviceType,
                                       impl_->deviceIndex,
                                       frame.channel,
                                       &object,
                                       1);
    if (sent != 1) {
        emit errorOccurred(QStringLiteral("VCI_Transmit failed on channel %1")
                               .arg(frame.channel));
    }
}

void ControlCanWorker::pollReceive()
{
    if (!impl_->connected || !impl_->receive) {
        return;
    }

    std::array<VCI_CAN_OBJ, 256> buffer{};

    for (DWORD channel = 0; channel < 2; ++channel) {
        const ULONG count = impl_->receive(impl_->deviceType,
                                           impl_->deviceIndex,
                                           channel,
                                           buffer.data(),
                                           static_cast<ULONG>(buffer.size()),
                                           0);

        for (ULONG index = 0; index < count && index < buffer.size(); ++index) {
            const VCI_CAN_OBJ &object = buffer.at(index);

            CanFrame frame;
            frame.id = object.ID;
            frame.channel = static_cast<quint8>(channel);
            frame.extended = object.ExternFlag != 0;
            frame.remote = object.RemoteFlag != 0;
            frame.timestampUs = object.TimeFlag != 0
                                    ? static_cast<quint64>(object.TimeStamp) * 100u
                                    : 0u;
            frame.payload = QByteArray(
                reinterpret_cast<const char *>(object.Data),
                qMin<int>(object.DataLen, 8));

            emit frameReceived(frame);
        }
    }
}

#else

ControlCanWorker::ControlCanWorker(QObject *parent)
    : QObject(parent)
{
}

ControlCanWorker::~ControlCanWorker() = default;

void ControlCanWorker::openDevice(quint32,
                                  quint32,
                                  quint32,
                                  const QString &)
{
    emit errorOccurred(QStringLiteral("ControlCAN.dll is available only on Windows"));
}

void ControlCanWorker::stopAndClose()
{
}

void ControlCanWorker::sendFrame(const CanFrame &)
{
    emit errorOccurred(QStringLiteral("ControlCAN.dll is available only on Windows"));
}

void ControlCanWorker::pollReceive()
{
}

#endif

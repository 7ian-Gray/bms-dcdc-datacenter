#pragma once

#include "CanFrame.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

class QTimer;

class ControlCanWorker : public QObject
{
    Q_OBJECT

public:
    explicit ControlCanWorker(QObject *parent = nullptr);
    ~ControlCanWorker() override;

public slots:
    void openDevice(quint32 deviceType,
                    quint32 deviceIndex,
                    quint32 baudRateKbps,
                    quint8 channel,
                    bool listenOnly,
                    const QString &libraryPath = QString());
    void startChannel(quint8 channel);
    void stopChannel(quint8 channel);
    void stopAndClose();
    void sendFrame(const CanFrame &frame);

signals:
    void deviceOpened(const QString &message);
    void channelStarted(const QString &message);
    void channelStopped(const QString &message);
    void deviceClosed(const QString &message);
    void frameReceived(const CanFrame &frame);
    void frameTransmitted(const CanFrame &frame);
    void connectionChanged(bool connected, const QString &message);
    void errorOccurred(const QString &message);

private slots:
    void pollReceive();

private:
    class Impl;
    Impl *impl_ = nullptr;
    QTimer *pollTimer_ = nullptr;
};

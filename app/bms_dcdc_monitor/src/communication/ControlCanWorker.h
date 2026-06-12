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
                    const QString &libraryPath = QString());
    void stopAndClose();
    void sendFrame(const CanFrame &frame);

signals:
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

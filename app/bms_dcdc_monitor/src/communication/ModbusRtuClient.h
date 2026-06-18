#pragma once

#include "protocol/ModbusRtuFrame.h"

#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <optional>

class ModbusTransport;

class ModbusRtuClient : public QObject
{
    Q_OBJECT

public:
    explicit ModbusRtuClient(ModbusTransport *transport, QObject *parent = nullptr);

    bool hasPendingRequest() const;
    int timeoutMs() const;
    int maxRetries() const;

    void setTimeoutMs(int timeoutMs);
    void setMaxRetries(int maxRetries);

    bool readHoldingRegisters(quint8 slaveAddress,
                              quint16 startRegister,
                              quint16 registerCount,
                              QString *error = nullptr);

    void cancelPendingRequest();

signals:
    void registersRead(quint8 slaveAddress,
                       quint16 startRegister,
                       QVector<quint16> registers);

    void requestFailed(QString message);

    void rawFrameSent(QByteArray frame);
    void rawFrameReceived(QByteArray frame);

private slots:
    void onFrameReceived(const QByteArray &frame);
    void onTimeout();

private:
    bool sendPendingRequest(QString *error = nullptr);

    ModbusTransport *transport_ = nullptr;
    QTimer requestTimeoutTimer_;

    int timeoutMs_ = 500;
    int maxRetries_ = 2;

    struct PendingRequest
    {
        PendingModbusRequest modbus;
        QByteArray frame;
        int retryCount = 0;
    };

    std::optional<PendingRequest> pending_;
};

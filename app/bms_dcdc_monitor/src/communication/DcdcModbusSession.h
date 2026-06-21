#pragma once

#include "ModbusRtuClient.h"
#include "ModbusTransport.h"
#include "protocol/DcdcModbusRegisterMap.h"

#include <QByteArray>
#include <QObject>
#include <QTimer>

class DcdcModbusSession : public QObject
{
    Q_OBJECT

public:
    explicit DcdcModbusSession(ModbusTransport *transport,
                               QObject *parent = nullptr);

    bool isRunning() const;

    int pollIntervalMs() const;
    void setPollIntervalMs(int pollIntervalMs);

    quint8 slaveAddress() const;
    void setSlaveAddress(quint8 slaveAddress);

    quint16 startRegister() const;
    quint16 registerCount() const;
    void setReadRange(quint16 startRegister, quint16 registerCount);

    int requestTimeoutMs() const;
    void setRequestTimeoutMs(int timeoutMs);

    int maxRetries() const;
    void setMaxRetries(int maxRetries);

public slots:
    void start();
    void stop();

signals:
    void dcdcDataDecoded(DcdcRegisterDecodeResult result);
    void communicationError(QString message);
    void rawFrameSent(QByteArray frame);
    void rawFrameReceived(QByteArray frame);

private slots:
    void pollNow();
    void onRegistersRead(quint8 slaveAddress,
                         quint16 startRegister,
                         QVector<quint16> registers);
    void onRequestFailed(QString message);

private:
    // Borrowed transport; the caller must keep it alive for the lifetime of this session.
    ModbusTransport *transport_ = nullptr;
    ModbusRtuClient client_;
    QTimer pollTimer_;

    bool running_ = false;
    int pollIntervalMs_ = 1000;
    // Default DCDC slave address is provisional and must be confirmed against the actual device protocol.
    quint8 slaveAddress_ = 1;
    quint16 startRegister_ = DcdcModbusRegisterMap::DefaultStartRegister;
    quint16 registerCount_ = DcdcModbusRegisterMap::DefaultRegisterCount;
};

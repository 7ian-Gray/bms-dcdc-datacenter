#pragma once

#include "ModbusTransport.h"

#include <QByteArray>
#include <QSerialPort>
#include <QTimer>

class QSerialPortModbusTransport : public ModbusTransport
{
    Q_OBJECT

public:
    explicit QSerialPortModbusTransport(QObject *parent = nullptr);
    ~QSerialPortModbusTransport() override = default;

    bool isOpen() const override;
    bool writeFrame(const QByteArray &frame) override;

    QString portName() const;
    qint32 baudRate() const;
    QSerialPort::DataBits dataBits() const;
    QSerialPort::Parity parity() const;
    QSerialPort::StopBits stopBits() const;
    QSerialPort::FlowControl flowControl() const;
    int interFrameTimeoutMs() const;

    void setPortName(const QString &portName);
    void setBaudRate(qint32 baudRate);
    void setDataBits(QSerialPort::DataBits dataBits);
    void setParity(QSerialPort::Parity parity);
    void setStopBits(QSerialPort::StopBits stopBits);
    void setFlowControl(QSerialPort::FlowControl flowControl);
    void setInterFrameTimeoutMs(int timeoutMs);

    bool open(QString *error = nullptr);
    void close();

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);
    void emitBufferedFrame();

private:
    void clearReadBuffer();

    QSerialPort serialPort_;
    QByteArray readBuffer_;
    QTimer interFrameTimer_;
    int interFrameTimeoutMs_ = 20;
};

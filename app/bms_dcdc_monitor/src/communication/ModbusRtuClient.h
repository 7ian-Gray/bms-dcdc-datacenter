#pragma once

#include <QByteArray>
#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QVector>
#include <QtGlobal>

class ModbusRtuClient : public QObject
{
    Q_OBJECT

public:
    explicit ModbusRtuClient(QObject *parent = nullptr);

    bool open(const QString &portName, qint32 baudRate);
    void close();
    bool isOpen() const;

    void startPolling(quint8 slaveAddress,
                      quint16 startRegister,
                      quint16 registerCount,
                      int intervalMs);
    void stopPolling();

public slots:
    void readHoldingRegisters(quint8 slaveAddress,
                              quint16 startRegister,
                              quint16 registerCount);

signals:
    void connectionChanged(bool connected, const QString &message);
    void rawFrameReceived(const QByteArray &frame);
    void holdingRegistersReceived(quint8 slaveAddress,
                                  quint16 startRegister,
                                  const QVector<quint16> &registers);
    void protocolError(const QString &message);

private slots:
    void onReadyRead();
    void onPollTimeout();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    static quint16 crc16(const QByteArray &data);
    static bool hasValidCrc(const QByteArray &frame);
    void parseBufferedFrames();

    QSerialPort serialPort_;
    QTimer pollTimer_;
    QByteArray receiveBuffer_;

    quint8 pollSlaveAddress_ = 1;
    quint16 pollStartRegister_ = 0;
    quint16 pollRegisterCount_ = 1;

    quint16 pendingStartRegister_ = 0;
};

#pragma once

#include <QByteArray>
#include <QObject>
#include <QSerialPort>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QtGlobal>

#include <optional>

enum class ModbusError
{
    InvalidRequest,
    PortNotOpen,
    CrcMismatch,
    ResponseMismatch,
    ExceptionResponse,
    Timeout,
    BufferOverflow
};

struct PendingModbusRequest
{
    quint8 slaveAddress = 0;
    quint8 functionCode = 0;
    quint16 startRegister = 0;
    quint16 registerCount = 0;
    int retryCount = 0;
    QByteArray requestFrame;
};

struct ModbusResponseValidation
{
    enum class Status
    {
        Complete,
        NeedMoreData,
        CrcMismatch,
        ResponseMismatch,
        ExceptionResponse,
        InvalidFrame
    };

    Status status = Status::NeedMoreData;
    int frameLength = 0;
    quint8 slaveAddress = 0;
    quint8 functionCode = 0;
    quint8 exceptionCode = 0;
    QVector<quint16> registers;
    QString error;
};

class ModbusRtuClient : public QObject
{
    Q_OBJECT

public:
    explicit ModbusRtuClient(QObject *parent = nullptr);

    static constexpr quint8 ReadHoldingRegistersFunction = 0x03;

    static quint16 crc16(const QByteArray &data);
    static QByteArray buildReadHoldingRegistersRequest(quint8 slaveAddress,
                                                       quint16 startRegister,
                                                       quint16 registerCount);
    static bool isValidRequestRange(quint8 slaveAddress,
                                    quint16 startRegister,
                                    quint16 registerCount,
                                    QString *error = nullptr);
    static ModbusResponseValidation validateResponse(const QByteArray &buffer,
                                                     const PendingModbusRequest &pending);

    bool open(const QString &portName, qint32 baudRate);
    void close();
    bool isOpen() const;

    void setRequestTimeoutMs(int timeoutMs);
    void setMaxRetries(int maxRetries);

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
    void errorOccurred(ModbusError error, const QString &message);
    void protocolError(const QString &message);
    void requestTimedOut(quint8 slaveAddress,
                         quint8 functionCode,
                         quint16 startRegister,
                         quint16 registerCount);
    void requestRetried(int currentRetry, int maxRetries);

private slots:
    void onReadyRead();
    void onPollTimeout();
    void onRequestTimeout();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    static bool hasValidCrc(const QByteArray &frame);
    static QString errorMessage(ModbusError error, const QString &detail);

    void emitError(ModbusError error, const QString &detail);
    bool sendReadHoldingRegisters(quint8 slaveAddress,
                                  quint16 startRegister,
                                  quint16 registerCount);
    void writePendingRequest();
    void clearPendingRequest(bool clearReceiveBuffer);
    void parseBufferedFrames();

    // QSerialPort and QTimer are value members; keep this object in one thread.
    QSerialPort serialPort_;
    QTimer pollTimer_;
    QTimer requestTimeoutTimer_;
    QByteArray receiveBuffer_;

    quint8 pollSlaveAddress_ = 1;
    quint16 pollStartRegister_ = 0;
    quint16 pollRegisterCount_ = 1;

    int requestTimeoutMs_ = 500;
    int maxRetries_ = 2;
    std::optional<PendingModbusRequest> pendingRequest_;
};

Q_DECLARE_METATYPE(ModbusError)

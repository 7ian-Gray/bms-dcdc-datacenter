#include "DcdcModbusSession.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(dcdcModbusSessionLog, "bms.dcdc.modbus.session")

DcdcModbusSession::DcdcModbusSession(ModbusTransport *transport,
                                     QObject *parent)
    : QObject(parent)
    , transport_(transport)
    , client_(transport, this)
{
    Q_ASSERT(transport_ != nullptr);

    pollTimer_.setSingleShot(false);
    pollTimer_.setInterval(pollIntervalMs_);

    connect(&pollTimer_, &QTimer::timeout,
            this, &DcdcModbusSession::pollNow);
    connect(&client_, &ModbusRtuClient::registersRead,
            this, &DcdcModbusSession::onRegistersRead);
    connect(&client_, &ModbusRtuClient::requestFailed,
            this, &DcdcModbusSession::onRequestFailed);
    connect(&client_, &ModbusRtuClient::rawFrameSent,
            this, &DcdcModbusSession::rawFrameSent);
    connect(&client_, &ModbusRtuClient::rawFrameReceived,
            this, &DcdcModbusSession::rawFrameReceived);
}

bool DcdcModbusSession::isRunning() const
{
    return running_;
}

int DcdcModbusSession::pollIntervalMs() const
{
    return pollIntervalMs_;
}

void DcdcModbusSession::setPollIntervalMs(int pollIntervalMs)
{
    pollIntervalMs_ = qMax(pollIntervalMs, 1);
    pollTimer_.setInterval(pollIntervalMs_);
}

quint8 DcdcModbusSession::slaveAddress() const
{
    return slaveAddress_;
}

void DcdcModbusSession::setSlaveAddress(quint8 slaveAddress)
{
    slaveAddress_ = slaveAddress;
}

quint16 DcdcModbusSession::startRegister() const
{
    return startRegister_;
}

quint16 DcdcModbusSession::registerCount() const
{
    return registerCount_;
}

void DcdcModbusSession::setReadRange(quint16 startRegister, quint16 registerCount)
{
    startRegister_ = startRegister;
    registerCount_ = registerCount;
}

int DcdcModbusSession::requestTimeoutMs() const
{
    return client_.timeoutMs();
}

void DcdcModbusSession::setRequestTimeoutMs(int timeoutMs)
{
    client_.setTimeoutMs(timeoutMs);
}

int DcdcModbusSession::maxRetries() const
{
    return client_.maxRetries();
}

void DcdcModbusSession::setMaxRetries(int maxRetries)
{
    client_.setMaxRetries(maxRetries);
}

void DcdcModbusSession::start()
{
    if (running_) {
        return;
    }

    running_ = true;
    qCDebug(dcdcModbusSessionLog) << "Starting DCDC Modbus session";
    pollTimer_.start(pollIntervalMs_);
    pollNow();
}

void DcdcModbusSession::stop()
{
    if (!running_) {
        return;
    }

    running_ = false;
    qCDebug(dcdcModbusSessionLog) << "Stopping DCDC Modbus session";
    pollTimer_.stop();
    client_.cancelPendingRequest();
}

void DcdcModbusSession::pollNow()
{
    if (!running_) {
        return;
    }

    // ModbusRtuClient supports a single pending request. Skip this tick instead of queuing another request.
    if (client_.hasPendingRequest()) {
        qCDebug(dcdcModbusSessionLog) << "Skipping DCDC poll because a request is already pending";
        return;
    }

    qCDebug(dcdcModbusSessionLog) << "Polling DCDC registers"
                                  << "slave" << slaveAddress_
                                  << "start" << startRegister_
                                  << "count" << registerCount_;

    QString error;
    if (!client_.readHoldingRegisters(slaveAddress_, startRegister_, registerCount_, &error)) {
        qCWarning(dcdcModbusSessionLog) << "DCDC Modbus request failed:" << error;
        emit communicationError(error);
    }
}

void DcdcModbusSession::onRegistersRead(quint8 /*slaveAddress*/,
                                        quint16 startRegister,
                                        QVector<quint16> registers)
{
    const auto result = DcdcModbusRegisterMap::decodeHoldingRegisters(
        startRegister, registers);

    // Emit the decode result even when it contains errors so callers can inspect partial values and diagnostics.
    emit dcdcDataDecoded(result);

    if (!result.ok) {
        qCWarning(dcdcModbusSessionLog) << "DCDC register decode failed:" << result.errors.join("; ");
        emit communicationError(result.errors.join("; "));
    }
}

void DcdcModbusSession::onRequestFailed(QString message)
{
    qCWarning(dcdcModbusSessionLog) << "DCDC Modbus request failed:" << message;
    emit communicationError(message);
}

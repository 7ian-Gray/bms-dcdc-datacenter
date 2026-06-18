#include "ModbusRtuClient.h"
#include "ModbusTransport.h"

ModbusRtuClient::ModbusRtuClient(ModbusTransport *transport, QObject *parent)
    : QObject(parent)
    , transport_(transport)
{
    requestTimeoutTimer_.setSingleShot(true);

    connect(transport_, &ModbusTransport::frameReceived,
            this, &ModbusRtuClient::onFrameReceived);
    connect(&requestTimeoutTimer_, &QTimer::timeout,
            this, &ModbusRtuClient::onTimeout);
}

bool ModbusRtuClient::hasPendingRequest() const
{
    return pending_.has_value();
}

int ModbusRtuClient::timeoutMs() const
{
    return timeoutMs_;
}

int ModbusRtuClient::maxRetries() const
{
    return maxRetries_;
}

void ModbusRtuClient::setTimeoutMs(int timeoutMs)
{
    timeoutMs_ = qMax(timeoutMs, 1);
}

void ModbusRtuClient::setMaxRetries(int maxRetries)
{
    maxRetries_ = qMax(maxRetries, 0);
}

bool ModbusRtuClient::readHoldingRegisters(quint8 slaveAddress,
                                           quint16 startRegister,
                                           quint16 registerCount,
                                           QString *error)
{
    if (pending_.has_value()) {
        if (error) {
            *error = QStringLiteral("A request is already pending");
        }
        return false;
    }

    if (!transport_ || !transport_->isOpen()) {
        if (error) {
            *error = QStringLiteral("Transport is not open");
        }
        return false;
    }

    if (!ModbusRtuFrame::isValidReadHoldingRegistersRequest(
            slaveAddress, startRegister, registerCount, error)) {
        return false;
    }

    PendingRequest req;
    req.modbus.slaveAddress = slaveAddress;
    req.modbus.functionCode = ModbusRtuFrame::ReadHoldingRegistersFunction;
    req.modbus.startRegister = startRegister;
    req.modbus.registerCount = registerCount;
    req.frame = ModbusRtuFrame::buildReadHoldingRegistersRequest(
        slaveAddress, startRegister, registerCount);
    req.retryCount = 0;

    pending_ = req;
    return sendPendingRequest(error);
}

void ModbusRtuClient::cancelPendingRequest()
{
    requestTimeoutTimer_.stop();
    pending_.reset();
}

void ModbusRtuClient::onFrameReceived(const QByteArray &frame)
{
    emit rawFrameReceived(frame);

    if (!pending_.has_value()) {
        return;
    }

    const ModbusResponseValidation result =
        ModbusRtuFrame::validateReadHoldingRegistersResponse(frame, pending_->modbus);

    switch (result.error) {
    case ModbusRtuError::None:
        requestTimeoutTimer_.stop();
        emit registersRead(pending_->modbus.slaveAddress,
                           pending_->modbus.startRegister,
                           result.registers);
        pending_.reset();
        break;

    case ModbusRtuError::NeedMoreData:
        // Wait for more data
        break;

    case ModbusRtuError::ExceptionResponse:
        requestTimeoutTimer_.stop();
        emit requestFailed(result.message);
        pending_.reset();
        break;

    case ModbusRtuError::CrcMismatch:
    case ModbusRtuError::ResponseMismatch:
        requestTimeoutTimer_.stop();
        emit requestFailed(result.message);
        pending_.reset();
        break;

    default:
        requestTimeoutTimer_.stop();
        emit requestFailed(result.message);
        pending_.reset();
        break;
    }
}

void ModbusRtuClient::onTimeout()
{
    if (!pending_.has_value()) {
        return;
    }

    if (pending_->retryCount < maxRetries_) {
        ++pending_->retryCount;
        sendPendingRequest();
        return;
    }

    const QString message = QStringLiteral("Request timed out after %1 retries")
                                .arg(pending_->retryCount);
    emit requestFailed(message);
    pending_.reset();
}

bool ModbusRtuClient::sendPendingRequest(QString *error)
{
    if (!pending_.has_value()) {
        if (error) {
            *error = QStringLiteral("No pending request");
        }
        return false;
    }

    if (!transport_->writeFrame(pending_->frame)) {
        if (error) {
            *error = QStringLiteral("Failed to write frame to transport");
        }
        pending_.reset();
        return false;
    }

    emit rawFrameSent(pending_->frame);
    requestTimeoutTimer_.start(timeoutMs_);
    return true;
}

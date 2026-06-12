#include "ModbusRtuClient.h"

#include <QIODevice>

namespace
{
constexpr int MaxReceiveBufferSize = 4096;
constexpr int MinPollIntervalMs = 50;
constexpr int MinimumRequestTimeoutMs = 1;
constexpr int MinimumRetryCount = 0;
constexpr int ModbusExceptionFrameLength = 5;
constexpr int ModbusNormalHeaderLength = 3;
constexpr int ModbusCrcLength = 2;
constexpr quint8 ModbusExceptionBit = 0x80;
}

ModbusRtuClient::ModbusRtuClient(QObject *parent)
    : QObject(parent)
{
    requestTimeoutTimer_.setSingleShot(true);

    connect(&serialPort_, &QSerialPort::readyRead,
            this, &ModbusRtuClient::onReadyRead);
    connect(&serialPort_, &QSerialPort::errorOccurred,
            this, &ModbusRtuClient::onSerialError);
    connect(&pollTimer_, &QTimer::timeout,
            this, &ModbusRtuClient::onPollTimeout);
    connect(&requestTimeoutTimer_, &QTimer::timeout,
            this, &ModbusRtuClient::onRequestTimeout);
}

quint16 ModbusRtuClient::crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;

    for (const char byte : data) {
        crc ^= static_cast<quint8>(byte);

        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x0001u) != 0u) {
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001u);
            } else {
                crc = static_cast<quint16>(crc >> 1);
            }
        }
    }

    return crc;
}

QByteArray ModbusRtuClient::buildReadHoldingRegistersRequest(quint8 slaveAddress,
                                                             quint16 startRegister,
                                                             quint16 registerCount)
{
    QByteArray request;
    request.reserve(8);
    request.append(static_cast<char>(slaveAddress));
    request.append(static_cast<char>(ReadHoldingRegistersFunction));
    request.append(static_cast<char>((startRegister >> 8) & 0xFF));
    request.append(static_cast<char>(startRegister & 0xFF));
    request.append(static_cast<char>((registerCount >> 8) & 0xFF));
    request.append(static_cast<char>(registerCount & 0xFF));

    const quint16 checksum = crc16(request);
    request.append(static_cast<char>(checksum & 0xFF));
    request.append(static_cast<char>((checksum >> 8) & 0xFF));
    return request;
}

bool ModbusRtuClient::isValidRequestRange(quint8 slaveAddress,
                                          quint16 startRegister,
                                          quint16 registerCount,
                                          QString *error)
{
    if (slaveAddress == 0 || slaveAddress > 247) {
        if (error != nullptr) {
            *error = QStringLiteral("Modbus slave address must be in the range 1..247");
        }
        return false;
    }

    if (registerCount == 0 || registerCount > 125) {
        if (error != nullptr) {
            *error = QStringLiteral("Modbus register count must be in the range 1..125");
        }
        return false;
    }

    const quint32 endRegister =
        static_cast<quint32>(startRegister) + static_cast<quint32>(registerCount) - 1u;
    if (endRegister > 0xFFFFu) {
        if (error != nullptr) {
            *error = QStringLiteral("Modbus register range exceeds 16-bit address space");
        }
        return false;
    }

    return true;
}

ModbusResponseValidation ModbusRtuClient::validateResponse(const QByteArray &buffer,
                                                           const PendingModbusRequest &pending)
{
    ModbusResponseValidation result;

    if (buffer.size() < ModbusExceptionFrameLength) {
        return result;
    }

    result.slaveAddress = static_cast<quint8>(buffer.at(0));
    result.functionCode = static_cast<quint8>(buffer.at(1));

    if (result.functionCode == (pending.functionCode | ModbusExceptionBit)) {
        result.frameLength = ModbusExceptionFrameLength;
    } else if (result.functionCode == pending.functionCode) {
        const quint8 byteCount = static_cast<quint8>(buffer.at(2));
        const int expectedByteCount = static_cast<int>(pending.registerCount) * 2;
        result.frameLength = ModbusNormalHeaderLength + byteCount + ModbusCrcLength;

        if (byteCount != expectedByteCount) {
            result.status = ModbusResponseValidation::Status::ResponseMismatch;
            result.error = QStringLiteral("Modbus byte count does not match the pending request");
            if (buffer.size() < result.frameLength) {
                result.status = ModbusResponseValidation::Status::NeedMoreData;
                result.error.clear();
            }
            return result;
        }
    } else {
        result.status = ModbusResponseValidation::Status::ResponseMismatch;
        result.frameLength = 1;
        result.error = QStringLiteral("Modbus response function code does not match the pending request");
        return result;
    }

    if (buffer.size() < result.frameLength) {
        result.status = ModbusResponseValidation::Status::NeedMoreData;
        return result;
    }

    const QByteArray frame = buffer.left(result.frameLength);
    if (!hasValidCrc(frame)) {
        result.status = ModbusResponseValidation::Status::CrcMismatch;
        result.error = QStringLiteral("Modbus response CRC mismatch");
        return result;
    }

    if (result.slaveAddress != pending.slaveAddress) {
        result.status = ModbusResponseValidation::Status::ResponseMismatch;
        result.error = QStringLiteral("Modbus response slave address does not match the pending request");
        return result;
    }

    if (result.functionCode == (pending.functionCode | ModbusExceptionBit)) {
        result.exceptionCode = static_cast<quint8>(frame.at(2));
        result.status = ModbusResponseValidation::Status::ExceptionResponse;
        result.error =
            QStringLiteral("Modbus exception response: function=0x%1, code=0x%2")
                .arg(pending.functionCode, 2, 16, QLatin1Char('0'))
                .arg(result.exceptionCode, 2, 16, QLatin1Char('0'));
        return result;
    }

    const quint8 byteCount = static_cast<quint8>(frame.at(2));
    result.registers.reserve(byteCount / 2);
    for (int offset = ModbusNormalHeaderLength;
         offset < ModbusNormalHeaderLength + byteCount;
         offset += 2) {
        const quint16 value =
            (static_cast<quint16>(static_cast<quint8>(frame.at(offset))) << 8) |
            static_cast<quint8>(frame.at(offset + 1));
        result.registers.append(value);
    }

    result.status = ModbusResponseValidation::Status::Complete;
    return result;
}

bool ModbusRtuClient::open(const QString &portName, qint32 baudRate)
{
    close();

    serialPort_.setPortName(portName);
    serialPort_.setBaudRate(baudRate);
    serialPort_.setDataBits(QSerialPort::Data8);
    serialPort_.setParity(QSerialPort::NoParity);
    serialPort_.setStopBits(QSerialPort::OneStop);
    serialPort_.setFlowControl(QSerialPort::NoFlowControl);

    if (!serialPort_.open(QIODevice::ReadWrite)) {
        emit connectionChanged(false, serialPort_.errorString());
        emitError(ModbusError::PortNotOpen, serialPort_.errorString());
        return false;
    }

    receiveBuffer_.clear();
    emit connectionChanged(true, QStringLiteral("Modbus RTU serial port opened"));
    return true;
}

void ModbusRtuClient::close()
{
    pollTimer_.stop();
    clearPendingRequest(true);

    if (serialPort_.isOpen()) {
        serialPort_.close();
        emit connectionChanged(false, QStringLiteral("Modbus RTU serial port closed"));
    }
}

bool ModbusRtuClient::isOpen() const
{
    return serialPort_.isOpen();
}

void ModbusRtuClient::setRequestTimeoutMs(int timeoutMs)
{
    requestTimeoutMs_ = qMax(timeoutMs, MinimumRequestTimeoutMs);
}

void ModbusRtuClient::setMaxRetries(int maxRetries)
{
    maxRetries_ = qMax(maxRetries, MinimumRetryCount);
}

void ModbusRtuClient::startPolling(quint8 slaveAddress,
                                   quint16 startRegister,
                                   quint16 registerCount,
                                   int intervalMs)
{
    QString error;
    if (!isValidRequestRange(slaveAddress, startRegister, registerCount, &error)) {
        emitError(ModbusError::InvalidRequest, error);
        return;
    }

    pollSlaveAddress_ = slaveAddress;
    pollStartRegister_ = startRegister;
    pollRegisterCount_ = registerCount;
    pollTimer_.start(qMax(intervalMs, MinPollIntervalMs));
}

void ModbusRtuClient::stopPolling()
{
    pollTimer_.stop();
}

void ModbusRtuClient::readHoldingRegisters(quint8 slaveAddress,
                                           quint16 startRegister,
                                           quint16 registerCount)
{
    if (!serialPort_.isOpen()) {
        emitError(ModbusError::PortNotOpen, QStringLiteral("Modbus serial port is not open"));
        return;
    }

    if (pendingRequest_.has_value()) {
        emitError(ModbusError::InvalidRequest,
                  QStringLiteral("A Modbus request is already pending"));
        return;
    }

    sendReadHoldingRegisters(slaveAddress, startRegister, registerCount);
}

void ModbusRtuClient::onReadyRead()
{
    receiveBuffer_.append(serialPort_.readAll());

    if (receiveBuffer_.size() > MaxReceiveBufferSize) {
        receiveBuffer_.clear();
        emitError(ModbusError::BufferOverflow,
                  QStringLiteral("Modbus receive buffer exceeded %1 bytes")
                      .arg(MaxReceiveBufferSize));
        return;
    }

    parseBufferedFrames();
}

void ModbusRtuClient::onPollTimeout()
{
    if (!serialPort_.isOpen() || pendingRequest_.has_value()) {
        return;
    }

    sendReadHoldingRegisters(pollSlaveAddress_,
                             pollStartRegister_,
                             pollRegisterCount_);
}

void ModbusRtuClient::onRequestTimeout()
{
    if (!pendingRequest_.has_value()) {
        return;
    }

    PendingModbusRequest &pending = *pendingRequest_;
    if (pending.retryCount < maxRetries_) {
        ++pending.retryCount;
        emit requestRetried(pending.retryCount, maxRetries_);
        writePendingRequest();
        requestTimeoutTimer_.start(requestTimeoutMs_);
        return;
    }

    const PendingModbusRequest timedOut = pending;
    clearPendingRequest(true);
    emit requestTimedOut(timedOut.slaveAddress,
                         timedOut.functionCode,
                         timedOut.startRegister,
                         timedOut.registerCount);
    emitError(ModbusError::Timeout,
              QStringLiteral("Modbus request timed out after %1 retries")
                  .arg(maxRetries_));
}

void ModbusRtuClient::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    if (error == QSerialPort::ResourceError) {
        clearPendingRequest(true);
        emit connectionChanged(false, serialPort_.errorString());
    }

    emitError(ModbusError::PortNotOpen, serialPort_.errorString());
}

bool ModbusRtuClient::hasValidCrc(const QByteArray &frame)
{
    if (frame.size() < 4) {
        return false;
    }

    const QByteArray payload = frame.left(frame.size() - ModbusCrcLength);
    const quint16 expected = crc16(payload);
    const quint16 received =
        static_cast<quint8>(frame.at(frame.size() - 2)) |
        (static_cast<quint16>(static_cast<quint8>(frame.at(frame.size() - 1))) << 8);

    return expected == received;
}

QString ModbusRtuClient::errorMessage(ModbusError error, const QString &detail)
{
    QString prefix;
    switch (error) {
    case ModbusError::InvalidRequest:
        prefix = QStringLiteral("Invalid Modbus request");
        break;
    case ModbusError::PortNotOpen:
        prefix = QStringLiteral("Modbus serial port is not open");
        break;
    case ModbusError::CrcMismatch:
        prefix = QStringLiteral("Modbus CRC mismatch");
        break;
    case ModbusError::ResponseMismatch:
        prefix = QStringLiteral("Modbus response mismatch");
        break;
    case ModbusError::ExceptionResponse:
        prefix = QStringLiteral("Modbus exception response");
        break;
    case ModbusError::Timeout:
        prefix = QStringLiteral("Modbus request timeout");
        break;
    case ModbusError::BufferOverflow:
        prefix = QStringLiteral("Modbus receive buffer overflow");
        break;
    }

    return detail.isEmpty() ? prefix : QStringLiteral("%1: %2").arg(prefix, detail);
}

void ModbusRtuClient::emitError(ModbusError error, const QString &detail)
{
    const QString message = errorMessage(error, detail);
    emit errorOccurred(error, message);
    emit protocolError(message);
}

bool ModbusRtuClient::sendReadHoldingRegisters(quint8 slaveAddress,
                                               quint16 startRegister,
                                               quint16 registerCount)
{
    QString error;
    if (!isValidRequestRange(slaveAddress, startRegister, registerCount, &error)) {
        emitError(ModbusError::InvalidRequest, error);
        return false;
    }

    PendingModbusRequest pending;
    pending.slaveAddress = slaveAddress;
    pending.functionCode = ReadHoldingRegistersFunction;
    pending.startRegister = startRegister;
    pending.registerCount = registerCount;
    pending.requestFrame =
        buildReadHoldingRegistersRequest(slaveAddress, startRegister, registerCount);

    pendingRequest_ = pending;
    writePendingRequest();
    requestTimeoutTimer_.start(requestTimeoutMs_);
    return true;
}

void ModbusRtuClient::writePendingRequest()
{
    if (!pendingRequest_.has_value() || !serialPort_.isOpen()) {
        return;
    }

    serialPort_.write(pendingRequest_->requestFrame);
}

void ModbusRtuClient::clearPendingRequest(bool clearReceiveBuffer)
{
    requestTimeoutTimer_.stop();
    pendingRequest_.reset();

    if (clearReceiveBuffer) {
        receiveBuffer_.clear();
    }
}

void ModbusRtuClient::parseBufferedFrames()
{
    // This parser uses buffered frame validation and byte-wise resync. It does
    // not implement physical Modbus RTU 3.5-character silent interval timing.
    while (receiveBuffer_.size() >= ModbusExceptionFrameLength) {
        if (!pendingRequest_.has_value()) {
            receiveBuffer_.remove(0, 1);
            continue;
        }

        const ModbusResponseValidation response =
            validateResponse(receiveBuffer_, *pendingRequest_);

        if (response.status == ModbusResponseValidation::Status::NeedMoreData) {
            return;
        }

        if (response.status == ModbusResponseValidation::Status::CrcMismatch ||
            response.status == ModbusResponseValidation::Status::InvalidFrame) {
            receiveBuffer_.remove(0, 1);
            emitError(response.status == ModbusResponseValidation::Status::CrcMismatch
                          ? ModbusError::CrcMismatch
                          : ModbusError::ResponseMismatch,
                      response.error);
            continue;
        }

        const int frameLength = qMax(response.frameLength, 1);

        if (response.status == ModbusResponseValidation::Status::ResponseMismatch) {
            receiveBuffer_.remove(0, frameLength);
            emitError(ModbusError::ResponseMismatch, response.error);
            continue;
        }

        const QByteArray frame = receiveBuffer_.left(frameLength);
        receiveBuffer_.remove(0, frameLength);
        emit rawFrameReceived(frame);

        if (response.status == ModbusResponseValidation::Status::ExceptionResponse) {
            clearPendingRequest(true);
            emitError(ModbusError::ExceptionResponse, response.error);
            continue;
        }

        const PendingModbusRequest completed = *pendingRequest_;
        clearPendingRequest(true);
        emit holdingRegistersReceived(completed.slaveAddress,
                                      completed.startRegister,
                                      response.registers);
    }
}

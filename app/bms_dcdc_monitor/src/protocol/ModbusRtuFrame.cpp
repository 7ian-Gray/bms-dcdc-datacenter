#include "ModbusRtuFrame.h"

namespace
{
quint16 readU16Be(const QByteArray &data, int offset)
{
    const auto high = static_cast<quint8>(data.at(offset));
    const auto low = static_cast<quint8>(data.at(offset + 1));
    return static_cast<quint16>((static_cast<quint16>(high) << 8) | low);
}
} // namespace

quint16 ModbusRtuFrame::crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;

    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<quint8>(data.at(i));

        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

bool ModbusRtuFrame::hasValidCrc(const QByteArray &frame)
{
    if (frame.size() < CrcLength) {
        return false;
    }

    const QByteArray payload = frame.left(frame.size() - CrcLength);
    const quint16 computed = crc16(payload);

    const auto receivedLow = static_cast<quint8>(frame.at(frame.size() - 2));
    const auto receivedHigh = static_cast<quint8>(frame.at(frame.size() - 1));
    const quint16 received = static_cast<quint16>(
        (static_cast<quint16>(receivedHigh) << 8) | receivedLow);

    return computed == received;
}

bool ModbusRtuFrame::isValidReadHoldingRegistersRequest(quint8 slaveAddress,
                                                        quint16 startRegister,
                                                        quint16 registerCount,
                                                        QString *error)
{
    if (slaveAddress < 1 || slaveAddress > 247) {
        if (error) {
            *error = QStringLiteral("Slave address must be 1-247, got %1").arg(slaveAddress);
        }
        return false;
    }

    if (registerCount < 1 || registerCount > 125) {
        if (error) {
            *error = QStringLiteral("Register count must be 1-125, got %1").arg(registerCount);
        }
        return false;
    }

    const quint32 endRegister = static_cast<quint32>(startRegister) + registerCount - 1;
    if (endRegister > 0xFFFF) {
        if (error) {
            *error = QStringLiteral("Register range overflow: start=%1 count=%2")
                         .arg(startRegister)
                         .arg(registerCount);
        }
        return false;
    }

    return true;
}

QByteArray ModbusRtuFrame::buildReadHoldingRegistersRequest(quint8 slaveAddress,
                                                            quint16 startRegister,
                                                            quint16 registerCount)
{
    QByteArray frame;
    frame.reserve(8);

    frame.append(static_cast<char>(slaveAddress));
    frame.append(static_cast<char>(ReadHoldingRegistersFunction));
    frame.append(static_cast<char>((startRegister >> 8) & 0xFF));
    frame.append(static_cast<char>(startRegister & 0xFF));
    frame.append(static_cast<char>((registerCount >> 8) & 0xFF));
    frame.append(static_cast<char>(registerCount & 0xFF));

    const quint16 crc = crc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));

    return frame;
}

ModbusResponseValidation ModbusRtuFrame::validateReadHoldingRegistersResponse(
    const QByteArray &buffer,
    const PendingModbusRequest &pending)
{
    ModbusResponseValidation result;

    if (buffer.size() < 3) {
        result.error = ModbusRtuError::NeedMoreData;
        result.message = QStringLiteral("Need more data: minimum frame is 3 bytes (addr+func+crc), got %1")
                             .arg(buffer.size());
        return result;
    }

    const auto slaveAddress = static_cast<quint8>(buffer.at(0));
    const auto functionCode = static_cast<quint8>(buffer.at(1));

    if (slaveAddress != pending.slaveAddress) {
        result.error = ModbusRtuError::ResponseMismatch;
        result.message = QStringLiteral("Slave address mismatch: expected %1, got %2")
                             .arg(pending.slaveAddress)
                             .arg(slaveAddress);
        return result;
    }

    // Exception response: function code with high bit set
    if (functionCode == (pending.functionCode | ExceptionMask)) {
        if (buffer.size() < 5) {
            result.error = ModbusRtuError::NeedMoreData;
            result.message = QStringLiteral("Need more data: exception frame needs 5 bytes, got %1")
                                 .arg(buffer.size());
            return result;
        }

        const QByteArray frameData = buffer.left(5);
        if (!hasValidCrc(frameData)) {
            result.error = ModbusRtuError::CrcMismatch;
            result.message = QStringLiteral("CRC mismatch on exception response");
            return result;
        }

        result.error = ModbusRtuError::ExceptionResponse;
        result.exceptionCode = static_cast<quint8>(buffer.at(2));
        result.consumedBytes = 5;
        result.message = QStringLiteral("Exception response: code %1").arg(result.exceptionCode);
        return result;
    }

    if (functionCode != pending.functionCode) {
        result.error = ModbusRtuError::ResponseMismatch;
        result.message = QStringLiteral("Function code mismatch: expected %1, got %2")
                             .arg(pending.functionCode)
                             .arg(functionCode);
        return result;
    }

    // Normal response: slave(1) + func(1) + byteCount(1) + data(N) + crc(2)
    const auto byteCount = static_cast<quint8>(buffer.at(2));
    const int expectedFrameSize = 3 + byteCount + CrcLength;

    if (buffer.size() < expectedFrameSize) {
        result.error = ModbusRtuError::NeedMoreData;
        result.message = QStringLiteral("Need more data: expected %1 bytes, got %2")
                             .arg(expectedFrameSize)
                             .arg(buffer.size());
        return result;
    }

    const int expectedDataBytes = static_cast<int>(pending.registerCount) * 2;
    if (byteCount != expectedDataBytes) {
        result.error = ModbusRtuError::ResponseMismatch;
        result.message = QStringLiteral("Byte count mismatch: expected %1 (registers=%2), got %3")
                             .arg(expectedDataBytes)
                             .arg(pending.registerCount)
                             .arg(byteCount);
        return result;
    }

    const QByteArray frameData = buffer.left(expectedFrameSize);
    if (!hasValidCrc(frameData)) {
        result.error = ModbusRtuError::CrcMismatch;
        result.message = QStringLiteral("CRC mismatch on normal response");
        return result;
    }

    // Parse register values
    result.registers.reserve(pending.registerCount);
    for (quint16 i = 0; i < pending.registerCount; ++i) {
        const int offset = 3 + i * 2;
        result.registers.append(readU16Be(buffer, offset));
    }

    result.error = ModbusRtuError::None;
    result.consumedBytes = expectedFrameSize;
    result.message = QStringLiteral("OK: %1 registers decoded").arg(pending.registerCount);
    return result;
}

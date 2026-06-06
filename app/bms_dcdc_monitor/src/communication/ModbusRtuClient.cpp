#include "ModbusRtuClient.h"

#include <QIODevice>

ModbusRtuClient::ModbusRtuClient(QObject *parent)
    : QObject(parent)
{
    connect(&serialPort_, &QSerialPort::readyRead,
            this, &ModbusRtuClient::onReadyRead);
    connect(&serialPort_, &QSerialPort::errorOccurred,
            this, &ModbusRtuClient::onSerialError);
    connect(&pollTimer_, &QTimer::timeout,
            this, &ModbusRtuClient::onPollTimeout);
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
        return false;
    }

    receiveBuffer_.clear();
    emit connectionChanged(true, QStringLiteral("Modbus RTU serial port opened"));
    return true;
}

void ModbusRtuClient::close()
{
    pollTimer_.stop();
    receiveBuffer_.clear();

    if (serialPort_.isOpen()) {
        serialPort_.close();
        emit connectionChanged(false, QStringLiteral("Modbus RTU serial port closed"));
    }
}

bool ModbusRtuClient::isOpen() const
{
    return serialPort_.isOpen();
}

void ModbusRtuClient::startPolling(quint8 slaveAddress,
                                   quint16 startRegister,
                                   quint16 registerCount,
                                   int intervalMs)
{
    pollSlaveAddress_ = slaveAddress;
    pollStartRegister_ = startRegister;
    pollRegisterCount_ = registerCount;
    pollTimer_.start(qMax(intervalMs, 50));
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
        emit protocolError(QStringLiteral("Modbus serial port is not open"));
        return;
    }

    if (registerCount == 0 || registerCount > 125) {
        emit protocolError(QStringLiteral("Modbus register count must be in the range 1..125"));
        return;
    }

    QByteArray request;
    request.reserve(8);
    request.append(static_cast<char>(slaveAddress));
    request.append(static_cast<char>(0x03));
    request.append(static_cast<char>((startRegister >> 8) & 0xFF));
    request.append(static_cast<char>(startRegister & 0xFF));
    request.append(static_cast<char>((registerCount >> 8) & 0xFF));
    request.append(static_cast<char>(registerCount & 0xFF));

    const quint16 checksum = crc16(request);
    request.append(static_cast<char>(checksum & 0xFF));
    request.append(static_cast<char>((checksum >> 8) & 0xFF));

    pendingStartRegister_ = startRegister;
    serialPort_.write(request);
}

void ModbusRtuClient::onReadyRead()
{
    receiveBuffer_.append(serialPort_.readAll());
    parseBufferedFrames();
}

void ModbusRtuClient::onPollTimeout()
{
    readHoldingRegisters(pollSlaveAddress_,
                         pollStartRegister_,
                         pollRegisterCount_);
}

void ModbusRtuClient::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    emit protocolError(serialPort_.errorString());
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

bool ModbusRtuClient::hasValidCrc(const QByteArray &frame)
{
    if (frame.size() < 4) {
        return false;
    }

    const QByteArray payload = frame.left(frame.size() - 2);
    const quint16 expected = crc16(payload);
    const quint16 received =
        static_cast<quint8>(frame.at(frame.size() - 2)) |
        (static_cast<quint16>(static_cast<quint8>(frame.at(frame.size() - 1))) << 8);

    return expected == received;
}

void ModbusRtuClient::parseBufferedFrames()
{
    while (receiveBuffer_.size() >= 5) {
        const quint8 functionCode = static_cast<quint8>(receiveBuffer_.at(1));
        int expectedLength = 0;

        if ((functionCode & 0x80u) != 0u) {
            expectedLength = 5;
        } else if (functionCode == 0x03u || functionCode == 0x04u) {
            const quint8 byteCount = static_cast<quint8>(receiveBuffer_.at(2));
            expectedLength = 5 + byteCount;
        } else {
            receiveBuffer_.remove(0, 1);
            continue;
        }

        if (receiveBuffer_.size() < expectedLength) {
            return;
        }

        const QByteArray frame = receiveBuffer_.left(expectedLength);
        receiveBuffer_.remove(0, expectedLength);

        if (!hasValidCrc(frame)) {
            emit protocolError(QStringLiteral("Discarded a Modbus frame with invalid CRC"));
            continue;
        }

        emit rawFrameReceived(frame);

        const quint8 slaveAddress = static_cast<quint8>(frame.at(0));

        if ((functionCode & 0x80u) != 0u) {
            const quint8 exceptionCode = static_cast<quint8>(frame.at(2));
            emit protocolError(
                QStringLiteral("Modbus exception response: function=0x%1, code=0x%2")
                    .arg(functionCode & 0x7Fu, 2, 16, QLatin1Char('0'))
                    .arg(exceptionCode, 2, 16, QLatin1Char('0')));
            continue;
        }

        const quint8 byteCount = static_cast<quint8>(frame.at(2));
        if ((byteCount % 2u) != 0u) {
            emit protocolError(QStringLiteral("Modbus register payload has an odd byte count"));
            continue;
        }

        QVector<quint16> registers;
        registers.reserve(byteCount / 2);

        for (int offset = 3; offset < 3 + byteCount; offset += 2) {
            const quint16 value =
                (static_cast<quint16>(static_cast<quint8>(frame.at(offset))) << 8) |
                static_cast<quint8>(frame.at(offset + 1));
            registers.append(value);
        }

        emit holdingRegistersReceived(slaveAddress,
                                      pendingStartRegister_,
                                      registers);
    }
}

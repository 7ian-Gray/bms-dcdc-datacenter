#include "MockModbusTransport.h"

MockModbusTransport::MockModbusTransport(QObject *parent)
    : ModbusTransport(parent)
{
}

bool MockModbusTransport::isOpen() const
{
    return open_;
}

bool MockModbusTransport::writeFrame(const QByteArray &frame)
{
    if (!open_) {
        emit errorOccurred(QStringLiteral("Transport is not open"));
        return false;
    }

    if (writeFailure_) {
        emit errorOccurred(QStringLiteral("Write failure simulated"));
        return false;
    }

    writtenFrames_.append(frame);
    return true;
}

void MockModbusTransport::setOpen(bool open)
{
    open_ = open;
}

void MockModbusTransport::setWriteFailure(bool enabled)
{
    writeFailure_ = enabled;
}

void MockModbusTransport::injectFrame(const QByteArray &frame)
{
    emit frameReceived(frame);
}

QList<QByteArray> MockModbusTransport::writtenFrames() const
{
    return writtenFrames_;
}

void MockModbusTransport::clearWrittenFrames()
{
    writtenFrames_.clear();
}

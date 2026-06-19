#include "QSerialPortModbusTransport.h"

QSerialPortModbusTransport::QSerialPortModbusTransport(QObject *parent)
    : ModbusTransport(parent)
    , serialPort_(this)
{
    serialPort_.setBaudRate(QSerialPort::Baud9600);
    serialPort_.setDataBits(QSerialPort::Data8);
    serialPort_.setParity(QSerialPort::NoParity);
    serialPort_.setStopBits(QSerialPort::OneStop);
    serialPort_.setFlowControl(QSerialPort::NoFlowControl);

    interFrameTimer_.setSingleShot(true);
    interFrameTimer_.setInterval(interFrameTimeoutMs_);

    connect(&serialPort_, &QSerialPort::readyRead,
            this, &QSerialPortModbusTransport::onReadyRead);
    connect(&serialPort_, &QSerialPort::errorOccurred,
            this, &QSerialPortModbusTransport::onSerialError);
    connect(&interFrameTimer_, &QTimer::timeout,
            this, &QSerialPortModbusTransport::emitBufferedFrame);
}

bool QSerialPortModbusTransport::isOpen() const
{
    return serialPort_.isOpen();
}

bool QSerialPortModbusTransport::writeFrame(const QByteArray &frame)
{
    if (!serialPort_.isOpen())
    {
        emit errorOccurred(QStringLiteral("Serial port is not open"));
        return false;
    }

    if (frame.isEmpty())
    {
        emit errorOccurred(QStringLiteral("Cannot write empty Modbus RTU frame"));
        return false;
    }

    qint64 written = serialPort_.write(frame);
    if (written != frame.size())
    {
        emit errorOccurred(
            QStringLiteral("Serial port write failed: wrote %1 of %2 bytes")
                .arg(written)
                .arg(frame.size()));
        return false;
    }

    serialPort_.flush();
    return true;
}

QString QSerialPortModbusTransport::portName() const
{
    return serialPort_.portName();
}

qint32 QSerialPortModbusTransport::baudRate() const
{
    return serialPort_.baudRate();
}

QSerialPort::DataBits QSerialPortModbusTransport::dataBits() const
{
    return serialPort_.dataBits();
}

QSerialPort::Parity QSerialPortModbusTransport::parity() const
{
    return serialPort_.parity();
}

QSerialPort::StopBits QSerialPortModbusTransport::stopBits() const
{
    return serialPort_.stopBits();
}

QSerialPort::FlowControl QSerialPortModbusTransport::flowControl() const
{
    return serialPort_.flowControl();
}

int QSerialPortModbusTransport::interFrameTimeoutMs() const
{
    return interFrameTimeoutMs_;
}

void QSerialPortModbusTransport::setPortName(const QString &portName)
{
    serialPort_.setPortName(portName);
}

void QSerialPortModbusTransport::setBaudRate(qint32 baudRate)
{
    serialPort_.setBaudRate(baudRate);
}

void QSerialPortModbusTransport::setDataBits(QSerialPort::DataBits dataBits)
{
    serialPort_.setDataBits(dataBits);
}

void QSerialPortModbusTransport::setParity(QSerialPort::Parity parity)
{
    serialPort_.setParity(parity);
}

void QSerialPortModbusTransport::setStopBits(QSerialPort::StopBits stopBits)
{
    serialPort_.setStopBits(stopBits);
}

void QSerialPortModbusTransport::setFlowControl(QSerialPort::FlowControl flowControl)
{
    serialPort_.setFlowControl(flowControl);
}

void QSerialPortModbusTransport::setInterFrameTimeoutMs(int timeoutMs)
{
    interFrameTimeoutMs_ = timeoutMs;
    interFrameTimer_.setInterval(interFrameTimeoutMs_);
}

bool QSerialPortModbusTransport::open(QString *error)
{
    if (serialPort_.isOpen())
        return true;

    clearReadBuffer();

    if (!serialPort_.open(QIODevice::ReadWrite))
    {
        QString msg = QStringLiteral("Failed to open serial port '%1': %2")
                          .arg(serialPort_.portName(), serialPort_.errorString());
        if (error)
            *error = msg;
        emit errorOccurred(msg);
        return false;
    }

    serialPort_.clear();
    return true;
}

void QSerialPortModbusTransport::close()
{
    interFrameTimer_.stop();
    clearReadBuffer();

    if (serialPort_.isOpen())
    {
        serialPort_.clear();
        serialPort_.close();
    }
}

void QSerialPortModbusTransport::onReadyRead()
{
    readBuffer_.append(serialPort_.readAll());

    if (!readBuffer_.isEmpty())
        interFrameTimer_.start();
}

void QSerialPortModbusTransport::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    emit errorOccurred(
        QStringLiteral("Serial port error: %1").arg(serialPort_.errorString()));
}

void QSerialPortModbusTransport::emitBufferedFrame()
{
    if (readBuffer_.isEmpty())
        return;

    QByteArray frame = readBuffer_;
    clearReadBuffer();
    emit frameReceived(frame);
}

void QSerialPortModbusTransport::clearReadBuffer()
{
    readBuffer_.clear();
}

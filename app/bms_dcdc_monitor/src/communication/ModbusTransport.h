#pragma once

#include <QByteArray>
#include <QObject>

class ModbusTransport : public QObject
{
    Q_OBJECT

public:
    explicit ModbusTransport(QObject *parent = nullptr);
    ~ModbusTransport() override = default;

    virtual bool isOpen() const = 0;
    virtual bool writeFrame(const QByteArray &frame) = 0;

signals:
    void frameReceived(const QByteArray &frame);
    void errorOccurred(const QString &message);
};

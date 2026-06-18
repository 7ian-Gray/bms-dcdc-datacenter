#pragma once

#include "ModbusTransport.h"

#include <QByteArray>
#include <QList>

class MockModbusTransport : public ModbusTransport
{
    Q_OBJECT

public:
    explicit MockModbusTransport(QObject *parent = nullptr);

    bool isOpen() const override;
    bool writeFrame(const QByteArray &frame) override;

    void setOpen(bool open);
    void setWriteFailure(bool enabled);
    void injectFrame(const QByteArray &frame);

    QList<QByteArray> writtenFrames() const;
    void clearWrittenFrames();

private:
    bool open_ = true;
    bool writeFailure_ = false;
    QList<QByteArray> writtenFrames_;
};

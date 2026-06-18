#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QtGlobal>

enum class ModbusRtuError
{
    None,
    NeedMoreData,
    InvalidRequest,
    CrcMismatch,
    ResponseMismatch,
    ExceptionResponse,
    InvalidLength
};

struct PendingModbusRequest
{
    quint8 slaveAddress = 0;
    quint8 functionCode = 0;
    quint16 startRegister = 0;
    quint16 registerCount = 0;
};

struct ModbusResponseValidation
{
    ModbusRtuError error = ModbusRtuError::None;
    QVector<quint16> registers;
    quint8 exceptionCode = 0;
    qsizetype consumedBytes = 0;
    QString message;
};

class ModbusRtuFrame
{
public:
    static constexpr quint8 ReadHoldingRegistersFunction = 0x03;
    static constexpr quint8 ExceptionMask = 0x80;
    static constexpr int CrcLength = 2;

    static quint16 crc16(const QByteArray &data);
    static bool hasValidCrc(const QByteArray &frame);

    static bool isValidReadHoldingRegistersRequest(quint8 slaveAddress,
                                                   quint16 startRegister,
                                                   quint16 registerCount,
                                                   QString *error = nullptr);

    static QByteArray buildReadHoldingRegistersRequest(quint8 slaveAddress,
                                                      quint16 startRegister,
                                                      quint16 registerCount);

    static ModbusResponseValidation validateReadHoldingRegistersResponse(
        const QByteArray &buffer,
        const PendingModbusRequest &pending);
};

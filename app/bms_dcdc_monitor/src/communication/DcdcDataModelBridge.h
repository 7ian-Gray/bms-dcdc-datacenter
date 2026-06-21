#pragma once

#include "DataModel.h"
#include "protocol/DcdcModbusRegisterMap.h"

#include <QObject>
#include <QVector>

class DcdcModbusSession;

Q_DECLARE_METATYPE(DcdcData)

class DcdcDataModelBridge : public QObject
{
    Q_OBJECT

public:
    enum class Field {
        HvVoltage,
        HvCurrent,
        LvVoltage,
        LvCurrent,
        BoardTemperature,
        ChargeCurrentLimit,
        DischargeCurrentLimit,
        ChargeStartVoltage,
        ChargeStopVoltage,
        ChargeTargetVoltage,
        DischargeStartVoltage,
        DischargeStopVoltage,
        DischargeTargetVoltage
    };
    Q_ENUM(Field)

    struct RegisterMapping {
        quint16 address = 0;
        Field field = Field::HvVoltage;
    };

    explicit DcdcDataModelBridge(QObject *parent = nullptr);
    explicit DcdcDataModelBridge(DcdcModbusSession *session, QObject *parent = nullptr);

    DcdcData currentData() const;

    QVector<RegisterMapping> mappings() const;
    void setMappings(const QVector<RegisterMapping> &mappings);

    // Returns the provisional default mapping (addresses 101-113).
    // This mapping is provisional and must be replaced after the actual DCDC Modbus
    // register table is confirmed.
    static QVector<RegisterMapping> defaultMappings();

    // Connects to session's dcdcDataDecoded signal. Bridge does not own the session.
    // Replaces any previously attached session.
    void attachSession(DcdcModbusSession *session);

public slots:
    void onDcdcDataDecoded(DcdcRegisterDecodeResult result);

signals:
    void dcdcDataUpdated(DcdcData data);
    void bridgeError(QString message);

private:
    void applyValueToData(DcdcData &data, quint16 address, double scaledValue) const;

    DcdcData currentData_{};
    QVector<RegisterMapping> mappings_;
    DcdcModbusSession *attachedSession_ = nullptr;
};

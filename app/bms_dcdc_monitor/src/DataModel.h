#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QString>
#include <QVector>
#include <QtGlobal>

struct BmsData
{
    double packVoltage = 0.0;
    double packCurrent = 0.0;
    double soc = 0.0;
    double soh = 0.0;

    // Existing cumulative-energy fields used by the dashboard.
    double chargedEnergy = 0.0;
    double dischargedEnergy = 0.0;
    double energyDelta = 0.0;

    double maxCellVoltage = 0.0;
    double minCellVoltage = 0.0;
    double maxCellTemperature = 0.0;
    double minCellTemperature = 0.0;

    double chargeCurrentLimit = 0.0;
    double dischargeCurrentLimit = 0.0;

    // Added for CAN frames 0x18E2xxxx and 0x18E3xxxx.
    double chargeVoltageLimit = 0.0;
    double dischargeVoltageLimit = 0.0;
    double chargeAvailableEnergy = 0.0;
    double dischargeAvailableEnergy = 0.0;
    quint16 statusWord = 0;
    double sop = 0.0;
};

struct DcdcData
{
    double hvVoltage = 0.0;
    double hvCurrent = 0.0;
    double lvVoltage = 0.0;
    double lvCurrent = 0.0;
    double boardTemperature = 0.0;
    double chargeCurrentLimit = 0.0;
    double dischargeCurrentLimit = 0.0;
    double chargeStartVoltage = 0.0;
    double chargeStopVoltage = 0.0;
    double chargeTargetVoltage = 0.0;
    double dischargeStartVoltage = 0.0;
    double dischargeStopVoltage = 0.0;
    double dischargeTargetVoltage = 0.0;
};

struct CellRecord
{
    QString id;
    double voltage = 0.0;
    double temperature = 0.0;
    QString status;
};

struct RuntimeSample
{
    double minute = 0.0;
    double hvVoltage = 0.0;
    double hvCurrent = 0.0;
    double lvVoltage = 0.0;
    double lvCurrent = 0.0;
    double chargeCurrentLimit = 0.0;
    double dischargeCurrentLimit = 0.0;
    double boardTemperature = 0.0;
};

struct DashboardData
{
    BmsData bms;
    DcdcData dcdc;
    QVector<CellRecord> cells;
    QVector<RuntimeSample> runtimeSamples;
};

#endif // DATAMODEL_H

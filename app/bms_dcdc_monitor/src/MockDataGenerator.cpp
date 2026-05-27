#include "MockDataGenerator.h"

#include <limits>

DashboardData MockDataGenerator::createDashboardData()
{
    DashboardData data;

    const QVector<double> voltages = {
        3.421, 3.419, 3.398, 3.412,
        3.406, 3.315, 3.384, 3.392,
        3.401, 3.409, 3.417, 3.387,
        3.396, 3.404, 3.411, 3.389
    };

    const QVector<double> temperatures = {
        31.2, 30.8, 31.2, 32.1,
        33.6, 29.7, 30.3, 28.9,
        31.5, 30.6, 32.8, 27.5,
        29.9, 30.1, 31.0, 30.4
    };

    double maxVoltage = std::numeric_limits<double>::lowest();
    double minVoltage = std::numeric_limits<double>::max();
    double maxTemperature = std::numeric_limits<double>::lowest();
    double minTemperature = std::numeric_limits<double>::max();

    for (int index = 0; index < voltages.size(); ++index) {
        CellRecord cell;
        cell.id = QStringLiteral("Cell %1").arg(index + 1, 2, 10, QLatin1Char('0'));
        cell.voltage = voltages.at(index);
        cell.temperature = temperatures.at(index);

        if (cell.voltage < 3.35 || cell.temperature < 28.0) {
            cell.status = QStringLiteral("偏低");
        } else if (cell.voltage > 3.42 || cell.temperature > 33.0) {
            cell.status = QStringLiteral("偏高");
        } else {
            cell.status = QStringLiteral("正常");
        }

        maxVoltage = qMax(maxVoltage, cell.voltage);
        minVoltage = qMin(minVoltage, cell.voltage);
        maxTemperature = qMax(maxTemperature, cell.temperature);
        minTemperature = qMin(minTemperature, cell.temperature);

        data.cells.append(cell);
    }

    data.bms.packVoltage = 742.6;
    data.bms.packCurrent = -36.4;
    data.bms.soc = 78.2;
    data.bms.soh = 96.8;
    data.bms.chargedEnergy = 1280.5;
    data.bms.dischargedEnergy = 1196.2;
    data.bms.energyDelta = data.bms.chargedEnergy - data.bms.dischargedEnergy;
    data.bms.maxCellVoltage = maxVoltage;
    data.bms.minCellVoltage = minVoltage;
    data.bms.maxCellTemperature = maxTemperature;
    data.bms.minCellTemperature = minTemperature;
    data.bms.chargeCurrentLimit = 150.0;
    data.bms.dischargeCurrentLimit = 180.0;

    data.dcdc.hvVoltage = 746.0;
    data.dcdc.hvCurrent = 39.8;
    data.dcdc.lvVoltage = 52.4;
    data.dcdc.lvCurrent = 118.0;
    data.dcdc.boardTemperature = 41.6;
    data.dcdc.chargeCurrentLimit = 150.0;
    data.dcdc.dischargeCurrentLimit = 180.0;
    data.dcdc.chargeStartVoltage = 690.0;
    data.dcdc.chargeStopVoltage = 754.0;
    data.dcdc.chargeTargetVoltage = 748.0;
    data.dcdc.dischargeStartVoltage = 712.0;
    data.dcdc.dischargeStopVoltage = 640.0;
    data.dcdc.dischargeTargetVoltage = 660.0;

    data.runtimeSamples = {
        {0.0, 742.0, 52.1, 112.0, 150.0, 180.0, 40.8},
        {5.0, 743.2, 52.3, 115.0, 150.0, 180.0, 41.0},
        {10.0, 744.8, 52.5, 118.0, 150.0, 180.0, 41.4},
        {15.0, 746.4, 52.8, 120.0, 150.0, 180.0, 41.7},
        {20.0, 747.0, 53.0, 122.0, 150.0, 180.0, 42.2},
        {25.0, 746.2, 52.7, 119.0, 150.0, 180.0, 42.4},
        {30.0, 745.1, 52.6, 117.0, 150.0, 180.0, 42.1},
        {35.0, 744.3, 52.4, 114.0, 150.0, 180.0, 41.9},
        {40.0, 743.9, 52.2, 111.0, 150.0, 180.0, 41.6},
        {45.0, 744.6, 52.4, 116.0, 150.0, 180.0, 41.8},
        {50.0, 745.7, 52.6, 121.0, 150.0, 180.0, 42.3},
        {55.0, 746.5, 52.8, 123.0, 150.0, 180.0, 42.7}
    };

    return data;
}

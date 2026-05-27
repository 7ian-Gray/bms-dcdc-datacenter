#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "DataModel.h"

#include <QMainWindow>

class QGroupBox;
class QLabel;
class QTableWidget;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    struct CellStatistics
    {
        double maxVoltage = 0.0;
        double minVoltage = 0.0;
        double averageVoltage = 0.0;
        double voltageDiff = 0.0;
        double maxTemperature = 0.0;
        double minTemperature = 0.0;
        double temperatureDiff = 0.0;
        QString consistencyStatus;
    };

    void loadMockData();
    void setupUi();
    void updateCurrentTime();
    QWidget *setupTopBar();
    QGroupBox *setupTopLeftSummaryArea();
    QGroupBox *setupTopRightTableArea();
    QGroupBox *setupBottomLeftChartArea();
    QGroupBox *setupBottomRightBarChartArea();

    QGroupBox *createBmsSummaryGroup();
    QGroupBox *createDcdcSummaryGroup();
    QTableWidget *createVoltageTemperatureTable();
    QWidget *createStatisticsPanel();
    QWidget *createRuntimeChartPanel();
    QWidget *createCellVoltageChartPanel();
    QWidget *createConsistencyPanel();
    QWidget *createMetricCard(const QString &title,
                              const QString &value,
                              const QString &accentColor = QString()) const;
    QLabel *createStatusBadge(const QString &channel,
                              const QString &state,
                              const QString &color) const;
    QString formatNumber(double value, int decimals, const QString &unit) const;
    CellStatistics calculateCellStatistics() const;

    DashboardData dashboardData_;
    QLabel *currentTimeLabel = nullptr;
};

#endif // MAINWINDOW_H

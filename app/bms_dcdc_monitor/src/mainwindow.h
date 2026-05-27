#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "DataModel.h"

#include <QMainWindow>

class QGroupBox;
class QLabel;
class QChartView;
class QButtonGroup;
class QScrollArea;
class QSplitter;
class QStackedWidget;
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
    void setupMainLayoutWithScrollArea(QWidget *centralWidget);
    void updateCurrentTime();
    QWidget *setupTopBar();
    QGroupBox *setupTopLeftSummaryArea();
    QGroupBox *setupTopRightTableArea();
    QGroupBox *setupBottomLeftChartArea();
    QGroupBox *setupBottomRightVoltageArea();
    QSplitter *createDashboardSplitterLayout();
    QSplitter *createTopSplitter();
    QSplitter *createBottomSplitter();
    void configureSplitterRatios(QSplitter *verticalSplitter,
                                 QSplitter *topSplitter,
                                 QSplitter *bottomSplitter);
    QSplitter *createSummaryInnerSplitter();
    QSplitter *createTableStatsSplitter();
    QSplitter *createRuntimeInnerSplitter();
    QSplitter *createVoltageInnerSplitter();
    void configureInnerSplitterRatios(QSplitter *summarySplitter,
                                      QSplitter *tableStatsSplitter,
                                      QSplitter *runtimeSplitter,
                                      QSplitter *voltageSplitter);
    QWidget *setupRuntimeChartToolbar();

    QGroupBox *createBmsSummaryGroup();
    QGroupBox *createDcdcSummaryGroup();
    QTableWidget *createVoltageTemperatureTable();
    QWidget *createStatisticsPanel();
    QWidget *createRuntimeChartPanel();
    QWidget *createOverviewRuntimeChart();
    QWidget *createSingleRuntimeChartPage();
    QWidget *createRuntimeMetricCards();
    void switchRuntimeMetricChart(int metricIndex);
    QWidget *createCellVoltageChartPanel();
    QWidget *createBottomRightScrollContent();
    QWidget *createCellVoltageOverviewChart();
    QWidget *createGroupedCellVoltageCards();
    QWidget *createGroupedVoltageCardsScrollArea();
    QWidget *createConsistencyPanel();
    QWidget *createCompactConsistencyPanel();
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
    QStackedWidget *runtimeChartStack_ = nullptr;
    QStackedWidget *runtimeCardsStack_ = nullptr;
    QChartView *singleRuntimeChartView_ = nullptr;
    QButtonGroup *runtimeMetricButtonGroup_ = nullptr;
    int currentRuntimeMetricIndex_ = 0;
};

#endif // MAINWINDOW_H

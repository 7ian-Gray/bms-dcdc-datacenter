#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "DataModel.h"
#include "communication/CanFrame.h"
#include "communication/CanSessionController.h"
#include "protocol/BmsCanParser.h"

#include <QMainWindow>

class QButtonGroup;
class QChartView;
class QGroupBox;
class QLabel;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QTableWidget;
class QWidget;
class BmsCommandPage;
class CanMonitorPage;
class CanSessionController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onCanFrameReceived(const CanFrame &frame);
    void refreshBmsSummary();

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
    void initializeCommunication();
    void shutdownCommunication();

    void setupMainLayoutWithScrollArea(QWidget *centralWidget);
    QWidget *createOverviewPage();
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

    void setMetricValue(const QString &metricTitle, const QString &value);
    void setCommunicationBadge(QLabel *badge,
                               const QString &channel,
                               bool online,
                               const QString &detail = QString());

    QString formatNumber(double value, int decimals, const QString &unit) const;
    CellStatistics calculateCellStatistics() const;

    DashboardData dashboardData_;
    BmsCanParser bmsParser_;
    CanSessionController *canSessionController_ = nullptr;

    QLabel *currentTimeLabel = nullptr;
    QLabel *canStatusLabel_ = nullptr;
    QTabWidget *pageTabWidget_ = nullptr;
    CanMonitorPage *canMonitorPage_ = nullptr;
    BmsCommandPage *bmsCommandPage_ = nullptr;

    QStackedWidget *runtimeChartStack_ = nullptr;
    QStackedWidget *runtimeCardsStack_ = nullptr;
    QChartView *singleRuntimeChartView_ = nullptr;
    QButtonGroup *runtimeMetricButtonGroup_ = nullptr;
    int currentRuntimeMetricIndex_ = 0;
};

#endif // MAINWINDOW_H

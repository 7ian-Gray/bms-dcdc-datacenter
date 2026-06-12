#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "DataModel.h"
#include "communication/CanFrame.h"
#include "communication/ModbusRtuClient.h"
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
class CanMonitorPage;
class MockCanSource;

#ifdef BMS_HAS_CONTROLCAN
class ControlCanWorker;
class QThread;
#endif

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 收到通信线程传回的标准化 CAN 帧后，在主线程解析并刷新界面。
    void onCanFrameReceived(const CanFrame &frame);

    // 根据 dashboardData_ 中的最新 BMS 数据刷新顶部数据卡片。
    void refreshBmsSummary();

private:
    enum class CanSourceMode
    {
        Mock,
        Hardware
    };

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
    void setCanSourceMode(CanSourceMode mode, const QString &message);
    void routeCanFrameToMonitor(CanSourceMode sourceMode, const CanFrame &frame);

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
    ModbusRtuClient *modbusClient_ = nullptr;

    QLabel *currentTimeLabel = nullptr;
    QLabel *canStatusLabel_ = nullptr;
    QLabel *modbusStatusLabel_ = nullptr;
    QTabWidget *pageTabWidget_ = nullptr;
    CanMonitorPage *canMonitorPage_ = nullptr;

    QStackedWidget *runtimeChartStack_ = nullptr;
    QStackedWidget *runtimeCardsStack_ = nullptr;
    QChartView *singleRuntimeChartView_ = nullptr;
    QButtonGroup *runtimeMetricButtonGroup_ = nullptr;
    int currentRuntimeMetricIndex_ = 0;
    MockCanSource *mockCanSource_ = nullptr;
    CanSourceMode canSourceMode_ = CanSourceMode::Mock;
    bool shuttingDown_ = false;

#ifdef BMS_HAS_CONTROLCAN
    QThread *canIoThread_ = nullptr;
    ControlCanWorker *controlCanWorker_ = nullptr;
#endif
};

#endif // MAINWINDOW_H

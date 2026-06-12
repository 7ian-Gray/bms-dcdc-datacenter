#pragma once

#include "communication/CanFrame.h"
#include "models/CanFrameFilterProxyModel.h"

#include <QWidget>

class CanFrameTableModel;
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableView;
class QTimer;

class CanMonitorPage : public QWidget
{
    Q_OBJECT

public:
    explicit CanMonitorPage(QWidget *parent = nullptr);

    void setDataSourceStatus(const QString &sourceName,
                             const QString &message,
                             bool online);
    void clearFrames();
    void resetSession();
    void setPaused(bool paused);
    bool isPaused() const;

public slots:
    void appendFrame(const CanFrame &frame);

private:
    void setupUi();
    void updatePauseButton();
    void updateSourceStatusLabel();
    void updateCachedFrameCount();
    void updateDisplayedFrameCount();
    void updateStatistics();
    void updateCanIdFilterState();
    bool isViewingLatest() const;

    CanFrameTableModel *model_ = nullptr;
    CanFrameFilterProxyModel *filterProxyModel_ = nullptr;
    QTableView *tableView_ = nullptr;
    QLabel *sourceStatusValueLabel_ = nullptr;
    QLabel *cachedFrameCountValueLabel_ = nullptr;
    QLabel *displayedFrameCountValueLabel_ = nullptr;
    QLabel *totalRxCountValueLabel_ = nullptr;
    QLabel *totalTxCountValueLabel_ = nullptr;
    QPushButton *pauseButton_ = nullptr;
    QLineEdit *canIdFilterLineEdit_ = nullptr;
    QComboBox *directionFilterComboBox_ = nullptr;
    QSpinBox *maximumFrameCountSpinBox_ = nullptr;
    QTimer *statisticsTimer_ = nullptr;
    QString sourceName_ = QStringLiteral("未选择");
    QString sourceStatusMessage_ = QStringLiteral("等待数据源");
    bool sourceOnline_ = false;
    quint64 totalRxCount_ = 0;
    quint64 totalTxCount_ = 0;
    bool pendingScrollToBottom_ = false;
};

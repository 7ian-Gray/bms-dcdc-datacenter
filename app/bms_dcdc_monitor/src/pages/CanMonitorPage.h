#pragma once

#include "communication/CanFrame.h"
#include "communication/CanSessionController.h"
#include "models/CanFrameFilterProxyModel.h"

#include <QWidget>

class CanFrameTableModel;
class QComboBox;
class QCheckBox;
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
    void setConnectionState(CanConnectionState state);
    void setSourceMode(CanSourceMode mode);
    void setPlatformCapabilityText(const QString &text);
    void setLastError(const QString &message);
    void setSendResult(const QString &message, bool success);

public slots:
    void appendFrame(const CanFrame &frame);

signals:
    void openDeviceRequested(const CanConnectionConfig &config);
    void startChannelRequested();
    void stopChannelRequested();
    void closeDeviceRequested();
    void sendFrameRequested(const CanFrame &frame);

private:
    void setupUi();
    QWidget *createConnectionPanel();
    QWidget *createRawSendPanel();
    QWidget *createMonitorToolbar();
    CanConnectionConfig currentConnectionConfig() const;
    void validateRawFrameInput();
    void updateConnectionControls();
    void updateRawSendControls();
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
    QComboBox *sourceModeComboBox_ = nullptr;
    QComboBox *driverTypeComboBox_ = nullptr;
    QSpinBox *deviceTypeSpinBox_ = nullptr;
    QSpinBox *deviceIndexSpinBox_ = nullptr;
    QSpinBox *connectionChannelSpinBox_ = nullptr;
    QComboBox *bitrateComboBox_ = nullptr;
    QComboBox *workModeComboBox_ = nullptr;
    QPushButton *openDeviceButton_ = nullptr;
    QPushButton *startChannelButton_ = nullptr;
    QPushButton *stopChannelButton_ = nullptr;
    QPushButton *closeDeviceButton_ = nullptr;
    QLabel *connectionStateValueLabel_ = nullptr;
    QLabel *platformCapabilityValueLabel_ = nullptr;
    QLabel *lastErrorValueLabel_ = nullptr;
    QSpinBox *sendChannelSpinBox_ = nullptr;
    QLineEdit *sendCanIdLineEdit_ = nullptr;
    QComboBox *sendFrameFormatComboBox_ = nullptr;
    QComboBox *sendFrameTypeComboBox_ = nullptr;
    QLineEdit *sendDataLineEdit_ = nullptr;
    QLabel *sendDlcValueLabel_ = nullptr;
    QPushButton *sendFrameButton_ = nullptr;
    QPushButton *clearSendInputButton_ = nullptr;
    QLabel *sendValidationValueLabel_ = nullptr;
    QLabel *sendResultValueLabel_ = nullptr;
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
    CanConnectionState connectionState_ = CanConnectionState::Disconnected;
    quint64 totalRxCount_ = 0;
    quint64 totalTxCount_ = 0;
    bool pendingScrollToBottom_ = false;
};

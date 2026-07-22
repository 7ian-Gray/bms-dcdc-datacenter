#pragma once

#include "communication/BmsCommandConfirmationGate.h"
#include "protocol/BmsCommand.h"

#include <QList>
#include <QString>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;
class QWidget;

// Demo/Mock only: configures a demo BMS command, previews the CAN frame that
// BmsCommandEncoder would produce, and lets the operator confirm that snapshot.
// The page only emits a confirmed snapshot request for Mock-only dispatch; it
// never owns or calls a session, transport, worker, or vendor backend, and it
// never emits a raw CanFrame.
class BmsCommandPage : public QWidget
{
    Q_OBJECT

public:
    explicit BmsCommandPage(QWidget *parent = nullptr);

public slots:
    void setMockDispatchAvailability(bool available, int channel, const QString &reason);
    void handleMockDispatchSucceeded(quint64 revision,
                                     const QString &fingerprint,
                                     const QDateTime &transmittedAtUtc);
    void handleMockDispatchFailed(quint64 revision,
                                  const QString &code,
                                  const QString &message);

signals:
    // Carries the whole confirmed snapshot; the dispatcher owns every send rule.
    void mockDispatchRequested(const BmsCommandConfirmationSnapshot &snapshot);

private:
    struct ParameterControl
    {
        QString parameterId;
        BmsValueType valueType = BmsValueType::UnsignedInteger;
        QWidget *widget = nullptr;
    };

    void setupUi();
    QWidget *createSafetyBanner();
    QWidget *createCommandSelectionPanel();
    QGroupBox *createCommandInfoPanel();
    QGroupBox *createParameterPanel();
    QGroupBox *createValidationPanel();
    QGroupBox *createPreviewPanel();
    QGroupBox *createEncodedParameterPanel();
    QGroupBox *createConfirmationPanel();
    QWidget *createLeftPane();
    QWidget *createRightPane();

    void onCommandSelectionChanged();
    void rebuildParameterForm(const BmsCommandDefinition &definition);
    void updateCommandInfo(const BmsCommandDefinition &definition);
    void generatePreview();
    void showPreview(const EncodedBmsCommand &command);
    void showValidationIssues(const ValidationResult &validation);
    void clearPreview();
    bool currentDefinition(BmsCommandDefinition *definition) const;
    BmsCommandRequest buildRequest(const BmsCommandDefinition &definition) const;

    void onParameterEdited();
    void confirmCurrentPreview();
    void showStagedConfirmation(const BmsCommandConfirmationSnapshot &snapshot);
    void clearConfirmation(const QString &statusMessage);
    void setConfirmationStatus(const QString &message, bool success);

    QGroupBox *createMockDispatchPanel();
    void requestMockDispatch();
    void updateSendButtonState();
    void resetDispatchState();
    void setMockDispatchStatus(const QString &message, bool success);

    QLabel *safetyBannerLabel_ = nullptr;
    QComboBox *commandComboBox_ = nullptr;

    QLabel *commandIdValueLabel_ = nullptr;
    QLabel *commandDescriptionValueLabel_ = nullptr;
    QLabel *commandSafetyLevelValueLabel_ = nullptr;
    QLabel *commandDemoOnlyValueLabel_ = nullptr;
    QLabel *commandCanIdValueLabel_ = nullptr;
    QLabel *commandChannelValueLabel_ = nullptr;
    QLabel *commandFrameFormatValueLabel_ = nullptr;
    QLabel *commandDlcValueLabel_ = nullptr;

    QGroupBox *parameterGroup_ = nullptr;
    QFormLayout *parameterFormLayout_ = nullptr;
    QList<ParameterControl> parameterControls_;

    QPushButton *previewCommandButton_ = nullptr;
    QPushButton *sendCommandButton_ = nullptr;

    QLabel *validationStatusLabel_ = nullptr;
    QListWidget *validationIssuesList_ = nullptr;

    QLabel *previewCommandIdValueLabel_ = nullptr;
    QLabel *previewCanIdValueLabel_ = nullptr;
    QLabel *previewChannelValueLabel_ = nullptr;
    QLabel *previewFrameFormatValueLabel_ = nullptr;
    QLabel *previewDlcValueLabel_ = nullptr;
    QLabel *previewPayloadValueLabel_ = nullptr;
    QLabel *previewFingerprintValueLabel_ = nullptr;
    QLabel *previewEncodedAtValueLabel_ = nullptr;

    QTableWidget *encodedParameterTable_ = nullptr;

    QLabel *confirmationWarningLabel_ = nullptr;
    QLabel *confirmationRevisionValueLabel_ = nullptr;
    QLabel *confirmationCodeValueLabel_ = nullptr;
    QCheckBox *confirmationAcknowledgementCheckBox_ = nullptr;
    QLineEdit *confirmationCodeLineEdit_ = nullptr;
    QPushButton *confirmPreviewButton_ = nullptr;
    QLabel *confirmationStatusLabel_ = nullptr;
    QLabel *confirmedFingerprintValueLabel_ = nullptr;
    QLabel *confirmedAtValueLabel_ = nullptr;

    QLabel *mockDispatchAvailabilityLabel_ = nullptr;
    QLabel *mockDispatchStatusLabel_ = nullptr;

    BmsCommandConfirmationGate confirmationGate_;
    quint64 currentRevision_ = 0;

    bool mockDispatchAvailable_ = false;
    int mockDispatchChannel_ = -1;
    bool dispatchInFlight_ = false;
    quint64 dispatchedRevision_ = 0;
    // Suppresses the "parameters changed" path while controls are being created.
    bool rebuildingParameterForm_ = false;
};

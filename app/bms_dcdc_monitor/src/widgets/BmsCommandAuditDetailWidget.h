#pragma once

#include "audit/BmsCommandAudit.h"
#include "protocol/BmsCommand.h"

#include <QDateTime>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QWidget>

#include <optional>

class QLabel;
class QTableWidget;

// Read-only viewer for a single BMS command lifecycle audit record.
//
// The widget holds only a value copy of the record it displays. It never owns or
// queries the audit model, never touches a session, gate, or dispatcher, never
// generates an audit event, and never edits, deletes, exports, or persists a
// record. It is a pure presentation surface for an already-recorded event.
class BmsCommandAuditDetailWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BmsCommandAuditDetailWidget(QWidget *parent = nullptr);

    bool hasRecord() const;

public slots:
    void setRecord(const BmsCommandAuditRecord &record);
    void clearRecord();

private:
    void setupUi();
    void renderRecord(const BmsCommandAuditRecord &record);
    void renderEmpty();
    void populateParameterTable(QTableWidget *table, const QVariantMap &parameters);

    static QString timestampText(const QDateTime &timestamp);
    static QString safetyLevelText(BmsSafetyLevel level);
    static QString frameFormatText(bool extended);
    static QString variantTypeText(const QVariant &value);
    static QString variantValueText(const QVariant &value);

    QLabel *stateLabel_ = nullptr;

    QLabel *sequenceValue_ = nullptr;
    QLabel *eventAtValue_ = nullptr;
    QLabel *eventTypeValue_ = nullptr;
    QLabel *outcomeValue_ = nullptr;
    QLabel *modeValue_ = nullptr;
    QLabel *codeValue_ = nullptr;
    QLabel *messageValue_ = nullptr;

    QLabel *contextValue_ = nullptr;
    QLabel *revisionValue_ = nullptr;
    QLabel *commandIdValue_ = nullptr;
    QLabel *safetyLevelValue_ = nullptr;
    QLabel *channelValue_ = nullptr;
    QLabel *canIdValue_ = nullptr;
    QLabel *frameFormatValue_ = nullptr;
    QLabel *dlcValue_ = nullptr;
    QLabel *payloadValue_ = nullptr;
    QLabel *fingerprintValue_ = nullptr;

    QLabel *eventAtUtcValue_ = nullptr;
    QLabel *stagedAtUtcValue_ = nullptr;
    QLabel *confirmedAtUtcValue_ = nullptr;
    QLabel *transmittedAtUtcValue_ = nullptr;

    QTableWidget *engineeringTable_ = nullptr;
    QTableWidget *rawTable_ = nullptr;

    std::optional<BmsCommandAuditRecord> currentRecord_;
};

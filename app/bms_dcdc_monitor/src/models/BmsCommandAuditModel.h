#pragma once

#include "audit/BmsCommandAudit.h"

#include <QAbstractTableModel>
#include <QList>
#include <QtGlobal>

#include <optional>

// Append-only table model for the Demo/Mock BMS command lifecycle audit trail.
//
// The model stores records in the exact order they are appended, assigns each a
// monotonic process-local sequence number, and exposes them read-only. It never
// encodes commands, validates confirmations, judges dispatch safety, calls a
// session, writes files, or removes records. There is deliberately no clear,
// delete, replace, or edit path: the trail can only grow.
class BmsCommandAuditModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        SequenceColumn = 0,
        EventTimeColumn,
        EventTypeColumn,
        OutcomeColumn,
        RevisionColumn,
        CommandIdColumn,
        ModeColumn,
        ChannelColumn,
        CanIdColumn,
        PayloadColumn,
        FingerprintColumn,
        DetailsColumn,
        ColumnCount
    };

    explicit BmsCommandAuditModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    int recordCount() const;
    std::optional<BmsCommandAuditRecord> recordAt(int row) const;

public slots:
    // Appends record and returns true, assigning a fresh sequence number and
    // ignoring any sequence the producer set. Returns false without appending or
    // consuming a sequence when eventAtUtc is invalid.
    bool appendRecord(BmsCommandAuditRecord record);

private:
    QString displayText(const BmsCommandAuditRecord &record, int column) const;
    QString detailsText(const BmsCommandAuditRecord &record) const;

    QList<BmsCommandAuditRecord> records_;
    quint64 nextSequence_ = 1;
};

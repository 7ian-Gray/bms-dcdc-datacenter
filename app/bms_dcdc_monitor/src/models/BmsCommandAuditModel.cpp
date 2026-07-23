#include "models/BmsCommandAuditModel.h"

BmsCommandAuditModel::BmsCommandAuditModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int BmsCommandAuditModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return records_.size();
}

int BmsCommandAuditModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant BmsCommandAuditModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= records_.size()) {
        return QVariant();
    }

    const BmsCommandAuditRecord &record = records_.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return displayText(record, index.column());
    case Qt::ToolTipRole:
        if (index.column() == FingerprintColumn) {
            return record.fingerprint;
        }
        if (index.column() == DetailsColumn) {
            return detailsText(record);
        }
        return QVariant();
    case Qt::UserRole:
        return QVariant::fromValue(record);
    default:
        return QVariant();
    }
}

QString BmsCommandAuditModel::displayText(const BmsCommandAuditRecord &record, int column) const
{
    switch (column) {
    case SequenceColumn:
        return QString::number(record.sequence);
    case EventTimeColumn:
        return record.eventAtUtc.toString(Qt::ISODate);
    case EventTypeColumn:
        return bmsCommandAuditEventText(record.eventType);
    case OutcomeColumn:
        return bmsCommandAuditOutcomeText(record.outcome);
    case RevisionColumn:
        return record.revision == 0 ? QStringLiteral("-") : QString::number(record.revision);
    case CommandIdColumn:
        return record.commandId.isEmpty() ? QStringLiteral("-") : record.commandId;
    case ModeColumn:
        return bmsCommandAuditModeText(record.mode);
    case ChannelColumn:
        return QString::number(record.channel);
    case CanIdColumn:
        return bmsCommandAuditCanIdText(record.canId);
    case PayloadColumn:
        return bmsCommandAuditPayloadText(record.payload);
    case FingerprintColumn:
        return bmsCommandAuditFingerprintShortText(record.fingerprint);
    case DetailsColumn:
        return detailsText(record);
    default:
        return QString();
    }
}

QString BmsCommandAuditModel::detailsText(const BmsCommandAuditRecord &record) const
{
    if (record.code.isEmpty() && record.message.isEmpty()) {
        return QStringLiteral("-");
    }
    if (record.message.isEmpty()) {
        return record.code;
    }
    if (record.code.isEmpty()) {
        return record.message;
    }
    return QStringLiteral("%1：%2").arg(record.code, record.message);
}

QVariant BmsCommandAuditModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    switch (section) {
    case SequenceColumn:
        return QStringLiteral("序号");
    case EventTimeColumn:
        return QStringLiteral("时间 UTC");
    case EventTypeColumn:
        return QStringLiteral("事件");
    case OutcomeColumn:
        return QStringLiteral("结果");
    case RevisionColumn:
        return QStringLiteral("版本");
    case CommandIdColumn:
        return QStringLiteral("指令 ID");
    case ModeColumn:
        return QStringLiteral("模式");
    case ChannelColumn:
        return QStringLiteral("通道");
    case CanIdColumn:
        return QStringLiteral("CAN ID");
    case PayloadColumn:
        return QStringLiteral("Payload");
    case FingerprintColumn:
        return QStringLiteral("Fingerprint");
    case DetailsColumn:
        return QStringLiteral("详情");
    default:
        return QVariant();
    }
}

Qt::ItemFlags BmsCommandAuditModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    // Read-only and append-only: never editable.
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

int BmsCommandAuditModel::recordCount() const
{
    return records_.size();
}

std::optional<BmsCommandAuditRecord> BmsCommandAuditModel::recordAt(int row) const
{
    if (row < 0 || row >= records_.size()) {
        return std::nullopt;
    }
    return records_.at(row);
}

bool BmsCommandAuditModel::appendRecord(BmsCommandAuditRecord record)
{
    // An invalid timestamp means the producer never captured a real event time;
    // such a record is rejected and consumes no sequence number.
    if (!record.eventAtUtc.isValid()) {
        return false;
    }

    // The producer never assigns sequence numbers; the model owns that authority.
    record.sequence = nextSequence_++;

    const int row = records_.size();
    beginInsertRows(QModelIndex(), row, row);
    records_.append(record);
    endInsertRows();
    return true;
}

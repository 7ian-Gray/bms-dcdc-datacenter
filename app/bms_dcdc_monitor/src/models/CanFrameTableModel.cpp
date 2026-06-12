#include "models/CanFrameTableModel.h"

#include <QDateTime>
#include <QFontDatabase>

namespace
{
constexpr int kTrimBatchSize = 256;
}

CanFrameTableModel::CanFrameTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int CanFrameTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return frames_.size();
}

int CanFrameTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant CanFrameTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() ||
        index.row() < 0 ||
        index.row() >= frames_.size() ||
        index.column() < 0 ||
        index.column() >= ColumnCount) {
        return {};
    }

    const StoredFrame &storedFrame = frames_.at(index.row());
    const CanFrame &frame = storedFrame.frame;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case SequenceColumn:
            return QString::number(storedFrame.sequence);
        case TimeColumn:
            return formatTimestamp(frame.timestampUs);
        case DirectionColumn:
            return formatDirection(frame.direction);
        case ChannelColumn:
            return formatChannel(frame.channel);
        case CanIdColumn:
            return formatCanId(frame);
        case FrameFormatColumn:
            return formatFrameFormat(frame);
        case FrameTypeColumn:
            return formatFrameType(frame);
        case DlcColumn:
            return QString::number(frame.payload.size());
        case DataColumn:
            return formatPayload(frame.payload);
        default:
            break;
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case TimeColumn:
        case DirectionColumn:
        case ChannelColumn:
        case CanIdColumn:
        case FrameFormatColumn:
        case FrameTypeColumn:
        case DlcColumn:
            return QVariant::fromValue(Qt::AlignCenter);
        default:
            return QVariant::fromValue(Qt::AlignVCenter | Qt::AlignLeft);
        }
    }

    if (role == Qt::FontRole &&
        (index.column() == TimeColumn ||
         index.column() == CanIdColumn ||
         index.column() == DataColumn)) {
        return QFontDatabase::systemFont(QFontDatabase::FixedFont);
    }

    return {};
}

QVariant CanFrameTableModel::headerData(int section,
                                        Qt::Orientation orientation,
                                        int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    switch (section) {
    case SequenceColumn:
        return QStringLiteral("序号");
    case TimeColumn:
        return QStringLiteral("时间");
    case DirectionColumn:
        return QStringLiteral("方向");
    case ChannelColumn:
        return QStringLiteral("通道");
    case CanIdColumn:
        return QStringLiteral("CAN ID");
    case FrameFormatColumn:
        return QStringLiteral("帧格式");
    case FrameTypeColumn:
        return QStringLiteral("帧类型");
    case DlcColumn:
        return QStringLiteral("DLC");
    case DataColumn:
        return QStringLiteral("Data");
    default:
        return {};
    }
}

void CanFrameTableModel::appendFrame(const CanFrame &frame)
{
    if (paused_) {
        return;
    }

    trimToMaximumIfNeeded();

    const int row = frames_.size();
    beginInsertRows(QModelIndex(), row, row);
    frames_.append(StoredFrame{nextSequence_++, frame});
    endInsertRows();
}

void CanFrameTableModel::clear()
{
    if (frames_.isEmpty()) {
        return;
    }

    beginResetModel();
    frames_.clear();
    endResetModel();
}

void CanFrameTableModel::setPaused(bool paused)
{
    paused_ = paused;
}

bool CanFrameTableModel::isPaused() const
{
    return paused_;
}

void CanFrameTableModel::setMaximumFrameCount(int maximumFrameCount)
{
    const int sanitizedMaximum = qMax(1, maximumFrameCount);
    if (maximumFrameCount_ == sanitizedMaximum) {
        return;
    }

    maximumFrameCount_ = sanitizedMaximum;
    if (frames_.size() <= maximumFrameCount_) {
        return;
    }

    const int trimCount = frames_.size() - maximumFrameCount_;
    beginRemoveRows(QModelIndex(), 0, trimCount - 1);
    frames_.erase(frames_.begin(), frames_.begin() + trimCount);
    endRemoveRows();
}

int CanFrameTableModel::maximumFrameCount() const
{
    return maximumFrameCount_;
}

CanFrame CanFrameTableModel::frameAt(int row) const
{
    if (row < 0 || row >= frames_.size()) {
        return {};
    }

    return frames_.at(row).frame;
}

QString CanFrameTableModel::formatTimestamp(quint64 timestampUs)
{
    if (timestampUs == 0) {
        return QStringLiteral("--");
    }

    const qint64 timestampMs = static_cast<qint64>(timestampUs / 1000u);
    return QDateTime::fromMSecsSinceEpoch(timestampMs)
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

QString CanFrameTableModel::formatDirection(CanFrameDirection direction)
{
    return direction == CanFrameDirection::Tx
               ? QStringLiteral("TX")
               : QStringLiteral("RX");
}

QString CanFrameTableModel::formatChannel(quint8 channel)
{
    return QStringLiteral("CH%1").arg(channel);
}

QString CanFrameTableModel::formatCanId(const CanFrame &frame)
{
    const int width = frame.extended ? 8 : 3;
    return QStringLiteral("%1")
        .arg(frame.id, width, 16, QLatin1Char('0'))
        .toUpper();
}

QString CanFrameTableModel::formatFrameFormat(const CanFrame &frame)
{
    return frame.extended
               ? QStringLiteral("扩展帧")
               : QStringLiteral("标准帧");
}

QString CanFrameTableModel::formatFrameType(const CanFrame &frame)
{
    return frame.remote
               ? QStringLiteral("远程帧")
               : QStringLiteral("数据帧");
}

QString CanFrameTableModel::formatPayload(const QByteArray &payload)
{
    if (payload.isEmpty()) {
        return QStringLiteral("--");
    }

    return QString::fromLatin1(payload.toHex(' ').toUpper());
}

void CanFrameTableModel::trimToMaximumIfNeeded()
{
    if (frames_.size() < maximumFrameCount_) {
        return;
    }

    const int overflow = frames_.size() - maximumFrameCount_ + 1;
    int trimCount = overflow;
    if (maximumFrameCount_ > kTrimBatchSize) {
        trimCount = qMax(overflow, qMin(kTrimBatchSize, frames_.size()));
    }

    trimCount = qMin(trimCount, frames_.size());
    beginRemoveRows(QModelIndex(), 0, trimCount - 1);
    frames_.erase(frames_.begin(), frames_.begin() + trimCount);
    endRemoveRows();
}

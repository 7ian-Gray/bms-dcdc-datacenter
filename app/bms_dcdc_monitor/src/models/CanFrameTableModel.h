#pragma once

#include "communication/CanFrame.h"

#include <QAbstractTableModel>
#include <QVector>

class CanFrameTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        SequenceColumn = 0,
        TimeColumn,
        DirectionColumn,
        ChannelColumn,
        CanIdColumn,
        FrameFormatColumn,
        FrameTypeColumn,
        DlcColumn,
        DataColumn,
        ColumnCount
    };

    explicit CanFrameTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void appendFrame(const CanFrame &frame);
    void clear();

    void setPaused(bool paused);
    bool isPaused() const;

    void setMaximumFrameCount(int maximumFrameCount);
    int maximumFrameCount() const;

    CanFrame frameAt(int row) const;

    static QString formatTimestamp(quint64 timestampUs);
    static QString formatDirection(CanFrameDirection direction);
    static QString formatChannel(quint8 channel);
    static QString formatCanId(const CanFrame &frame);
    static QString formatFrameFormat(const CanFrame &frame);
    static QString formatFrameType(const CanFrame &frame);
    static QString formatPayload(const QByteArray &payload);

private:
    struct StoredFrame
    {
        quint64 sequence = 0;
        CanFrame frame;
    };

    void trimToMaximumIfNeeded();

    QVector<StoredFrame> frames_;
    quint64 nextSequence_ = 1;
    int maximumFrameCount_ = 10000;
    bool paused_ = false;
};

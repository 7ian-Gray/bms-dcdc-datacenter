#pragma once

#include <QSortFilterProxyModel>

class CanFrameFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum class DirectionFilter
    {
        All = 0,
        RxOnly,
        TxOnly
    };

    explicit CanFrameFilterProxyModel(QObject *parent = nullptr);

    void setCanIdFilterText(const QString &text);
    QString canIdFilterText() const;

    void setDirectionFilter(DirectionFilter filter);
    DirectionFilter directionFilter() const;

    bool isCanIdFilterValid() const;
    static QString normalizeCanIdFilter(const QString &text, bool *valid = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    QString rawCanIdFilterText_;
    QString normalizedCanIdFilterText_;
    DirectionFilter directionFilter_ = DirectionFilter::All;
    bool canIdFilterValid_ = true;
};

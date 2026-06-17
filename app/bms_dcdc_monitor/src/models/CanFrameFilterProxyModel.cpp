#include "models/CanFrameFilterProxyModel.h"

#include "models/CanFrameTableModel.h"

CanFrameFilterProxyModel::CanFrameFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void CanFrameFilterProxyModel::setCanIdFilterText(const QString &text)
{
    bool isValid = true;
    const QString normalizedText = normalizeCanIdFilter(text, &isValid);
    if (rawCanIdFilterText_ == text &&
        normalizedCanIdFilterText_ == normalizedText &&
        canIdFilterValid_ == isValid) {
        return;
    }

    rawCanIdFilterText_ = text;
    normalizedCanIdFilterText_ = normalizedText;
    canIdFilterValid_ = isValid;
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

QString CanFrameFilterProxyModel::canIdFilterText() const
{
    return rawCanIdFilterText_;
}

void CanFrameFilterProxyModel::setDirectionFilter(DirectionFilter filter)
{
    if (directionFilter_ == filter) {
        return;
    }

    directionFilter_ = filter;
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

CanFrameFilterProxyModel::DirectionFilter CanFrameFilterProxyModel::directionFilter() const
{
    return directionFilter_;
}

bool CanFrameFilterProxyModel::isCanIdFilterValid() const
{
    return canIdFilterValid_;
}

QString CanFrameFilterProxyModel::normalizeCanIdFilter(const QString &text, bool *valid)
{
    QString normalized = text.trimmed().toUpper();
    normalized.remove(QLatin1Char(' '));

    if (normalized.startsWith(QStringLiteral("0X"))) {
        normalized.remove(0, 2);
    }

    if (normalized.isEmpty()) {
        if (valid != nullptr) {
            *valid = true;
        }
        return {};
    }

    for (const QChar &character : normalized) {
        if (!character.isDigit() &&
            (character < QLatin1Char('A') || character > QLatin1Char('F'))) {
            if (valid != nullptr) {
                *valid = false;
            }
            return {};
        }
    }

    if (valid != nullptr) {
        *valid = true;
    }
    return normalized;
}

bool CanFrameFilterProxyModel::filterAcceptsRow(int sourceRow,
                                                const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent);

    const auto *frameModel = qobject_cast<const CanFrameTableModel *>(sourceModel());
    if (frameModel == nullptr) {
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }

    const CanFrame frame = frameModel->frameAt(sourceRow);

    if (directionFilter_ == DirectionFilter::RxOnly &&
        frame.direction != CanFrameDirection::Rx) {
        return false;
    }

    if (directionFilter_ == DirectionFilter::TxOnly &&
        frame.direction != CanFrameDirection::Tx) {
        return false;
    }

    if (!canIdFilterValid_ || normalizedCanIdFilterText_.isEmpty()) {
        return canIdFilterValid_ || rawCanIdFilterText_.trimmed().isEmpty();
    }

    const QString canonicalId = CanFrameTableModel::formatCanId(frame);
    const QString unpaddedId = QString::number(frame.id, 16).toUpper();
    return canonicalId.contains(normalizedCanIdFilterText_) ||
           unpaddedId.contains(normalizedCanIdFilterText_);
}

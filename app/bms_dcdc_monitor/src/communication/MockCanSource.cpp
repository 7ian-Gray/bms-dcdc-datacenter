#include "communication/MockCanSource.h"

#include <QDateTime>
#include <QTimer>

#include <array>

namespace
{
constexpr std::array<quint32, 5> kMockCanIds = {
    0x18E10101u,
    0x18E20101u,
    0x18E30101u,
    0x18E40101u,
    0x18FF5001u
};
}

MockCanSource::MockCanSource(QObject *parent)
    : QObject(parent),
      timer_(new QTimer(this))
{
    connect(timer_, &QTimer::timeout, this, &MockCanSource::generateFrame);
}

void MockCanSource::start(int intervalMs)
{
    timer_->setInterval(qMax(10, intervalMs));
    if (timer_->isActive()) {
        return;
    }

    timer_->start();
    emit runningChanged(true,
                        QStringLiteral("Mock CAN 在线，周期 %1 ms")
                            .arg(timer_->interval()));
}

void MockCanSource::stop()
{
    if (!timer_->isActive()) {
        return;
    }

    timer_->stop();
    emit runningChanged(false, QStringLiteral("Mock CAN 已停止"));
}

bool MockCanSource::isRunning() const
{
    return timer_->isActive();
}

int MockCanSource::intervalMs() const
{
    return timer_->interval();
}

void MockCanSource::generateFrame()
{
    const int currentIdIndex = idIndex_;
    const quint32 canId = kMockCanIds.at(static_cast<size_t>(currentIdIndex));
    idIndex_ = (idIndex_ + 1) % static_cast<int>(kMockCanIds.size());

    QByteArray payload(8, char(0));
    payload[0] = static_cast<char>(counter_ & 0xFFu);
    payload[1] = static_cast<char>((counter_ >> 8) & 0xFFu);
    payload[2] = static_cast<char>(currentIdIndex);
    payload[3] = static_cast<char>((canId >> 16) & 0xFFu);
    payload[4] = static_cast<char>((canId >> 8) & 0xFFu);
    payload[5] = static_cast<char>(canId & 0xFFu);
    payload[6] = static_cast<char>((counter_ * 3u) & 0xFFu);
    payload[7] = static_cast<char>((counter_ * 7u) & 0xFFu);

    CanFrame frame;
    frame.id = canId;
    frame.channel = static_cast<quint8>(counter_ % 2u);
    frame.extended = true;
    frame.remote = false;
    frame.direction = CanFrameDirection::Rx;
    frame.timestampUs =
        static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000u;
    frame.payload = payload;

    ++counter_;
    emit frameGenerated(frame);
}

#pragma once

#include "CanFrame.h"

#include <QObject>

class QTimer;

class MockCanSource : public QObject
{
    Q_OBJECT

public:
    explicit MockCanSource(QObject *parent = nullptr);

    void start(int intervalMs = 100);
    void stop();

    bool isRunning() const;
    int intervalMs() const;

signals:
    void frameGenerated(const CanFrame &frame);
    void runningChanged(bool running, const QString &message);

private slots:
    void generateFrame();

private:
    QTimer *timer_ = nullptr;
    quint64 counter_ = 0;
    int idIndex_ = 0;
};

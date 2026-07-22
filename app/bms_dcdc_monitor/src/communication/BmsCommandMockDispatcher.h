#pragma once

#include "communication/BmsCommandConfirmationGate.h"
#include "communication/CanFrame.h"

#include <QDateTime>
#include <QObject>
#include <QSet>
#include <QString>
#include <QtGlobal>

#include <optional>

class CanSessionController;

// The only object allowed to turn a confirmed BMS command snapshot into a CAN
// session send. It enforces the dispatch boundary: Demo-only commands, a running
// writable Mock channel with a matching channel index, and at most one successful
// transmission per confirmed revision.
//
// The consumption set is in-process bookkeeping that prevents accidentally
// sending the same confirmed snapshot twice while the application runs. It is not
// persisted and is not an authentication, authorization, or cryptographic
// replay-protection mechanism. Snapshot checks are structural consistency checks
// only; they do not authenticate where the snapshot came from.
class BmsCommandMockDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit BmsCommandMockDispatcher(CanSessionController *sessionController,
                                      QObject *parent = nullptr);

    bool isAvailable() const;
    int availableChannel() const;
    QString availabilityText() const;

public slots:
    void dispatch(const BmsCommandConfirmationSnapshot &snapshot);

signals:
    void availabilityChanged(bool available, int channel, const QString &reason);
    void dispatchSucceeded(quint64 revision,
                           const QString &fingerprint,
                           const QDateTime &transmittedAtUtc);
    void dispatchFailed(quint64 revision, const QString &code, const QString &message);

private slots:
    void refreshAvailability();
    void onFrameTransmitted(const CanFrame &frame);
    void onSessionError(const QString &message);

private:
    // Returns an empty string when the snapshot may be dispatched, otherwise the
    // failure code; message receives the human-readable reason.
    QString validateSnapshot(const BmsCommandConfirmationSnapshot &snapshot,
                             QString *message) const;
    QString validateSessionForDispatch(QString *message) const;
    static QString snapshotKey(const BmsCommandConfirmationSnapshot &snapshot);
    void failDispatch(quint64 revision, const QString &code, const QString &message);

    CanSessionController *sessionController_ = nullptr;

    std::optional<BmsCommandConfirmationSnapshot> pending_;
    QString pendingKey_;
    QSet<QString> consumedKeys_;

    bool lastAvailable_ = false;
    int lastChannel_ = -1;
    QString lastReason_;
};

#pragma once

#include "DocumentRecords.h"
#include "SealingQueue.h"
#include "ServerConnection.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

/// This person's devices, from a device they already use: approving a new one's link
/// code, and unlinking one that is lost or retired.
///
/// The device that approves a link remembers it, and seals it into every ward it
/// holds through the sealing queue - now, and in any ward it is granted later. No
/// other device seals it, so a device the server merely lists under this person never
/// receives a key.
class DeviceLinks : public QObject
{
    Q_OBJECT

public:
    DeviceLinks(DocumentRecords& records, ServerConnection& server, SealingQueue& queue, QObject* parent);

    /// Looks up a link code another device shows; linkRead says what it names.
    void read(const QString& code);

    /// Adds the device behind the code to this person, and seals it everywhere.
    void approve(const QString& code, const DeviceKeys& device);

    /// Takes these devices off this person, and so off every ward at once.
    void unlink(const QList<Fingerprint>& devices);

signals:
    void linkRead(const QString& code, const ServerContract::LinkState& state);
    void approved(const QString& deviceName);
    void unlinked(int count);
    void failed(ServerContract::ServerError error);

private:
    void onRead(const QString& code, const ServerContract::Result<ServerContract::LinkState>& state);
    void onApproved(const DeviceKeys& device,
                    const QString& deviceName,
                    const ServerContract::Result<ServerContract::Done>& result);
    void onUnlinked(const Fingerprint& device, const ServerContract::Result<ServerContract::Done>& result);
    static bool isDevice(const Fingerprint& fingerprint, const DeviceKeys& device);

    DocumentRecords& m_records;
    ServerConnection& m_server;
    SealingQueue& m_queue;

    /// The name the last read gave each code, for the approved signal.
    QHash<QString, QString> m_names;
    int m_unlinking = 0;
    int m_unlinked = 0;
};

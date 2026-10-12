#pragma once

#include "DeviceIdentity.h"
#include "DocumentRecords.h"
#include "ServerConnection.h"
#include "ServerVersion.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

/// This person's devices, from a device they already use: approving a new one's link
/// code, and unlinking one that is lost or retired.
///
/// Approving sends, with the approval, every ward this device holds resealed to the new
/// device, so the server adds the device and stores those versions together. The device
/// is then remembered in linkedDevices, so every later write by this device seals to it
/// too: that is how it reaches a ward this device did not hold at approval time. No
/// other device seals it, so a device the server merely lists under this person never
/// receives a key.
class DeviceLinks : public QObject
{
    Q_OBJECT

public:
    DeviceLinks(const DeviceIdentity& identity,
                DocumentRecords& records,
                ServerConnection& server,
                std::function<DocumentPaths(const QString&)> pathsFor,
                QObject* parent);

    /// Looks up a link code another device shows; linkRead says what it names.
    void read(const QString& code);

    /// Adds the device behind the code to this person, with every ward this device
    /// holds resealed to it. One approval at a time; a second while one is in flight
    /// is ignored.
    void approve(const QString& code, const DeviceKeys& device);

    /// Takes these devices off this person, and so off every ward at once.
    void unlink(const QList<Fingerprint>& devices);

signals:
    void linkRead(const QString& code, const ServerContract::LinkState& state);
    void approved(const QString& deviceName);
    void unlinked(int count);
    void failed(ServerContract::ServerError error);

private:
    /// One approval in progress: the wards still being built, and those built.
    struct Approval
    {
        QString code;
        DeviceKeys device;
        bool mayRetry = true;
        int building = 0;
        QList<ServerContract::Resealing> built;
        ServerContract::ServerError unanswered = ServerContract::ServerError::None;   ///< a ward's call that got no answer
    };

    void onRead(const QString& code, const ServerContract::Result<ServerContract::LinkState>& state);
    void startApproval(const QString& code, const DeviceKeys& device, bool mayRetry);
    void onBuilt(const ServerVersion::Fetched& theirs, const std::optional<ServerContract::Resealing>& built);
    void sendApproval();
    void onApproved(const ServerContract::Result<ServerContract::Done>& result);
    void onUnlinked(const Fingerprint& device, const ServerContract::Result<ServerContract::Done>& result);
    static bool isDevice(const Fingerprint& fingerprint, const DeviceKeys& device);

    DeviceIdentity m_identity;
    DocumentRecords& m_records;
    ServerConnection& m_server;
    std::function<DocumentPaths(const QString&)> m_pathsFor;
    std::optional<Approval> m_approval;

    /// The name the last read gave each code, for the approved signal.
    QHash<QString, QString> m_names;
    int m_unlinking = 0;
    int m_unlinked = 0;
};

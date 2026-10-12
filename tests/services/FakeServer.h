#pragma once

#include "ServerContract.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <functional>
#include <optional>

/// The server contract, in memory, enforcing every rule the real server must.
///
/// A client written against a permissive fake will not work against a strict
/// server, so nothing here is relaxed for convenience. The payload is opaque to
/// the fake exactly as it is to the real server: an upload's blob is stored, never
/// read. If a test ever needs the fake to look inside it, the design has been
/// violated somewhere.
///
/// Every call is decided when it is made and answered later: its reply waits in a
/// queue and is delivered on a later turn of the event loop, or when the test says
/// so.
///
/// Beyond ServerApi it offers controls the real server must never have - going
/// offline, holding or losing replies, forcing a conflict, holding the lease as
/// someone else, serving a version of its own making, and leaving a device out of
/// what it says - so the client's negative tests can make the server misbehave.
class FakeServer : public ServerContract::ServerApi
{
public:
    /// The server's configuration values. Every limit the real server applies is
    /// here, so a test can reach it without generating thousands of calls.
    struct Limits
    {
        int registrationsPerKey = 20;
        int registrationsPerHour = 5;
        int linksPerHour = 10;
        int requestsPerHour = 10;
        int uploadsPerHour = 600;
        qint64 maxBlobBytes = 16 * 1024 * 1024;
        qint64 sessionSeconds = 15 * 60;
        qint64 leaseSeconds = 2 * 60;
        qint64 linkSeconds = 10 * 60;
    };

    FakeServer();
    explicit FakeServer(const Limits& limits);

    // --- ServerApi ---

    void challenge(ServerContract::Reply<QByteArray> reply) override;
    void authenticate(const VerifyKey& device,
                      const QByteArray& challenge,
                      const QByteArray& signature,
                      ServerContract::Reply<ServerContract::Session> reply) override;

    void registerPerson(const ServerContract::Session& session,
                        const ServerContract::Registration& registration,
                        ServerContract::Reply<ServerContract::Done> reply) override;
    void me(const ServerContract::Session& session, ServerContract::Reply<ServerContract::Me> reply) override;
    void unlinkDevice(const ServerContract::Session& session,
                      const Fingerprint& device,
                      ServerContract::Reply<ServerContract::Done> reply) override;

    void startLink(const ServerContract::Session& session,
                   const PublicKey& publicKey,
                   const QString& deviceName,
                   ServerContract::Reply<QString> reply) override;
    void link(const ServerContract::Session& session,
              const QString& code,
              ServerContract::Reply<ServerContract::LinkState> reply) override;
    void approveLink(const ServerContract::Session& session,
                     const QString& code,
                     const QList<ServerContract::Resealing>& resealings,
                     ServerContract::Reply<ServerContract::Done> reply) override;

    void registerDocument(const ServerContract::Session& session,
                          const ServerContract::Upload& upload,
                          ServerContract::Reply<qint64> reply) override;
    void head(const ServerContract::Session& session,
              const QString& documentId,
              ServerContract::Reply<ServerContract::Head> reply) override;
    void download(const ServerContract::Session& session,
                  const QString& documentId,
                  ServerContract::Reply<ServerContract::Snapshot> reply) override;
    void upload(const ServerContract::Session& session,
                const QString& documentId,
                qint64 baseVersion,
                const ServerContract::Upload& upload,
                ServerContract::Reply<qint64> reply) override;

    void takeLease(const ServerContract::Session& session,
                   const QString& documentId,
                   ServerContract::Reply<ServerContract::Lease> reply) override;
    void releaseLease(const ServerContract::Session& session,
                      const QString& documentId,
                      ServerContract::Reply<ServerContract::Done> reply) override;

    void history(const ServerContract::Session& session,
                 const QString& documentId,
                 qint64 after,
                 ServerContract::Reply<QList<ServerContract::HistoryEntry>> reply) override;
    void requestAccess(const ServerContract::Session& session,
                       const QString& documentId,
                       ServerContract::Reply<ServerContract::Done> reply) override;
    void accessRequests(const ServerContract::Session& session,
                        const QString& documentId,
                        ServerContract::Reply<QList<ServerContract::AccessRequest>> reply) override;
    void declineRequest(const ServerContract::Session& session,
                        const QString& documentId,
                        qint64 index,
                        ServerContract::Reply<ServerContract::Done> reply) override;
    void myRequestStatus(const ServerContract::Session& session,
                         const QString& documentId,
                         ServerContract::Reply<ServerContract::RequestStatus> reply) override;
    void grant(const ServerContract::Session& session,
               const QString& documentId,
               qint64 index,
               const ServerContract::Resealing& resealing,
               ServerContract::Reply<ServerContract::Done> reply) override;
    void removeMember(const ServerContract::Session& session,
                      const QString& documentId,
                      const MemberId& member,
                      ServerContract::Reply<ServerContract::Done> reply) override;
    void setRole(const ServerContract::Session& session,
                 const QString& documentId,
                 const MemberId& member,
                 ServerContract::Role role,
                 ServerContract::Reply<ServerContract::Done> reply) override;

    void advanceClock(qint64 seconds, ServerContract::Reply<ServerContract::Done> reply) override;

    // --- Controls the real server must never offer ---

    void setOffline(bool offline) { m_offline = offline; }

    /// Queued replies wait for deliverNext() while held.
    void holdReplies(bool hold);

    /// Delivers the oldest queued reply; false when none is waiting.
    bool deliverNext();

    qsizetype waitingReplies() const { return m_replies.size(); }

    /// Login challenges handed out, which is how many sign-ins were started.
    int challengesIssued() const { return m_challengesIssued; }

    /// Head and download calls taken, whatever they answered.
    int headsServed() const { return m_headsServed; }
    int downloadsServed() const { return m_downloadsServed; }

    /// Calls still take effect, and their replies arrive as Unreachable - a
    /// request the server acted on whose answer never came back.
    void loseReplies(bool lose) { m_loseReplies = lose; }

    /// The next upload to this document - a PUT, or the resealing a grant or an
    /// approval carries - is refused as a version conflict.
    void forceConflict(const QString& documentId);

    /// Another device holds the lease until the test says otherwise.
    void holdLeaseAs(const QString& documentId, const Fingerprint& holder);

    /// Every head and download of this document serves this header and blob in
    /// place of the stored ones, as a hostile or broken server would. Uploads still
    /// build on the stored version. An empty header serves the stored one again.
    void serveInstead(const QString& documentId, const QJsonObject& header, const QByteArray& blob);

    /// Heads list every member without this device, while it keeps its access.
    void hideFromHeads(const Fingerprint& device) { m_hidden.insert(device.toHex()); }

    const QDateTime& now() const { return m_now; }

private:
    struct StoredRequest
    {
        ServerContract::AccessRequest request;
        ServerContract::RequestStatus status = ServerContract::RequestStatus::Pending;
    };

    struct StoredDocument
    {
        qint64 version = 0;
        QJsonObject header;
        QByteArray blob;
        ServerContract::Publication published;
        QHash<QString, ServerContract::Role> members;           ///< by member id hex
        QList<ServerContract::HistoryEntry> history;
        QList<StoredRequest> requests;
        QHash<QString, ServerContract::LostAccess> lost;        ///< by member id hex
        std::optional<ServerContract::Lease> lease;
        bool forceConflict = false;
        std::optional<QJsonObject> servedHeader;
        QByteArray servedBlob;
    };

    struct StoredLink
    {
        QString deviceName;
        DeviceKeys keys;
        std::optional<QString> approvedAs;      ///< member id hex
        QDateTime expires;
    };

    struct SessionRecord
    {
        VerifyKey device;
        QDateTime expires;
    };

    /// Who is calling: the device signed in, and its person once it has one.
    struct Caller
    {
        VerifyKey device;
        Fingerprint fingerprint;
        ServerContract::Person* person = nullptr;
    };

    // --- The rules, decided at call time ---

    ServerContract::Result<QByteArray> decideChallenge();
    ServerContract::Result<ServerContract::Session> decideAuthenticate(const VerifyKey& device,
                                                                       const QByteArray& challenge,
                                                                       const QByteArray& signature);
    ServerContract::Result<ServerContract::Done> decideRegisterPerson(const ServerContract::Session& session,
                                                                      const ServerContract::Registration& registration);
    ServerContract::Result<ServerContract::Me> decideMe(const ServerContract::Session& session);
    ServerContract::Result<ServerContract::Done> decideUnlinkDevice(const ServerContract::Session& session,
                                                                    const Fingerprint& device);
    ServerContract::Result<QString> decideStartLink(const ServerContract::Session& session,
                                                    const PublicKey& publicKey,
                                                    const QString& deviceName);
    ServerContract::Result<ServerContract::LinkState> decideLink(const ServerContract::Session& session,
                                                                 const QString& code);
    ServerContract::Result<ServerContract::Done> decideApproveLink(const ServerContract::Session& session,
                                                                   const QString& code,
                                                                   const QList<ServerContract::Resealing>& resealings);
    ServerContract::Result<qint64> decideRegisterDocument(const ServerContract::Session& session,
                                                          const ServerContract::Upload& upload);
    ServerContract::Result<ServerContract::Head> decideHead(const ServerContract::Session& session,
                                                            const QString& documentId);
    ServerContract::Result<ServerContract::Snapshot> decideDownload(const ServerContract::Session& session,
                                                                    const QString& documentId);
    ServerContract::Result<qint64> decideUpload(const ServerContract::Session& session,
                                                const QString& documentId,
                                                qint64 baseVersion,
                                                const ServerContract::Upload& upload);
    ServerContract::Result<ServerContract::Lease> decideTakeLease(const ServerContract::Session& session,
                                                                  const QString& documentId);
    ServerContract::Result<ServerContract::Done> decideReleaseLease(const ServerContract::Session& session,
                                                                    const QString& documentId);
    ServerContract::Result<QList<ServerContract::HistoryEntry>> decideHistory(const ServerContract::Session& session,
                                                                              const QString& documentId,
                                                                              qint64 after);
    ServerContract::Result<ServerContract::Done> decideRequestAccess(const ServerContract::Session& session,
                                                                     const QString& documentId);
    ServerContract::Result<QList<ServerContract::AccessRequest>> decideAccessRequests(
        const ServerContract::Session& session,
        const QString& documentId);
    ServerContract::Result<ServerContract::Done> decideDeclineRequest(const ServerContract::Session& session,
                                                                      const QString& documentId,
                                                                      qint64 index);
    ServerContract::Result<ServerContract::RequestStatus> decideMyRequestStatus(const ServerContract::Session& session,
                                                                                const QString& documentId);
    ServerContract::Result<ServerContract::Done> decideGrant(const ServerContract::Session& session,
                                                             const QString& documentId,
                                                             qint64 index,
                                                             const ServerContract::Resealing& resealing);
    ServerContract::Result<ServerContract::Done> decideRemoveMember(const ServerContract::Session& session,
                                                                    const QString& documentId,
                                                                    const MemberId& member);
    ServerContract::Result<ServerContract::Done> decideSetRole(const ServerContract::Session& session,
                                                               const QString& documentId,
                                                               const MemberId& member,
                                                               ServerContract::Role role);
    ServerContract::Result<ServerContract::Done> decideAdvanceClock(qint64 seconds);

    // --- Replies ---

    /// Queues the reply to a call already decided. A lost reply still reports
    /// Unreachable: the caller learns nothing of what the server did.
    template<typename T>
    void answer(const ServerContract::Result<T>& result, ServerContract::Reply<T> reply)
    {
        const ServerContract::Result<T> delivered =
            m_loseReplies ? ServerContract::Result<T>::failure(ServerContract::ServerError::Unreachable)
                          : result;
        m_replies.append([reply, delivered]() { reply(delivered); });
        scheduleDelivery();
    }

    void scheduleDelivery();
    void deliverWaiting();

    // --- Helpers ---

    /// The caller of a session: None, Unreachable, or Unauthenticated.
    ServerContract::ServerError signedIn(const ServerContract::Session& session, Caller& caller);

    /// The caller, allow-listed on documentId. AccessLost, with lost set, for a device
    /// that had access and no longer does; NotMember for one that never had it.
    ServerContract::ServerError allowListed(const ServerContract::Session& session,
                                            const QString& documentId,
                                            Caller& caller,
                                            StoredDocument*& document,
                                            std::optional<ServerContract::LostAccess>& lost);

    /// The same, for callers that must also be admins.
    ServerContract::ServerError admin(const ServerContract::Session& session,
                                      const QString& documentId,
                                      Caller& caller,
                                      StoredDocument*& document,
                                      std::optional<ServerContract::LostAccess>& lost);

    QSet<QString> allowListOf(const StoredDocument& document) const;

    /// The upload checks PUT, grant, and approve share, against the allow-list the
    /// caller supplies - for grant and approve, one that already includes who is being
    /// added. Stores the version only when store is set, so a batch can be checked
    /// whole before any of it lands. The caller has already been resolved.
    ServerContract::ServerError storeUpload(StoredDocument& document,
                                            const QString& documentId,
                                            const Caller& caller,
                                            qint64 baseVersion,
                                            const ServerContract::Upload& upload,
                                            const QSet<QString>& allowList,
                                            bool store);
    bool isHidden(const ServerContract::Device& device) const;
    QList<ServerContract::Member> membersOf(const StoredDocument& document) const;
    bool keepsAnAdmin(const StoredDocument& document) const;

    /// A device off the allow-list holds no lease anywhere.
    void releaseLeasesOf(const Fingerprint& device);
    void record(StoredDocument& document,
                ServerContract::HistoryEntry::Kind kind,
                const QString& actor,
                const QString& subject);

    bool withinLimits(const QHash<QByteArray, QList<QDateTime>>& log,
                      const VerifyKey& device,
                      int perHour,
                      int total) const;
    QString randomToken() const;

    template<typename T>
    static ServerContract::Result<T> refused(ServerContract::ServerError error,
                                             const std::optional<ServerContract::LostAccess>& lost)
    {
        return error == ServerContract::ServerError::AccessLost && lost.has_value()
                   ? ServerContract::Result<T>::lostAccess(lost.value())
                   : ServerContract::Result<T>::failure(error);
    }

    Limits m_limits;
    QDateTime m_now;
    bool m_offline = false;

    QHash<QString, ServerContract::Person> m_people;        ///< by member id hex
    QHash<QByteArray, QString> m_personOfDevice;            ///< verify key bytes to member id hex
    QHash<QByteArray, ServerContract::LostAccess> m_unlinked;   ///< verify key bytes
    QHash<QByteArray, QString> m_unlinkedFrom;                  ///< verify key bytes to member id hex
    QHash<QString, StoredDocument> m_documents;
    QHash<QString, StoredLink> m_links;
    QSet<QString> m_hidden;     ///< fingerprint hex
    QHash<QString, SessionRecord> m_sessions;
    QSet<QByteArray> m_challenges;
    QHash<QByteArray, QList<QDateTime>> m_registrations;
    QHash<QByteArray, QList<QDateTime>> m_linkStarts;
    QHash<QByteArray, QList<QDateTime>> m_accessRequests;
    QHash<QByteArray, QList<QDateTime>> m_uploads;

    int m_challengesIssued = 0;
    int m_headsServed = 0;
    int m_downloadsServed = 0;

    QList<std::function<void()>> m_replies;
    bool m_holdReplies = false;
    bool m_loseReplies = false;
    bool m_deliveryScheduled = false;

    /// Owns the scheduled delivery, so a server destroyed with replies waiting
    /// delivers none of them.
    QObject m_replyContext;
};

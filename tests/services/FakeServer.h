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
/// the fake exactly as it is to the real server: an upload's blob is stored and
/// hashed, never read. If a test ever needs the fake to look inside it, the
/// design has been violated somewhere.
///
/// Every call is decided when it is made and answered later: its reply waits in a
/// queue and is delivered on a later turn of the event loop, or when the test says
/// so.
///
/// Beyond ServerApi it offers controls the real server must never have - going
/// offline, holding or losing replies, forcing a conflict, holding the lease as
/// someone else, withholding or substituting a header in the chain, and scripting
/// what successive pairing reads return - so the client's negative tests can make
/// the server misbehave.
class FakeServer : public ServerContract::ServerApi
{
public:
    /// The server's configuration values. Every limit the real server applies is
    /// here, so a test can reach it without generating thousands of calls.
    struct Limits
    {
        int registrationsPerKey = 20;
        int registrationsPerHour = 5;
        int pairingsPerHour = 10;
        int requestsPerHour = 10;
        int uploadsPerHour = 600;
        qint64 maxBlobBytes = 16 * 1024 * 1024;
        qint64 sessionSeconds = 15 * 60;
        qint64 leaseSeconds = 2 * 60;
        qint64 pairingSeconds = 10 * 60;
    };

    FakeServer();
    explicit FakeServer(const Limits& limits);

    // --- ServerApi ---

    void challenge(ServerContract::Reply<QByteArray> reply) override;
    void authenticate(const VerifyKey& device,
                      const QByteArray& challenge,
                      const QByteArray& signature,
                      ServerContract::Reply<ServerContract::Session> reply) override;

    void registerDocument(const ServerContract::Session& session,
                          const ServerContract::Upload& upload,
                          const QByteArray& requestSignature,
                          ServerContract::Reply<qint64> reply) override;
    void head(const ServerContract::Session& session,
              const QString& documentId,
              ServerContract::Reply<ServerContract::Head> reply) override;
    void headers(const ServerContract::Session& session,
                 const QString& documentId,
                 qint64 after,
                 ServerContract::Reply<ServerContract::HeaderChain> reply) override;
    void download(const ServerContract::Session& session,
                  const QString& documentId,
                  qint64 after,
                  ServerContract::Reply<ServerContract::Snapshot> reply) override;
    void upload(const ServerContract::Session& session,
                const QString& documentId,
                qint64 baseVersion,
                const ServerContract::Upload& upload,
                const QByteArray& requestSignature,
                ServerContract::Reply<qint64> reply) override;

    void takeLease(const ServerContract::Session& session,
                   const QString& documentId,
                   const QByteArray& requestSignature,
                   ServerContract::Reply<ServerContract::Lease> reply) override;
    void releaseLease(const ServerContract::Session& session,
                      const QString& documentId,
                      const QByteArray& requestSignature,
                      ServerContract::Reply<ServerContract::Done> reply) override;

    void requestAccess(const ServerContract::Session& session,
                       const QString& documentId,
                       const ServerContract::AccessRequest& request,
                       const QByteArray& requestSignature,
                       ServerContract::Reply<ServerContract::Done> reply) override;
    void accessRequests(const ServerContract::Session& session,
                        const QString& documentId,
                        ServerContract::Reply<QList<ServerContract::AccessRequest>> reply) override;
    void clearAccessRequest(const ServerContract::Session& session,
                            const QString& documentId,
                            const Fingerprint& device,
                            const QByteArray& requestSignature,
                            ServerContract::Reply<ServerContract::Done> reply) override;
    void myRequestStatus(const ServerContract::Session& session,
                         const QString& documentId,
                         ServerContract::Reply<ServerContract::RequestStatus> reply) override;

    void myDocuments(const ServerContract::Session& session,
                     ServerContract::Reply<QStringList> reply) override;

    void startPairing(const ServerContract::Session& session,
                      const QString& deviceName,
                      const QByteArray& commitment,
                      const QByteArray& requestSignature,
                      ServerContract::Reply<QString> reply) override;
    void pairing(const ServerContract::Session& session,
                 const QString& code,
                 ServerContract::Reply<ServerContract::PairingState> reply) override;
    void respondToPairing(const ServerContract::Session& session,
                          const QString& code,
                          const DeviceKeys& responder,
                          const QByteArray& requestSignature,
                          ServerContract::Reply<ServerContract::Done> reply) override;
    void revealPairing(const ServerContract::Session& session,
                       const QString& code,
                       const DeviceKeys& initiator,
                       const QByteArray& nonce,
                       const QByteArray& requestSignature,
                       ServerContract::Reply<ServerContract::Done> reply) override;
    void handOver(const ServerContract::Session& session,
                  const QString& code,
                  const QByteArray& handover,
                  const QByteArray& requestSignature,
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

    /// Calls still take effect, and their replies arrive as Unreachable - a
    /// request the server acted on whose answer never came back.
    void loseReplies(bool lose) { m_loseReplies = lose; }

    /// The next upload to this document is refused as a version conflict.
    void forceConflict(const QString& documentId);

    /// Another device holds the lease until the test says otherwise.
    void holdLeaseAs(const QString& documentId, const Fingerprint& holder);

    /// The chain served for this document leaves out this version's header,
    /// unless it is the last header a read serves.
    void withholdHeader(const QString& documentId, qint64 version);

    /// Every read of this document carries a different header for this version -
    /// in a chain or as a download's last header. The blob is unchanged.
    void substituteHeader(const QString& documentId,
                          qint64 version,
                          const DocumentHeader& replacement);

    /// Successive reads of this pairing return these states, in order, before
    /// returning to the true one.
    void scriptPairingReads(const QString& code, const QList<ServerContract::PairingState>& reads);

    const QDateTime& now() const { return m_now; }

private:
    struct StoredVersion
    {
        qint64 version = 0;
        DocumentHeader header;
        bool kept = false;      ///< its membership differed from the version before
    };

    struct StoredRequest
    {
        ServerContract::AccessRequest request;
        ServerContract::RequestStatus status = ServerContract::RequestStatus::Pending;
        VerifyKey requester;
    };

    struct StoredDocument
    {
        qint64 version = 0;
        QByteArray blob;
        QList<StoredVersion> history;
        std::optional<ServerContract::Lease> lease;
        QHash<QString, StoredRequest> requests;     ///< keyed by the requesting device's fingerprint hex

        bool forceConflict = false;
        QSet<qint64> withheld;
        QHash<qint64, DocumentHeader> substituted;

        const DocumentHeader& current() const { return history.last().header; }
    };

    struct StoredPairing
    {
        ServerContract::PairingState state;
        VerifyKey initiator;
        std::optional<VerifyKey> responder;
        QList<ServerContract::PairingState> scriptedReads;
    };

    struct SessionRecord
    {
        VerifyKey device;
        QDateTime expires;
    };

    /// A completed pairing: this device may be added under the responder's person
    /// in any document that person belongs to.
    struct LinkPermission
    {
        Fingerprint initiator;
        VerifyKey responder;
    };

    // --- The rules, decided at call time ---

    ServerContract::Result<QByteArray> decideChallenge();
    ServerContract::Result<ServerContract::Session> decideAuthenticate(const VerifyKey& device,
                                                                       const QByteArray& challenge,
                                                                       const QByteArray& signature);
    ServerContract::Result<qint64> decideRegisterDocument(const ServerContract::Session& session,
                                                          const ServerContract::Upload& upload,
                                                          const QByteArray& requestSignature);
    ServerContract::Result<ServerContract::Head> decideHead(const ServerContract::Session& session,
                                                            const QString& documentId);
    ServerContract::Result<ServerContract::HeaderChain> decideHeaders(const ServerContract::Session& session,
                                                                      const QString& documentId,
                                                                      qint64 after);
    ServerContract::Result<ServerContract::Snapshot> decideDownload(const ServerContract::Session& session,
                                                                    const QString& documentId,
                                                                    qint64 after);
    ServerContract::Result<qint64> decideUpload(const ServerContract::Session& session,
                                                const QString& documentId,
                                                qint64 baseVersion,
                                                const ServerContract::Upload& upload,
                                                const QByteArray& requestSignature);
    ServerContract::Result<ServerContract::Lease> decideTakeLease(const ServerContract::Session& session,
                                                                  const QString& documentId,
                                                                  const QByteArray& requestSignature);
    ServerContract::Result<ServerContract::Done> decideReleaseLease(const ServerContract::Session& session,
                                                                    const QString& documentId,
                                                                    const QByteArray& requestSignature);
    ServerContract::Result<ServerContract::Done> decideRequestAccess(
        const ServerContract::Session& session,
        const QString& documentId,
        const ServerContract::AccessRequest& request,
        const QByteArray& requestSignature);
    ServerContract::Result<QList<ServerContract::AccessRequest>> decideAccessRequests(
        const ServerContract::Session& session,
        const QString& documentId);
    ServerContract::Result<ServerContract::Done> decideClearAccessRequest(
        const ServerContract::Session& session,
        const QString& documentId,
        const Fingerprint& device,
        const QByteArray& requestSignature);
    ServerContract::Result<ServerContract::RequestStatus> decideMyRequestStatus(
        const ServerContract::Session& session,
        const QString& documentId);
    ServerContract::Result<QStringList> decideMyDocuments(const ServerContract::Session& session);
    ServerContract::Result<QString> decideStartPairing(const ServerContract::Session& session,
                                                       const QString& deviceName,
                                                       const QByteArray& commitment,
                                                       const QByteArray& requestSignature);
    ServerContract::Result<ServerContract::PairingState> decidePairing(
        const ServerContract::Session& session,
        const QString& code);
    ServerContract::Result<ServerContract::Done> decideRespondToPairing(
        const ServerContract::Session& session,
        const QString& code,
        const DeviceKeys& responder,
        const QByteArray& requestSignature);
    ServerContract::Result<ServerContract::Done> decideRevealPairing(
        const ServerContract::Session& session,
        const QString& code,
        const DeviceKeys& initiator,
        const QByteArray& nonce,
        const QByteArray& requestSignature);
    ServerContract::Result<ServerContract::Done> decideHandOver(const ServerContract::Session& session,
                                                                const QString& code,
                                                                const QByteArray& handover,
                                                                const QByteArray& requestSignature);
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

    QList<QJsonObject> chainOf(const StoredDocument& document, qint64 after, qint64 last) const;

    ServerContract::ServerError authenticated(const ServerContract::Session& session,
                                              VerifyKey& device) const;
    ServerContract::ServerError member(const ServerContract::Session& session,
                                       const QString& documentId,
                                       VerifyKey& device,
                                       StoredDocument*& document);

    bool isMemberAnywhere(const VerifyKey& device) const;
    bool withinLimits(const QHash<QByteArray, QList<QDateTime>>& log,
                      const VerifyKey& device,
                      int perHour,
                      int total) const;
    bool linkPermitted(const Fingerprint& device,
                       const MemberId& member,
                       const DocumentHeader& current) const;

    /// Unauthenticated unless the signature is this device's over this request.
    ServerContract::ServerError signedBy(const ServerContract::Session& session,
                                         const VerifyKey& device,
                                         ServerContract::Endpoint endpoint,
                                         const QString& documentId,
                                         const QByteArray& body,
                                         const QByteArray& signature) const;

    /// Whether this device completed a pairing with exactly this responder.
    bool pairedWith(const Fingerprint& initiator, const VerifyKey& responder) const;
    QString randomToken() const;

    Limits m_limits;
    QDateTime m_now;
    bool m_offline = false;

    QHash<QString, StoredDocument> m_documents;
    QHash<QString, StoredPairing> m_pairings;
    QHash<QString, SessionRecord> m_sessions;
    QSet<QByteArray> m_challenges;
    QList<LinkPermission> m_linkPermissions;
    QHash<QByteArray, QList<QDateTime>> m_registrations;
    QHash<QByteArray, QList<QDateTime>> m_pairingStarts;
    QHash<QByteArray, QList<QDateTime>> m_accessRequests;
    QHash<QByteArray, QList<QDateTime>> m_uploads;

    int m_challengesIssued = 0;

    QList<std::function<void()>> m_replies;
    bool m_holdReplies = false;
    bool m_loseReplies = false;
    bool m_deliveryScheduled = false;

    /// Owns the scheduled delivery, so a server destroyed with replies waiting
    /// delivers none of them.
    QObject m_replyContext;
};

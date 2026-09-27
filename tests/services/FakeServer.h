#pragma once

#include "ServerContract.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include <optional>

/// The server contract, in memory, enforcing every rule the real server must.
///
/// A client written against a permissive fake will not work against a strict
/// server, so nothing here is relaxed for convenience. The payload is opaque to
/// the fake exactly as it is to the real server: an upload's blob is stored and
/// hashed, never read. If a test ever needs the fake to look inside it, the
/// design has been violated somewhere.
///
/// Beyond ServerApi it offers controls the real server must never have - going
/// offline, forcing a conflict, holding the lease as someone else, withholding or
/// substituting a header in the chain, and scripting what successive pairing
/// reads return - so the client's negative tests can make the server misbehave.
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

    ServerContract::Result<QByteArray> challenge() override;
    ServerContract::Result<ServerContract::Session> authenticate(const VerifyKey& device,
                                                                 const QByteArray& challenge,
                                                                 const QByteArray& signature) override;

    ServerContract::Result<qint64> registerDocument(const ServerContract::Session& session,
                                                    const ServerContract::Upload& upload,
                                                    const QByteArray& requestSignature) override;
    ServerContract::Result<ServerContract::Head> head(const ServerContract::Session& session,
                                                      const QString& documentId) override;
    ServerContract::Result<ServerContract::HeaderChain> headers(const ServerContract::Session& session,
                                                                const QString& documentId,
                                                                qint64 after,
                                                                qint64 through) override;
    ServerContract::Result<ServerContract::Snapshot> download(const ServerContract::Session& session,
                                                              const QString& documentId) override;
    ServerContract::Result<qint64> upload(const ServerContract::Session& session,
                                          const QString& documentId,
                                          qint64 baseVersion,
                                          const ServerContract::Upload& upload,
                                          const QByteArray& requestSignature) override;

    ServerContract::Result<ServerContract::Lease> takeLease(const ServerContract::Session& session,
                                                            const QString& documentId,
                                                            const QByteArray& requestSignature) override;
    ServerContract::Result<ServerContract::Done> releaseLease(const ServerContract::Session& session,
                                                              const QString& documentId,
                                                              const QByteArray& requestSignature) override;

    ServerContract::Result<ServerContract::Done> requestAccess(
        const ServerContract::Session& session,
        const QString& documentId,
        const ServerContract::AccessRequest& request,
        const QByteArray& requestSignature) override;
    ServerContract::Result<QList<ServerContract::AccessRequest>> accessRequests(
        const ServerContract::Session& session,
        const QString& documentId) override;
    ServerContract::Result<ServerContract::Done> clearAccessRequest(
        const ServerContract::Session& session,
        const QString& documentId,
        const Fingerprint& device,
        const QByteArray& requestSignature) override;
    ServerContract::Result<ServerContract::RequestStatus> myRequestStatus(
        const ServerContract::Session& session,
        const QString& documentId) override;

    ServerContract::Result<QStringList> myDocuments(const ServerContract::Session& session) override;

    ServerContract::Result<QString> startPairing(const ServerContract::Session& session,
                                                 const QString& deviceName,
                                                 const QByteArray& commitment,
                                                 const QByteArray& requestSignature) override;
    ServerContract::Result<ServerContract::PairingState> pairing(
        const ServerContract::Session& session,
        const QString& code) override;
    ServerContract::Result<ServerContract::Done> respondToPairing(
        const ServerContract::Session& session,
        const QString& code,
        const DeviceKeys& responder,
        const QByteArray& requestSignature) override;
    ServerContract::Result<ServerContract::Done> revealPairing(
        const ServerContract::Session& session,
        const QString& code,
        const DeviceKeys& initiator,
        const QByteArray& nonce,
        const QByteArray& requestSignature) override;
    ServerContract::Result<ServerContract::Done> handOver(const ServerContract::Session& session,
                                                          const QString& code,
                                                          const QByteArray& handover,
                                                          const QByteArray& requestSignature) override;

    ServerContract::Result<ServerContract::Done> advanceClock(qint64 seconds) override;

    // --- Controls the real server must never offer ---

    void setOffline(bool offline) { m_offline = offline; }

    /// The next upload to this document is refused as a version conflict.
    void forceConflict(const QString& documentId);

    /// Another device holds the lease until the test says otherwise.
    void holdLeaseAs(const QString& documentId, const Fingerprint& holder);

    /// The chain served for this document leaves out this version's header.
    void withholdHeader(const QString& documentId, qint64 version);

    /// The chain served for this document carries a different header for this
    /// version.
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
    ServerContract::ServerError signedBy(const VerifyKey& device,
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
};

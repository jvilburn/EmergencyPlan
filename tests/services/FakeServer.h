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
/// someone else, serving a version of its own making - so the client's negative tests
/// can make the server misbehave.
class FakeServer : public ServerContract::ServerApi
{
public:
    /// The server's configuration values. Every limit the real server applies is
    /// here, so a test can reach it without generating thousands of calls.
    struct Limits
    {
        int uploadsPerHour = 600;
        qint64 maxBlobBytes = 16 * 1024 * 1024;
        qint64 leaseSeconds = 2 * 60;
    };

    FakeServer();
    explicit FakeServer(const Limits& limits);

    // --- ServerApi ---

    void registerDocument(const ServerContract::Signer& signer,
                          const ServerContract::Upload& upload,
                          ServerContract::Reply<qint64> reply) override;
    void head(const QString& documentId, ServerContract::Reply<ServerContract::Head> reply) override;
    void download(const QString& documentId, ServerContract::Reply<ServerContract::Snapshot> reply) override;
    void upload(const ServerContract::Signer& signer,
                const QString& documentId,
                qint64 baseVersion,
                const ServerContract::Upload& upload,
                ServerContract::Reply<qint64> reply) override;
    void takeLease(const ServerContract::Signer& signer,
                   const QString& documentId,
                   ServerContract::Reply<ServerContract::Lease> reply) override;
    void releaseLease(const ServerContract::Signer& signer,
                      const QString& documentId,
                      ServerContract::Reply<ServerContract::Done> reply) override;
    void advanceClock(qint64 seconds, ServerContract::Reply<ServerContract::Done> reply) override;

    // --- Controls the real server must never offer ---

    void setOffline(bool offline) { m_offline = offline; }

    /// Queued replies wait for deliverNext() while held.
    void holdReplies(bool hold);

    /// Delivers the oldest queued reply; false when none is waiting.
    bool deliverNext();

    qsizetype waitingReplies() const { return m_replies.size(); }

    /// Head and download calls taken, whatever they answered.
    int headsServed() const { return m_headsServed; }
    int downloadsServed() const { return m_downloadsServed; }

    /// Calls still take effect, and their replies arrive as Unreachable - a
    /// request the server acted on whose answer never came back.
    void loseReplies(bool lose) { m_loseReplies = lose; }

    /// The next upload to this document is refused as a version conflict.
    void forceConflict(const QString& documentId);

    /// Another device holds the lease until the test says otherwise.
    void holdLeaseAs(const QString& documentId, const Fingerprint& holder);

    /// Every head and download of this document serves this header and blob in
    /// place of the stored ones, as a hostile or broken server would. Uploads still
    /// build on the stored version. An empty header serves the stored one again.
    void serveInstead(const QString& documentId, const QJsonObject& header, const QByteArray& blob);

    const QDateTime& now() const { return m_now; }

private:
    struct StoredDocument
    {
        qint64 version = 0;
        QJsonObject header;
        QSet<QString> recipients;       ///< the header's fingerprints, hex
        QByteArray blob;
        ServerContract::Publication published;
        std::optional<ServerContract::Lease> lease;
        bool forceConflict = false;
        std::optional<QJsonObject> servedHeader;
        QByteArray servedBlob;
    };

    // --- The rules, decided at call time ---

    ServerContract::Result<qint64> decideRegisterDocument(const ServerContract::Signer& signer,
                                                          const ServerContract::Upload& upload);
    ServerContract::Result<ServerContract::Head> decideHead(const QString& documentId);
    ServerContract::Result<ServerContract::Snapshot> decideDownload(const QString& documentId);
    ServerContract::Result<qint64> decideUpload(const ServerContract::Signer& signer,
                                                const QString& documentId,
                                                qint64 baseVersion,
                                                const ServerContract::Upload& upload);
    ServerContract::Result<ServerContract::Lease> decideTakeLease(const ServerContract::Signer& signer,
                                                                  const QString& documentId);
    ServerContract::Result<ServerContract::Done> decideReleaseLease(const ServerContract::Signer& signer,
                                                                    const QString& documentId);
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

    /// The document, found and its signer among its current recipients, with the
    /// signature verified over bytes: None, Unreachable, UnknownDocument,
    /// NotAKeyHolder, or InvalidRequest.
    ServerContract::ServerError keyHolder(const ServerContract::Signer& signer,
                                          const QString& documentId,
                                          const QByteArray& bytes,
                                          StoredDocument*& document);

    static QSet<QString> recipientsOf(const DocumentHeader& header);
    static bool wellFormed(const QString& documentId);
    bool withinUploadLimit(const VerifyKey& device) const;

    Limits m_limits;
    QDateTime m_now;
    bool m_offline = false;

    QHash<QString, StoredDocument> m_documents;
    QHash<QByteArray, QList<QDateTime>> m_uploads;

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

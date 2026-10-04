#include "ServerVersion.h"

#include <QFile>
#include <QObject>
#include <QTimer>

#include <memory>

using namespace ServerContract;
using namespace std::placeholders;

namespace
{

using ServerVersion::Fetched;

Fetched outcomeOnly(Fetched::Outcome outcome)
{
    Fetched fetched;
    fetched.outcome = outcome;
    return fetched;
}

Fetched fault(const QString& reason)
{
    Fetched fetched;
    fetched.outcome = Fetched::Outcome::ServerFault;
    fetched.reason = reason;
    return fetched;
}

Fetched notProven()
{
    return fault(QObject::tr("The server refused this device, and nothing proves it was removed."));
}

/// A call that got no answer to judge. Worth trying again, whatever the reason:
/// only a version that fails validation is the server misbehaving.
Fetched failedCall()
{
    return outcomeOnly(Fetched::Outcome::CallFailed);
}

/// One fetch, alive for as long as a reply is outstanding. Its steps are bound to
/// a shared pointer, so a fetch whose receiver is destroyed simply ends.
class Fetch : public std::enable_shared_from_this<Fetch>
{
public:
    Fetch(const DeviceIdentity& identity,
          DocumentRecords& records,
          ServerConnection& server,
          const QString& documentId,
          const DocumentPaths& paths,
          QObject* receiver,
          ServerVersion::FetchDone done);

    void download();
    void proveRemoval();

private:
    void finish(const Fetched& fetched) { m_done(fetched); }
    void onSnapshot(const Result<Snapshot>& snapshot);
    void onChain(const Result<HeaderChain>& chain);

    /// Which header dropped this device, who wrote it, and whether it was this
    /// person's own leave. The chain has already been proved.
    Fetched removal(const QList<DocumentHeader>& laterHeaders) const;

    DeviceIdentity m_identity;
    DocumentRecords& m_records;
    ServerConnection& m_server;
    QString m_documentId;
    DocumentPaths m_paths;
    QObject* m_receiver;
    ServerVersion::FetchDone m_done;

    std::optional<qint64> m_baseVersion;
    std::optional<ServerVersion::BaseCopy> m_base;
};

Fetch::Fetch(const DeviceIdentity& identity,
             DocumentRecords& records,
             ServerConnection& server,
             const QString& documentId,
             const DocumentPaths& paths,
             QObject* receiver,
             ServerVersion::FetchDone done)
    : m_identity(identity)
    , m_records(records)
    , m_server(server)
    , m_documentId(documentId)
    , m_paths(paths)
    , m_receiver(receiver)
    , m_done(done)
    , m_baseVersion(records.record(documentId).baseVersion)
    , m_base(ServerVersion::readBase(documentId, paths, records, identity.self()))
{
}

void Fetch::download()
{
    m_server.download(m_documentId,
                      m_baseVersion.value_or(0),
                      m_receiver,
                      std::bind(&Fetch::onSnapshot, shared_from_this(), _1));
}

void Fetch::proveRemoval()
{
    // Without a base version there is no anchor, and a header without this device
    // proves nothing.
    // Answered on a later turn like every other outcome, so no caller is re-entered.
    if (!m_baseVersion.has_value())
    {
        QTimer::singleShot(0, m_receiver, std::bind(&Fetch::finish, shared_from_this(), notProven()));
        return;
    }

    m_server.headers(m_documentId,
                     m_baseVersion.value(),
                     m_receiver,
                     std::bind(&Fetch::onChain, shared_from_this(), _1));
}

void Fetch::onSnapshot(const Result<Snapshot>& snapshot)
{
    if (snapshot.error == ServerError::NotAllowListed)
    {
        proveRemoval();
        return;
    }

    if (!snapshot.ok())
    {
        m_done(failedCall());
        return;
    }

    QList<DocumentHeader> headers;
    switch (Reconcile::readHeaders(snapshot.value->headers, headers))
    {
    case Reconcile::Verdict::Accept:
        break;
    case Reconcile::Verdict::NeedsNewerVersion:
        m_done(outcomeOnly(Fetched::Outcome::NeedsNewerVersion));
        return;
    case Reconcile::Verdict::Removed:
    case Reconcile::Verdict::ServerFault:
        m_done(fault(QObject::tr("The server sent a header this build cannot read.")));
        return;
    }

    if (headers.isEmpty())
    {
        m_done(fault(QObject::tr("The server sent no header for the version it offered.")));
        return;
    }

    // A snapshot at the base version is the base itself, with no chain after it
    // to walk, so it is judged directly - and must be the header already held.
    std::optional<DocumentHeader> anchor;
    if (m_base.has_value())
    {
        anchor = m_base.value().sealed.header;
    }
    if (m_baseVersion.has_value() && snapshot.value->version == m_baseVersion.value())
    {
        if (anchor.has_value() && headers.last() != anchor.value())
        {
            m_done(fault(QObject::tr("The server's copy of this version is not the one this device holds.")));
            return;
        }
        anchor.reset();
    }

    const Reconcile::DownloadCheck check = Reconcile::validateDownload(m_documentId,
                                                                       anchor,
                                                                       headers,
                                                                       snapshot.value->blob,
                                                                       m_identity.self(),
                                                                       m_records.record(m_documentId).highWaterMark);
    switch (check.verdict)
    {
    case Reconcile::Verdict::Accept:
        break;
    case Reconcile::Verdict::Removed:
        m_done(removal(headers));
        return;
    case Reconcile::Verdict::NeedsNewerVersion:
        m_done(outcomeOnly(Fetched::Outcome::NeedsNewerVersion));
        return;
    case Reconcile::Verdict::ServerFault:
        m_done(fault(check.reason));
        return;
    }

    Fetched fetched;
    fetched.outcome = Fetched::Outcome::Accepted;
    fetched.version = snapshot.value->version;
    fetched.sealed = DocumentFile::Sealed{check.header.value(), snapshot.value->blob};
    fetched.dataKey = check.dataKey.value();
    fetched.document = check.document.value();
    fetched.laterHeaders = headers;
    m_done(fetched);
}

void Fetch::onChain(const Result<HeaderChain>& chain)
{
    if (!chain.ok())
    {
        m_done(failedCall());
        return;
    }

    QList<DocumentHeader> headers;
    if (Reconcile::readHeaders(chain.value->headers, headers) != Reconcile::Verdict::Accept)
    {
        m_done(notProven());
        return;
    }

    std::optional<DocumentHeader> anchor;
    if (m_base.has_value())
    {
        anchor = m_base.value().sealed.header;
    }

    // Only a chain-proven removal is acted on.
    if (Reconcile::checkRemoval(m_documentId, anchor, headers, m_identity.fingerprint()) != Reconcile::Verdict::Removed)
    {
        m_done(notProven());
        return;
    }

    m_done(removal(headers));
}

Fetched Fetch::removal(const QList<DocumentHeader>& laterHeaders) const
{
    Fetched fetched;
    fetched.outcome = Fetched::Outcome::Removed;
    fetched.laterHeaders = laterHeaders;

    const Fingerprint self = m_identity.fingerprint();

    // The last step from a header listing this device to one that does not.
    for (qsizetype index = laterHeaders.size() - 1; index >= 0; --index)
    {
        const DocumentHeader& proving = laterHeaders.at(index);
        const DocumentHeader& previous = index == 0 ? m_base.value().sealed.header : laterHeaders.at(index - 1);
        if (proving.entryFor(self).has_value() || !previous.entryFor(self).has_value())
        {
            continue;
        }

        fetched.remover = proving.lastWriter;
        const std::optional<RecipientEntry> writer = previous.entryFor(proving.lastWriter);
        fetched.left = writer.has_value() && writer.value().member == m_identity.member;
        break;
    }

    return fetched;
}

} // namespace

namespace ServerVersion
{

std::optional<BaseCopy> readBase(const QString& documentId,
                                 const DocumentPaths& paths,
                                 const DocumentRecords& records,
                                 const Reconcile::Self& self)
{
    const DocumentRecord record = records.record(documentId);
    const std::optional<QByteArray> bytes = DocumentFile::readAll(paths.base);
    if (!record.baseVersion.has_value() || !bytes.has_value())
    {
        return std::nullopt;
    }

    DocumentFile::Sealed sealed;
    if (DocumentFile::decode(bytes.value(), sealed) != DocumentFile::Read::Ok
        || DocumentCrypto::payloadHash(sealed.ciphertext) != record.baseHash)
    {
        return std::nullopt;
    }

    const Reconcile::DownloadCheck check = Reconcile::validateDownload(documentId,
                                                                       std::nullopt,
                                                                       {sealed.header},
                                                                       sealed.ciphertext,
                                                                       self,
                                                                       Reconcile::HighWaterMark());
    if (check.verdict != Reconcile::Verdict::Accept)
    {
        return std::nullopt;
    }

    return BaseCopy{sealed, check.document.value(), check.dataKey.value()};
}

bool writeBase(DocumentRecords& records,
               const QString& documentId,
               const DocumentPaths& paths,
               const DocumentFile::Sealed& sealed,
               qint64 version,
               QString* errorMessage)
{
    if (!DocumentFile::write(paths.base, DocumentFile::encode(sealed), errorMessage))
    {
        return false;
    }

    DocumentRecord record = records.record(documentId);
    record.baseVersion = version;
    record.baseHash = sealed.header.payloadHash;
    if (!record.highWaterMark.isAbove(sealed.header))
    {
        record.highWaterMark.keyGeneration = sealed.header.keyGeneration;
        record.highWaterMark.saveCounter = sealed.header.saveCounter;
    }
    records.setRecord(documentId, record);
    return true;
}

void forget(DocumentRecords& records, const QString& documentId, const DocumentPaths& paths)
{
    QFile::remove(paths.file);
    QFile::remove(paths.base);
    records.removeRecord(documentId);
}

void fetch(const DeviceIdentity& identity,
           DocumentRecords& records,
           ServerConnection& server,
           const QString& documentId,
           const DocumentPaths& paths,
           QObject* receiver,
           FetchDone done)
{
    std::make_shared<Fetch>(identity, records, server, documentId, paths, receiver, done)->download();
}

void proveRemoval(const DeviceIdentity& identity,
                  DocumentRecords& records,
                  ServerConnection& server,
                  const QString& documentId,
                  const DocumentPaths& paths,
                  QObject* receiver,
                  FetchDone done)
{
    std::make_shared<Fetch>(identity, records, server, documentId, paths, receiver, done)->proveRemoval();
}

} // namespace ServerVersion

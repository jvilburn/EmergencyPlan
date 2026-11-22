#include "FakeServer.h"

#include "Base64Url.h"
#include "Crypto.h"
#include "Identity.h"
#include "SignedLine.h"
#include "WardHeader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <optional>

/// A request on its way to the fake server.
class FakeCall : public TransportCall
{
public:
    explicit FakeCall(QObject* parent) : TransportCall(parent) {}

    void answer(const TransportReply& reply)
    {
        emit answered(reply);
        deleteLater();
    }
};

/// A channel to the fake server, open from the moment the server accepts it until either side
/// closes it.
class FakeChannel : public TransportChannel
{
public:
    FakeChannel(FakeServer* server, const QString& document, const QByteArray& fingerprint, QObject* parent)
        : TransportChannel(parent)
        , m_server(server)
        , m_document(document)
        , m_fingerprint(fingerprint)
    {
    }

    ~FakeChannel() override { close(); }

    void close() override
    {
        if (m_server != nullptr)
        {
            FakeServer* server = m_server;
            m_server = nullptr;
            server->channelGone(this);
        }
    }

    /// The server has let go of this channel.
    void forgetServer() { m_server = nullptr; }

    const QString& document() const { return m_document; }
    const QByteArray& fingerprint() const { return m_fingerprint; }

    void deliverMessage(const QByteArray& message) { emit messageReceived(message); }
    void deliverClosed() { emit closed(); }

private:
    FakeServer* m_server;
    QString m_document;
    QByteArray m_fingerprint;
};

namespace
{
TransportReply status(int code)
{
    return TransportReply{code, QByteArray()};
}

QByteArray headerOf(const TransportRequest& request, const QByteArray& name)
{
    for (const QPair<QByteArray, QByteArray>& header : request.headers)
    {
        if (header.first == name)
        {
            return header.second;
        }
    }
    return QByteArray();
}

/// Who signed a request, from its Ward-Signer and Ward-Signature headers.
struct Signer
{
    QByteArray fingerprint;
    QByteArray verifyKey;
    QByteArray signature;
};

std::optional<Signer> signerOf(const TransportRequest& request)
{
    QStringList keys = QString::fromLatin1(headerOf(request, "Ward-Signer")).split('.');
    if (keys.size() != 2)
    {
        return std::nullopt;
    }
    std::optional<QByteArray> publicKey = Base64Url::decode(keys[0], Crypto::KeyBytes);
    std::optional<QByteArray> verifyKey = Base64Url::decode(keys[1], Crypto::KeyBytes);
    std::optional<QByteArray> signature = Base64Url::decode(
        QString::fromLatin1(headerOf(request, "Ward-Signature")), Crypto::SignatureBytes);
    if (!publicKey.has_value() || !verifyKey.has_value() || !signature.has_value())
    {
        return std::nullopt;
    }
    return Signer{Identity::fingerprintOf(*publicKey, *verifyKey), *verifyKey, *signature};
}

/// The header of bytes, a version in a ward file's framing, when it has one that reads.
std::optional<WardHeader> headerIn(const QByteArray& bytes)
{
    qsizetype boundary = bytes.indexOf('\n');
    if (boundary < 0)
    {
        return std::nullopt;
    }
    HeaderRead read = WardHeader::parse(bytes.left(boundary));
    if (read.status != HeaderStatus::Read)
    {
        return std::nullopt;
    }
    return read.header;
}

bool names(const WardHeader& header, const QByteArray& fingerprint)
{
    return header.sealedKeyFor(fingerprint).has_value();
}
}  // namespace

FakeServer::~FakeServer()
{
    for (const Ward& ward : std::as_const(m_wards))
    {
        for (FakeChannel* channel : ward.channels)
        {
            channel->forgetServer();
        }
    }
}

TransportCall* FakeServer::send(const TransportRequest& request, QObject* parent)
{
    m_calls.append(request.method);
    FakeCall* call = new FakeCall(parent);
    qsizetype before = m_events.size();
    TransportReply reply = m_reachable ? handle(request) : status(TransportReply::NoAnswer);
    bool write = request.method == "POST" || request.method == "PUT";
    if (write && m_reachable && m_dropNextWriteReply)
    {
        m_dropNextWriteReply = false;
        reply = status(TransportReply::NoAnswer);
    }
    m_events.insert(m_repliesFirst ? before : m_events.size(), Event{EventKind::Reply, call, reply, QByteArray()});
    return call;
}

TransportChannel* FakeServer::open(const TransportRequest& request, QObject* parent)
{
    m_calls.append("OPEN");
    QString document = request.path.section('/', 2, 2);
    std::optional<Signer> signer = signerOf(request);
    QByteArray fingerprint = signer.has_value() ? signer->fingerprint : QByteArray();
    FakeChannel* channel = new FakeChannel(this, document, fingerprint, parent);

    std::optional<WardHeader> current = headerIn(m_wards.value(document).bytes);
    QByteArray line = SignedLine::ofChannel(DocumentId::fromString(document));
    bool accepted = m_reachable
        && signer.has_value()
        && current.has_value()
        && Crypto::verify(signer->signature, line, signer->verifyKey)
        && names(*current, fingerprint);
    if (!accepted)
    {
        channel->forgetServer();
        m_events.append(Event{EventKind::Closed, channel, TransportReply(), QByteArray()});
        return channel;
    }

    Ward& ward = m_wards[document];
    ward.channels.append(channel);
    tellEveryone(document);
    return channel;
}

void FakeServer::setReachable(bool reachable)
{
    m_reachable = reachable;
    if (reachable)
    {
        return;
    }
    const QStringList documents = m_wards.keys();
    for (const QString& document : documents)
    {
        const QList<FakeChannel*> open = m_wards[document].channels;
        for (FakeChannel* channel : open)
        {
            closeFromServer(channel);
        }
    }
}

bool FakeServer::deliverNext()
{
    while (!m_events.isEmpty())
    {
        if (deliver(m_events.takeFirst()))
        {
            return true;
        }
    }
    return false;
}

bool FakeServer::deliverNextReply()
{
    for (qsizetype i = 0; i < m_events.size(); ++i)
    {
        if (m_events[i].kind == EventKind::Reply && deliver(m_events.takeAt(i)))
        {
            return true;
        }
    }
    return false;
}

bool FakeServer::deliver(const Event& event)
{
    if (event.target.isNull())
    {
        return false;
    }
    switch (event.kind)
    {
    case EventKind::Reply:
        static_cast<FakeCall*>(event.target.data())->answer(event.reply);
        break;
    case EventKind::Message:
        static_cast<FakeChannel*>(event.target.data())->deliverMessage(event.message);
        break;
    case EventKind::Closed:
        static_cast<FakeChannel*>(event.target.data())->deliverClosed();
        break;
    }
    return true;
}

void FakeServer::settle()
{
    while (deliverNext())
    {
    }
}

void FakeServer::serve(const DocumentId& document, const QByteArray& bytes)
{
    m_wards[document.toString()].bytes = bytes;
}

void FakeServer::forget(const DocumentId& document)
{
    const QList<FakeChannel*> open = m_wards.value(document.toString()).channels;
    for (FakeChannel* channel : open)
    {
        closeFromServer(channel);
    }
    m_wards.remove(document.toString());
}

QByteArray FakeServer::version(const DocumentId& document) const
{
    return m_wards.value(document.toString()).bytes;
}

qint64 FakeServer::number(const DocumentId& document) const
{
    std::optional<WardHeader> header = headerIn(version(document));
    return header.has_value() ? header->version().number : 0;
}

QSet<QByteArray> FakeServer::present(const DocumentId& document) const
{
    QSet<QByteArray> present;
    for (const FakeChannel* channel : m_wards.value(document.toString()).channels)
    {
        present.insert(channel->fingerprint());
    }
    return present;
}

void FakeServer::channelGone(FakeChannel* channel)
{
    if (m_wards.contains(channel->document()) && m_wards[channel->document()].channels.removeOne(channel))
    {
        tellEveryone(channel->document());
    }
}

TransportReply FakeServer::handle(const TransportRequest& request)
{
    QStringList parts = request.path.split('/');
    if (request.method == "POST" && request.path == "/documents")
    {
        return create(request);
    }
    if (parts.size() == 3 && parts[1] == "documents" && request.method == "PUT")
    {
        return push(request, parts[2]);
    }
    if (parts.size() == 3 && parts[1] == "documents" && request.method == "GET")
    {
        return read(parts[2], true);
    }
    if (parts.size() == 4 && parts[1] == "documents" && parts[3] == "head" && request.method == "GET")
    {
        return read(parts[2], false);
    }
    return status(404);
}

TransportReply FakeServer::create(const TransportRequest& request)
{
    std::optional<WardHeader> header = headerIn(request.body);
    std::optional<Signer> signer = signerOf(request);
    if (!header.has_value() || !signer.has_value())
    {
        return status(400);
    }
    QString document = header->document().toString();
    QByteArray line = SignedLine::ofVersion(SignedCall::Create, header->document(), request.body);
    if (!m_wards.value(document).bytes.isEmpty())
    {
        return status(409);
    }
    if (!Crypto::verify(signer->signature, line, signer->verifyKey)
        || header->version().writer != signer->fingerprint
        || !names(*header, signer->fingerprint))
    {
        return status(403);
    }
    m_wards[document].bytes = request.body;
    return status(201);
}

TransportReply FakeServer::push(const TransportRequest& request, const QString& document)
{
    std::optional<WardHeader> header = headerIn(request.body);
    std::optional<Signer> signer = signerOf(request);
    if (!header.has_value() || !signer.has_value() || header->document().toString() != document)
    {
        return status(400);
    }
    QByteArray line = SignedLine::ofVersion(SignedCall::Push, header->document(), request.body);
    std::optional<WardHeader> current = headerIn(m_wards.value(document).bytes);
    if (!current.has_value())
    {
        return status(404);
    }
    if (!Crypto::verify(signer->signature, line, signer->verifyKey)
        || header->version().writer != signer->fingerprint
        || !names(*current, signer->fingerprint))
    {
        return status(403);
    }
    if (header->version().number <= current->version().number)
    {
        return status(409);
    }
    Ward& ward = m_wards[document];
    ward.bytes = request.body;
    const QList<FakeChannel*> open = ward.channels;
    for (FakeChannel* channel : open)
    {
        if (!names(*header, channel->fingerprint()))
        {
            closeFromServer(channel);
        }
    }
    tellEveryone(document);
    return status(200);
}

TransportReply FakeServer::read(const QString& document, bool whole) const
{
    QByteArray bytes = m_wards.value(document).bytes;
    if (bytes.isEmpty())
    {
        return status(404);
    }
    return TransportReply{200, whole ? bytes : bytes.left(bytes.indexOf('\n'))};
}

void FakeServer::closeFromServer(FakeChannel* channel)
{
    m_wards[channel->document()].channels.removeOne(channel);
    channel->forgetServer();
    m_events.append(Event{EventKind::Closed, channel, TransportReply(), QByteArray()});
}

void FakeServer::tellEveryone(const QString& document)
{
    const Ward ward = m_wards.value(document);
    QStringList present;
    for (const FakeChannel* channel : ward.channels)
    {
        present.append(Base64Url::encode(channel->fingerprint()));
    }
    present.removeDuplicates();
    present.sort();
    std::optional<WardHeader> header = headerIn(ward.bytes);
    QJsonObject json;
    json["present"] = QJsonArray::fromStringList(present);
    json["version"] = header.has_value() ? header->version().number : 0;
    QByteArray message = QJsonDocument(json).toJson(QJsonDocument::Compact);
    for (FakeChannel* channel : ward.channels)
    {
        m_events.append(Event{EventKind::Message, channel, TransportReply(), message});
    }
}

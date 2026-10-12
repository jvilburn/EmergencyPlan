#include "HttpServerApiTest.h"

#include "AppSettings.h"
#include "DocumentCrypto.h"
#include "DocumentFile.h"
#include "DocumentLibrary.h"
#include "HttpServerApi.h"
#include "HttpWire.h"
#include "SessionTestSupport.h"

#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

#include <memory>

using namespace ServerContract;

namespace
{

QJsonObject jsonOf(const char* text)
{
    return QJsonDocument::fromJson(QByteArray(text)).object();
}

QString base64(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes.toBase64());
}

/// A header and blob as a ward's first version writes them.
Upload firstUpload()
{
    const DeviceIdentity alice = SessionTestSupport::newIdentity("Alice", "Laptop");
    Document document = Document::empty();
    document.setSealingSet(SealingSet::of(alice.keys()));

    DocumentHeader first;
    first.documentId = DocumentLibrary::newDocumentId();
    const DocumentFile::Sealed sealed = DocumentFile::seal(document, first, DocumentCrypto::generateDataKey()).value();
    return Upload{sealed.header, sealed.ciphertext};
}

const char* const When = "2026-10-11T09:00:00.000Z";

PublicKey aliceLaptopKey()
{
    return PublicKey{QByteArray(32, '\x0a')};
}

VerifyKey aliceLaptopVerifyKey()
{
    return VerifyKey{QByteArray(32, '\x0b')};
}

Fingerprint aliceLaptopFingerprint()
{
    return DocumentCrypto::fingerprintOf(aliceLaptopKey(), aliceLaptopVerifyKey());
}

/// A device as the wire carries it.
QJsonObject deviceJson(const PublicKey& publicKey, const VerifyKey& verifyKey, const QString& name)
{
    return QJsonObject{{"fingerprint", DocumentCrypto::fingerprintOf(publicKey, verifyKey).toHex()},
                       {"publicKey", base64(publicKey.bytes)},
                       {"verifyKey", base64(verifyKey.bytes)},
                       {"name", name},
                       {"linkedAt", When}};
}

QJsonObject aliceLaptop()
{
    return deviceJson(aliceLaptopKey(), aliceLaptopVerifyKey(), "Laptop");
}

QJsonObject otherDevice()
{
    return deviceJson(PublicKey{QByteArray(32, '\x0c')}, VerifyKey{QByteArray(32, '\x0d')}, "Phone");
}

QJsonObject personJson(const QString& member, const QString& name, const QList<QJsonObject>& devices)
{
    QJsonArray array;
    for (const QJsonObject& device : devices)
    {
        array.append(device);
    }
    return QJsonObject{{"member", member}, {"displayName", name}, {"devices", array}};
}

void markAnswered(bool& answered)
{
    answered = true;
}

QByteArray httpResponse(int status, const QByteArray& body)
{
    return "HTTP/1.1 " + QByteArray::number(status) + " Status\r\n"
         + "Content-Type: application/json\r\n"
         + "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
         + "Connection: close\r\n\r\n"
         + body;
}

/// One request as it arrived.
struct Captured
{
    QByteArray method;
    QByteArray path;
    QHash<QByteArray, QByteArray> headers;   ///< names in lowercase
    QByteArray body;
};

/// A local HTTP server that answers each request with one canned response and
/// remembers the last request.
class CannedServer
{
public:
    explicit CannedServer(const QByteArray& response)
        : m_response(response)
    {
        m_server.listen(QHostAddress::LocalHost);
        QObject::connect(&m_server, &QTcpServer::newConnection, &m_server, std::bind(&CannedServer::accept, this));
    }

    bool isListening() const { return m_server.isListening(); }
    QString address() const { return QString("http://127.0.0.1:%1").arg(m_server.serverPort()); }
    const std::optional<Captured>& captured() const { return m_captured; }

private:
    void accept()
    {
        QTcpSocket* socket = m_server.nextPendingConnection();
        if (socket == nullptr)
        {
            return;
        }
        std::shared_ptr<QByteArray> buffer = std::make_shared<QByteArray>();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, std::bind(&CannedServer::read, this, socket, buffer));
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }

    void read(QTcpSocket* socket, const std::shared_ptr<QByteArray>& buffer)
    {
        buffer->append(socket->readAll());
        answerOnceComplete(socket, *buffer);
    }

    void answerOnceComplete(QTcpSocket* socket, const QByteArray& buffer)
    {
        const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
        {
            return;
        }

        Captured request;
        const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
        const QList<QByteArray> requestLine = lines.at(0).trimmed().split(' ');
        request.method = requestLine.value(0);
        request.path = requestLine.value(1);
        for (qsizetype i = 1; i < lines.size(); ++i)
        {
            const qsizetype colon = lines.at(i).indexOf(':');
            if (colon > 0)
            {
                request.headers.insert(lines.at(i).left(colon).trimmed().toLower(), lines.at(i).mid(colon + 1).trimmed());
            }
        }

        const qsizetype length = request.headers.value("content-length", "0").toLongLong();
        if (buffer.size() < headerEnd + 4 + length)
        {
            return;
        }
        request.body = buffer.mid(headerEnd + 4, length);
        m_captured = request;

        socket->write(m_response);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QByteArray m_response;
    std::optional<Captured> m_captured;
};

/// Longer than any answer from a local responder takes; a reply not in by then
/// never came.
constexpr int AnswerMilliseconds = 15 * 1000;

/// Records a reply's answer, and that it came.
template<typename T>
void record(std::optional<Result<T>>& out, const Result<T>& result)
{
    out = result;
}

template<typename T>
Reply<T> recordTo(std::optional<Result<T>>& out)
{
    return std::bind(&record<T>, std::ref(out), std::placeholders::_1);
}

/// Settings over a fresh folder, pointing at address.
std::unique_ptr<AppSettings> settingsFor(const QTemporaryDir& folder, const QString& address)
{
    std::unique_ptr<AppSettings> settings = AppSettings::open(folder.filePath("settings.json"), nullptr);
    if (!address.isEmpty())
    {
        settings->setServerAddressOverride(address);
    }
    return settings;
}

} // namespace

void HttpServerApiTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void HttpServerApiTest::requestsAreWrittenInTheWireFormat()
{
    const QByteArray key32(32, '\x01');
    const QByteArray challenge("one-time bytes");
    const QByteArray signature(64, '\x02');
    VerifyKey verifyKey;
    verifyKey.bytes = key32;
    QCOMPARE(HttpWire::authenticateRequest(verifyKey, challenge, signature),
             (QJsonObject{{"device", base64(key32)}, {"challenge", base64(challenge)}, {"signature", base64(signature)}}));

    const Upload upload = firstUpload();
    QCOMPARE(HttpWire::registerRequest(upload),
             (QJsonObject{{"header", upload.header.toJson()}, {"blob", base64(upload.blob)}}));
    QCOMPARE(HttpWire::uploadRequest(7, upload),
             (QJsonObject{{"baseVersion", 7}, {"header", upload.header.toJson()}, {"blob", base64(upload.blob)}}));

    const PublicKey publicKey{QByteArray(32, '\x03')};
    const Registration registration{MemberId::fromHex("00112233445566778899aabbccddeeff"), "Bob", "Phone", publicKey};
    QCOMPARE(HttpWire::registerPersonRequest(registration),
             (QJsonObject{{"member", "00112233445566778899aabbccddeeff"},
                          {"displayName", "Bob"},
                          {"deviceName", "Phone"},
                          {"publicKey", base64(publicKey.bytes)}}));
    QCOMPARE(HttpWire::startLinkRequest(publicKey, "Phone"),
             (QJsonObject{{"publicKey", base64(publicKey.bytes)}, {"deviceName", "Phone"}}));
    QCOMPARE(HttpWire::grantRequest(4), (QJsonObject{{"request", 4}}));
    QCOMPARE(HttpWire::setRoleRequest(Role::Admin), (QJsonObject{{"role", "admin"}}));
    QCOMPARE(HttpWire::setRoleRequest(Role::Member), (QJsonObject{{"role", "member"}}));
    QCOMPARE(HttpWire::advanceClockRequest(86400), (QJsonObject{{"seconds", 86400}}));
}

void HttpServerApiTest::answersAreReadFromTheWireFormat()
{
    QCOMPARE(HttpWire::readChallenge(jsonOf(R"({"challenge": "b25lLXRpbWU="})")).value(), QByteArray("one-time"));
    QCOMPARE(HttpWire::readSession(jsonOf(R"({"token": "abc.def"})"))->token, QString("abc.def"));
    QCOMPARE(HttpWire::readVersion(jsonOf(R"({"version": 12})")).value(), qint64(12));
    QCOMPARE(HttpWire::readLinkCode(jsonOf(R"({"code": "K7Q2M9"})")).value(), QString("K7Q2M9"));

    const Upload upload = firstUpload();
    const QJsonObject alice = personJson("00112233445566778899aabbccddeeff", "Alice", {aliceLaptop()});

    // A head: who published, the header, the members and roles, the history index.
    const std::optional<Head> head = HttpWire::readHead(QJsonObject{
        {"version", 4},
        {"published", QJsonObject{{"name", "Alice"}, {"device", aliceLaptopFingerprint().toHex()}, {"when", When}}},
        {"leaseHolder", aliceLaptopFingerprint().toHex()},
        {"header", upload.header.toJson()},
        {"members", QJsonArray{QJsonObject{{"person", alice}, {"role", "admin"}}}},
        {"history", 2}});
    QVERIFY(head.has_value());
    QCOMPARE(head->version, qint64(4));
    QCOMPARE(head->published.name, QString("Alice"));
    QCOMPARE(head->published.when, QDateTime::fromString(When, Qt::ISODateWithMs));
    QCOMPARE(head->leaseHolder, std::optional<Fingerprint>(aliceLaptopFingerprint()));
    QCOMPARE(head->header, upload.header.toJson());
    QCOMPARE(head->history, qint64(2));
    QCOMPARE(head->members.size(), 1);
    QCOMPARE(head->members.first().role, Role::Admin);
    QCOMPARE(head->members.first().person.displayName, QString("Alice"));
    QCOMPARE(head->members.first().person.devices.first().name, QString("Laptop"));
    QVERIFY(!head->members.first().person.devices.first().linkedBy.has_value());

    const std::optional<Snapshot> snapshot = HttpWire::readSnapshot(QJsonObject{
        {"version", 4},
        {"published", QJsonObject{{"name", "Alice"}, {"device", aliceLaptopFingerprint().toHex()}, {"when", When}}},
        {"header", upload.header.toJson()},
        {"blob", base64(upload.blob)}});
    QVERIFY(snapshot.has_value());
    QCOMPARE(snapshot->header, upload.header.toJson());
    QCOMPARE(snapshot->blob, upload.blob);

    const QString first = DocumentLibrary::newDocumentId();
    const std::optional<Me> me = HttpWire::readMe(QJsonObject{{"person", alice}, {"documents", QJsonArray{first}}});
    QVERIFY(me.has_value());
    QCOMPARE(me->documents, QStringList{first});
    QCOMPARE(me->person.member, MemberId::fromHex("00112233445566778899aabbccddeeff"));

    // A link before and after approval.
    const QJsonObject phone = aliceLaptop();
    QJsonObject pending{{"deviceName", "Phone"},
                        {"publicKey", phone["publicKey"]},
                        {"verifyKey", phone["verifyKey"]},
                        {"expires", When}};
    QVERIFY(!HttpWire::readLinkState(pending)->approvedAs.has_value());
    pending["approvedAs"] = alice;
    QCOMPARE(HttpWire::readLinkState(pending)->approvedAs->displayName, QString("Alice"));

    const std::optional<QList<HistoryEntry>> history = HttpWire::readHistory(QJsonObject{{"entries", QJsonArray{
        QJsonObject{{"index", 1}, {"kind", "added"}, {"actor", "Alice"}, {"subject", "Bob"}, {"when", When}},
        QJsonObject{{"index", 2}, {"kind", "madeMember"}, {"actor", "Bob"}, {"subject", "Bob"}, {"when", When}}}}});
    QVERIFY(history.has_value());
    QCOMPARE(history->size(), 2);
    QCOMPARE(history->at(0).kind, HistoryEntry::Kind::Added);
    QCOMPARE(history->at(1).kind, HistoryEntry::Kind::MadeMember);
    QCOMPARE(history->at(1).index, qint64(2));

    const std::optional<QList<AccessRequest>> requests = HttpWire::readAccessRequests(QJsonObject{
        {"requests", QJsonArray{QJsonObject{{"index", 3}, {"person", alice}, {"when", When}}}}});
    QVERIFY(requests.has_value());
    QCOMPARE(requests->first().index, qint64(3));
    QCOMPARE(requests->first().person.displayName, QString("Alice"));
    QVERIFY(HttpWire::readAccessRequests(jsonOf(R"({"requests": []})"))->isEmpty());

    QCOMPARE(HttpWire::readRequestStatus(jsonOf(R"({"status": "pending"})")).value(), RequestStatus::Pending);
    QCOMPARE(HttpWire::readRequestStatus(jsonOf(R"({"status": "granted"})")).value(), RequestStatus::Granted);
    QCOMPARE(HttpWire::readRequestStatus(jsonOf(R"({"status": "declined"})")).value(), RequestStatus::Declined);

    const std::optional<Lease> lease = HttpWire::readLease(
        QJsonObject{{"holder", aliceLaptopFingerprint().toHex()}, {"expires", When}});
    QCOMPARE(lease->holder, aliceLaptopFingerprint());
}

void HttpServerApiTest::illFormedAnswersAreRefused_data()
{
    QTest::addColumn<QString>("reader");
    QTest::addColumn<QJsonObject>("answer");

    const QJsonObject alice = personJson("00112233445566778899aabbccddeeff", "Alice", {aliceLaptop()});
    const QJsonObject published{{"name", "Alice"}, {"device", aliceLaptopFingerprint().toHex()}, {"when", When}};
    const QJsonObject header = firstUpload().header.toJson();
    const QJsonObject head{{"version", 4},
                           {"published", published},
                           {"header", header},
                           {"members", QJsonArray{QJsonObject{{"person", alice}, {"role", "admin"}}}},
                           {"history", 0}};
    QTest::newRow("a good head") << "good head" << head;

    QJsonObject noVersion = head;
    noVersion.remove("version");
    QTest::newRow("head with no version") << "head" << noVersion;

    QJsonObject badRole = head;
    badRole["members"] = QJsonArray{QJsonObject{{"person", alice}, {"role", "owner"}}};
    QTest::newRow("head with an unknown role") << "head" << badRole;

    // A device whose fingerprint is not its keys' names one device and seals to another.
    QJsonObject wrongDevice = aliceLaptop();
    wrongDevice["fingerprint"] = QString(32, QLatin1Char('0'));
    QJsonObject wrongFingerprint = head;
    wrongFingerprint["members"] = QJsonArray{QJsonObject{
        {"person", personJson("00112233445566778899aabbccddeeff", "Alice", {wrongDevice})}, {"role", "admin"}}};
    QTest::newRow("head with a device not its keys") << "head" << wrongFingerprint;

    QJsonObject noDevices = head;
    noDevices["members"] = QJsonArray{
        QJsonObject{{"person", personJson("00112233445566778899aabbccddeeff", "Alice", {})}, {"role", "admin"}}};
    QTest::newRow("head with a person with no devices") << "head" << noDevices;

    QJsonObject negativeHistory = head;
    negativeHistory["history"] = -1;
    QTest::newRow("head with negative history") << "head" << negativeHistory;

    QJsonObject noPublished = head;
    noPublished.remove("published");
    QTest::newRow("head with nobody published") << "head" << noPublished;

    QTest::newRow("snapshot with no blob")
        << "snapshot" << QJsonObject{{"version", 3}, {"published", published}, {"header", header}};

    QTest::newRow("me with a document that is not an id")
        << "me" << QJsonObject{{"person", alice}, {"documents", QJsonArray{"../../etc"}}};

    QTest::newRow("history with an unknown kind")
        << "history"
        << QJsonObject{{"entries", QJsonArray{QJsonObject{
               {"index", 1}, {"kind", "promoted"}, {"actor", "A"}, {"subject", "B"}, {"when", When}}}}};

    QTest::newRow("a request listing two devices")
        << "requests"
        << QJsonObject{{"requests", QJsonArray{QJsonObject{
               {"index", 1},
               {"person", personJson("00112233445566778899aabbccddeeff", "Alice", {aliceLaptop(), otherDevice()})},
               {"when", When}}}}};

    QTest::newRow("a status this build does not know") << "status" << QJsonObject{{"status", "withdrawn"}};
    QTest::newRow("an empty link code") << "code" << QJsonObject{{"code", ""}};
    QTest::newRow("a link with no expiry")
        << "link"
        << QJsonObject{{"deviceName", "Phone"},
                       {"publicKey", aliceLaptop()["publicKey"]},
                       {"verifyKey", aliceLaptop()["verifyKey"]}};
}

void HttpServerApiTest::illFormedAnswersAreRefused()
{
    QFETCH(QString, reader);
    QFETCH(QJsonObject, answer);

    if (reader == "good head")
    {
        // Anchor: the shape every refused row departs from reads.
        QVERIFY(HttpWire::readHead(answer).has_value());
    }
    else if (reader == "head")
    {
        QVERIFY(!HttpWire::readHead(answer).has_value());
    }
    else if (reader == "snapshot")
    {
        QVERIFY(!HttpWire::readSnapshot(answer).has_value());
    }
    else if (reader == "me")
    {
        QVERIFY(!HttpWire::readMe(answer).has_value());
    }
    else if (reader == "history")
    {
        QVERIFY(!HttpWire::readHistory(answer).has_value());
    }
    else if (reader == "requests")
    {
        QVERIFY(!HttpWire::readAccessRequests(answer).has_value());
    }
    else if (reader == "status")
    {
        QVERIFY(!HttpWire::readRequestStatus(answer).has_value());
    }
    else if (reader == "code")
    {
        QVERIFY(!HttpWire::readLinkCode(answer).has_value());
    }
    else if (reader == "link")
    {
        QVERIFY(!HttpWire::readLinkState(answer).has_value());
    }
    else
    {
        QFAIL("no such reader");
    }
}

void HttpServerApiTest::errorsAreReadByNameThenStatus_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<ServerError>("error");

    // Each name the server sends.
    const QList<std::pair<const char*, ServerError>> names = {
        {"unauthenticated", ServerError::Unauthenticated},
        {"notMember", ServerError::NotMember},
        {"notAdmin", ServerError::NotAdmin},
        {"lastAdmin", ServerError::LastAdmin},
        {"notOnAllowList", ServerError::NotOnAllowList},
        {"unknownDocument", ServerError::UnknownDocument},
        {"versionConflict", ServerError::VersionConflict},
        {"leaseHeld", ServerError::LeaseHeld},
        {"idTaken", ServerError::IdTaken},
        {"rateLimited", ServerError::RateLimited},
        {"notFound", ServerError::NotFound},
        {"expired", ServerError::Expired},
        {"invalidRequest", ServerError::InvalidRequest},
    };
    for (const auto& name : names)
    {
        QTest::newRow(name.first) << 400 << (QByteArray(R"({"error": ")") + name.first + R"("})") << name.second;
    }

    // A name this build does not know is not acted on as another.
    QTest::newRow("unknown name") << 409 << QByteArray(R"({"error": "somethingNew"})") << ServerError::InvalidRequest;

    // An accessLost answer deletes a ward, so one that does not read in full is not one.
    QTest::newRow("access lost without how")
        << 403 << QByteArray(R"({"error": "accessLost", "when": "2026-10-11T09:00:00.000Z"})")
        << ServerError::InvalidRequest;

    // With no name, only what a front end may send for the server means anything.
    QTest::newRow("bare 401") << 401 << QByteArray() << ServerError::Unauthenticated;
    QTest::newRow("bare 429") << 429 << QByteArray() << ServerError::RateLimited;
    QTest::newRow("bare 403") << 403 << QByteArray() << ServerError::Unreachable;
    QTest::newRow("a page, not json") << 404 << QByteArray("<html>Not Found</html>") << ServerError::Unreachable;
    QTest::newRow("an error that is not text") << 409 << QByteArray(R"({"error": 7})") << ServerError::Unreachable;

    // A redirect not followed, or a server failing, whatever it names.
    QTest::newRow("302") << 302 << QByteArray() << ServerError::Unreachable;
    QTest::newRow("500") << 500 << QByteArray(R"({"error": "leaseHeld"})") << ServerError::Unreachable;
    QTest::newRow("503") << 503 << QByteArray() << ServerError::Unreachable;
}

void HttpServerApiTest::errorsAreReadByNameThenStatus()
{
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(ServerError, error);
    const HttpWire::Failure failure = HttpWire::readFailure(status, body);
    QCOMPARE(failure.error, error);
    QVERIFY(!failure.lost.has_value());
}

void HttpServerApiTest::lostAccessIsReadInFull()
{
    const HttpWire::Failure removed = HttpWire::readFailure(
        403, R"({"error": "accessLost", "how": "removed", "byWhom": "Alice", "when": "2026-10-11T09:00:00.000Z"})");
    QCOMPARE(removed.error, ServerError::AccessLost);
    QVERIFY(removed.lost.has_value());
    QCOMPARE(removed.lost->how, LostAccess::How::Removed);
    QCOMPARE(removed.lost->byWhom, QString("Alice"));
    QCOMPARE(removed.lost->when, QDateTime::fromString("2026-10-11T09:00:00.000Z", Qt::ISODateWithMs));

    const HttpWire::Failure left =
        HttpWire::readFailure(403, R"({"error": "accessLost", "how": "left", "when": "2026-10-11T09:00:00.000Z"})");
    QCOMPARE(left.lost->how, LostAccess::How::Left);
    QVERIFY(left.lost->byWhom.isEmpty());

    const HttpWire::Failure unlinked = HttpWire::readFailure(
        403, R"({"error": "accessLost", "how": "unlinked", "byWhom": "Laptop", "when": "2026-10-11T09:00:00.000Z"})");
    QCOMPARE(unlinked.lost->how, LostAccess::How::Unlinked);

    const HttpWire::Failure unknownHow = HttpWire::readFailure(
        403, R"({"error": "accessLost", "how": "banished", "when": "2026-10-11T09:00:00.000Z"})");
    QCOMPARE(unknownHow.error, ServerError::InvalidRequest);
    QVERIFY(!unknownHow.lost.has_value());
}

void HttpServerApiTest::aCallTravelsWithItsTokenAndBody()
{
    CannedServer canned(httpResponse(200, R"({"version": 8})"));
    QVERIFY(canned.isListening());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address() + "/");
    HttpServerApi api(*settings, nullptr);

    const Upload upload = firstUpload();
    std::optional<Result<qint64>> result;
    api.upload(Session{"session-token"}, upload.header.documentId, 7, upload, recordTo(result));
    QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), AnswerMilliseconds);

    QVERIFY(result->ok());
    QCOMPARE(result->value.value(), qint64(8));
    const Captured& request = canned.captured().value();
    QCOMPARE(request.method, QByteArray("PUT"));
    QCOMPARE(request.path, "/documents/" + upload.header.documentId.toLatin1());
    QCOMPARE(request.headers.value("authorization"), QByteArray("Bearer session-token"));
    QVERIFY(!request.headers.contains("x-request-signature"));
    QCOMPARE(request.headers.value("content-type"), QByteArray("application/json"));
    QCOMPARE(QJsonDocument::fromJson(request.body).object(), HttpWire::uploadRequest(7, upload));
}

void HttpServerApiTest::anAccessLostAnswerCarriesHowThroughTheReply()
{
    CannedServer canned(httpResponse(
        403, R"({"error": "accessLost", "how": "removed", "byWhom": "Alice", "when": "2026-10-11T09:00:00.000Z"})"));
    QVERIFY(canned.isListening());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address());
    HttpServerApi api(*settings, nullptr);

    std::optional<Result<Head>> result;
    api.head(Session{"token"}, DocumentLibrary::newDocumentId(), recordTo(result));
    QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), AnswerMilliseconds);
    QCOMPARE(result->error, ServerError::AccessLost);
    QVERIFY(result->lost.has_value());
    QCOMPARE(result->lost->byWhom, QString("Alice"));
}

void HttpServerApiTest::everyCallTakesItsRoute_data()
{
    QTest::addColumn<QString>("call");
    QTest::addColumn<QByteArray>("method");
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("withToken");

    // %1 is the document id, %2 the link code, %3 a device fingerprint, %4 a member id.
    QTest::newRow("challenge") << "challenge" << QByteArray("POST") << "/auth/challenge" << false;
    QTest::newRow("authenticate") << "authenticate" << QByteArray("POST") << "/auth/token" << false;
    QTest::newRow("registerPerson") << "registerPerson" << QByteArray("PUT") << "/me" << true;
    QTest::newRow("me") << "me" << QByteArray("GET") << "/me" << true;
    QTest::newRow("unlinkDevice") << "unlinkDevice" << QByteArray("DELETE") << "/me/devices/%3" << true;
    QTest::newRow("startLink") << "startLink" << QByteArray("POST") << "/links" << true;
    QTest::newRow("link") << "link" << QByteArray("GET") << "/links/%2" << true;
    QTest::newRow("approveLink") << "approveLink" << QByteArray("POST") << "/links/%2/approve" << true;
    QTest::newRow("registerDocument") << "registerDocument" << QByteArray("POST") << "/documents" << true;
    QTest::newRow("head") << "head" << QByteArray("GET") << "/documents/%1/head" << true;
    QTest::newRow("download") << "download" << QByteArray("GET") << "/documents/%1" << true;
    QTest::newRow("upload") << "upload" << QByteArray("PUT") << "/documents/%1" << true;
    QTest::newRow("takeLease") << "takeLease" << QByteArray("POST") << "/documents/%1/lease" << true;
    QTest::newRow("releaseLease") << "releaseLease" << QByteArray("DELETE") << "/documents/%1/lease" << true;
    QTest::newRow("history") << "history" << QByteArray("GET") << "/documents/%1/history?after=3" << true;
    QTest::newRow("requestAccess") << "requestAccess" << QByteArray("POST") << "/documents/%1/requests" << true;
    QTest::newRow("accessRequests") << "accessRequests" << QByteArray("GET") << "/documents/%1/requests" << true;
    QTest::newRow("declineRequest") << "declineRequest" << QByteArray("DELETE") << "/documents/%1/requests/5" << true;
    QTest::newRow("myRequestStatus") << "myRequestStatus" << QByteArray("GET") << "/documents/%1/requests/mine" << true;
    QTest::newRow("grant") << "grant" << QByteArray("POST") << "/documents/%1/members" << true;
    QTest::newRow("removeMember") << "removeMember" << QByteArray("DELETE") << "/documents/%1/members/%4" << true;
    QTest::newRow("setRole") << "setRole" << QByteArray("PUT") << "/documents/%1/members/%4/role" << true;
    QTest::newRow("advanceClock") << "advanceClock" << QByteArray("POST") << "/dev/clock" << false;
}

void HttpServerApiTest::everyCallTakesItsRoute()
{
    QFETCH(QString, call);
    QFETCH(QByteArray, method);
    QFETCH(QString, path);
    QFETCH(bool, withToken);

    // Every call is refused, so each reply comes with no answer to read.
    CannedServer canned(httpResponse(409, R"({"error": "versionConflict"})"));
    QVERIFY(canned.isListening());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address());
    HttpServerApi api(*settings, nullptr);

    const QString documentId = DocumentLibrary::newDocumentId();
    const QString code = "K7Q2M9";
    const Session session{"session-token"};
    const QByteArray signature(64, '\x07');
    const Upload upload = firstUpload();
    const PublicKey publicKey{QByteArray(32, '\x03')};
    const VerifyKey verifyKey{QByteArray(32, '\x04')};
    const Fingerprint device = DocumentCrypto::fingerprintOf(publicKey, verifyKey);
    const MemberId member = MemberId::fromHex("00112233445566778899aabbccddeeff");
    const Registration registration{member, "Bob", "Phone", publicKey};

    // The body each call sends; none for a call that sends none. Each reply records
    // that it came.
    std::optional<QJsonObject> body;
    bool answered = false;
    const std::function<void()> note = std::bind(&markAnswered, std::ref(answered));

    if (call == "challenge")
    {
        api.challenge(std::bind(note));
    }
    else if (call == "authenticate")
    {
        body = HttpWire::authenticateRequest(verifyKey, "challenge", signature);
        api.authenticate(verifyKey, "challenge", signature, std::bind(note));
    }
    else if (call == "registerPerson")
    {
        body = HttpWire::registerPersonRequest(registration);
        api.registerPerson(session, registration, std::bind(note));
    }
    else if (call == "me")
    {
        api.me(session, std::bind(note));
    }
    else if (call == "unlinkDevice")
    {
        api.unlinkDevice(session, device, std::bind(note));
    }
    else if (call == "startLink")
    {
        body = HttpWire::startLinkRequest(publicKey, "Phone");
        api.startLink(session, publicKey, "Phone", std::bind(note));
    }
    else if (call == "link")
    {
        api.link(session, code, std::bind(note));
    }
    else if (call == "approveLink")
    {
        api.approveLink(session, code, std::bind(note));
    }
    else if (call == "registerDocument")
    {
        body = HttpWire::registerRequest(upload);
        api.registerDocument(session, upload, std::bind(note));
    }
    else if (call == "head")
    {
        api.head(session, documentId, std::bind(note));
    }
    else if (call == "download")
    {
        api.download(session, documentId, std::bind(note));
    }
    else if (call == "upload")
    {
        body = HttpWire::uploadRequest(5, upload);
        api.upload(session, documentId, 5, upload, std::bind(note));
    }
    else if (call == "takeLease")
    {
        api.takeLease(session, documentId, std::bind(note));
    }
    else if (call == "releaseLease")
    {
        api.releaseLease(session, documentId, std::bind(note));
    }
    else if (call == "history")
    {
        api.history(session, documentId, 3, std::bind(note));
    }
    else if (call == "requestAccess")
    {
        api.requestAccess(session, documentId, std::bind(note));
    }
    else if (call == "accessRequests")
    {
        api.accessRequests(session, documentId, std::bind(note));
    }
    else if (call == "declineRequest")
    {
        api.declineRequest(session, documentId, 5, std::bind(note));
    }
    else if (call == "myRequestStatus")
    {
        api.myRequestStatus(session, documentId, std::bind(note));
    }
    else if (call == "grant")
    {
        body = HttpWire::grantRequest(5);
        api.grant(session, documentId, 5, std::bind(note));
    }
    else if (call == "removeMember")
    {
        api.removeMember(session, documentId, member, std::bind(note));
    }
    else if (call == "setRole")
    {
        body = HttpWire::setRoleRequest(Role::Admin);
        api.setRole(session, documentId, member, Role::Admin, std::bind(note));
    }
    else if (call == "advanceClock")
    {
        body = HttpWire::advanceClockRequest(60);
        api.advanceClock(60, std::bind(note));
    }
    else
    {
        QFAIL("no such call");
    }

    // The reply comes, once the server has answered.
    QTRY_VERIFY_WITH_TIMEOUT(canned.captured().has_value(), AnswerMilliseconds);
    QTRY_VERIFY_WITH_TIMEOUT(answered, AnswerMilliseconds);

    const Captured& sent = canned.captured().value();
    QCOMPARE(sent.method, method);
    QString expected = path;
    expected.replace("%1", documentId).replace("%2", code).replace("%3", device.toHex()).replace("%4", member.toHex());
    QCOMPARE(QString::fromLatin1(sent.path), expected);
    QCOMPARE(sent.headers.value("authorization"), withToken ? QByteArray("Bearer session-token") : QByteArray());
    QVERIFY(!sent.headers.contains("x-request-signature"));
    if (body.has_value())
    {
        QCOMPARE(QJsonDocument::fromJson(sent.body).object(), body.value());
    }
    else
    {
        QVERIFY(sent.body.isEmpty());
    }

    // A POST or PUT says its length, an empty body included.
    if (method == "POST" || method == "PUT")
    {
        QCOMPARE(sent.headers.value("content-length"), QByteArray::number(sent.body.size()));
    }
}

void HttpServerApiTest::aFailedAnswerIsItsNamedError()
{
    CannedServer canned(httpResponse(409, R"({"error": "leaseHeld"})"));
    QVERIFY(canned.isListening());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address());
    HttpServerApi api(*settings, nullptr);

    std::optional<Result<Lease>> result;
    api.takeLease(Session{"token"}, DocumentLibrary::newDocumentId(), recordTo(result));
    QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), AnswerMilliseconds);
    QVERIFY(!result->ok());
    QCOMPARE(result->error, ServerError::LeaseHeld);
}

void HttpServerApiTest::anUnreadableOrFailingServerIsUnreachable()
{
    const QList<std::pair<QByteArray, bool>> responses = {
        {httpResponse(200, "not json"), true},
        {httpResponse(200, R"({"version": "eight"})"), true},
        {httpResponse(503, R"({"error": "versionConflict"})"), false},
        {httpResponse(404, "<html>Not Found</html>"), false},
    };
    for (const auto& [response, unreadable] : responses)
    {
        CannedServer canned(response);
        QVERIFY(canned.isListening());
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address());
        HttpServerApi api(*settings, nullptr);

        if (unreadable)
        {
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unreadable server answer to \"POST /documents\""));
        }
        std::optional<Result<qint64>> result;
        api.registerDocument(Session{"token"}, firstUpload(), recordTo(result));

        // The reply comes: Unreachable here is the answer, not a wait that gave up.
        QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), AnswerMilliseconds);
        QCOMPARE(result->error, ServerError::Unreachable);
    }

    // Anchor: a success with nothing to read is Done.
    CannedServer canned(httpResponse(204, QByteArray()));
    QVERIFY(canned.isListening());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address());
    HttpServerApi api(*settings, nullptr);
    std::optional<Result<Done>> released;
    api.releaseLease(Session{"token"}, DocumentLibrary::newDocumentId(), recordTo(released));
    QTRY_VERIFY_WITH_TIMEOUT(released.has_value(), AnswerMilliseconds);
    QVERIFY(released->ok());
}

void HttpServerApiTest::noAddressIsUnreachableAfterTheCallReturns()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, QString());
    QVERIFY(settings->serverAddress().isEmpty());
    HttpServerApi api(*settings, nullptr);

    std::optional<Result<QByteArray>> answer;
    api.challenge(recordTo(answer));
    QVERIFY(!answer.has_value());

    QTRY_VERIFY(answer.has_value());
    QCOMPARE(answer->error, ServerError::Unreachable);
}

void HttpServerApiTest::aRefusedConnectionIsUnreachable()
{
    // A port that was just free.
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost));
    const QString address = QString("http://127.0.0.1:%1").arg(probe.serverPort());
    probe.close();

    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, address);
    HttpServerApi api(*settings, nullptr);

    std::optional<Result<Head>> result;
    api.head(Session{"token"}, DocumentLibrary::newDocumentId(), recordTo(result));
    QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), AnswerMilliseconds);
    QCOMPARE(result->error, ServerError::Unreachable);
}

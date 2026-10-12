#include "HttpServerApiTest.h"

#include "AppSettings.h"
#include "DocumentCrypto.h"
#include "DocumentFile.h"
#include "DocumentLibrary.h"
#include "HttpServerApi.h"
#include "HttpWire.h"
#include "ServerTestSupport.h"
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
    document.setSealingSet(SealingSet::of(alice.member, alice.displayName, alice.device()));

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

/// A signer whose signature is fixed bytes, for checking how it travels.
Signer fixedSigner()
{
    return Signer{DeviceKeys{PublicKey{QByteArray(32, '\x03')}, VerifyKey{QByteArray(32, '\x04')}}, QByteArray(64, '\x07')};
}

QJsonObject publishedJson()
{
    return QJsonObject{{"device", aliceLaptopFingerprint().toHex()}, {"when", When}};
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
    // The header travels as the text its signature covers, not as an object.
    const Upload upload = firstUpload();
    const QString text = QString::fromUtf8(headerText(upload.header));
    QVERIFY(text.startsWith("{"));
    QVERIFY(!text.contains(' '));
    QVERIFY(!text.contains('\n'));
    QCOMPARE(HttpWire::registerRequest(upload), (QJsonObject{{"header", text}, {"blob", base64(upload.blob)}}));
    QCOMPARE(HttpWire::uploadRequest(7, upload),
             (QJsonObject{{"baseVersion", 7}, {"header", text}, {"blob", base64(upload.blob)}}));
    QCOMPARE(HttpWire::advanceClockRequest(86400), (QJsonObject{{"seconds", 86400}}));

    // The signer's keys and signature, each in base64.
    const QList<QPair<QByteArray, QByteArray>> headers = HttpWire::signerHeaders(fixedSigner());
    QCOMPARE(headers.size(), 3);
    QCOMPARE(headers.at(0), (QPair<QByteArray, QByteArray>("X-Signer-Public", QByteArray(32, '\x03').toBase64())));
    QCOMPARE(headers.at(1), (QPair<QByteArray, QByteArray>("X-Signer-Verify", QByteArray(32, '\x04').toBase64())));
    QCOMPARE(headers.at(2), (QPair<QByteArray, QByteArray>("X-Signature", QByteArray(64, '\x07').toBase64())));
}

void HttpServerApiTest::answersAreReadFromTheWireFormat()
{
    QCOMPARE(HttpWire::readVersion(jsonOf(R"({"version": 12})")).value(), qint64(12));

    const Upload upload = firstUpload();
    const std::optional<Head> head = HttpWire::readHead(QJsonObject{{"version", 4},
                                                                    {"published", publishedJson()},
                                                                    {"leaseHolder", aliceLaptopFingerprint().toHex()},
                                                                    {"header", upload.header.toJson()}});
    QVERIFY(head.has_value());
    QCOMPARE(head->version, qint64(4));
    QCOMPARE(head->published.device, aliceLaptopFingerprint());
    QCOMPARE(head->published.when, QDateTime::fromString(When, Qt::ISODateWithMs));
    QCOMPARE(head->leaseHolder, std::optional<Fingerprint>(aliceLaptopFingerprint()));
    QCOMPARE(head->header, upload.header.toJson());

    // With nobody holding the lease, there is no holder.
    const std::optional<Head> free = HttpWire::readHead(
        QJsonObject{{"version", 4}, {"published", publishedJson()}, {"header", upload.header.toJson()}});
    QVERIFY(free.has_value());
    QVERIFY(!free->leaseHolder.has_value());

    const std::optional<Snapshot> snapshot = HttpWire::readSnapshot(QJsonObject{
        {"version", 4}, {"published", publishedJson()}, {"header", upload.header.toJson()}, {"blob", base64(upload.blob)}});
    QVERIFY(snapshot.has_value());
    QCOMPARE(snapshot->version, qint64(4));
    QCOMPARE(snapshot->published.device, aliceLaptopFingerprint());
    QCOMPARE(snapshot->header, upload.header.toJson());
    QCOMPARE(snapshot->blob, upload.blob);

    const std::optional<Lease> lease =
        HttpWire::readLease(QJsonObject{{"holder", aliceLaptopFingerprint().toHex()}, {"expires", When}});
    QVERIFY(lease.has_value());
    QCOMPARE(lease->holder, aliceLaptopFingerprint());
    QCOMPARE(lease->expires, QDateTime::fromString(When, Qt::ISODateWithMs));
}

void HttpServerApiTest::illFormedAnswersAreRefused_data()
{
    QTest::addColumn<QString>("reader");
    QTest::addColumn<QJsonObject>("answer");

    const QJsonObject header = firstUpload().header.toJson();
    const QJsonObject head{{"version", 4}, {"published", publishedJson()}, {"header", header}};
    QTest::newRow("a good head") << "good head" << head;

    QJsonObject noVersion = head;
    noVersion.remove("version");
    QTest::newRow("head with no version") << "head" << noVersion;

    QJsonObject versionZero = head;
    versionZero["version"] = 0;
    QTest::newRow("head at version zero") << "head" << versionZero;

    QJsonObject noPublished = head;
    noPublished.remove("published");
    QTest::newRow("head with nobody published") << "head" << noPublished;

    QJsonObject namedPublisher = head;
    QJsonObject withName = publishedJson();
    withName["name"] = "Alice";
    namedPublisher["published"] = withName;
    QTest::newRow("head naming its publisher, which the server cannot know") << "head" << namedPublisher;

    QJsonObject shortHolder = head;
    shortHolder["leaseHolder"] = QString("abcd");
    QTest::newRow("head with a lease holder that is no fingerprint") << "head" << shortHolder;

    QJsonObject headerAsText = head;
    headerAsText["header"] = QString::fromUtf8(QJsonDocument(header).toJson(QJsonDocument::Compact));
    QTest::newRow("head with its header as text") << "head" << headerAsText;

    QJsonObject withMembers = head;
    withMembers["members"] = QJsonArray();
    QTest::newRow("head with members, which no server sends") << "head" << withMembers;

    QTest::newRow("snapshot with no blob")
        << "snapshot" << QJsonObject{{"version", 3}, {"published", publishedJson()}, {"header", header}};
    QTest::newRow("snapshot with a blob not in base64")
        << "snapshot"
        << QJsonObject{{"version", 3}, {"published", publishedJson()}, {"header", header}, {"blob", "not base64!"}};
    QTest::newRow("version as text") << "version" << QJsonObject{{"version", "8"}};
    QTest::newRow("lease with no expiry") << "lease" << QJsonObject{{"holder", aliceLaptopFingerprint().toHex()}};
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
    else if (reader == "version")
    {
        QVERIFY(!HttpWire::readVersion(answer).has_value());
    }
    else if (reader == "lease")
    {
        QVERIFY(!HttpWire::readLease(answer).has_value());
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
        {"notAKeyHolder", ServerError::NotAKeyHolder},
        {"unknownDocument", ServerError::UnknownDocument},
        {"versionConflict", ServerError::VersionConflict},
        {"leaseHeld", ServerError::LeaseHeld},
        {"idTaken", ServerError::IdTaken},
        {"rateLimited", ServerError::RateLimited},
        {"invalidRequest", ServerError::InvalidRequest},
    };
    for (const auto& name : names)
    {
        QTest::newRow(name.first) << 400 << (QByteArray(R"({"error": ")") + name.first + R"("})") << name.second;
    }

    // A name this build does not know is not acted on as another, a retired one included.
    QTest::newRow("unknown name") << 409 << QByteArray(R"({"error": "somethingNew"})") << ServerError::InvalidRequest;
    QTest::newRow("a retired name") << 403 << QByteArray(R"({"error": "accessLost"})") << ServerError::InvalidRequest;

    // With no name, only what a front end may send for the server means anything.
    QTest::newRow("bare 429") << 429 << QByteArray() << ServerError::RateLimited;
    QTest::newRow("bare 401") << 401 << QByteArray() << ServerError::Unreachable;
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
    QCOMPARE(HttpWire::readFailure(status, body), error);
}

void HttpServerApiTest::aSignedCallCarriesItsSignerAndTheHeaderItSigned()
{
    CannedServer canned(httpResponse(200, R"({"version": 8})"));
    QVERIFY(canned.isListening());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address() + "/");
    HttpServerApi api(*settings, nullptr);

    const ServerTestSupport::TestDevice alice = ServerTestSupport::newDevice("Alice");
    const Upload upload = firstUpload();
    const QString documentId = upload.header.documentId;
    const Signer signer = alice.signer(signedBytes(CallUpload, documentId, 7, upload));
    std::optional<Result<qint64>> result;
    api.upload(signer, documentId, 7, upload, recordTo(result));
    QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), AnswerMilliseconds);

    QVERIFY(result->ok());
    QCOMPARE(result->value.value(), qint64(8));
    const Captured& request = canned.captured().value();
    QCOMPARE(request.method, QByteArray("PUT"));
    QCOMPARE(request.path, "/documents/" + documentId.toLatin1());
    QVERIFY(!request.headers.contains("authorization"));
    QCOMPARE(request.headers.value("content-type"), QByteArray("application/json"));
    QCOMPARE(QJsonDocument::fromJson(request.body).object(), HttpWire::uploadRequest(7, upload));

    // What a server does with it: the keys from the headers, the header text exactly
    // as received, and the signature checks out over what the call changes.
    const VerifyKey verifyKey{QByteArray::fromBase64(request.headers.value("x-signer-verify"))};
    const PublicKey publicKey{QByteArray::fromBase64(request.headers.value("x-signer-public"))};
    QVERIFY(verifyKey == alice.signing.verifyKey);
    QVERIFY(publicKey == alice.sealing.publicKey);
    const QByteArray received = QJsonDocument::fromJson(request.body).object()["header"].toString().toUtf8();
    QCOMPARE(received, headerText(upload.header));
    const QByteArray signature = QByteArray::fromBase64(request.headers.value("x-signature"));
    QVERIFY(DocumentCrypto::verifies(signedBytes(CallUpload, documentId, 7, upload), signature, verifyKey));
    QVERIFY(!DocumentCrypto::verifies(signedBytes(CallUpload, documentId, 6, upload), signature, verifyKey));
}

void HttpServerApiTest::everyCallTakesItsRoute_data()
{
    QTest::addColumn<QString>("call");
    QTest::addColumn<QByteArray>("method");
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("isSigned");

    // %1 is the document id.
    QTest::newRow("registerDocument") << "registerDocument" << QByteArray("POST") << "/documents" << true;
    QTest::newRow("head") << "head" << QByteArray("GET") << "/documents/%1/head" << false;
    QTest::newRow("download") << "download" << QByteArray("GET") << "/documents/%1" << false;
    QTest::newRow("upload") << "upload" << QByteArray("PUT") << "/documents/%1" << true;
    QTest::newRow("takeLease") << "takeLease" << QByteArray("POST") << "/documents/%1/lease" << true;
    QTest::newRow("releaseLease") << "releaseLease" << QByteArray("DELETE") << "/documents/%1/lease" << true;
    QTest::newRow("advanceClock") << "advanceClock" << QByteArray("POST") << "/dev/clock" << false;
}

void HttpServerApiTest::everyCallTakesItsRoute()
{
    QFETCH(QString, call);
    QFETCH(QByteArray, method);
    QFETCH(QString, path);
    QFETCH(bool, isSigned);

    // Every call is refused, so each reply comes with no answer to read.
    CannedServer canned(httpResponse(409, R"({"error": "versionConflict"})"));
    QVERIFY(canned.isListening());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address());
    HttpServerApi api(*settings, nullptr);

    const QString documentId = DocumentLibrary::newDocumentId();
    const Upload upload = firstUpload();
    const Signer signer = fixedSigner();

    // The body each call sends; none for a call that sends none. Each reply records
    // that it came.
    std::optional<QJsonObject> body;
    bool answered = false;
    const std::function<void()> note = std::bind(&markAnswered, std::ref(answered));

    if (call == "registerDocument")
    {
        body = HttpWire::registerRequest(upload);
        api.registerDocument(signer, upload, std::bind(note));
    }
    else if (call == "head")
    {
        api.head(documentId, std::bind(note));
    }
    else if (call == "download")
    {
        api.download(documentId, std::bind(note));
    }
    else if (call == "upload")
    {
        body = HttpWire::uploadRequest(5, upload);
        api.upload(signer, documentId, 5, upload, std::bind(note));
    }
    else if (call == "takeLease")
    {
        api.takeLease(signer, documentId, std::bind(note));
    }
    else if (call == "releaseLease")
    {
        api.releaseLease(signer, documentId, std::bind(note));
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
    QCOMPARE(QString::fromLatin1(sent.path), QString(path).replace("%1", documentId));
    QVERIFY(!sent.headers.contains("authorization"));
    QCOMPARE(sent.headers.value("x-signer-public"), isSigned ? QByteArray(32, '\x03').toBase64() : QByteArray());
    QCOMPARE(sent.headers.value("x-signer-verify"), isSigned ? QByteArray(32, '\x04').toBase64() : QByteArray());
    QCOMPARE(sent.headers.value("x-signature"), isSigned ? QByteArray(64, '\x07').toBase64() : QByteArray());
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
    CannedServer canned(httpResponse(423, R"({"error": "leaseHeld"})"));
    QVERIFY(canned.isListening());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address());
    HttpServerApi api(*settings, nullptr);

    std::optional<Result<Lease>> result;
    api.takeLease(fixedSigner(), DocumentLibrary::newDocumentId(), recordTo(result));
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
        api.registerDocument(fixedSigner(), firstUpload(), recordTo(result));

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
    api.releaseLease(fixedSigner(), DocumentLibrary::newDocumentId(), recordTo(released));
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

    std::optional<Result<Head>> answer;
    api.head(DocumentLibrary::newDocumentId(), recordTo(answer));
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
    api.head(DocumentLibrary::newDocumentId(), recordTo(result));
    QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), AnswerMilliseconds);
    QCOMPARE(result->error, ServerError::Unreachable);
}

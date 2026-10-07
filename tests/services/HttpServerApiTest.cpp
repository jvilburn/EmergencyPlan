#include "HttpServerApiTest.h"

#include "AppSettings.h"
#include "DocumentCrypto.h"
#include "DocumentFile.h"
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
using ServerTestSupport::wait;

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
    AccessRoster roster;
    roster.addPerson(alice.asNewPerson());
    Document document = Document::empty();
    document.setAccessRoster(roster);

    DocumentHeader first;
    first.documentId = ChainTestSupport::newDocumentId();
    first.saveCounter = 0;
    const DocumentFile::Sealed sealed =
        DocumentFile::seal(document, first, DocumentCrypto::generateDataKey(), alice).value();
    return Upload{sealed.header, sealed.ciphertext};
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
        QObject::connect(&m_server, &QTcpServer::newConnection, &m_server, [this]() { accept(); });
    }

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
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer]()
        {
            buffer->append(socket->readAll());
            answerOnceComplete(socket, *buffer);
        });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
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

    DeviceKeys keys;
    keys.publicKey.bytes = QByteArray(32, '\x03');
    keys.verifyKey.bytes = QByteArray(32, '\x04');
    AccessRequest request;
    request.member = MemberId::fromHex("00112233445566778899aabbccddeeff");
    request.displayName = "Bob";
    request.device = RosterDevice::create("Phone", keys.publicKey, keys.verifyKey);
    const QJsonObject withoutCode{{"member", "00112233445566778899aabbccddeeff"},
                                  {"displayName", "Bob"},
                                  {"deviceName", "Phone"},
                                  {"publicKey", base64(keys.publicKey.bytes)},
                                  {"verifyKey", base64(keys.verifyKey.bytes)}};
    QCOMPARE(HttpWire::accessRequestRequest(request), withoutCode);
    request.pairingCode = "K7Q2M9";
    QJsonObject withCode = withoutCode;
    withCode["pairingCode"] = "K7Q2M9";
    QCOMPARE(HttpWire::accessRequestRequest(request), withCode);

    const QByteArray commitment(32, '\x05');
    QCOMPARE(HttpWire::startPairingRequest("Phone", commitment),
             (QJsonObject{{"deviceName", "Phone"}, {"commitment", base64(commitment)}}));
    QCOMPARE(HttpWire::deviceKeysRequest(keys),
             (QJsonObject{{"publicKey", base64(keys.publicKey.bytes)}, {"verifyKey", base64(keys.verifyKey.bytes)}}));
    const QByteArray nonce(32, '\x06');
    QCOMPARE(HttpWire::revealPairingRequest(keys, nonce),
             (QJsonObject{{"publicKey", base64(keys.publicKey.bytes)},
                          {"verifyKey", base64(keys.verifyKey.bytes)},
                          {"nonce", base64(nonce)}}));
    QCOMPARE(HttpWire::handOverRequest("signed handover"), (QJsonObject{{"handover", base64("signed handover")}}));
    QCOMPARE(HttpWire::advanceClockRequest(86400), (QJsonObject{{"seconds", 86400}}));
}

void HttpServerApiTest::answersAreReadFromTheWireFormat()
{
    QCOMPARE(HttpWire::readChallenge(jsonOf(R"({"challenge": "b25lLXRpbWU="})")).value(), QByteArray("one-time"));
    QCOMPARE(HttpWire::readSession(jsonOf(R"({"token": "abc.def"})"))->token, QString("abc.def"));
    QCOMPARE(HttpWire::readVersion(jsonOf(R"({"version": 12})")).value(), qint64(12));

    const std::optional<Head> free = HttpWire::readHead(jsonOf(R"({"version": 3})"));
    QCOMPARE(free->version, qint64(3));
    QVERIFY(!free->leaseHolder.has_value());
    const std::optional<Head> held =
        HttpWire::readHead(jsonOf(R"({"version": 4, "leaseHolder": "00112233445566778899aabbccddeeff"})"));
    QCOMPARE(held->leaseHolder->toHex(), QString("00112233445566778899aabbccddeeff"));

    const Upload upload = firstUpload();
    QJsonObject snapshotJson{{"version", 5},
                             {"headers", QJsonArray{upload.header.toJson()}},
                             {"blob", base64(upload.blob)}};
    const std::optional<Snapshot> snapshot = HttpWire::readSnapshot(snapshotJson);
    QCOMPARE(snapshot->version, qint64(5));
    QCOMPARE(snapshot->headers, QList<QJsonObject>{upload.header.toJson()});
    QCOMPARE(snapshot->blob, upload.blob);
    QCOMPARE(HttpWire::readHeaderChain(QJsonObject{{"headers", QJsonArray{upload.header.toJson()}}})->headers,
             QList<QJsonObject>{upload.header.toJson()});

    const std::optional<Lease> lease = HttpWire::readLease(
        jsonOf(R"({"holder": "00112233445566778899aabbccddeeff", "expires": "2026-10-07T12:00:00.000Z"})"));
    QCOMPARE(lease->holder.toHex(), QString("00112233445566778899aabbccddeeff"));
    QCOMPARE(lease->expires, QDateTime(QDate(2026, 10, 7), QTime(12, 0), QTimeZone::UTC));

    const QString keyA = base64(QByteArray(32, '\x03'));
    const QString keyB = base64(QByteArray(32, '\x04'));
    const std::optional<QList<AccessRequest>> requests = HttpWire::readAccessRequests(QJsonObject{
        {"requests",
         QJsonArray{QJsonObject{{"member", "00112233445566778899aabbccddeeff"},
                                {"displayName", "Bob"},
                                {"deviceName", "Phone"},
                                {"publicKey", keyA},
                                {"verifyKey", keyB},
                                {"pairingCode", "K7Q2M9"}}}}});
    QCOMPARE(requests->size(), qsizetype(1));
    QCOMPARE(requests->at(0).member.toHex(), QString("00112233445566778899aabbccddeeff"));
    QCOMPARE(requests->at(0).displayName, QString("Bob"));
    QCOMPARE(requests->at(0).device.deviceName(), QString("Phone"));
    QCOMPARE(requests->at(0).device.publicKey().bytes, QByteArray(32, '\x03'));
    QCOMPARE(requests->at(0).device.verifyKey().bytes, QByteArray(32, '\x04'));
    QCOMPARE(requests->at(0).pairingCode, QString("K7Q2M9"));
    QVERIFY(HttpWire::readAccessRequests(jsonOf(R"({"requests": []})"))->isEmpty());

    QCOMPARE(HttpWire::readRequestStatus(jsonOf(R"({"status": "pending"})")).value(), RequestStatus::Pending);
    QCOMPARE(HttpWire::readRequestStatus(jsonOf(R"({"status": "granted"})")).value(), RequestStatus::Granted);
    QCOMPARE(HttpWire::readRequestStatus(jsonOf(R"({"status": "withdrawn"})")).value(), RequestStatus::Withdrawn);

    QCOMPARE(HttpWire::readDocumentIds(jsonOf(R"({"documents": ["a1", "b2"]})")).value(), (QStringList{"a1", "b2"}));
    QCOMPARE(HttpWire::readPairingCode(jsonOf(R"({"code": "K7Q2M9"})")).value(), QString("K7Q2M9"));

    // A pairing as first posted, and once every step is written.
    const std::optional<PairingState> started = HttpWire::readPairingState(QJsonObject{
        {"code", "K7Q2M9"},
        {"initiatorDeviceName", "Phone"},
        {"commitment", base64(QByteArray(32, '\x05'))},
        {"expires", "2026-10-07T12:10:00.000Z"}});
    QCOMPARE(started->code, QString("K7Q2M9"));
    QCOMPARE(started->initiatorDeviceName, QString("Phone"));
    QCOMPARE(started->commitment, QByteArray(32, '\x05'));
    QVERIFY(!started->responder.has_value());
    QVERIFY(!started->initiator.has_value());
    QVERIFY(started->nonce.isEmpty());
    QVERIFY(started->handover.isEmpty());

    const std::optional<PairingState> finished = HttpWire::readPairingState(QJsonObject{
        {"code", "K7Q2M9"},
        {"initiatorDeviceName", "Phone"},
        {"commitment", base64(QByteArray(32, '\x05'))},
        {"responder", QJsonObject{{"publicKey", keyA}, {"verifyKey", keyB}}},
        {"initiator", QJsonObject{{"publicKey", keyB}, {"verifyKey", keyA}}},
        {"nonce", base64(QByteArray(32, '\x06'))},
        {"handover", base64("signed handover")},
        {"expires", "2026-10-07T12:10:00.000Z"}});
    QCOMPARE(finished->responder->publicKey.bytes, QByteArray(32, '\x03'));
    QCOMPARE(finished->initiator->publicKey.bytes, QByteArray(32, '\x04'));
    QCOMPARE(finished->nonce, QByteArray(32, '\x06'));
    QCOMPARE(finished->handover, QByteArray("signed handover"));
    QCOMPARE(finished->expires, QDateTime(QDate(2026, 10, 7), QTime(12, 10), QTimeZone::UTC));

    // An answer may carry fields a later server adds.
    QCOMPARE(HttpWire::readVersion(jsonOf(R"({"version": 2, "keyGeneration": 1})")).value(), qint64(2));
}

void HttpServerApiTest::illFormedAnswersAreRefused_data()
{
    QTest::addColumn<QString>("reader");
    QTest::addColumn<QByteArray>("json");

    QTest::newRow("challenge not base64") << "challenge" << QByteArray(R"({"challenge": "not base64!"})");
    QTest::newRow("empty challenge") << "challenge" << QByteArray(R"({"challenge": ""})");
    QTest::newRow("empty token") << "session" << QByteArray(R"({"token": ""})");
    QTest::newRow("version zero") << "version" << QByteArray(R"({"version": 0})");
    QTest::newRow("version as text") << "version" << QByteArray(R"({"version": "3"})");
    QTest::newRow("lease holder too short") << "head" << QByteArray(R"({"version": 3, "leaseHolder": "0011"})");
    QTest::newRow("lease holder in capitals")
        << "head" << QByteArray(R"({"version": 3, "leaseHolder": "00112233445566778899AABBCCDDEEFF"})");
    QTest::newRow("no headers") << "snapshot" << QByteArray(R"({"version": 3, "headers": [], "blob": ""})");
    QTest::newRow("no blob") << "snapshot" << QByteArray(R"({"version": 3, "headers": [{}]})");
    QTest::newRow("lease with no expiry") << "lease" << QByteArray(R"({"holder": "00112233445566778899aabbccddeeff"})");
    QTest::newRow("request with a bad member")
        << "requests"
        << QByteArray(R"({"requests": [{"member": "zz", "displayName": "Bob", "deviceName": "Phone",)"
                      R"( "publicKey": "AwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwM=",)"
                      R"( "verifyKey": "BAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQ="}]})");
    QTest::newRow("request with a short key")
        << "requests"
        << QByteArray(R"({"requests": [{"member": "00112233445566778899aabbccddeeff", "displayName": "Bob",)"
                      R"( "deviceName": "Phone", "publicKey": "AwM=",)"
                      R"( "verifyKey": "BAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQ="}]})");
    QTest::newRow("unknown status") << "status" << QByteArray(R"({"status": "maybe"})");
    QTest::newRow("empty document id") << "documents" << QByteArray(R"({"documents": [""]})");
    QTest::newRow("pairing with no expiry")
        << "pairing" << QByteArray(R"({"code": "K7Q2M9", "initiatorDeviceName": "Phone", "commitment": "BQU="})");
}

void HttpServerApiTest::illFormedAnswersAreRefused()
{
    QFETCH(QString, reader);
    QFETCH(QByteArray, json);
    const QJsonObject object = QJsonDocument::fromJson(json).object();

    if (reader == "challenge")
    {
        QVERIFY(!HttpWire::readChallenge(object).has_value());
    }
    else if (reader == "session")
    {
        QVERIFY(!HttpWire::readSession(object).has_value());
    }
    else if (reader == "version")
    {
        QVERIFY(!HttpWire::readVersion(object).has_value());
    }
    else if (reader == "head")
    {
        QVERIFY(!HttpWire::readHead(object).has_value());
    }
    else if (reader == "snapshot")
    {
        QVERIFY(!HttpWire::readSnapshot(object).has_value());
    }
    else if (reader == "lease")
    {
        QVERIFY(!HttpWire::readLease(object).has_value());
    }
    else if (reader == "requests")
    {
        QVERIFY(!HttpWire::readAccessRequests(object).has_value());
    }
    else if (reader == "status")
    {
        QVERIFY(!HttpWire::readRequestStatus(object).has_value());
    }
    else if (reader == "documents")
    {
        QVERIFY(!HttpWire::readDocumentIds(object).has_value());
    }
    else if (reader == "pairing")
    {
        QVERIFY(!HttpWire::readPairingState(object).has_value());
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
        {"notAllowListed", ServerError::NotAllowListed},
        {"unknownDocument", ServerError::UnknownDocument},
        {"versionConflict", ServerError::VersionConflict},
        {"leaseHeld", ServerError::LeaseHeld},
        {"chainRefused", ServerError::ChainRefused},
        {"payloadMismatch", ServerError::PayloadMismatch},
        {"counterJump", ServerError::CounterJump},
        {"deviceNotInvited", ServerError::DeviceNotInvited},
        {"idTaken", ServerError::IdTaken},
        {"rateLimited", ServerError::RateLimited},
        {"notFound", ServerError::NotFound},
        {"alreadyWritten", ServerError::AlreadyWritten},
        {"outOfOrder", ServerError::OutOfOrder},
        {"expired", ServerError::Expired},
        {"invalidRequest", ServerError::InvalidRequest},
    };
    for (const auto& name : names)
    {
        QTest::newRow(name.first) << 400 << (QByteArray(R"({"error": ")") + name.first + R"("})") << name.second;
    }

    // With no name this build knows, the status.
    QTest::newRow("401") << 401 << QByteArray() << ServerError::Unauthenticated;
    QTest::newRow("403") << 403 << QByteArray() << ServerError::NotAllowListed;
    QTest::newRow("404") << 404 << QByteArray("not json") << ServerError::NotFound;
    QTest::newRow("409") << 409 << QByteArray(R"({"error": "somethingNew"})") << ServerError::VersionConflict;
    QTest::newRow("410") << 410 << QByteArray() << ServerError::Expired;
    QTest::newRow("423") << 423 << QByteArray() << ServerError::LeaseHeld;
    QTest::newRow("429") << 429 << QByteArray() << ServerError::RateLimited;
    QTest::newRow("418") << 418 << QByteArray() << ServerError::InvalidRequest;

    // A server failing is not there, whatever it names.
    QTest::newRow("500") << 500 << QByteArray(R"({"error": "leaseHeld"})") << ServerError::Unreachable;
    QTest::newRow("503") << 503 << QByteArray() << ServerError::Unreachable;
}

void HttpServerApiTest::errorsAreReadByNameThenStatus()
{
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(ServerError, error);
    QCOMPARE(HttpWire::readError(status, body), error);
}

void HttpServerApiTest::aCallTravelsWithItsTokenSignatureAndBody()
{
    CannedServer canned(httpResponse(200, R"({"version": 8})"));
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address() + "/");
    HttpServerApi api(*settings, nullptr);

    const Upload upload = firstUpload();
    const Session session{"session-token"};
    const QByteArray signature(64, '\x07');
    const Result<qint64> result = wait<qint64>([&](Reply<qint64> reply)
    {
        api.upload(session, upload.header.documentId, 7, upload, signature, reply);
    });

    QVERIFY(result.ok());
    QCOMPARE(result.value.value(), qint64(8));
    QVERIFY(canned.captured().has_value());
    const Captured& request = canned.captured().value();
    QCOMPARE(request.method, QByteArray("PUT"));
    QCOMPARE(request.path, "/documents/" + upload.header.documentId.toLatin1());
    QCOMPARE(request.headers.value("authorization"), QByteArray("Bearer session-token"));
    QCOMPARE(request.headers.value("x-request-signature"), signature.toBase64());
    QCOMPARE(QJsonDocument::fromJson(request.body).object(), HttpWire::uploadRequest(7, upload));

    // A read carries the token, no signature, and its query. The canned answer is
    // not a snapshot.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unreadable server answer"));
    const Result<Snapshot> download = wait<Snapshot>([&](Reply<Snapshot> reply)
    {
        api.download(session, upload.header.documentId, 4, reply);
    });
    QVERIFY(!download.ok());
    QCOMPARE(canned.captured()->method, QByteArray("GET"));
    QCOMPARE(canned.captured()->path, "/documents/" + upload.header.documentId.toLatin1() + "?after=4");
    QCOMPARE(canned.captured()->headers.value("authorization"), QByteArray("Bearer session-token"));
    QVERIFY(!canned.captured()->headers.contains("x-request-signature"));
}

void HttpServerApiTest::aFailedAnswerIsItsNamedError()
{
    CannedServer canned(httpResponse(409, R"({"error": "leaseHeld"})"));
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address());
    HttpServerApi api(*settings, nullptr);

    const Result<Lease> result = wait<Lease>([&](Reply<Lease> reply)
    {
        api.takeLease(Session{"token"}, ChainTestSupport::newDocumentId(), QByteArray(64, '\x01'), reply);
    });
    QVERIFY(!result.ok());
    QCOMPARE(result.error, ServerError::LeaseHeld);
    QCOMPARE(canned.captured()->method, QByteArray("POST"));
}

void HttpServerApiTest::anUnreadableOrFailingServerIsUnreachable()
{
    const QList<std::pair<QByteArray, bool>> responses = {
        {httpResponse(200, "not json"), true},
        {httpResponse(200, R"({"version": "eight"})"), true},
        {httpResponse(503, R"({"error": "versionConflict"})"), false},
    };
    for (const auto& [response, unreadable] : responses)
    {
        CannedServer canned(response);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address());
        HttpServerApi api(*settings, nullptr);

        if (unreadable)
        {
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Unreadable server answer to \"POST /documents\""));
        }
        const Result<qint64> result = wait<qint64>([&](Reply<qint64> reply)
        {
            api.registerDocument(Session{"token"}, firstUpload(), QByteArray(64, '\x01'), reply);
        });
        QCOMPARE(result.error, ServerError::Unreachable);
        QVERIFY(canned.captured().has_value());
    }

    // Anchor: a success with nothing to read is Done.
    CannedServer canned(httpResponse(204, QByteArray()));
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, canned.address());
    HttpServerApi api(*settings, nullptr);
    const Result<Done> released = wait<Done>([&](Reply<Done> reply)
    {
        api.releaseLease(Session{"token"}, ChainTestSupport::newDocumentId(), QByteArray(64, '\x01'), reply);
    });
    QVERIFY(released.ok());
    QCOMPARE(canned.captured()->method, QByteArray("DELETE"));
}

void HttpServerApiTest::noAddressIsUnreachableAfterTheCallReturns()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const std::unique_ptr<AppSettings> settings = settingsFor(folder, QString());
    QVERIFY(settings->serverAddress().isEmpty());
    HttpServerApi api(*settings, nullptr);

    bool returned = false;
    bool answeredBeforeReturn = false;
    std::optional<Result<QByteArray>> answer;
    api.challenge([&](const Result<QByteArray>& result)
    {
        answeredBeforeReturn = !returned;
        answer = result;
    });
    returned = true;
    QVERIFY(!answer.has_value());

    QTRY_VERIFY(answer.has_value());
    QVERIFY(!answeredBeforeReturn);
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

    const Result<Head> result = wait<Head>([&](Reply<Head> reply)
    {
        api.head(Session{"token"}, ChainTestSupport::newDocumentId(), reply);
    });
    QCOMPARE(result.error, ServerError::Unreachable);
}

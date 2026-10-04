#include "KeyStoreTest.h"

#include "ChainTestSupport.h"
#include "DocumentCrypto.h"
#include "HexCodec.h"
#include "KeyStore.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtTest>

#include <functional>

namespace
{

const QString Account("identity-test");

constexpr int KeychainMilliseconds = 10 * 1000;

/// Which signal a KeyStore call answered with, and what came with it.
struct Answer
{
    enum class Kind
    {
        None,
        Stored,
        Loaded,
        NotFound,
        Removed,
        Failed
    };

    Kind kind = Kind::None;
    QByteArray secret;
    QString error;

    void onStored() { kind = Kind::Stored; }
    void onLoaded(const QByteArray& loaded);
    void onNotFound() { kind = Kind::NotFound; }
    void onRemoved() { kind = Kind::Removed; }
    void onFailed(const QString& message);
};

void Answer::onLoaded(const QByteArray& loaded)
{
    kind = Kind::Loaded;
    secret = loaded;
}

void Answer::onFailed(const QString& message)
{
    kind = Kind::Failed;
    error = message;
}

/// Makes one call and waits for its answer. None when the keychain never answers.
Answer ask(const std::function<void(KeyStore&)>& call)
{
    KeyStore store(Account, nullptr);
    Answer answer;
    QEventLoop loop;
    QObject::connect(&store, &KeyStore::stored, &store, std::bind(&Answer::onStored, &answer));
    QObject::connect(&store, &KeyStore::loaded, &store, std::bind(&Answer::onLoaded, &answer, std::placeholders::_1));
    QObject::connect(&store, &KeyStore::notFound, &store, std::bind(&Answer::onNotFound, &answer));
    QObject::connect(&store, &KeyStore::removed, &store, std::bind(&Answer::onRemoved, &answer));
    QObject::connect(&store, &KeyStore::failed, &store, std::bind(&Answer::onFailed, &answer, std::placeholders::_1));
    QObject::connect(&store, &KeyStore::stored, &loop, &QEventLoop::quit);
    QObject::connect(&store, &KeyStore::loaded, &loop, &QEventLoop::quit);
    QObject::connect(&store, &KeyStore::notFound, &loop, &QEventLoop::quit);
    QObject::connect(&store, &KeyStore::removed, &loop, &QEventLoop::quit);
    QObject::connect(&store, &KeyStore::failed, &loop, &QEventLoop::quit);
    QTimer::singleShot(KeychainMilliseconds, &loop, &QEventLoop::quit);

    call(store);
    loop.exec();
    return answer;
}

void storeSecret(const QByteArray& secret, KeyStore& store)
{
    store.store(secret);
}

void loadSecret(KeyStore& store)
{
    store.load();
}

void removeSecret(KeyStore& store)
{
    store.remove();
}

IdentityRecord newRecord()
{
    IdentityRecord record;
    record.sealing = DocumentCrypto::generateKeypair();
    record.signing = DocumentCrypto::generateSigningKeypair();
    record.member = ChainTestSupport::newMember();
    return record;
}

/// Whether a keychain answered at all: one that fails or never answers means
/// there is none usable on this machine.
bool keychainAnswered(const Answer& answer)
{
    return answer.kind != Answer::Kind::Failed && answer.kind != Answer::Kind::None;
}

} // namespace

void KeyStoreTest::initTestCase()
{
    QVERIFY(DocumentCrypto::initialise());
}

void KeyStoreTest::cleanup()
{
    ask(&removeSecret);
}

// --- The record ------------------------------------------------------------------

void KeyStoreTest::aRecordRoundTripsWithAndWithoutAPerson()
{
    const IdentityRecord withPerson = newRecord();
    QVERIFY(IdentityRecord::fromBytes(withPerson.toBytes()) == withPerson);

    IdentityRecord keysOnly = newRecord();
    keysOnly.member.reset();
    const std::optional<IdentityRecord> back = IdentityRecord::fromBytes(keysOnly.toBytes());
    QVERIFY(back == keysOnly);
    QVERIFY(!back->member.has_value());

    // Every key is carried, so a record with one key changed differs.
    IdentityRecord changed = withPerson;
    changed.signing = DocumentCrypto::generateSigningKeypair();
    QVERIFY(IdentityRecord::fromBytes(changed.toBytes()) != withPerson);
}

void KeyStoreTest::aRecordWithAKeyOfTheWrongSizeIsRefused()
{
    const IdentityRecord record = newRecord();
    for (const char* key : {"sealingPublic", "sealingSecret", "verifyKey", "signingKey", "member"})
    {
        QJsonObject json = QJsonDocument::fromJson(record.toBytes()).object();
        json[key] = HexCodec::encode(QByteArray(5, 'x'));
        QVERIFY2(!IdentityRecord::fromBytes(QJsonDocument(json).toJson()).has_value(), key);
    }

    QVERIFY(!IdentityRecord::fromBytes("not json").has_value());
}

void KeyStoreTest::aRecordWithAnUnknownFieldIsRefused()
{
    QJsonObject json = QJsonDocument::fromJson(newRecord().toBytes()).object();
    json["displayName"] = "Alice";
    QVERIFY(!IdentityRecord::fromBytes(QJsonDocument(json).toJson()).has_value());
}

// --- The keychain ----------------------------------------------------------------

void KeyStoreTest::aStoredSecretLoadsBack()
{
    const QByteArray secret("a secret with \x00 and \xff bytes", 30);
    const Answer stored = ask(std::bind(&storeSecret, secret, std::placeholders::_1));
    if (!keychainAnswered(stored))
    {
        QSKIP(qPrintable("No usable keychain here: " + stored.error));
    }
    QCOMPARE(stored.kind, Answer::Kind::Stored);

    const Answer loaded = ask(&loadSecret);
    QCOMPARE(loaded.kind, Answer::Kind::Loaded);
    QCOMPARE(loaded.secret, secret);
}

void KeyStoreTest::aMissingSecretIsNotFound()
{
    const Answer removed = ask(&removeSecret);
    if (!keychainAnswered(removed))
    {
        QSKIP(qPrintable("No usable keychain here: " + removed.error));
    }

    const Answer loaded = ask(&loadSecret);
    QCOMPARE(loaded.kind, Answer::Kind::NotFound);
}

void KeyStoreTest::anIdentityRecordRoundTripsThroughTheKeychain()
{
    const IdentityRecord record = newRecord();
    const Answer stored = ask(std::bind(&storeSecret, record.toBytes(), std::placeholders::_1));
    if (!keychainAnswered(stored))
    {
        QSKIP(qPrintable("No usable keychain here: " + stored.error));
    }

    const Answer loaded = ask(&loadSecret);
    QCOMPARE(loaded.kind, Answer::Kind::Loaded);
    QVERIFY(IdentityRecord::fromBytes(loaded.secret) == record);
}

void KeyStoreTest::removingAMissingSecretStillAnswers()
{
    const Answer first = ask(&removeSecret);
    if (!keychainAnswered(first))
    {
        QSKIP(qPrintable("No usable keychain here: " + first.error));
    }

    QCOMPARE(ask(&removeSecret).kind, Answer::Kind::Removed);
}

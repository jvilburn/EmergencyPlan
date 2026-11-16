#include "SignedLineTest.h"

#include "Base64Url.h"
#include "SignedLine.h"

#include <QTest>

namespace
{
/// The id the known answers use: the 16 bytes 0x00 to 0x0f.
DocumentId fixedId()
{
    QByteArray bytes;
    for (char byte = 0; byte < 16; ++byte)
    {
        bytes.append(byte);
    }
    return DocumentId::fromString(Base64Url::encode(bytes));
}

/// The header line the known answers hash, then payload.
QByteArray versionWith(const QByteArray& payload)
{
    return QByteArray("{\"format\":1}\n") + payload;
}

// BLAKE2b-256 in base64url, computed with Python's hashlib.blake2b(digest_size=32).
const QByteArray HeaderHash = "IwQnH8tNd2cuYFDIfSfN36CjzNjV3rqCJKmxN8hvk1E";
const QByteArray AbcHash = "vd2BPGNCOXIxce8_7phXm5SWTjuxyz5CcmLIwGjVIxk";
const QByteArray EmptyHash = "DldRwCblQ7Loqy6wYJnaodHl30d3j3eH-qtFzfEv46g";
}  // namespace

SignedLineTest::SignedLineTest(QObject* parent)
    : QObject(parent)
{
}

void SignedLineTest::createLine()
{
    QCOMPARE(SignedLine::ofVersion(SignedCall::Create, fixedId(), versionWith("abc")),
             "wardplanning-v1 create AAECAwQFBgcICQoLDA0ODw " + HeaderHash + ' ' + AbcHash);
}

void SignedLineTest::pushLine()
{
    QCOMPARE(SignedLine::ofVersion(SignedCall::Push, fixedId(), versionWith(QByteArray())),
             "wardplanning-v1 push AAECAwQFBgcICQoLDA0ODw " + HeaderHash + ' ' + EmptyHash);
}

void SignedLineTest::updateLine()
{
    QCOMPARE(SignedLine::ofVersion(SignedCall::Update, fixedId(), versionWith("abc")),
             "wardplanning-v1 update AAECAwQFBgcICQoLDA0ODw " + HeaderHash + ' ' + AbcHash);
}

void SignedLineTest::channelLine()
{
    QCOMPARE(SignedLine::ofChannel(fixedId()), QByteArray("wardplanning-v1 channel AAECAwQFBgcICQoLDA0ODw - -"));
}

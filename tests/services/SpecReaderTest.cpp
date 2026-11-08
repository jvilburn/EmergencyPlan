#include "SpecReaderTest.h"
#include "AppData.h"
#include "Crypto.h"
#include "EveryField.h"
#include "FileWardStore.h"
#include "KeyFile.h"
#include "WardFile.h"
#include "WardFileEdits.h"

#include <QJsonDocument>
#include <QProcess>
#include <QTest>

SpecReaderTest::SpecReaderTest(QObject* parent)
    : QObject(parent)
    , m_phone(Identity::generate())
{
}

void SpecReaderTest::initTestCase()
{
    QVERIFY(m_appData.isValid());
    KeyFileResult keys = KeyFile::loadOrCreate(AppData(m_appData.path()).keyFilePath());
    QVERIFY2(keys.status == KeyFileStatus::Created, qPrintable(keys.problem));
    m_laptop = keys.identity;
}

void SpecReaderTest::init()
{
    m_document = DocumentId::generate();
    m_dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    m_payload = EveryField::payload(*m_laptop, m_phone);
}

bool SpecReaderTest::putWard(const DocumentId& document, const QByteArray& file) const
{
    QString problem;
    return FileWardStore(AppData(m_appData.path()).wardsDirectory()).write(document, file, &problem);
}

SpecReaderTest::Run SpecReaderTest::read(const DocumentId& document) const
{
    QProcess reader;
    reader.start(WARD_PLANNING_SPEC_READER, {m_appData.path(), document.toString()});
    if (!reader.waitForFinished(60000) || reader.exitStatus() != QProcess::NormalExit)
    {
        return Run{-1, QByteArray(), "the reader did not finish"};
    }
    return Run{reader.exitCode(), reader.readAllStandardOutput(), QString::fromUtf8(reader.readAllStandardError())};
}

QByteArray SpecReaderTest::sealedTo(const QList<RecipientKeys>& recipients, const QJsonObject& payload) const
{
    return WardFileEdits::sealedFile(m_document, m_dataKey, recipients, payload);
}

void SpecReaderTest::specReaderRecoversEveryFieldTheAppWrites()
{
    QVERIFY(putWard(m_document, WardFile::write(m_document, m_dataKey, m_payload)));

    Run run = read(m_document);

    QVERIFY2(run.exitCode == 0, qPrintable(run.problem));
    QJsonParseError error;
    QJsonDocument output = QJsonDocument::fromJson(run.output, &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));
    QCOMPARE(output.object(), m_payload.toJson());
}

void SpecReaderTest::specReaderRefusesAFieldTheSpecLacks()
{
    QList<RecipientKeys> recipients = m_payload.ward.access().recipients();
    QJsonObject json = m_payload.toJson();
    QVERIFY(putWard(m_document, sealedTo(recipients, json)));
    Run unmutated = read(m_document);
    QVERIFY2(unmutated.exitCode == 0, qPrintable(unmutated.problem));

    QJsonObject mutated = WardFileEdits::edited(json, {"ward", "families", "0", "nickname"}, "Bobby");
    QVERIFY(putWard(m_document, sealedTo(recipients, mutated)));
    Run run = read(m_document);

    QCOMPARE(run.exitCode, 1);
    QVERIFY2(run.problem.contains("nickname"), qPrintable(run.problem));
}

void SpecReaderTest::specReaderRefusesAHeaderThatDiffersFromTheAccessList()
{
    QList<RecipientKeys> recipients = m_payload.ward.access().recipients();
    QJsonObject json = m_payload.toJson();
    QVERIFY(putWard(m_document, sealedTo(recipients, json)));
    Run unmutated = read(m_document);
    QVERIFY2(unmutated.exitCode == 0, qPrintable(unmutated.problem));

    // The last open invite has no entry.
    QVERIFY(putWard(m_document, sealedTo(recipients.mid(0, recipients.size() - 1), json)));
    Run missing = read(m_document);
    QCOMPARE(missing.exitCode, 1);
    QVERIFY2(missing.problem.contains("access list"), qPrintable(missing.problem));

    // An entry built from this device's X25519 key and the phone's verify key.
    QList<RecipientKeys> mixed = recipients;
    mixed.append(RecipientKeys{m_laptop->publicKey(), m_phone.verifyKey()});
    QVERIFY(putWard(m_document, sealedTo(mixed, json)));
    Run extra = read(m_document);
    QCOMPARE(extra.exitCode, 1);
    QVERIFY2(extra.problem.contains("access list"), qPrintable(extra.problem));
}

void SpecReaderTest::specReaderRefusesAFileNamedForAnotherWard()
{
    QByteArray file = WardFile::write(m_document, m_dataKey, m_payload);
    DocumentId other = DocumentId::generate();
    QVERIFY(putWard(m_document, file));
    QVERIFY(putWard(other, file));
    Run named = read(m_document);
    QVERIFY2(named.exitCode == 0, qPrintable(named.problem));

    Run run = read(other);

    QCOMPARE(run.exitCode, 1);
    QVERIFY2(run.problem.contains("another document"), qPrintable(run.problem));
}

void SpecReaderTest::specReaderReportsANewerFormat()
{
    QByteArray file = WardFile::write(m_document, m_dataKey, m_payload);
    QJsonObject header = WardFileEdits::headerOf(file);

    QVERIFY(putWard(m_document, WardFileEdits::withHeader(file, QJsonObject{{"format", 2}})));
    QCOMPARE(read(m_document).exitCode, 2);

    // A format below every version, or one that is not an integer, is unreadable rather than newer.
    header["format"] = 0;
    QVERIFY(putWard(m_document, WardFileEdits::withHeader(file, header)));
    QCOMPARE(read(m_document).exitCode, 1);
    header["format"] = 1.5;
    QVERIFY(putWard(m_document, WardFileEdits::withHeader(file, header)));
    QCOMPARE(read(m_document).exitCode, 1);
}

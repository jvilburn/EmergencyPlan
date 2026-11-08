#include "SpecReaderTest.h"
#include "AppData.h"
#include "CommandRegistry.h"
#include "Crypto.h"
#include "EveryField.h"
#include "FileWardStore.h"
#include "KeyFile.h"
#include "UpdateFile.h"
#include "WardFile.h"
#include "WardFileEdits.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTest>

namespace
{

/// The value at path in json, each step a key, or an index when json there is an array.
QJsonValue valueAt(const QJsonObject& json, const QStringList& path)
{
    QJsonValue value = json;
    for (const QString& step : path)
    {
        value = value.isArray() ? value.toArray().at(step.toInt()) : value.toObject().value(step);
    }
    return value;
}

/// path as the reader names it, from the payload down.
QString readerPath(const QJsonObject& json, const QStringList& path)
{
    QString named = "payload";
    for (int i = 0; i < path.size(); ++i)
    {
        named += valueAt(json, path.mid(0, i)).isArray() ? "[" + path[i] + "]" : "." + path[i];
    }
    return named;
}

/// The path of the fields of the journal entry in json whose command is of type.
QStringList fieldsOf(const QJsonObject& json, const QString& type)
{
    const QJsonArray entries = json["device"]["journal"]["entries"].toArray();
    for (int i = 0; i < entries.size(); ++i)
    {
        if (entries.at(i)["command"]["type"].toString() == type)
        {
            return {"device", "journal", "entries", QString::number(i), "command", "fields"};
        }
    }
    return {};
}

}  // namespace

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
    return runReader({m_appData.path(), document.toString()});
}

SpecReaderTest::Run SpecReaderTest::readUpdate(const DocumentId& document, const QByteArray& update) const
{
    QString path = m_appData.filePath("received.wardupdate");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(update) != update.size())
    {
        return Run{-1, QByteArray(), "the update file could not be written"};
    }
    file.close();
    return runReader({m_appData.path(), document.toString(), path});
}

QByteArray SpecReaderTest::phoneVersion() const
{
    WardPayload version = m_payload;
    version.device = std::nullopt;
    return WardFile::write(m_document, VersionRank{5, m_phone.fingerprint()}, m_dataKey, version);
}

SpecReaderTest::Run SpecReaderTest::runReader(const QStringList& arguments)
{
    QProcess reader;
    reader.start(WARD_PLANNING_SPEC_READER, arguments);
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
    QVERIFY(putWard(m_document, WardFile::write(m_document, WardFileEdits::version(), m_dataKey, m_payload)));

    Run run = read(m_document);

    QVERIFY2(run.exitCode == 0, qPrintable(run.problem));
    QJsonParseError error;
    QJsonDocument output = QJsonDocument::fromJson(run.output, &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));
    QCOMPARE(output.object(), m_payload.toJson());
}

void SpecReaderTest::specReaderRefusesAFieldTheSpecLacksInEveryTable()
{
    QList<RecipientKeys> recipients = m_payload.ward.access().recipients();
    QJsonObject json = m_payload.toJson();
    QVERIFY(putWard(m_document, sealedTo(recipients, json)));
    Run unmutated = read(m_document);
    QVERIFY2(unmutated.exitCode == 0, qPrintable(unmutated.problem));

    // An object of each table in docs/FORMAT.md, and the fields of each command type.
    QStringList task = {"ward", "emergencyResponse", "tasks", "0"};
    QStringList updateFamily = fieldsOf(json, "updateFamily");
    QStringList entry = {"device", "journal", "entries", "0"};
    QList<QStringList> objects = {
        {},
        {"ward"},
        {"ward", "stakes", "0"},
        {"ward", "wards", "0"},
        {"ward", "families", "1"},
        {"ward", "families", "1", "members", "0"},
        {"ward", "families", "1", "members", "0", "birthday"},
        {"ward", "teams", "0"},
        {"ward", "addressCoordinates", "0"},
        {"ward", "emergencyResources", "0"},
        {"ward", "eqGroups", "0"},
        {"ward", "eqDistricts", "0"},
        {"ward", "emergencyResponse"},
        {"ward", "emergencyResponse", "contactAttempts", "0"},
        task,
        task + QStringList{"assignment"},
        task + QStringList{"assignment", "notification"},
        task + QStringList{"resolution"},
        {"access"},
        {"access", "users", "0"},
        {"access", "users", "0", "devices", "0"},
        {"access", "invites", "0"},
        {"landed"},
        {"device"},
        {"device", "journal"},
        entry,
        entry + QStringList{"command"},
        entry + QStringList{"command", "description"},
        updateFamily + QStringList{"change"},
        updateFamily + QStringList{"change", "members", "0"},
        updateFamily + QStringList{"change", "members", "0", "name"},
        updateFamily + QStringList{"change", "added", "0"},
        updateFamily + QStringList{"removedCleanup"},
        fieldsOf(json, "setAddressCoordinates") + QStringList{"typed"},
        fieldsOf(json, "updateTeam") + QStringList{"members"},
        fieldsOf(json, "importWardDirectory") + QStringList{"families"},
        fieldsOf(json, "importWardDirectory") + QStringList{"families", "changed", "0"},
        fieldsOf(json, "importMinistering") + QStringList{"collection", "before"},
    };
    for (const CommandType& type : CommandRegistry::types())
    {
        objects.append(fieldsOf(json, type.name));
    }

    for (const QStringList& path : objects)
    {
        QString named = readerPath(json, path);
        QVERIFY2(valueAt(json, path).isObject(), qPrintable(named + " is not an object of the payload"));
        QJsonObject mutated = WardFileEdits::edited(json, path + QStringList{"notInTheFormat"}, "x");
        QVERIFY(putWard(m_document, sealedTo(recipients, mutated)));

        Run run = read(m_document);

        QVERIFY2(run.exitCode == 1, qPrintable(named + ": " + run.problem));
        QVERIFY2(run.problem.contains(named + ".notInTheFormat"), qPrintable(named + ": " + run.problem));
    }
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
    QByteArray file = WardFile::write(m_document, WardFileEdits::version(), m_dataKey, m_payload);
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
    QByteArray file = WardFile::write(m_document, WardFileEdits::version(), m_dataKey, m_payload);
    QJsonObject header = WardFileEdits::headerOf(file);

    QVERIFY(putWard(m_document, WardFileEdits::withHeader(file, QJsonObject{{"format", 2}})));
    QCOMPARE(read(m_document).exitCode, 2);

    // A format below every version, or one that is not an integer, is unreadable rather than newer.
    header["format"] = 0;
    QVERIFY(putWard(m_document, WardFileEdits::withHeader(file, header)));
    Run below = read(m_document);
    QCOMPARE(below.exitCode, 1);
    QVERIFY2(below.problem.contains("header.format"), qPrintable(below.problem));
    header["format"] = 1.5;
    QVERIFY(putWard(m_document, WardFileEdits::withHeader(file, header)));
    Run fractional = read(m_document);
    QCOMPARE(fractional.exitCode, 1);
    QVERIFY2(fractional.problem.contains("header.format"), qPrintable(fractional.problem));
}

void SpecReaderTest::specReaderRefusesAHeaderWithoutAVersion()
{
    QByteArray file = WardFile::write(m_document, WardFileEdits::version(), m_dataKey, m_payload);
    QJsonObject header = WardFileEdits::headerOf(file);
    QVERIFY(putWard(m_document, file));
    Run unmutated = read(m_document);
    QVERIFY2(unmutated.exitCode == 0, qPrintable(unmutated.problem));

    const QList<QPair<QJsonObject, QString>> refused = {
        {WardFileEdits::edited(header, {"number"}, QJsonValue(QJsonValue::Undefined)), "header.number"},
        {WardFileEdits::edited(header, {"number"}, 0), "header.number is below 1"},
        {WardFileEdits::edited(header, {"writer"}, QJsonValue(QJsonValue::Undefined)), "header.writer"},
    };
    for (const QPair<QJsonObject, QString>& each : refused)
    {
        QVERIFY(putWard(m_document, WardFileEdits::withHeader(file, each.first)));

        Run run = read(m_document);

        QCOMPARE(run.exitCode, 1);
        QVERIFY2(run.problem.contains(each.second), qPrintable(run.problem));
    }
}

void SpecReaderTest::specReaderReadsAnUpdateFileTheAppWrites()
{
    QVERIFY(putWard(m_document, WardFile::write(m_document, WardFileEdits::version(), m_dataKey, m_payload)));
    WardPayload version = m_payload;
    version.device = std::nullopt;

    Run run = readUpdate(m_document, UpdateFile::write(m_document, phoneVersion(), m_phone));

    QVERIFY2(run.exitCode == 0, qPrintable(run.problem));
    QJsonParseError error;
    QJsonDocument output = QJsonDocument::fromJson(run.output, &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));
    QCOMPARE(output.object(), version.toJson());
    QVERIFY(!output.object().contains("device"));
}

void SpecReaderTest::specReaderRefusesAnUpdateWhoseSignatureOrWriterFails()
{
    QVERIFY(putWard(m_document, WardFile::write(m_document, WardFileEdits::version(), m_dataKey, m_payload)));
    QByteArray version = phoneVersion();
    Run control = readUpdate(m_document, UpdateFile::write(m_document, version, m_phone));
    QVERIFY2(control.exitCode == 0, qPrintable(control.problem));

    // The phone's version, signed by this device rather than its writer.
    Run resigned = readUpdate(m_document, UpdateFile::write(m_document, version, *m_laptop));
    QCOMPARE(resigned.exitCode, 1);
    QVERIFY2(resigned.problem.contains("signature does not verify"), qPrintable(resigned.problem));

    // A version written and signed by a device the held access list does not name.
    Identity stranger = Identity::generate();
    WardPayload payload = m_payload;
    payload.device = std::nullopt;
    QByteArray strangers = WardFile::write(m_document, VersionRank{5, stranger.fingerprint()}, m_dataKey, payload);
    Run unlisted = readUpdate(m_document, UpdateFile::write(m_document, strangers, stranger));
    QCOMPARE(unlisted.exitCode, 1);
    QVERIFY2(unlisted.problem.contains("writer is not on the access list"), qPrintable(unlisted.problem));
}

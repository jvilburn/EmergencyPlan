#include "StrictReadersTest.h"
#include "AccessCommands.h"
#include "AddressCoordinatesCommands.h"
#include "CommandRegistry.h"
#include "EmergencyAssetCommands.h"
#include "EmergencyCommands.h"
#include "EveryField.h"
#include "FamilyCommands.h"
#include "ImportMinisteringCommand.h"
#include "ImportWardDirectoryCommand.h"
#include "TeamCommands.h"
#include "WardFileEdits.h"

#include <QJsonArray>
#include <QSet>
#include <QTest>

namespace
{

/// One place in a part's JSON: the steps WardFileEdits::edited takes to reach it, its path
/// as the part's reader names it in a problem, and what is there. A key of an object also
/// has the object's path and its number of keys.
struct Place
{
    QStringList steps;
    QString path;
    QJsonValue value;
    QString parentPath;
    qsizetype parentKeys;
};

/// The places in a part's JSON, collected in one walk.
struct Places
{
    QList<Place> objects;  ///< every object, the part itself first
    QList<Place> fields;   ///< every key of every object
    QList<Place> leaves;   ///< every string, number and boolean, in an object or an array
};

/// What reading one part's JSON gave: the problem, or the JSON of what it read, written again.
/// A read that fails writes nothing, since what it read can hold values no writer takes, such
/// as an empty id.
struct Reading
{
    QString problem;
    QJsonObject rewritten;
};

/// One part of a local file's payload, walked with its own reader so a change to it re-reads
/// only that part: its name, its JSON, its reader, and the paths within it whose contents
/// another part's walk covers.
struct Part
{
    QString name;
    QJsonObject json;
    Reading (*read)(const QJsonObject& json);
    QSet<QString> walkedElsewhere;
};

void collect(const Place& place, const QSet<QString>& walkedElsewhere, Places& places)
{
    if (place.value.isObject() && !walkedElsewhere.contains(place.path))
    {
        places.objects.append(place);
        const QJsonObject object = place.value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
        {
            Place field{place.steps + QStringList(it.key()),
                        place.path.isEmpty() ? it.key() : place.path + "." + it.key(),
                        it.value(),
                        place.path,
                        object.size()};
            places.fields.append(field);
            collect(field, walkedElsewhere, places);
        }
    }
    else if (place.value.isArray())
    {
        const QJsonArray array = place.value.toArray();
        for (int i = 0; i < array.size(); ++i)
        {
            collect(Place{place.steps + QStringList(QString::number(i)), QString("%1[%2]").arg(place.path).arg(i), array[i],
                          QString(), 0},
                    walkedElsewhere, places);
        }
    }
    else if (place.value.isString() || place.value.isDouble() || place.value.isBool())
    {
        places.leaves.append(place);
    }
}

Places placesIn(const Part& part)
{
    Places places;
    collect(Place{QStringList(), QString(), part.json, QString(), 0}, part.walkedElsewhere, places);
    return places;
}

/// What reading json as a local file's payload gave: the payload, or the problem.
struct PayloadReading
{
    QString problem;
    WardPayload payload;
};

PayloadReading readPayload(const QJsonObject& json)
{
    JsonReader reader(json);
    WardPayload payload = WardPayload::fromJson(reader, PayloadKind::LocalFile);
    return PayloadReading{reader.problem(), payload};
}

/// payload written again, with each journal entry's command rebuilt and written again
/// rather than kept as it was read, so the command readers are checked too.
QJsonObject rewritten(const WardPayload& payload)
{
    QJsonObject json = payload.toJson();
    const QList<JournalEntry>& entries = payload.device->journal.entries();
    for (int i = 0; i < entries.size(); ++i)
    {
        CommandPtr command = CommandRegistry::fromJson(JsonReader(entries[i].command));
        json = WardFileEdits::edited(json, {"device", "journal", "entries", QString::number(i), "command"},
                                     CommandRegistry::toJson(*command));
    }
    return json;
}

Reading readPayloadPart(const QJsonObject& json)
{
    PayloadReading reading = readPayload(json);
    if (!reading.problem.isEmpty())
    {
        return Reading{reading.problem, QJsonObject()};
    }
    return Reading{QString(), reading.payload.toJson()};
}

Reading readWard(const QJsonObject& json)
{
    JsonReader reader(json);
    Document ward = Document::fromJson(reader);
    if (reader.failed())
    {
        return Reading{reader.problem(), QJsonObject()};
    }
    return Reading{QString(), ward.toJson()};
}

Reading readAccess(const QJsonObject& json)
{
    JsonReader reader(json);
    AccessList access = AccessList::fromJson(reader);
    if (reader.failed())
    {
        return Reading{reader.problem(), QJsonObject()};
    }
    return Reading{QString(), access.toJson()};
}

Reading readDevice(const QJsonObject& json)
{
    JsonReader reader(json);
    DeviceState device = DeviceState::fromJson(reader);
    if (reader.failed())
    {
        return Reading{reader.problem(), QJsonObject()};
    }
    return Reading{QString(), device.toJson()};
}

Reading readCommand(const QJsonObject& json)
{
    JsonReader reader(json);
    CommandPtr command = CommandRegistry::fromJson(reader);
    if (reader.failed())
    {
        return Reading{reader.problem(), QJsonObject()};
    }
    return Reading{QString(), CommandRegistry::toJson(*command)};
}

QString commandPath(int index)
{
    return QString("journal.entries[%1].command").arg(index);
}

/// The parts of payload's JSON json: the payload's own fields and its landed marks, the ward,
/// the access list, this device's state, and each journal entry's command.
QList<Part> partsOf(const QJsonObject& json)
{
    QJsonObject device = json["device"].toObject();
    QJsonArray entries = device["journal"].toObject()["entries"].toArray();
    QSet<QString> commands;
    for (int i = 0; i < entries.size(); ++i)
    {
        commands.insert(commandPath(i));
    }

    QList<Part> parts = {
        Part{"payload", json, &readPayloadPart, {"ward", "access", "device"}},
        Part{"ward", json["ward"].toObject(), &readWard, {}},
        Part{"access", json["access"].toObject(), &readAccess, {}},
        Part{"device", device, &readDevice, commands},
    };
    for (int i = 0; i < entries.size(); ++i)
    {
        parts.append(Part{"device." + commandPath(i), entries[i].toObject()["command"].toObject(), &readCommand, {}});
    }
    return parts;
}

/// The part with that name.
Part partNamed(const QList<Part>& parts, const QString& name)
{
    for (const Part& part : parts)
    {
        if (part.name == name)
        {
            return part;
        }
    }
    qFatal("StrictReadersTest: no part is named %s", qPrintable(name));
    Q_UNREACHABLE_RETURN(Part());
}

/// The index in the journal of EveryField::payload of the first entry of that type.
int journalIndexOf(const QString& type)
{
    const QList<CommandType>& types = CommandRegistry::types();
    for (int i = 0; i < types.size(); ++i)
    {
        if (types[i].name == type)
        {
            return i;
        }
    }
    return -1;
}

/// A local file's payload with every model at its emptiest: a family with only an id, and
/// one whose member has only an id and a name; a team with only an id and a name; an
/// emergency asset with no people; a ministering group of each organization and a district
/// with nobody in them; ward and stake records with no fields; an emergency with no attempts
/// or tasks; this device alone on the access list, with no invites or former keys; no landed
/// marks; and a device state with an empty journal, no held recipients and no flags.
WardPayload emptiestPayload(const Identity& device)
{
    Person bob = Person::createWithId(PersonId::fromString("person-bob"), Name("Smith", "Bob"), std::nullopt, Birthday(),
                                      Phone(), Phone(), QString(), QStringList(), false, QString(), QString());
    Document ward = Document::empty();
    ward.setFamily(Family::createWithId(FamilyId::fromString("family-bare"), Address(), {}));
    ward.setFamily(Family::createWithId(FamilyId::fromString("family-smith"), Address(), {bob}));
    ward.setTeam(Team::create("Setup"));
    ward.setEmergencyAsset(EmergencyAsset::create("Generator", ResponseArea::Recovery));
    ward.setGroup(MinisteringOrg::EldersQuorum, MinisteringGroup::createEQ({}, {}));
    ward.setGroup(MinisteringOrg::ReliefSociety, MinisteringGroup::createRS({}, {}));
    ward.setDistrict(MinisteringOrg::EldersQuorum, MinisteringDistrict::create("District 1", std::nullopt, {}));
    ward.metadata().updateWard(Ward());
    ward.metadata().updateStake(Stake());
    ward.setEmergencyResponse(EmergencyResponse::create("Storm"));
    ward.setAccess(AccessList::founding(UserId::generate(), {"Alice", ""}, "Laptop", device));
    return WardPayload{ward, {}, DeviceState()};
}

/// A command of every registered type with every field it may leave out left out: a family
/// edit that changes only the note, removals with nothing to clean up, coordinates typed
/// where there were none, edits that change only a name, an attempt with only its required
/// fields, a task with no assignment, a task edit that gives it an assignment with no
/// notification and a resolution with no notes, imports onto an empty ward that change
/// nothing, and an invite and a join for a device.
QList<CommandPtr> sparsestCommands()
{
    Family bare = Family::createWithId(FamilyId::fromString("family-bare"), Address(), {});
    Family noted = bare;
    noted.setNote("Gate code is 1234");
    Document ward = Document::empty();
    ward.setFamily(bare);
    Team team = Team::create("Setup");
    Team renamedTeam = team;
    renamedTeam.setName("Cleanup");
    EmergencyAsset asset = EmergencyAsset::create("Generator", ResponseArea::Recovery);
    EmergencyAsset renamedAsset = asset;
    renamedAsset.setName("Portable generator");
    EmergencyResponse storm = EmergencyResponse::create("Storm");
    ResponseTask task = ResponseTask::create(bare, "Flooding", "Water in the basement");
    ResponseTask done = task;
    done.setAssignment(TaskAssignment::toTeam(team));
    done.setResolution(TaskResolution::create(QString()));
    UndoableCommandPtr removeFamily = DeleteFamilyCommand::fromSeen(bare, ward);
    UndoableCommandPtr typed = SetAddressCoordinatesCommand::fromTyped(ward, Address::fromLines({"12 Elm St", "Provo, UT"}),
                                                                        Coordinates(40.2338, -111.6585));
    UndoableCommandPtr end = EndEmergencyCommand::fromReported(storm);
    UndoableCommandPtr addAttempt = AddContactAttemptCommand::fromNew(
        ContactAttempt::create(bare, std::nullopt, std::nullopt, std::nullopt, QString()));
    WardDirectoryImportResult directory;
    directory.success = true;
    MinisteringImportResult ministering;
    ministering.success = true;
    ministering.pdfDate = QDate(2026, 9, 1);
    Identity laptop = Identity::generate();
    Identity phone = Identity::generate();
    Identity invited = Identity::generate();
    AccessUser alice(UserId::generate(), {"Alice", ""}, AccessRole::Admin,
                     {AccessDevice("Laptop", laptop.publicKey(), laptop.verifyKey()),
                      AccessDevice("Phone", phone.publicKey(), phone.verifyKey())});
    AccessList access({alice}, {}, {});
    AccessInvite deviceInvite = AccessInvite::forDevice(alice.id(), invited.publicKey(), invited.verifyKey());

    return {
        UpdateFamilyCommand::fromEdit(bare, noted, ward),
        removeFamily,
        removeFamily->inverse(),
        typed,
        typed->inverse(),
        AddTeamCommand::fromNew(team, ward),
        UpdateTeamCommand::fromEdit(team, renamedTeam, ward),
        DeleteTeamCommand::fromSeen(team, ward),
        AddEmergencyAssetCommand::fromNew(asset, ward),
        UpdateEmergencyAssetCommand::fromEdit(asset, renamedAsset, ward),
        DeleteEmergencyAssetCommand::fromSeen(asset, ward),
        StartEmergencyCommand::fromNew(storm),
        end,
        end->inverse(),
        addAttempt,
        addAttempt->inverse(),
        AddTaskCommand::fromNew(task),
        UpdateTaskCommand::fromEdit(task, done),
        DeleteTaskCommand::fromSeen(task),
        AddTaskCategoryCommand::fromNew("Power lines"),
        ImportWardDirectoryCommand::fromImport(Document::empty(), directory, "Tanglewood.pdf"),
        ImportMinisteringCommand::fromImport(Document::empty(), ministering, "Tanglewood EQ.pdf"),
        InviteCommand::create(laptop.fingerprint(), access, std::nullopt, invited),
        RemoveUserCommand::create(laptop.fingerprint(), alice),
        RemoveDeviceCommand::create(laptop.fingerprint(), alice.devices()[1]),
        WithdrawInviteCommand::create(laptop.fingerprint(), deviceInvite, access),
        SetRoleCommand::create(laptop.fingerprint(), alice, AccessRole::Member),
        SetUserDetailsCommand::create(laptop.fingerprint(), access, {"Alice Jones", ""}),
        RenameDeviceCommand::create(laptop.fingerprint(), access, "Work laptop"),
        JoinCommand::create(deviceInvite, AccessDevice("Tablet", phone.publicKey(), phone.verifyKey()),
                            UserDetails(), "Tanglewood Ward"),
    };
}

}  // namespace

StrictReadersTest::StrictReadersTest(QObject* parent)
    : QObject(parent)
{
}

void StrictReadersTest::initTestCase()
{
    m_payload = EveryField::payload(Identity::generate(), Identity::generate());
    m_json = m_payload.toJson();
}

void StrictReadersTest::everyFieldRoundTrips()
{
    PayloadReading reading = readPayload(m_json);

    QCOMPARE(reading.problem, QString());
    QVERIFY(reading.payload == m_payload);
    QVERIFY(rewritten(reading.payload) == m_json);
    for (const Part& part : partsOf(m_json))
    {
        Reading partReading = part.read(part.json);
        QVERIFY2(partReading.problem.isEmpty(), qPrintable(part.name + ": " + partReading.problem));
        QVERIFY2(partReading.rewritten == part.json, qPrintable(part.name));
    }
}

void StrictReadersTest::emptiestRoundTrips()
{
    WardPayload payload = emptiestPayload(Identity::generate());
    QJsonObject json = payload.toJson();
    QJsonObject ward = json["ward"].toObject();
    QCOMPARE(ward["wards"].toArray(), QJsonArray({QJsonObject()}));
    QCOMPARE(ward["stakes"].toArray(), QJsonArray({QJsonObject()}));
    QCOMPARE(ward["families"].toArray()[0].toObject().keys(), QStringList({"id"}));
    QCOMPARE(ward["families"].toArray()[1].toObject()["members"].toArray()[0].toObject().keys(),
             QStringList({"id", "name"}));
    QCOMPARE(json["device"].toObject().keys(), QStringList({"heldRecipients", "journal"}));

    PayloadReading reading = readPayload(json);

    QCOMPARE(reading.problem, QString());
    QVERIFY(reading.payload == payload);
    QVERIFY(rewritten(reading.payload) == json);

    QSet<QString> types;
    for (const CommandPtr& command : sparsestCommands())
    {
        QJsonObject commandJson = CommandRegistry::toJson(*command);
        Reading commandReading = readCommand(commandJson);
        QVERIFY2(commandReading.problem.isEmpty(), qPrintable(command->type() + ": " + commandReading.problem));
        QVERIFY2(commandReading.rewritten == commandJson, qPrintable(command->type()));
        types.insert(command->type());
    }
    QCOMPARE(types.size(), CommandRegistry::types().size());
}

void StrictReadersTest::everyUnexpectedFieldIsRefusedAtItsPath()
{
    QStringList mismatches;
    qsizetype objects = 0;
    for (const Part& part : partsOf(m_json))
    {
        Places places = placesIn(part);
        objects += places.objects.size();
        for (const Place& object : places.objects)
        {
            QJsonObject json = WardFileEdits::edited(part.json, object.steps + QStringList("unexpected"), QJsonValue("hello"));
            QString expected = object.path.isEmpty() ? QString("unexpected: is not expected")
                             : part.name == "payload" && object.path == "landed"
                                 ? QString("landed.unexpected: is not a fingerprint")
                                 : object.path + ".unexpected: is not expected";
            QString problem = part.read(json).problem;
            if (problem != expected)
            {
                mismatches.append(QString("%1: %2 gave \"%3\"").arg(part.name, object.path, problem));
            }
        }
    }
    QVERIFY2(mismatches.isEmpty(), qPrintable(mismatches.mid(0, 10).join("; ")));
    QVERIFY(objects > 200);
}

void StrictReadersTest::everyRemovedFieldIsRefusedOrReadsAsEmpty()
{
    QStringList mismatches;
    qsizetype fields = 0;
    int missing = 0;
    int leftEmpty = 0;
    int readAsEmpty = 0;
    for (const Part& part : partsOf(m_json))
    {
        Places places = placesIn(part);
        fields += places.fields.size();
        for (const Place& field : places.fields)
        {
            QJsonObject json = WardFileEdits::edited(part.json, field.steps, QJsonValue(QJsonValue::Undefined));
            Reading reading = part.read(json);
            if (reading.problem == field.path + ": missing")
            {
                ++missing;
            }
            else if (field.parentKeys == 1 && reading.problem == field.parentPath + ": is written out empty")
            {
                ++leftEmpty;
            }
            else if (reading.problem.isEmpty() && reading.rewritten == json)
            {
                ++readAsEmpty;
            }
            else
            {
                mismatches.append(QString("%1: %2 gave \"%3\"").arg(part.name, field.path, reading.problem));
            }
        }
    }
    QVERIFY2(mismatches.isEmpty(), qPrintable(mismatches.mid(0, 10).join("; ")));
    QVERIFY(fields > 700);
    QVERIFY(missing > 400);
    QVERIFY(leftEmpty > 0);
    QVERIFY(readAsEmpty > 250);
}

void StrictReadersTest::everyLeafOfTheWrongTypeIsRefused()
{
    QStringList mismatches;
    qsizetype leaves = 0;
    for (const Part& part : partsOf(m_json))
    {
        Places places = placesIn(part);
        leaves += places.leaves.size();
        for (const Place& leaf : places.leaves)
        {
            QJsonObject json = WardFileEdits::edited(part.json, leaf.steps, QJsonObject());
            QString problem = part.read(json).problem;
            if (!problem.startsWith(leaf.path + ": "))
            {
                mismatches.append(QString("%1: %2 gave \"%3\"").arg(part.name, leaf.path, problem));
            }
        }
    }
    QVERIFY2(mismatches.isEmpty(), qPrintable(mismatches.mid(0, 10).join("; ")));
    QVERIFY(leaves > 600);
}

void StrictReadersTest::addressLinesAreReadAsWritten_data()
{
    QTest::addColumn<QString>("part");
    QTest::addColumn<QStringList>("steps");
    QTest::addColumn<QStringList>("lines");

    QString updateFamily = "device." + commandPath(journalIndexOf(UpdateFamilyCommand::TypeName));
    QStringList familyAddress = {"families", "0", "address"};
    QStringList changedAddress = {"fields", "change", "address", "after"};
    QStringList spaced = {"  12 Elm St ", "Provo, UT"};
    QStringList withEmptyLine = {"12 Elm St", "", "Provo, UT"};

    QTest::newRow("family, line with surrounding spaces") << "ward" << familyAddress << spaced;
    QTest::newRow("family, empty line") << "ward" << familyAddress << withEmptyLine;
    QTest::newRow("address change, line with surrounding spaces") << updateFamily << changedAddress << spaced;
    QTest::newRow("address change, empty line") << updateFamily << changedAddress << withEmptyLine;
}

void StrictReadersTest::addressLinesAreReadAsWritten()
{
    QFETCH(QString, part);
    QFETCH(QStringList, steps);
    QFETCH(QStringList, lines);
    Part target = partNamed(partsOf(m_json), part);
    QVERIFY(target.read(target.json).problem.isEmpty());

    QJsonObject json = WardFileEdits::edited(target.json, steps, QJsonArray::fromStringList(lines));
    Reading reading = target.read(json);

    QCOMPARE(reading.problem, QString());
    QCOMPARE(reading.rewritten, json);
}

void StrictReadersTest::unknownWordsAndUnparseableDatesAreRefused_data()
{
    QTest::addColumn<QStringList>("steps");
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("problem");

    QString importMinistering = QString::number(journalIndexOf(ImportMinisteringCommand::TypeName));

    QTest::newRow("gender") << QStringList({"ward", "families", "0", "members", "0", "gender"}) << "unknown"
                            << "ward.families[0].members[0].gender: is not one of male, female";
    QTest::newRow("response area") << QStringList({"ward", "emergencyResources", "0", "responseArea"}) << "Nowhere"
                                   << "ward.emergencyResources[0].responseArea: is not one of med, rec, com";
    QTest::newRow("contact method") << QStringList({"ward", "emergencyResponse", "contactAttempts", "1", "method"}) << "fax"
                                    << "ward.emergencyResponse.contactAttempts[1].method: is not one of phone, text, "
                                       "email, visit, other";
    QTest::newRow("ministering organization")
        << QStringList({"device", "journal", "entries", importMinistering, "command", "fields", "org"}) << "highPriests"
        << QString("device.journal.entries[%1].command.fields.org: is not one of eldersQuorum, reliefSociety")
               .arg(importMinistering);
    QTest::newRow("directory PDF date") << QStringList({"ward", "wardDirectoryPdfDate"}) << "2024-13-45"
                                        << "ward.wardDirectoryPdfDate: is not a date";
    QTest::newRow("start time") << QStringList({"ward", "emergencyResponse", "startedAt"}) << "yesterday"
                                << "ward.emergencyResponse.startedAt: is not a time";
}

void StrictReadersTest::unknownWordsAndUnparseableDatesAreRefused()
{
    QFETCH(QStringList, steps);
    QFETCH(QString, value);
    QFETCH(QString, problem);
    QVERIFY(readPayload(m_json).problem.isEmpty());

    QCOMPARE(readPayload(WardFileEdits::edited(m_json, steps, value)).problem, problem);
}

void StrictReadersTest::emptyIdIsRefused_data()
{
    QTest::addColumn<QStringList>("steps");
    QTest::addColumn<QString>("problem");

    QTest::newRow("family id") << QStringList({"ward", "families", "0", "id"}) << "ward.families[0].id: is an empty id";
    QTest::newRow("team member") << QStringList({"ward", "teams", "0", "memberIds", "0"})
                                 << "ward.teams[0].memberIds[0]: is an empty id";
}

void StrictReadersTest::emptyIdIsRefused()
{
    QFETCH(QStringList, steps);
    QFETCH(QString, problem);
    QVERIFY(readPayload(m_json).problem.isEmpty());

    QCOMPARE(readPayload(WardFileEdits::edited(m_json, steps, QString())).problem, problem);
}

#include "Payload.h"
#include "Fields.h"

#include <optional>
#include <string>

namespace
{

constexpr std::size_t IdBytes = 16;
constexpr std::size_t KeyBytes = 32;
constexpr std::size_t FingerprintBytes = 16;

const std::vector<std::string> Roles = {"admin", "member"};
const std::vector<std::string> InviteKinds = {"device", "user"};
const std::vector<std::string> Genders = {"male", "female"};
const std::vector<std::string> ContactMethods = {"phone", "text", "email", "visit", "other"};
const std::vector<std::string> Outcomes = {"ok", "unableToReach"};
const std::vector<std::string> ResponseAreas = {"med", "rec", "com"};
const std::vector<std::string> Organizations = {"eldersQuorum", "reliefSociety"};

using ObjectRead = void (*)(Fields fields);

/// Reads each of objects with read.
void eachOf(const std::vector<Fields>& objects, ObjectRead read)
{
    for (const Fields& fields : objects)
    {
        read(fields);
    }
}

// ============================================================================
// ward
// ============================================================================

void readStake(Fields fields)
{
    fields.optionalText("name");
    fields.optionalText("unitNumber");
    fields.optionalText("stakeCenterAddress");
    fields.optionalNumber("stakeCenterLat");
    fields.optionalNumber("stakeCenterLng");
    fields.optionalTexts("wardUnitNumbers");
    fields.done();
}

void readWardRecord(Fields fields)
{
    fields.optionalText("name");
    fields.optionalText("unitNumber");
    fields.optionalText("stakeUnitNumber");
    fields.optionalText("chapelAddress");
    fields.optionalNumber("chapelLat");
    fields.optionalNumber("chapelLng");
    fields.optionalText("chapelPhone");
    fields.optionalText("meetingTime");
    fields.done();
}

void readBirthday(Fields fields)
{
    fields.optionalInteger("year");
    fields.optionalInteger("month");
    fields.optionalInteger("day");
    fields.done();
}

void readPerson(Fields fields)
{
    fields.id("id");
    fields.text("name");
    fields.optionalText("phone");
    fields.optionalText("altPhone");
    fields.optionalText("email");
    fields.optionalWord("gender", Genders);
    std::optional<Fields> birthday = fields.optionalObject("birthday");
    if (birthday.has_value())
    {
        readBirthday(*birthday);
    }
    fields.optionalTexts("callings");
    fields.optionalText("stakeUnitNumber");
    fields.optionalText("wardUnitNumber");
    fields.optionalTrue("isParent");
    fields.optionalText("specialNeedNote");
    fields.optionalText("note");
    fields.done();
}

void readFamily(Fields fields)
{
    fields.id("id");
    fields.optionalTexts("address");
    eachOf(fields.optionalObjects("members"), readPerson);
    fields.optionalText("note");
    fields.done();
}

void readTeam(Fields fields)
{
    fields.id("id");
    fields.text("name");
    fields.optionalId("leaderId");
    fields.optionalIds("memberIds");
    fields.done();
}

void readAddressCoordinates(Fields fields)
{
    fields.text("address");
    fields.number("latitude");
    fields.number("longitude");
    fields.done();
}

void readEmergencyAsset(Fields fields)
{
    fields.id("id");
    fields.text("name");
    fields.word("responseArea", ResponseAreas);
    fields.ids("personIds");
    fields.done();
}

void readGroup(Fields fields)
{
    fields.id("id");
    fields.boolean("isRSFormat");
    fields.optionalIds("ministerIds");
    fields.optionalIds("familyIds");
    fields.optionalIds("ministeredPersonIds");
    fields.optionalId("presidencyMemberId");
    fields.done();
}

void readDistrict(Fields fields)
{
    fields.id("id");
    fields.text("name");
    fields.optionalId("presidencyMemberId");
    fields.optionalIds("groupIds");
    fields.done();
}

void readNotification(Fields fields)
{
    fields.word("method", ContactMethods);
    fields.time("timestamp");
    fields.optionalText("notes");
    fields.done();
}

void readAssignment(Fields fields)
{
    fields.optionalId("teamId");
    fields.optionalId("personId");
    fields.text("assigneeName");
    std::optional<Fields> notification = fields.optionalObject("notification");
    if (notification.has_value())
    {
        readNotification(*notification);
    }
    fields.done();
}

void readResolution(Fields fields)
{
    fields.optionalText("notes");
    fields.time("resolvedAt");
    fields.done();
}

void readContactAttempt(Fields fields)
{
    fields.id("id");
    fields.id("familyId");
    fields.text("familyName");
    fields.optionalWord("method", ContactMethods);
    fields.optionalId("who");
    fields.optionalText("whoName");
    fields.optionalWord("outcome", Outcomes);
    fields.time("timestamp");
    fields.optionalText("notes");
    fields.done();
}

void readTask(Fields fields)
{
    fields.id("id");
    fields.id("familyId");
    fields.text("familyName");
    fields.text("category");
    fields.text("description");
    fields.time("createdAt");
    std::optional<Fields> assignment = fields.optionalObject("assignment");
    if (assignment.has_value())
    {
        readAssignment(*assignment);
    }
    std::optional<Fields> resolution = fields.optionalObject("resolution");
    if (resolution.has_value())
    {
        readResolution(*resolution);
    }
    fields.done();
}

void readEmergency(Fields fields)
{
    fields.text("name");
    fields.time("startedAt");
    fields.texts("taskCategories");
    eachOf(fields.optionalObjects("contactAttempts"), readContactAttempt);
    eachOf(fields.optionalObjects("tasks"), readTask);
    fields.done();
}

void readWard(Fields fields)
{
    eachOf(fields.optionalObjects("stakes"), readStake);
    eachOf(fields.optionalObjects("wards"), readWardRecord);
    eachOf(fields.optionalObjects("families"), readFamily);
    eachOf(fields.optionalObjects("teams"), readTeam);
    eachOf(fields.optionalObjects("addressCoordinates"), readAddressCoordinates);
    eachOf(fields.optionalObjects("emergencyResources"), readEmergencyAsset);
    std::optional<Fields> emergency = fields.optionalObject("emergencyResponse");
    if (emergency.has_value())
    {
        readEmergency(*emergency);
    }
    eachOf(fields.optionalObjects("eqDistricts"), readDistrict);
    eachOf(fields.optionalObjects("eqGroups"), readGroup);
    eachOf(fields.optionalObjects("rsDistricts"), readDistrict);
    eachOf(fields.optionalObjects("rsGroups"), readGroup);
    fields.optionalDate("wardDirectoryPdfDate");
    fields.optionalDate("ministeringPdfDate");
    fields.done();
}

// ============================================================================
// Commands: the shapes
// ============================================================================

/// Reads the value under key of owner, of one value type.
using ValueRead = void (*)(Fields& owner, const std::string& key);

void textValue(Fields& owner, const std::string& key)
{
    owner.text(key);
}

void textsValue(Fields& owner, const std::string& key)
{
    owner.texts(key);
}

void booleanValue(Fields& owner, const std::string& key)
{
    owner.boolean(key);
}

void idOrNullValue(Fields& owner, const std::string& key)
{
    if (!owner.takeNull(key))
    {
        owner.id(key);
    }
}

void genderOrNullValue(Fields& owner, const std::string& key)
{
    if (!owner.takeNull(key))
    {
        owner.word(key, Genders);
    }
}

void dateOrNullValue(Fields& owner, const std::string& key)
{
    if (!owner.takeNull(key))
    {
        owner.date(key);
    }
}

/// A birthday object, {} when no part is known.
void birthdayValue(Fields& owner, const std::string& key)
{
    readBirthday(owner.object(key));
}

void assignmentOrNullValue(Fields& owner, const std::string& key)
{
    if (!owner.takeNull(key))
    {
        readAssignment(owner.object(key));
    }
}

void resolutionOrNullValue(Fields& owner, const std::string& key)
{
    if (!owner.takeNull(key))
    {
        readResolution(owner.object(key));
    }
}

void readMinisteringCollection(Fields fields)
{
    eachOf(fields.objects("districts"), readDistrict);
    eachOf(fields.objects("groups"), readGroup);
    fields.done();
}

void ministeringCollectionValue(Fields& owner, const std::string& key)
{
    readMinisteringCollection(owner.object(key));
}

/// A change: {before, after}, each a value read by read.
void readChange(Fields change, ValueRead read)
{
    read(change, "before");
    read(change, "after");
    change.done();
}

/// A change under key of owner, omitted when unchanged.
void readOptionalChange(Fields& owner, const std::string& key, ValueRead read)
{
    std::optional<Fields> change = owner.optionalObject(key);
    if (change.has_value())
    {
        readChange(*change, read);
    }
}

void readPeopleChange(Fields fields)
{
    fields.ids("added");
    fields.ids("removed");
    fields.done();
}

void readCleanup(Fields fields)
{
    fields.optionalIds("people");
    fields.optionalIds("families");
    fields.optionalIds("teams");
    eachOf(fields.optionalObjects("heldTeams"), readTeam);
    eachOf(fields.optionalObjects("heldAssets"), readEmergencyAsset);
    eachOf(fields.optionalObjects("heldGroups"), readGroup);
    eachOf(fields.optionalObjects("heldDistricts"), readDistrict);
    eachOf(fields.optionalObjects("heldTasks"), readTask);
    fields.done();
}

/// A cleanup under key of owner, omitted when it holds nothing.
void readOptionalCleanup(Fields& owner, const std::string& key)
{
    std::optional<Fields> cleanup = owner.optionalObject(key);
    if (cleanup.has_value())
    {
        readCleanup(*cleanup);
    }
}

void readCoordinates(Fields fields)
{
    fields.number("latitude");
    fields.number("longitude");
    fields.done();
}

void readMemberChange(Fields fields)
{
    fields.id("id");
    readOptionalChange(fields, "name", textValue);
    readOptionalChange(fields, "gender", genderOrNullValue);
    readOptionalChange(fields, "phone", textValue);
    readOptionalChange(fields, "altPhone", textValue);
    readOptionalChange(fields, "email", textValue);
    readOptionalChange(fields, "birthday", birthdayValue);
    readOptionalChange(fields, "callings", textsValue);
    readOptionalChange(fields, "isParent", booleanValue);
    readOptionalChange(fields, "wardUnitNumber", textValue);
    readOptionalChange(fields, "stakeUnitNumber", textValue);
    readOptionalChange(fields, "specialNeedNote", textValue);
    readOptionalChange(fields, "note", textValue);
    fields.done();
}

void readPlacedMember(Fields fields)
{
    readPerson(fields.object("person"));
    fields.integer("index");
    fields.done();
}

/// The fields of a family change, which a family collection change's changed families hold
/// beside their id.
void readFamilyChangeFields(Fields& fields)
{
    readOptionalChange(fields, "address", textsValue);
    readOptionalChange(fields, "note", textValue);
    eachOf(fields.optionalObjects("members"), readMemberChange);
    eachOf(fields.optionalObjects("added"), readPlacedMember);
    eachOf(fields.optionalObjects("removed"), readPlacedMember);
}

void readFamilyChange(Fields fields)
{
    readFamilyChangeFields(fields);
    fields.done();
}

void readChangedFamily(Fields fields)
{
    fields.id("id");
    readFamilyChangeFields(fields);
    fields.done();
}

void readFamilyCollectionChange(Fields fields)
{
    eachOf(fields.optionalObjects("added"), readFamily);
    eachOf(fields.optionalObjects("changed"), readChangedFamily);
    eachOf(fields.optionalObjects("removed"), readFamily);
    readOptionalCleanup(fields, "addedCleanup");
    readOptionalCleanup(fields, "removedCleanup");
    fields.done();
}

// ============================================================================
// Commands: each type's fields
// ============================================================================

void readUpdateFamilyFields(Fields fields)
{
    fields.id("familyId");
    readFamilyChange(fields.object("change"));
    readOptionalCleanup(fields, "addedCleanup");
    readOptionalCleanup(fields, "removedCleanup");
    fields.done();
}

void readFamilyRemovalFields(Fields fields)
{
    readFamily(fields.object("family"));
    readOptionalCleanup(fields, "cleanup");
    fields.done();
}

void readAddressCoordinatesFields(Fields fields)
{
    fields.text("address");
    std::optional<Fields> before = fields.optionalObject("before");
    if (before.has_value())
    {
        readCoordinates(*before);
    }
    readCoordinates(fields.object("typed"));
    fields.done();
}

void readTeamFields(Fields fields)
{
    readTeam(fields.object("team"));
    readOptionalCleanup(fields, "cleanup");
    fields.done();
}

void readUpdateTeamFields(Fields fields)
{
    fields.id("teamId");
    readOptionalChange(fields, "name", textValue);
    readOptionalChange(fields, "leader", idOrNullValue);
    readPeopleChange(fields.object("members"));
    fields.done();
}

void readEmergencyAssetFields(Fields fields)
{
    readEmergencyAsset(fields.object("asset"));
    fields.done();
}

void readUpdateEmergencyAssetFields(Fields fields)
{
    fields.id("assetId");
    readOptionalChange(fields, "name", textValue);
    readPeopleChange(fields.object("people"));
    fields.done();
}

void readEmergencyFields(Fields fields)
{
    readEmergency(fields.object("response"));
    fields.done();
}

void readContactAttemptFields(Fields fields)
{
    readContactAttempt(fields.object("attempt"));
    fields.done();
}

void readTaskFields(Fields fields)
{
    readTask(fields.object("task"));
    fields.done();
}

void readUpdateTaskFields(Fields fields)
{
    fields.id("familyId");
    fields.id("taskId");
    readOptionalChange(fields, "category", textValue);
    readOptionalChange(fields, "taskDescription", textValue);
    readOptionalChange(fields, "assignment", assignmentOrNullValue);
    readOptionalChange(fields, "resolution", resolutionOrNullValue);
    fields.done();
}

void readAddTaskCategoryFields(Fields fields)
{
    fields.text("category");
    fields.done();
}

void readImportWardDirectoryFields(Fields fields)
{
    readFamilyCollectionChange(fields.object("families"));
    readOptionalChange(fields, "pdfDate", dateOrNullValue);
    eachOf(fields.optionalObjects("coordinates"), readAddressCoordinates);
    readWardRecord(fields.object("ward"));
    dateOrNullValue(fields, "ministeringPdfDate");
    fields.done();
}

void readImportMinisteringFields(Fields fields)
{
    fields.word("org", Organizations);
    readChange(fields.object("collection"), ministeringCollectionValue);
    readFamilyCollectionChange(fields.object("families"));
    readOptionalChange(fields, "pdfDate", dateOrNullValue);
    dateOrNullValue(fields, "wardDirectoryPdfDate");
    fields.done();
}

struct CommandType
{
    const char* type;
    ObjectRead readFields;
};

const CommandType CommandTypes[] = {
    {"updateFamily", readUpdateFamilyFields},
    {"deleteFamily", readFamilyRemovalFields},
    {"restoreFamily", readFamilyRemovalFields},
    {"setAddressCoordinates", readAddressCoordinatesFields},
    {"revertAddressCoordinates", readAddressCoordinatesFields},
    {"addTeam", readTeamFields},
    {"deleteTeam", readTeamFields},
    {"updateTeam", readUpdateTeamFields},
    {"addEmergencyAsset", readEmergencyAssetFields},
    {"deleteEmergencyAsset", readEmergencyAssetFields},
    {"updateEmergencyAsset", readUpdateEmergencyAssetFields},
    {"startEmergency", readEmergencyFields},
    {"endEmergency", readEmergencyFields},
    {"restoreEmergency", readEmergencyFields},
    {"addContactAttempt", readContactAttemptFields},
    {"deleteContactAttempt", readContactAttemptFields},
    {"addTask", readTaskFields},
    {"deleteTask", readTaskFields},
    {"updateTask", readUpdateTaskFields},
    {"addTaskCategory", readAddTaskCategoryFields},
    {"importWardDirectory", readImportWardDirectoryFields},
    {"importMinistering", readImportMinisteringFields},
};

// ============================================================================
// device
// ============================================================================

void readDescription(Fields fields)
{
    fields.text("heading");
    fields.optionalTexts("lines");
    fields.done();
}

void readCommand(Fields fields)
{
    std::string type = fields.text("type");
    readDescription(fields.object("description"));
    for (const CommandType& commandType : CommandTypes)
    {
        if (type == commandType.type)
        {
            commandType.readFields(fields.object("fields"));
            fields.done();
            return;
        }
    }
    throw Refusal(fields.pathOf("type") + " is not a command type");
}

void readEntry(Fields fields)
{
    fields.integer("sequence");
    readCommand(fields.object("command"));
    fields.done();
}

void readJournal(Fields fields)
{
    fields.integer("lastSequence");
    eachOf(fields.optionalObjects("entries"), readEntry);
    fields.done();
}

// ============================================================================
// access and landed
// ============================================================================

PublicKeys readPublicKeys(Fields& fields)
{
    PublicKeys keys;
    keys.sealingPublic = fields.bytes("publicKey", KeyBytes);
    keys.verifyKey = fields.bytes("verifyKey", KeyBytes);
    return keys;
}

void readUser(Fields fields, std::vector<PublicKeys>* keys)
{
    fields.bytes("id", IdBytes);
    fields.text("name");
    fields.word("role", Roles);
    for (Fields& device : fields.objects("devices"))
    {
        device.text("name");
        keys->push_back(readPublicKeys(device));
        device.done();
    }
    fields.done();
}

void readInvite(Fields fields, std::vector<PublicKeys>* keys)
{
    std::string kind = fields.word("for", InviteKinds);
    fields.bytes("maker", IdBytes);
    if (kind == "user")
    {
        fields.word("role", Roles);
        fields.text("label");
    }
    keys->push_back(readPublicKeys(fields));
    fields.done();
}

/// An access object, whose devices' and open invites' keys it adds to keys.
void readAccess(Fields fields, std::vector<PublicKeys>* keys)
{
    for (const Fields& user : fields.objects("users"))
    {
        readUser(user, keys);
    }
    for (const Fields& invite : fields.optionalObjects("invites"))
    {
        readInvite(invite, keys);
    }
    fields.optionalBytesList("formerKeys", KeyBytes);
    fields.done();
}

void readLanded(Fields fields)
{
    for (const std::string& key : fields.keys())
    {
        if (!decode(key, FingerprintBytes).has_value())
        {
            throw Refusal(fields.pathOf(key) + " is not keyed by a fingerprint");
        }
        fields.integer(key);
    }
    fields.done();
}

void readDeviceState(Fields fields)
{
    readJournal(fields.object("journal"));
    fields.optionalInteger("baseVersion");
    std::optional<Fields> dismissedAccess = fields.optionalObject("dismissedAccess");
    if (dismissedAccess.has_value())
    {
        std::vector<PublicKeys> dismissedKeys;
        readAccess(*dismissedAccess, &dismissedKeys);
    }
    fields.optionalTrue("singleHolderNoticeDismissed");
    fields.optionalTrue("singleAdminNoticeDismissed");
    fields.optionalTrue("noLongerShared");
    fields.optionalBytesList("inviteSeeds", KeyBytes);
    fields.done();
}

}  // namespace

std::vector<PublicKeys> readLocalPayload(const nlohmann::json& payload)
{
    Fields fields(payload, "payload");
    readWard(fields.object("ward"));
    std::vector<PublicKeys> keys;
    readAccess(fields.object("access"), &keys);
    readLanded(fields.object("landed"));
    readDeviceState(fields.object("device"));
    fields.done();
    return keys;
}

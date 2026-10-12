#include "ResponseMergeTest.h"

#include "Address.h"
#include "DocumentCrypto.h"
#include "Family.h"
#include "ResponseMerge.h"
#include "SealingSet.h"

#include <QJsonArray>
#include <QtTest>

namespace
{

/// A storm with two families, Adams and Baker, neither contacted yet.
struct Storm
{
    FamilyId adams;
    FamilyId baker;
    EmergencyResponse response;
};

Storm storm()
{
    Storm s;
    s.adams = FamilyId::generate();
    s.baker = FamilyId::generate();
    s.response = EmergencyResponse::create("Storm");
    s.response.addFamilyRecord(FamilyResponseRecord::create(s.adams, "Adams", "1 Oak St"));
    s.response.addFamilyRecord(FamilyResponseRecord::create(s.baker, "Baker", "2 Elm St"));
    return s;
}

FamilyResponseRecord& recordOf(EmergencyResponse& response, const FamilyId& family)
{
    return *response.mutableRecord(family);
}

const FamilyResponseRecord& recordOf(const EmergencyResponse& response, const FamilyId& family)
{
    return *response.findRecord(family);
}

ContactAttempt attempt(const QString& notes)
{
    return ContactAttempt::create(ContactMethod::Phone, PersonId::generate(), notes);
}

bool hasAttempt(const FamilyResponseRecord& record, const ContactAttemptId& id)
{
    for (const ContactAttempt& candidate : record.contactAttempts())
    {
        if (candidate.id() == id)
        {
            return true;
        }
    }
    return false;
}

const ResponseTask* taskIn(const FamilyResponseRecord& record, const TaskId& id)
{
    for (const ResponseTask& task : record.tasks())
    {
        if (task.id() == id)
        {
            return &task;
        }
    }
    return nullptr;
}

/// The storm with one task on the Adams record.
Storm stormWithTask(TaskId& taskId)
{
    Storm s = storm();
    const ResponseTask task = ResponseTask::create("Tree removal", "Oak across the drive");
    taskId = task.id();
    recordOf(s.response, s.adams).addTask(task);
    return s;
}

EmergencyResponse merged(const EmergencyResponse& base,
                         const EmergencyResponse& mine,
                         const EmergencyResponse& theirs)
{
    const std::optional<EmergencyResponse> result = ResponseMerge::merge(base, mine, theirs);
    if (!result.has_value())
    {
        qFatal("merge returned no response where every side had one");
    }
    return result.value();
}

Family familyAt(const FamilyId& id, double latitude)
{
    return Family::createWithId(id, latitude, -111.0, Address(), {});
}

DeviceKeys newKeys()
{
    return DeviceKeys{DocumentCrypto::generateKeypair().publicKey, DocumentCrypto::generateSigningKeypair().verifyKey};
}

MemberId newMember()
{
    MemberId member;
    member.bytes = DocumentCrypto::randomBytes(MemberId::Size);
    return member;
}

SealingSet setOfOne()
{
    return SealingSet::of(newMember(), "Alice", NamedDevice{"Laptop", newKeys()});
}

/// The set with an open invite for Bob added by its one person.
SealingSet withInvite(const SealingSet& set)
{
    SealingSet invited = set;
    Invite invite;
    invite.purpose = Invite::For::User;
    invite.person = set.people().first().id;
    invite.label = "for Bob";
    invite.keys = newKeys();
    invited.addInvite(invite);
    return invited;
}

/// The set with Dave joined as a member through an invite of its one person.
SealingSet withDave(const SealingSet& set)
{
    SealingSet joined = withInvite(set);
    joined.join(joined.invites().first().fingerprint(), NamedDevice{"Tablet", newKeys()}, newMember(), "Dave");
    return joined;
}

MemberId idOf(const SealingSet& set, const QString& name)
{
    for (const Holder& person : set.people())
    {
        if (person.name == name)
        {
            return person.id;
        }
    }
    return MemberId();
}

/// A document with one family and the storm under way.
Document documentWith(const FamilyId& family, const Storm& s)
{
    Document document = Document::empty();
    document.addFamily(familyAt(family, 40.0));
    document.setEmergencyResponse(s.response);
    document.setSealingSet(setOfOne());
    return document;
}

} // namespace

// --- Records and attempts ----------------------------------------------------

void ResponseMergeTest::twoRespondersOfflineRecordsBothSurvive()
{
    const Storm s = storm();

    EmergencyResponse mine = s.response;
    const ContactAttempt mineCalled = attempt("Adams are fine");
    recordOf(mine, s.adams).addContactAttempt(mineCalled);
    recordOf(mine, s.adams).setContactStatus(ContactStatus::OK);

    EmergencyResponse theirs = s.response;
    const ContactAttempt theirsCalled = attempt("No answer at the Bakers");
    recordOf(theirs, s.baker).addContactAttempt(theirsCalled);
    recordOf(theirs, s.baker).setContactStatus(ContactStatus::UnableToReach);

    const EmergencyResponse result = merged(s.response, mine, theirs);
    QVERIFY(hasAttempt(recordOf(result, s.adams), mineCalled.id()));
    QVERIFY(hasAttempt(recordOf(result, s.baker), theirsCalled.id()));
    QCOMPARE(recordOf(result, s.adams).contactStatus(), ContactStatus::OK);
    QCOMPARE(recordOf(result, s.baker).contactStatus(), ContactStatus::UnableToReach);
}

void ResponseMergeTest::overlappingContactAttemptsAreAllKept()
{
    Storm s = storm();
    const ContactAttempt earlier = attempt("Called at 1am");
    recordOf(s.response, s.adams).addContactAttempt(earlier);

    // Both responders called the Adams family while offline.
    EmergencyResponse mine = s.response;
    const ContactAttempt mineCalled = attempt("Visited, tree down");
    recordOf(mine, s.adams).addContactAttempt(mineCalled);

    EmergencyResponse theirs = s.response;
    const ContactAttempt theirsCalled = attempt("Texted, no reply");
    recordOf(theirs, s.adams).addContactAttempt(theirsCalled);

    const EmergencyResponse response = merged(s.response, mine, theirs);
    const FamilyResponseRecord& result = recordOf(response, s.adams);
    QCOMPARE(result.contactAttempts().size(), qsizetype(3));
    QVERIFY(hasAttempt(result, earlier.id()));
    QVERIFY(hasAttempt(result, mineCalled.id()));
    QVERIFY(hasAttempt(result, theirsCalled.id()));

    // Most recent first, as the model keeps them.
    for (qsizetype index = 1; index < result.contactAttempts().size(); ++index)
    {
        QVERIFY(result.contactAttempts().at(index - 1).timestamp()
                >= result.contactAttempts().at(index).timestamp());
    }

    // An attempt deleted on one side is still a record that someone was contacted.
    EmergencyResponse deleting = s.response;
    recordOf(deleting, s.adams).removeContactAttempt(earlier.id());
    QVERIFY(hasAttempt(recordOf(merged(s.response, deleting, s.response), s.adams), earlier.id()));
}

void ResponseMergeTest::anAttemptsNotesEditedOnOneSideKeepTheEdit()
{
    Storm s = storm();
    const ContactAttempt called = attempt("Called");
    recordOf(s.response, s.adams).addContactAttempt(called);

    EmergencyResponse mine = s.response;
    EmergencyResponse theirs = s.response;
    ContactAttempt edited = called;
    edited.setNotes("Called; needs a generator");
    recordOf(theirs, s.adams).removeContactAttempt(called.id());
    recordOf(theirs, s.adams).addContactAttempt(edited);

    const EmergencyResponse response = merged(s.response, mine, theirs);
    const FamilyResponseRecord& result = recordOf(response, s.adams);
    QCOMPARE(result.contactAttempts().size(), qsizetype(1));
    QCOMPARE(result.contactAttempts().first().notes(), QString("Called; needs a generator"));
}

void ResponseMergeTest::anAttemptAbsentFromTheBaseAndEditedOnBothTakesMine()
{
    // A publish landed but its answer was lost, so the base never learned of the
    // attempt; the user then edited its notes. Theirs is the earlier upload, mine
    // the newer edit.
    const Storm s = storm();
    const ContactAttempt called = attempt("Called");

    EmergencyResponse theirs = s.response;
    recordOf(theirs, s.adams).addContactAttempt(called);

    EmergencyResponse mine = s.response;
    ContactAttempt edited = called;
    edited.setNotes("Called; roof damage");
    recordOf(mine, s.adams).addContactAttempt(edited);

    const EmergencyResponse response = merged(s.response, mine, theirs);
    const FamilyResponseRecord& result = recordOf(response, s.adams);
    QCOMPARE(result.contactAttempts().size(), qsizetype(1));
    QCOMPARE(result.contactAttempts().first().notes(), QString("Called; roof damage"));
}

void ResponseMergeTest::contactStatusChangedDifferentlyTakesMineAndOnlyOnTheirsTakesTheirs()
{
    const Storm s = storm();

    EmergencyResponse mine = s.response;
    recordOf(mine, s.adams).setContactStatus(ContactStatus::OK);

    EmergencyResponse theirs = s.response;
    recordOf(theirs, s.adams).setContactStatus(ContactStatus::UnableToReach);
    recordOf(theirs, s.baker).setContactStatus(ContactStatus::OK);

    const EmergencyResponse result = merged(s.response, mine, theirs);
    QCOMPARE(recordOf(result, s.adams).contactStatus(), ContactStatus::OK);
    QCOMPARE(recordOf(result, s.baker).contactStatus(), ContactStatus::OK);

    // And the other way round, so it is mine that wins rather than one argument.
    const EmergencyResponse reversed = merged(s.response, theirs, mine);
    QCOMPARE(recordOf(reversed, s.adams).contactStatus(), ContactStatus::UnableToReach);
}

void ResponseMergeTest::aFamilyRecordAddedOnEitherSideSurvives()
{
    const Storm s = storm();
    const FamilyId carter = FamilyId::generate();
    const FamilyId davis = FamilyId::generate();

    EmergencyResponse mine = s.response;
    mine.addFamilyRecord(FamilyResponseRecord::create(carter, "Carter", "3 Ash St"));

    EmergencyResponse theirs = s.response;
    theirs.addFamilyRecord(FamilyResponseRecord::create(davis, "Davis", "4 Fir St"));

    const EmergencyResponse result = merged(s.response, mine, theirs);
    QCOMPARE(result.familyRecords().size(), qsizetype(4));
    QVERIFY(result.findRecord(carter) != nullptr);
    QVERIFY(result.findRecord(davis) != nullptr);
}

// --- Tasks -------------------------------------------------------------------

void ResponseMergeTest::aTaskResolvedOnOneSideAndReassignedOnTheOtherKeepsBoth()
{
    TaskId taskId;
    const Storm s = stormWithTask(taskId);
    const TeamId team = TeamId::generate();

    EmergencyResponse mine = s.response;
    recordOf(mine, s.adams).mutableTask(taskId)->resolve("Cleared by neighbours");

    EmergencyResponse theirs = s.response;
    recordOf(theirs, s.adams).mutableTask(taskId)->assignToTeam(team, "Chainsaw crew");

    const EmergencyResponse response = merged(s.response, mine, theirs);
    const ResponseTask* task = taskIn(recordOf(response, s.adams), taskId);
    QVERIFY(task != nullptr);
    QVERIFY(task->isResolved());
    QCOMPARE(task->resolutionNotes(), QString("Cleared by neighbours"));
    QVERIFY(task->resolvedAt().has_value());
    QVERIFY(task->assignedTeamId() == std::optional<TeamId>(team));
    QCOMPARE(task->assignmentNotes(), QString("Chainsaw crew"));
}

void ResponseMergeTest::aTaskRemovedOnEitherSideStaysRemoved()
{
    TaskId taskId;
    const Storm s = stormWithTask(taskId);

    EmergencyResponse removed = s.response;
    recordOf(removed, s.adams).removeTask(taskId);

    QVERIFY(taskIn(recordOf(merged(s.response, removed, s.response), s.adams), taskId) == nullptr);
    QVERIFY(taskIn(recordOf(merged(s.response, s.response, removed), s.adams), taskId) == nullptr);

    // The control: untouched on both sides, it is still there.
    QVERIFY(taskIn(recordOf(merged(s.response, s.response, s.response), s.adams), taskId) != nullptr);
}

void ResponseMergeTest::aTaskRemovedOnOneSideAndEditedOnTheOtherKeepsTheEdit()
{
    TaskId taskId;
    const Storm s = stormWithTask(taskId);

    EmergencyResponse removed = s.response;
    recordOf(removed, s.adams).removeTask(taskId);

    EmergencyResponse edited = s.response;
    recordOf(edited, s.adams).mutableTask(taskId)->setDescription("Oak and a power line");

    for (const EmergencyResponse& result :
         {merged(s.response, removed, edited), merged(s.response, edited, removed)})
    {
        const ResponseTask* task = taskIn(recordOf(result, s.adams), taskId);
        QVERIFY(task != nullptr);
        QCOMPARE(task->description(), QString("Oak and a power line"));
    }
}

void ResponseMergeTest::aNotificationClearedWithItsAssignmentStaysCleared()
{
    TaskId taskId;
    Storm s = stormWithTask(taskId);
    ResponseTask* baseTask = recordOf(s.response, s.adams).mutableTask(taskId);
    baseTask->assignToPerson(PersonId::generate(), "Bring the saw");
    baseTask->setNotification(TaskNotification::create(ContactMethod::Text, "Sent"));

    // Mine takes the assignment away, which clears its notification; theirs edits
    // only the description.
    EmergencyResponse mine = s.response;
    recordOf(mine, s.adams).mutableTask(taskId)->clearAssignment();

    EmergencyResponse theirs = s.response;
    recordOf(theirs, s.adams).mutableTask(taskId)->setDescription("Oak, now blocking the road");

    const EmergencyResponse response = merged(s.response, mine, theirs);
    const ResponseTask* task = taskIn(recordOf(response, s.adams), taskId);
    QVERIFY(task != nullptr);
    QVERIFY(!task->isAssigned());
    QVERIFY(!task->isNotified());
    QCOMPARE(task->description(), QString("Oak, now blocking the road"));
}

void ResponseMergeTest::aNotificationNeverOutlivesItsAssignment()
{
    TaskId taskId;
    Storm s = stormWithTask(taskId);
    ResponseTask* baseTask = recordOf(s.response, s.adams).mutableTask(taskId);
    baseTask->assignToPerson(PersonId::generate(), "Bring the saw");
    baseTask->setNotification(TaskNotification::create(ContactMethod::Text, "Sent"));

    // Theirs takes the assignment away; mine sends a new notification for it.
    EmergencyResponse theirs = s.response;
    recordOf(theirs, s.adams).mutableTask(taskId)->clearAssignment();

    EmergencyResponse mine = s.response;
    recordOf(mine, s.adams)
        .mutableTask(taskId)
        ->setNotification(TaskNotification::create(ContactMethod::Phone, "Called again"));

    // One value changed differently on both sides: mine, whole - the assignee and
    // the notification together.
    for (const EmergencyResponse& response : {merged(s.response, mine, theirs), merged(s.response, theirs, mine)})
    {
        const ResponseTask* task = taskIn(recordOf(response, s.adams), taskId);
        QVERIFY(task != nullptr);
        QCOMPARE(task->isNotified(), task->isAssigned());
    }

    const EmergencyResponse response = merged(s.response, mine, theirs);
    const ResponseTask* task = taskIn(recordOf(response, s.adams), taskId);
    QVERIFY(task->isAssigned());
    QCOMPARE(task->notification().value().notes(), QString("Called again"));
}

void ResponseMergeTest::categoriesMergeAsASet()
{
    const Storm s = storm();
    QVERIFY(s.response.taskCategories().contains("Flooding"));

    EmergencyResponse mine = s.response;
    mine.addTaskCategory("Welfare check");

    EmergencyResponse theirs = s.response;
    QStringList withoutFlooding = theirs.taskCategories();
    withoutFlooding.removeAll("Flooding");
    theirs.setTaskCategories(withoutFlooding);

    const QStringList result = merged(s.response, mine, theirs).taskCategories();
    QVERIFY(result.contains("Welfare check"));
    QVERIFY(!result.contains("Flooding"));
    QCOMPARE(result.size(), s.response.taskCategories().size());
}

// --- The response itself -----------------------------------------------------

void ResponseMergeTest::anEmergencyStartedOnlyOnMineIsKept()
{
    const Storm s = storm();

    const std::optional<EmergencyResponse> result =
        ResponseMerge::merge(std::nullopt, s.response, std::nullopt);
    QVERIFY(result.has_value());
    QVERIFY(result.value() == s.response);

    QVERIFY(ResponseMerge::merge(std::nullopt, std::nullopt, s.response).value() == s.response);
}

void ResponseMergeTest::anEmergencyStartedOnBothIsOneResponseNamedByTheirs()
{
    Storm mineStorm = storm();
    mineStorm.response.setName("Windstorm");
    Storm theirsStorm = storm();
    theirsStorm.response.setName("Storm");
    theirsStorm.response.setStartedAt(mineStorm.response.startedAt().addSecs(-60));

    const std::optional<EmergencyResponse> result =
        ResponseMerge::merge(std::nullopt, mineStorm.response, theirsStorm.response);
    QVERIFY(result.has_value());
    QCOMPARE(result.value().name(), QString("Storm"));
    QCOMPARE(result.value().startedAt(), theirsStorm.response.startedAt());

    // Every family either side was tracking is in the one response.
    QCOMPARE(result.value().familyRecords().size(), qsizetype(4));
}

void ResponseMergeTest::endedOnTheirsWhileMineRecordedKeepsTheResponse()
{
    const Storm s = storm();

    EmergencyResponse mine = s.response;
    const ContactAttempt called = attempt("Checked on the Bakers");
    recordOf(mine, s.baker).addContactAttempt(called);

    const std::optional<EmergencyResponse> result =
        ResponseMerge::merge(s.response, mine, std::nullopt);
    QVERIFY(result.has_value());
    QVERIFY(hasAttempt(recordOf(result.value(), s.baker), called.id()));

    // Ended on mine while theirs recorded, the same.
    QVERIFY(ResponseMerge::merge(s.response, std::nullopt, mine).has_value());
}

void ResponseMergeTest::endedOnTheirsWithMineUnchangedIsEnded()
{
    const Storm s = storm();

    QVERIFY(!ResponseMerge::merge(s.response, s.response, std::nullopt).has_value());
    QVERIFY(!ResponseMerge::merge(s.response, std::nullopt, s.response).has_value());
}

void ResponseMergeTest::nothingChangedOnEitherSideIsTheBase()
{
    TaskId taskId;
    const Storm s = stormWithTask(taskId);

    QVERIFY(merged(s.response, s.response, s.response) == s.response);
    QVERIFY(!ResponseMerge::merge(std::nullopt, std::nullopt, std::nullopt).has_value());
}

// --- Documents ---------------------------------------------------------------

void ResponseMergeTest::aResponseChangeAndADirectoryEditBothSurviveWhicheverSideIsMine()
{
    const Storm s = storm();
    const Document base = documentWith(s.adams, s);

    Document responder = base;
    EmergencyResponse response = s.response;
    const ContactAttempt called = attempt("Adams are fine");
    recordOf(response, s.adams).addContactAttempt(called);
    responder.setEmergencyResponse(response);

    Document editor = base;
    editor.updateFamily(familyAt(s.adams, 41.5));

    QVERIFY(!ResponseMerge::directoryChanged(responder, base));
    QVERIFY(ResponseMerge::directoryChanged(editor, base));

    for (const std::optional<Document>& result : {ResponseMerge::mergeDocuments(base, responder, editor),
                                                  ResponseMerge::mergeDocuments(base, editor, responder)})
    {
        QVERIFY(result.has_value());
        QVERIFY(result.value().findFamilyById(s.adams).value().latitude() == std::optional<double>(41.5));
        QVERIFY(hasAttempt(recordOf(result.value().emergencyResponse().value(), s.adams), called.id()));
    }
}

void ResponseMergeTest::aDirectoryEditOnBothSidesIsAConflict()
{
    const Storm s = storm();
    const Document base = documentWith(s.adams, s);

    Document mine = base;
    mine.updateFamily(familyAt(s.adams, 41.5));

    Document theirs = base;
    theirs.updateFamily(familyAt(s.adams, 39.0));

    QVERIFY(!ResponseMerge::mergeDocuments(base, mine, theirs).has_value());

    // The same edit on both sides is not a conflict.
    QVERIFY(ResponseMerge::mergeDocuments(base, mine, mine).has_value());
}

void ResponseMergeTest::noDirectoryChangeMergesTheResponse()
{
    const Storm s = storm();
    const Document base = documentWith(s.adams, s);

    Document mine = base;
    EmergencyResponse mineResponse = s.response;
    const ContactAttempt mineCalled = attempt("Mine");
    recordOf(mineResponse, s.adams).addContactAttempt(mineCalled);
    mine.setEmergencyResponse(mineResponse);

    Document theirs = base;
    EmergencyResponse theirsResponse = s.response;
    const ContactAttempt theirsCalled = attempt("Theirs");
    recordOf(theirsResponse, s.baker).addContactAttempt(theirsCalled);
    theirs.setEmergencyResponse(theirsResponse);

    const std::optional<Document> result = ResponseMerge::mergeDocuments(base, mine, theirs);
    QVERIFY(result.has_value());
    QVERIFY(!ResponseMerge::directoryChanged(result.value(), base));

    const EmergencyResponse& response = result.value().emergencyResponse().value();
    QVERIFY(hasAttempt(recordOf(response, s.adams), mineCalled.id()));
    QVERIFY(hasAttempt(recordOf(response, s.baker), theirsCalled.id()));
}

void ResponseMergeTest::aRosterEditOnOneSideCarriesThrough()
{
    const Storm s = storm();
    const Document base = documentWith(s.adams, s);

    // Mine made an invite; theirs recorded a contact.
    Document mine = base;
    mine.setSealingSet(withInvite(base.sealingSet()));
    QCOMPARE(mine.sealingSet().invites().size(), 1);
    QVERIFY(!ResponseMerge::directoryChanged(mine, base));

    Document theirs = base;
    EmergencyResponse response = theirs.emergencyResponse().value();
    response.mutableRecord(s.baker)->setContactStatus(ContactStatus::OK);
    theirs.setEmergencyResponse(response);
    QVERIFY(!ResponseMerge::directoryChanged(theirs, base));

    const std::optional<Document> result = ResponseMerge::mergeDocuments(base, mine, theirs);
    QVERIFY(result.has_value());
    QVERIFY(result->sealingSet() == mine.sealingSet());
    QCOMPARE(recordOf(result->emergencyResponse().value(), s.baker).contactStatus(), ContactStatus::OK);

    // Either way round.
    const std::optional<Document> swapped = ResponseMerge::mergeDocuments(base, theirs, mine);
    QVERIFY(swapped.has_value());
    QVERIFY(swapped->sealingSet() == mine.sealingSet());
}

void ResponseMergeTest::rosterEditsOnBothSidesMergeEntryByEntry()
{
    const Storm s = storm();
    const Document base = documentWith(s.adams, s);
    const MemberId alice = base.sealingSet().people().first().id;

    // Mine made an invite; theirs added Dave: two entries, both carried.
    Document mine = base;
    mine.setSealingSet(withInvite(base.sealingSet()));
    Document theirs = base;
    theirs.setSealingSet(withDave(base.sealingSet()));
    const std::optional<Document> merged = ResponseMerge::mergeDocuments(base, mine, theirs);
    QVERIFY(merged.has_value());
    QCOMPARE(merged->sealingSet().invites().size(), 1);
    QCOMPARE(merged->sealingSet().people().size(), 2);
    QVERIFY(merged->sealingSet().person(alice)->role == Role::Admin);

    // A roster edit on one side and a family edit on the other both carry.
    Document moved = base;
    moved.updateFamily(familyAt(s.adams, 41.5));
    const std::optional<Document> both = ResponseMerge::mergeDocuments(base, mine, moved);
    QVERIFY(both.has_value());
    QCOMPARE(both->sealingSet().invites().size(), 1);
    QVERIFY(both->findFamilyById(s.adams).value().latitude() == std::optional<double>(41.5));

    // The former keys of both sides, theirs' first.
    const DataKey mineRotated = DocumentCrypto::generateDataKey();
    const DataKey theirsRotated = DocumentCrypto::generateDataKey();
    SealingSet mineKeys = base.sealingSet();
    mineKeys.addFormerKey(mineRotated);
    SealingSet theirsKeys = base.sealingSet();
    theirsKeys.addFormerKey(theirsRotated);
    const SealingSet keys = SealingSet::merge(base.sealingSet(), mineKeys, theirsKeys);
    QVERIFY(keys.descendsFrom(mineRotated));
    QVERIFY(keys.descendsFrom(theirsRotated));
    const QJsonArray theirsThenMine{theirsKeys.toJson().value("formerKeys").toArray().first(),
                                    mineKeys.toJson().value("formerKeys").toArray().first()};
    QCOMPARE(keys.toJson().value("formerKeys").toArray(), theirsThenMine);
}

void ResponseMergeTest::theSameEntryChangedOnBothSidesIsTheirs()
{
    const SealingSet base = withDave(setOfOne());
    const MemberId dave = idOf(base, "Dave");

    SealingSet daveAdmin = base;
    QVERIFY(daveAdmin.setRole(dave, Role::Admin));
    SealingSet daveGone = base;
    QVERIFY(daveGone.removePerson(dave));

    // Promoted here, removed there, and the other way round: theirs each time.
    QVERIFY(!SealingSet::merge(base, daveAdmin, daveGone).person(dave).has_value());
    QVERIFY(SealingSet::merge(base, daveGone, daveAdmin).person(dave)->role == Role::Admin);

    // Anchor: the same change on both sides is that change.
    QVERIFY(SealingSet::merge(base, daveAdmin, daveAdmin).person(dave)->role == Role::Admin);
}

void ResponseMergeTest::anUnsoundMergeIsTheirsWhole()
{
    const SealingSet base = withDave(setOfOne());
    const MemberId alice = idOf(base, "Alice");
    const MemberId dave = idOf(base, "Dave");

    // Each side removes the other: entry by entry, nobody is left, so theirs stands.
    SealingSet daveGone = base;
    QVERIFY(daveGone.removePerson(dave));
    SealingSet aliceGone = base;
    QVERIFY(aliceGone.removePerson(alice));
    QVERIFY(SealingSet::merge(base, daveGone, aliceGone) == aliceGone);
    QVERIFY(SealingSet::merge(base, aliceGone, daveGone) == daveGone);

    // Dave's invites are part of his entry: made here while he is removed there, or
    // the other way round, his entry is theirs, and his invite goes with it.
    SealingSet davesInvite = base;
    Invite byDave;
    byDave.purpose = Invite::For::User;
    byDave.person = dave;
    byDave.label = "for Erin";
    byDave.keys = newKeys();
    QVERIFY(davesInvite.addInvite(byDave));
    QVERIFY(SealingSet::merge(base, davesInvite, daveGone) == daveGone);
    QVERIFY(SealingSet::merge(base, daveGone, davesInvite) == davesInvite);
}

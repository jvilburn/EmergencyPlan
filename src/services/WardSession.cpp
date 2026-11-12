#include "WardSession.h"
#include "AccessCommands.h"
#include "CommandRegistry.h"
#include "Crypto.h"
#include "EmergencyCommands.h"
#include "InvitationFile.h"
#include "ReportGenerator.h"
#include "UpdateFile.h"

#include <QDateTime>
#include <QStringList>

namespace
{

/// The command's rule, then execute, on document: the one rule a first run and a replay
/// both follow.
CommitResult attempt(const Command& command, Document& document)
{
    QString refusal = command.refusal(document);
    if (!refusal.isEmpty())
    {
        return CommitResult{CommitStatus::Refused, refusal, command.description()};
    }
    if (command.execute(document) == CommandOutcome::Conflicted)
    {
        return CommitResult{CommitStatus::Conflicted, QString(), command.description()};
    }
    document.onDocumentChanged(command.documentChange());
    return CommitResult{CommitStatus::Applied, QString(), command.description()};
}

LocalRead notOpened(OpenStatus status, const QString& problem)
{
    return LocalRead{OpenResult{status, problem}, std::nullopt, QByteArray(), VersionRank()};
}

TakeResult notTaken(TakeStatus status, const QString& problem)
{
    return TakeResult{status, problem, ReplayResult{ReplayStatus::FastForwarded, QString(), QStringList()}};
}

InvitedWard notJoinable(InvitationStatus status, const QString& problem)
{
    return InvitedWard{status, problem, DocumentId(), QByteArray(), std::nullopt, std::nullopt, QString()};
}

/// The keys a copy under dataKey with access has held: the hash of dataKey, and every former
/// key access names.
QSet<QByteArray> keyHistory(const QByteArray& dataKey, const AccessList& access)
{
    QSet<QByteArray> keys(access.formerKeys().begin(), access.formerKeys().end());
    keys.insert(AccessList::formerKeyHash(dataKey));
    return keys;
}

/// True when a version read under its data key descends from the copy this device holds under
/// heldKey with the access list held: the two share a key. Keys are held per ward, so another
/// ward's version shares none, and only a holder of a key can name it, inside the payload.
bool descends(const WardRead& version, const QByteArray& heldKey, const AccessList& held)
{
    return keyHistory(version.dataKey, version.payload->ward.access()).intersects(keyHistory(heldKey, held));
}

}  // namespace

WardSession::WardSession(const Identity* identity, WardStore* store, Geocoder* geocoder, UnitLookup* unitLookup,
                         QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_store(store)
    , m_geocoder(geocoder)
    , m_unitLookup(unitLookup)
    , m_automaticUpdates(m_document, geocoder, unitLookup, nullptr)
{
    connect(&m_automaticUpdates, &AutomaticUpdates::documentChanged,
            this, &WardSession::documentChanged);
    connect(&m_automaticUpdates, &AutomaticUpdates::applied, this, &WardSession::save);
}

void WardSession::createWard(const Document& content, const UserDetails& user, const QString& deviceName)
{
    Document ward = content;
    ward.setAccess(AccessList::founding(UserId::generate(), user, deviceName, *m_identity));
    m_documentId = DocumentId::generate();
    m_version = VersionRank{1, m_identity->fingerprint()};
    m_dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    m_landed.clear();
    m_device = DeviceState();
    m_device.heldRecipients = ward.access().recipientFingerprints();
    m_savePending = false;
    m_automaticUpdates.wardOpened();
    clearUndoStack();
    replaceDocument(ward);
    save();
}

OpenResult WardSession::openWard(const DocumentId& document)
{
    LocalRead local = readLocal(*m_store, *m_identity, document);
    if (local.result.status != OpenStatus::Opened)
    {
        return local.result;
    }
    m_documentId = document;
    m_version = local.version;
    m_dataKey = local.dataKey;
    m_landed = local.payload->landed;
    m_device = local.payload->device.value();
    m_savePending = false;
    m_automaticUpdates.wardOpened();
    clearUndoStack();
    replaceDocument(local.payload->ward);
    return local.result;
}

LocalRead WardSession::readLocal(const WardStore& store, const Identity& identity, const DocumentId& document)
{
    StoreRead stored = store.read(document);
    switch (stored.status)
    {
    case StoreReadStatus::Read:
        break;
    case StoreReadStatus::Missing:
        return notOpened(OpenStatus::Missing, "this device holds no file for the ward");
    case StoreReadStatus::Fault:
        return notOpened(OpenStatus::Fault, stored.problem);
    }

    WardRead read = WardFile::read(stored.bytes, identity, PayloadKind::LocalFile);
    switch (read.status)
    {
    case WardReadStatus::Read:
        break;
    case WardReadStatus::Newer:
        return notOpened(OpenStatus::Newer, "a newer Ward Planning wrote the file");
    case WardReadStatus::Unreadable:
        return notOpened(OpenStatus::CannotRead, read.problem);
    case WardReadStatus::Deleted:
        return notOpened(OpenStatus::CannotRead, "the header names no one");
    case WardReadStatus::NotNamed:
        return notOpened(OpenStatus::CannotRead, "the header does not name this device");
    }
    if (read.header->document() != document)
    {
        return notOpened(OpenStatus::CannotRead, "the file holds another ward");
    }
    return LocalRead{OpenResult{OpenStatus::Opened, QString()}, read.payload, read.dataKey, read.header->version()};
}

bool WardSession::retryPendingSave()
{
    if (m_savePending)
    {
        save();
    }
    return !m_savePending;
}

bool WardSession::changesToShare(const Journal& journal, const QHash<QByteArray, qint64>& landed,
                                 const QByteArray& device)
{
    return !journal.isEmpty()
        && journal.entries().last().sequence > landed.value(device);
}

CommitResult WardSession::commit(CommandPtr command)
{
    if (!command)
    {
        return CommitResult{CommitStatus::NothingToDo, QString(), CommandDescription()};
    }
    CommitResult result = run(*command);
    if (result.status != CommitStatus::Applied)
    {
        return result;
    }

    m_redoStack.clear();
    UndoableCommandPtr undoable = std::dynamic_pointer_cast<const UndoableCommand>(command);
    if (undoable)
    {
        m_undoStack.append(undoable);
        if (m_undoStack.size() > UndoLimit)
        {
            m_undoStack.removeFirst();
        }
    }
    emit undoStackChanged();
    return result;
}

CommitResult WardSession::undo()
{
    Q_ASSERT(canUndo());
    CommitResult result = run(*m_undoStack.last()->inverse());
    if (result.status == CommitStatus::Applied)
    {
        m_redoStack.append(m_undoStack.takeLast());
        emit undoStackChanged();
    }
    return result;
}

CommitResult WardSession::redo()
{
    Q_ASSERT(canRedo());
    CommitResult result = run(*m_redoStack.last());
    if (result.status == CommitStatus::Applied)
    {
        m_undoStack.append(m_redoStack.takeLast());
        emit undoStackChanged();
    }
    return result;
}

QString WardSession::undoHeading() const
{
    if (m_undoStack.isEmpty())
    {
        return QString();
    }
    return m_undoStack.last()->description().heading();
}

QString WardSession::redoHeading() const
{
    if (m_redoStack.isEmpty())
    {
        return QString();
    }
    return m_redoStack.last()->description().heading();
}

EndResult WardSession::endEmergency(const QString& reportPath)
{
    std::optional<EmergencyResponse> response = m_document.emergencyResponse();
    if (!response.has_value())
    {
        return EndResult::AlreadyEnded;
    }
    if (!ReportGenerator::generateReport(*response, m_document, QDateTime::currentDateTimeUtc(), reportPath))
    {
        return EndResult::ReportFailed;
    }
    commit(EndEmergencyCommand::fromReported(*response));
    return EndResult::Ended;
}

ReplayResult WardSession::replayOnto(const Document& version)
{
    ReplayResult result = replay(version);
    save();
    return result;
}

QByteArray WardSession::writeUpdate()
{
    NextVersion next = nextVersion();
    AccessList access = m_document.access();
    access.setFormerKeys(next.formerKeys);
    m_document.setAccess(access);
    m_version = next.version;
    m_dataKey = next.dataKey;
    m_landed = next.landed;
    m_device.heldRecipients = next.recipients;
    save();
    return UpdateFile::write(m_documentId, next.bytes, next.writer);
}

TakeResult WardSession::takeUpdate(const QByteArray& file)
{
    UpdateRead update = UpdateFile::read(file);
    switch (update.status)
    {
    case UpdateReadStatus::Read:
        break;
    case UpdateReadStatus::Newer:
        return notTaken(TakeStatus::Newer, QString());
    case UpdateReadStatus::Unreadable:
        return notTaken(TakeStatus::Unreadable, update.problem);
    }
    const WardHeader& header = *update.header;
    if (header.document() != m_documentId)
    {
        return notTaken(TakeStatus::OtherWard, QString());
    }
    std::optional<QByteArray> verifyKey = m_document.access().verifyKeyOf(header.version().writer);
    if (!verifyKey.has_value())
    {
        return notTaken(TakeStatus::UnknownWriter, QString());
    }
    if (!UpdateFile::verifies(update, *verifyKey))
    {
        return notTaken(TakeStatus::Unreadable, "the signature does not verify");
    }
    if (!header.version().ranksAbove(m_version))
    {
        return notTaken(TakeStatus::NotNewer, QString());
    }

    WardRead read = readVersion(update.version);
    switch (read.status)
    {
    case WardReadStatus::Read:
        break;
    case WardReadStatus::Newer:
        return notTaken(TakeStatus::Newer, QString());
    case WardReadStatus::Unreadable:
        return notTaken(TakeStatus::Unreadable, read.problem);
    case WardReadStatus::Deleted:
    case WardReadStatus::NotNamed:
    {
        TakeStatus status = joinUnlanded() ? TakeStatus::InviteUsedOrWithdrawn : TakeStatus::NotNamed;
        becomeNoLongerShared();
        return notTaken(status, QString());
    }
    }
    if (!descends(read, m_dataKey, m_document.access()))
    {
        return notTaken(TakeStatus::Unreadable, "the version does not descend from the one this device holds");
    }

    // A version read with the join seed comes from a member who has not taken the join: the
    // join runs again on it first, so this device is on the list every replayed entry meets.
    Document base = read.payload->ward;
    CommandPtr rejoin;
    if (!read.header->sealedKeyFor(m_identity->fingerprint()).has_value())
    {
        QByteArray invite = Identity::fromInviteSeed(*m_device.joinSeed).fingerprint();
        rejoin = JoinCommand::rejoin(*base.access().findInvite(invite), m_document.access(),
                                     m_identity->fingerprint(), base.wardName());
        attempt(*rejoin, base);
    }
    m_device.journal.dropLanded(read.payload->landed.value(m_identity->fingerprint()));
    ReplayResult replayed = replay(base);
    if (rejoin)
    {
        m_device.journal.append(*rejoin);
    }
    m_version = header.version();
    m_dataKey = read.dataKey;
    m_landed = read.payload->landed;
    m_device.heldRecipients = read.payload->ward.access().recipientFingerprints();
    save();
    m_automaticUpdates.run();
    return TakeResult{TakeStatus::Taken, QString(), replayed};
}

InviteResult WardSession::invite(const std::optional<InvitedUser>& user)
{
    QByteArray seed = Crypto::randomBytes(Crypto::KeyBytes);
    Identity keys = Identity::fromInviteSeed(seed);
    m_device.inviteSeeds.append(seed);
    CommitResult result = commit(InviteCommand::create(m_identity->fingerprint(), m_document.access(), user, keys));
    if (result.status != CommitStatus::Applied)
    {
        m_device.inviteSeeds.removeOne(seed);
        return InviteResult{result, QByteArray()};
    }
    return InviteResult{result, keys.fingerprint()};
}

QSet<QByteArray> WardSession::invitesMadeHere() const
{
    QSet<QByteArray> invites;
    for (const QByteArray& seed : m_device.inviteSeeds)
    {
        invites.insert(Identity::fromInviteSeed(seed).fingerprint());
    }
    return invites;
}

QByteArray WardSession::writeInvitation(const QByteArray& invite) const
{
    QByteArray seed;
    for (const QByteArray& candidate : m_device.inviteSeeds)
    {
        if (Identity::fromInviteSeed(candidate).fingerprint() == invite)
        {
            seed = candidate;
        }
    }
    return InvitationFile::write(seed, nextVersion().bytes);
}

InvitedWard WardSession::readInvitation(const QByteArray& file) const
{
    InvitationRead invitation = InvitationFile::read(file);
    switch (invitation.status)
    {
    case InvitationReadStatus::Read:
        break;
    case InvitationReadStatus::Newer:
        return notJoinable(InvitationStatus::Newer, QString());
    case InvitationReadStatus::NotAnInvitation:
        return notJoinable(InvitationStatus::NotAnInvitation, invitation.problem);
    }

    Identity keys = Identity::fromInviteSeed(invitation.seed);
    WardRead read = WardFile::read(invitation.version, keys, PayloadKind::Version);
    switch (read.status)
    {
    case WardReadStatus::Read:
        break;
    case WardReadStatus::Newer:
        return notJoinable(InvitationStatus::Newer, QString());
    case WardReadStatus::Unreadable:
        return notJoinable(InvitationStatus::NotAnInvitation, read.problem);
    case WardReadStatus::Deleted:
    case WardReadStatus::NotNamed:
        return notJoinable(InvitationStatus::NotAnInvitation, "the version does not name the invite");
    }

    const DocumentId& document = read.header->document();
    QString wardName = read.payload->ward.wardName();
    LocalRead local = readLocal(*m_store, *m_identity, document);
    if (local.result.status == OpenStatus::Opened && !local.payload->device->noLongerShared)
    {
        return InvitedWard{InvitationStatus::AlreadyHasAccess, QString(), document, QByteArray(), std::nullopt,
                           std::nullopt, wardName};
    }
    return InvitedWard{InvitationStatus::Joinable, QString(), document, invitation.seed, read,
                       read.payload->ward.access().findInvite(keys.fingerprint()), wardName};
}

void WardSession::join(const InvitedWard& invited, const UserDetails& user, const QString& deviceName)
{
    takeFirstDownload(invited.document, *invited.version);
    m_device.joinSeed = invited.seed;
    AccessDevice device(deviceName, m_identity->publicKey(), m_identity->verifyKey());
    commit(JoinCommand::create(*invited.invite, device, user, invited.wardName));
}

AccessUser WardSession::ownUser() const
{
    return *m_document.access().userOfDevice(m_identity->fingerprint());
}

void WardSession::renameDeviceEverywhere(const QString& deviceName)
{
    const QByteArray& actor = m_identity->fingerprint();
    commitEverywhere([&](const AccessList& access) { return RenameDeviceCommand::create(actor, access, deviceName); });
}

void WardSession::setUserDetailsEverywhere(const UserDetails& details)
{
    const QByteArray& actor = m_identity->fingerprint();
    commitEverywhere([&](const AccessList& access) { return SetUserDetailsCommand::create(actor, access, details); });
}

void WardSession::commitEverywhere(const std::function<CommandPtr(const AccessList&)>& build)
{
    const QList<DocumentId> documents = m_store->documents();
    for (const DocumentId& document : documents)
    {
        if (document == m_documentId)
        {
            commit(build(m_document.access()));
            continue;
        }
        WardSession other(m_identity, m_store, m_geocoder, m_unitLookup, nullptr);
        if (other.openWard(document).status == OpenStatus::Opened)
        {
            other.commit(build(other.document().access()));
        }
    }
}

std::optional<UserDetails> WardSession::followUserDetails(const UserDetails& yours)
{
    UserDetails details = ownUser().details();
    if (details == yours)
    {
        return std::nullopt;
    }
    setUserDetailsEverywhere(details);
    return details;
}

ReplayResult WardSession::replay(const Document& version)
{
    if (m_device.journal.isEmpty())
    {
        replaceDocument(version);
        return ReplayResult{ReplayStatus::FastForwarded, QString(), QStringList()};
    }

    Document replayed = version;
    replayed.onDocumentChanged(DocumentChange::full());
    const QList<JournalEntry>& entries = m_device.journal.entries();
    for (int i = 0; i < entries.size(); ++i)
    {
        CommandPtr command = CommandRegistry::fromJson(JsonReader(entries[i].command));
        CommitResult result = attempt(*command, replayed);
        if (result.status != CommitStatus::Applied)
        {
            QString toRedo = toRedoText(i, result);
            QStringList stillHaveAccess = stillHavingAccess(version.access());
            m_device.journal.clear();
            clearUndoStack();
            replaceDocument(version);
            return ReplayResult{ReplayStatus::Abandoned, toRedo, stillHaveAccess};
        }
    }
    replaceDocument(replayed);
    return ReplayResult{ReplayStatus::Replayed, QString(), QStringList()};
}

CommitResult WardSession::run(const Command& command)
{
    if (m_device.noLongerShared)
    {
        return CommitResult{CommitStatus::Refused, tr("This ward is no longer shared with this device."),
                            command.description()};
    }
    CommitResult result = attempt(command, m_document);
    if (result.status != CommitStatus::Applied)
    {
        return result;
    }
    m_device.journal.append(command);
    save();
    emit documentChanged(command.documentChange());
    m_automaticUpdates.commandApplied(command);
    return result;
}

QString WardSession::toRedoText(int stopped, const CommitResult& stop) const
{
    QStringList text;
    if (stop.status == CommitStatus::Refused)
    {
        text.append(stop.refusal);
    }
    else
    {
        text.append(tr("Your changes could not be applied, because the ward changed first."));
    }
    text.append(tr("These need doing again:"));

    QList<CommandDescription> descriptions = m_device.journal.descriptions();
    for (int i = 0; i < descriptions.size(); ++i)
    {
        text.append(QString());
        if (i == stopped)
        {
            text.append(tr("%1 (stopped here)").arg(descriptions[i].heading()));
        }
        else
        {
            text.append(descriptions[i].heading());
        }
        for (const QString& line : descriptions[i].lines())
        {
            text.append(QString("- %1").arg(line));
        }
    }
    return text.join("\n") + "\n";
}

QStringList WardSession::stillHavingAccess(const AccessList& access) const
{
    QStringList names;
    for (const JournalEntry& entry : m_device.journal.entries())
    {
        std::shared_ptr<const RemoveUserCommand> removal
            = std::dynamic_pointer_cast<const RemoveUserCommand>(CommandRegistry::fromJson(JsonReader(entry.command)));
        if (!removal)
        {
            continue;
        }
        std::optional<AccessUser> user = access.findUser(removal->userId());
        if (user.has_value())
        {
            names.append(user->name());
        }
    }
    return names;
}

void WardSession::save()
{
    QList<QByteArray> keptSeeds;
    for (const QByteArray& seed : m_device.inviteSeeds)
    {
        if (m_document.access().findInvite(Identity::fromInviteSeed(seed).fingerprint()).has_value())
        {
            keptSeeds.append(seed);
        }
    }
    m_device.inviteSeeds = keptSeeds;
    QByteArray bytes = WardFile::write(m_documentId, m_version, m_dataKey, payload());
    QString problem;
    if (m_store->write(m_documentId, bytes, &problem))
    {
        m_savePending = false;
        return;
    }
    if (!m_savePending)
    {
        m_savePending = true;
        emit saveFailed(problem);
    }
}

WardSession::NextVersion WardSession::nextVersion() const
{
    Document document = m_document;
    AccessList access = document.access();
    QSet<QByteArray> recipients = access.recipientFingerprints();
    QByteArray dataKey = m_dataKey;
    if (!recipients.contains(m_device.heldRecipients))
    {
        QList<QByteArray> formerKeys = access.formerKeys();
        formerKeys.append(AccessList::formerKeyHash(m_dataKey));
        access.setFormerKeys(formerKeys);
        document.setAccess(access);
        dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    }

    QHash<QByteArray, qint64> landed;
    const QSet<QByteArray> devices = access.deviceFingerprints();
    for (auto it = m_landed.constBegin(); it != m_landed.constEnd(); ++it)
    {
        if (devices.contains(it.key()))
        {
            landed.insert(it.key(), it.value());
        }
    }
    landed.insert(m_identity->fingerprint(), m_device.journal.lastSequence());

    Identity signer = writer();
    VersionRank version{m_version.number + 1, signer.fingerprint()};
    QByteArray bytes = WardFile::write(m_documentId, version, dataKey, WardPayload{document, landed, std::nullopt});
    return NextVersion{bytes, version, dataKey, access.formerKeys(), landed, recipients, signer};
}

Identity WardSession::writer() const
{
    if (!m_device.heldRecipients.contains(m_identity->fingerprint()))
    {
        return Identity::fromInviteSeed(*m_device.joinSeed);
    }
    return *m_identity;
}

bool WardSession::joinUnlanded() const
{
    for (const JournalEntry& entry : m_device.journal.entries())
    {
        if (entry.command["type"].toString() == JoinCommand::TypeName)
        {
            return true;
        }
    }
    return false;
}

WardRead WardSession::readVersion(const QByteArray& version) const
{
    WardRead read = WardFile::read(version, *m_identity, PayloadKind::Version);
    if (read.status == WardReadStatus::NotNamed && m_device.joinSeed.has_value())
    {
        return WardFile::read(version, Identity::fromInviteSeed(*m_device.joinSeed), PayloadKind::Version);
    }
    return read;
}

void WardSession::takeFirstDownload(const DocumentId& document, const WardRead& version)
{
    m_documentId = document;
    m_version = version.header->version();
    m_dataKey = version.dataKey;
    m_landed = version.payload->landed;
    m_device = DeviceState();
    m_device.journal.dropLanded(m_landed.value(m_identity->fingerprint()));
    m_device.heldRecipients = version.payload->ward.access().recipientFingerprints();
    m_savePending = false;
    m_automaticUpdates.wardOpened();
    clearUndoStack();
    replaceDocument(version.payload->ward);
}

void WardSession::becomeNoLongerShared()
{
    m_device.noLongerShared = true;
    m_device.journal.clear();
    clearUndoStack();
    save();
}

void WardSession::replaceDocument(const Document& document)
{
    m_document = document;
    m_document.onDocumentChanged(DocumentChange::full());
    emit documentChanged(DocumentChange::full());
}

void WardSession::clearUndoStack()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoStackChanged();
}

#include "WardSession.h"
#include "CommandRegistry.h"
#include "Crypto.h"
#include "EmergencyCommands.h"
#include "ReportGenerator.h"
#include "UpdateFile.h"
#include "WardFile.h"

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
    return TakeResult{status, problem, ReplayResult{ReplayStatus::FastForwarded, QString()}};
}

/// True when a version read under its data key descends from the version this device holds
/// under heldKey: it is under that key, or its access list names that key among its former
/// keys.
bool descends(const WardRead& version, const QByteArray& heldKey)
{
    QByteArray heldKeyHash = Crypto::hash(heldKey, Crypto::HashBytes, "wp-former-key");
    return version.dataKey == heldKey
        || version.payload->ward.access().formerKeys().contains(heldKeyHash);
}

}  // namespace

WardSession::WardSession(const Identity* identity, WardStore* store, Geocoder* geocoder, UnitLookup* unitLookup,
                         QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_store(store)
    , m_automaticUpdates(m_document, geocoder, unitLookup, nullptr)
{
    connect(&m_automaticUpdates, &AutomaticUpdates::documentChanged,
            this, &WardSession::documentChanged);
    connect(&m_automaticUpdates, &AutomaticUpdates::applied, this, &WardSession::save);
}

void WardSession::createWard(const Document& content, const QString& userName, const QString& deviceName)
{
    Document ward = content;
    ward.setAccess(AccessList::founding(UserId::generate(), userName, deviceName, *m_identity));
    m_documentId = DocumentId::generate();
    m_version = VersionRank{1, m_identity->fingerprint()};
    m_dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    m_landed.clear();
    m_device = DeviceState();
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
    VersionRank next{m_version.number + 1, m_identity->fingerprint()};
    QHash<QByteArray, qint64> landed;
    const QList<RecipientKeys> recipients = m_document.access().recipients();
    for (const RecipientKeys& keys : recipients)
    {
        QByteArray fingerprint = Identity::fingerprintOf(keys.publicKey, keys.verifyKey);
        if (m_landed.contains(fingerprint))
        {
            landed.insert(fingerprint, m_landed.value(fingerprint));
        }
    }
    landed.insert(m_identity->fingerprint(), m_device.journal.lastSequence());

    QByteArray version = WardFile::write(m_documentId, next, m_dataKey, WardPayload{m_document, landed, std::nullopt});
    m_version = next;
    m_landed = landed;
    save();
    return UpdateFile::write(m_documentId, version, *m_identity);
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

    WardRead read = WardFile::read(update.version, *m_identity, PayloadKind::Version);
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
        return notTaken(TakeStatus::NotNamed, QString());
    }
    if (!descends(read, m_dataKey))
    {
        return notTaken(TakeStatus::Unreadable, "the version does not descend from the one this device holds");
    }

    m_device.journal.dropLanded(read.payload->landed.value(m_identity->fingerprint()));
    ReplayResult replayed = replay(read.payload->ward);
    m_version = header.version();
    m_dataKey = read.dataKey;
    m_landed = read.payload->landed;
    save();
    return TakeResult{TakeStatus::Taken, QString(), replayed};
}

ReplayResult WardSession::replay(const Document& version)
{
    if (m_device.journal.isEmpty())
    {
        replaceDocument(version);
        return ReplayResult{ReplayStatus::FastForwarded, QString()};
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
            m_device.journal.clear();
            clearUndoStack();
            replaceDocument(version);
            return ReplayResult{ReplayStatus::Abandoned, toRedo};
        }
    }
    replaceDocument(replayed);
    return ReplayResult{ReplayStatus::Replayed, QString()};
}

CommitResult WardSession::run(const Command& command)
{
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

void WardSession::save()
{
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

#include "WardSession.h"
#include "CommandRegistry.h"
#include "Crypto.h"
#include "EmergencyCommands.h"
#include "ReportGenerator.h"
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
    return LocalRead{OpenResult{status, problem}, std::nullopt, QByteArray()};
}

}  // namespace

WardSession::WardSession(const Identity* identity, WardStore* store, Geocoder* geocoder, UnitLookup* unitLookup,
                         QObject* parent)
    : QObject(parent)
    , m_identity(identity)
    , m_store(store)
    , m_automaticUpdates(m_document, m_device.failed, geocoder, unitLookup, nullptr)
{
    connect(&m_automaticUpdates, &AutomaticUpdates::documentChanged,
            this, &WardSession::documentChanged);
    connect(&m_automaticUpdates, &AutomaticUpdates::applied, this, &WardSession::save);
}

void WardSession::createWard(const Document& document)
{
    m_documentId = DocumentId::generate();
    m_dataKey = Crypto::randomBytes(Crypto::KeyBytes);
    m_landed.clear();
    m_device = DeviceState();
    m_savePending = false;
    clearUndoStack();
    replaceDocument(document);
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
    m_dataKey = local.dataKey;
    m_landed = local.payload->landed;
    m_device = local.payload->device.value();
    m_savePending = false;
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
    return LocalRead{OpenResult{OpenStatus::Opened, QString()}, read.payload, read.dataKey};
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
        CommandPtr command = CommandRegistry::fromJson(entries[i].command);
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
    QByteArray bytes = WardFile::write(m_documentId, m_dataKey, WardPayload{m_document, m_landed, m_device});
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

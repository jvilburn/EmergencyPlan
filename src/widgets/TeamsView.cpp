#include "TeamsView.h"
#include "BaseTreeModel.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"
#include "NotifyDialog.h"
#include "Team.h"
#include "TeamsTreeModel.h"
#include "FilterBar.h"
#include "Person.h"
#include "Phone.h"
#include "TeamCommands.h"
#include "SelectionPreservingTreeView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>

TeamsView::TeamsView(EmergencyManager* emergencyManager,
                     QWidget* parent)
    : QWidget(parent)
    , m_emergencyManager(emergencyManager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // FilterBar owns Filter
    m_filterBar = new FilterBar(this);
    layout->addWidget(m_filterBar);

    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);

    m_addButton = new QPushButton(tr("Add"));
    m_editButton = new QPushButton(tr("Edit"));
    m_deleteButton = new QPushButton(tr("Delete"));

    toolbar->addWidget(m_addButton);
    toolbar->addWidget(m_editButton);
    toolbar->addWidget(m_deleteButton);
    toolbar->addStretch();

    layout->addLayout(toolbar);

    // Create model with filter from FilterBar
    m_model = new TeamsTreeModel(emergencyManager,
                                 m_filterBar->filter(), this);

    // Create tree view with model
    m_tree = new SelectionPreservingTreeView(m_model, this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);
    m_tree->expandToDepth(0);
    layout->addWidget(m_tree);

    // Connections
    connect(m_addButton, &QPushButton::clicked, this, &TeamsView::addTeam);
    connect(m_editButton, &QPushButton::clicked, this, &TeamsView::editSelectedTeam);
    connect(m_deleteButton, &QPushButton::clicked, this, &TeamsView::deleteTeam);

    connect(m_tree, &SelectionPreservingTreeView::selectionChanged,
            this, &TeamsView::onSelectionChanged);
    connect(m_tree, &QTreeView::doubleClicked,
            this, &TeamsView::onTreeDoubleClicked);
    connect(m_tree, &QTreeView::customContextMenuRequested,
            this, &TeamsView::onContextMenu);

    // Expand top-level items when model is reset
    connect(m_model, &QAbstractItemModel::modelReset, this, &TeamsView::expandTeams);

    // Load contact details when member node is expanded
    connect(m_tree, &QTreeView::expanded,
            this, &TeamsView::onTreeExpanded);

    updateButtonStates();
}

void TeamsView::onSelectionChanged()
{
    updateButtonStates();
    emit highlightChanged();
}

void TeamsView::onTreeDoubleClicked(const QModelIndex& index)
{
    ItemType type = m_model->itemTypeAt(index);

    switch (type)
    {
    case ItemType::Team:
        editSelectedTeam();
        break;

    case ItemType::TeamMember:
    case ItemType::ContactDetail:
    case ItemType::TaskRow:
    case ItemType::UnassignedTasksHeader:
    case ItemType::Invalid:
        break;

    default:
        break;
    }
}

void TeamsView::onTreeExpanded(const QModelIndex& index)
{
    ItemType type = m_model->itemTypeAt(index);

    if (type == ItemType::TeamMember)
    {
        m_model->loadContactDetails(index);
    }
}

void TeamsView::onContextMenu(const QPoint& pos)
{
    QModelIndex index = m_tree->indexAt(pos);

    QMenu menu;

    bool archiveView = m_emergencyManager && m_emergencyManager->isViewingArchive();

    if (!index.isValid())
    {
        if (!archiveView)
        {
            menu.addAction(tr("Add Team..."), this, &TeamsView::addTeam);
        }
    }
    else
    {
        ItemType type = m_model->itemTypeAt(index);

        switch (type)
        {
        case ItemType::Team:
        {
            m_contextTeamId = m_model->teamIdAt(index);
            if (m_contextTeamId)
            {
                menu.addAction(tr("Edit..."), this, &TeamsView::editTeamFromContextMenu);
                menu.addSeparator();
                menu.addAction(tr("Delete"), this, &TeamsView::deleteTeam);
            }
            break;
        }

        case ItemType::TeamMember:
        {
            // Show contact info (disabled) if available
            std::optional<PersonId> personIdOpt = m_model->personIdAt(index);
            if (!personIdOpt)
            {
                break;
            }
            const Document& doc = DocumentManager::instance()->document();
            PersonId personId = *personIdOpt;
            std::optional<Person> personOpt = doc.findPersonById(personId);
            if (personOpt)
            {
                const Phone& phone = personOpt->phone();
                if (!phone.isEmpty())
                {
                    QAction* phoneAction = menu.addAction(phone);
                    phoneAction->setEnabled(false);
                }
                const QString& email = personOpt->email();
                if (!email.isEmpty())
                {
                    QAction* emailAction = menu.addAction(email);
                    emailAction->setEnabled(false);
                }
                if (!phone.isEmpty() || !email.isEmpty())
                {
                    menu.addSeparator();
                }
            }

            m_contextTeamId = m_model->teamIdAt(index);
            m_contextPersonId = personId;
            if (m_contextTeamId)
            {
                // Leader actions
                std::optional<Team> teamOpt = doc.findTeamById(*m_contextTeamId);
                if (teamOpt)
                {
                    bool isLeader = teamOpt->leaderId() && *teamOpt->leaderId() == personId;
                    if (isLeader)
                    {
                        menu.addAction(tr("Clear Leader"), this, &TeamsView::clearLeaderFromContextMenu);
                    }
                    else
                    {
                        menu.addAction(tr("Set as Leader"), this, &TeamsView::setLeaderFromContextMenu);
                    }
                }

                menu.addAction(tr("Remove from Team"), this, &TeamsView::removeMemberFromContextMenu);
            }
            break;
        }

        case ItemType::TaskRow:
        {
            if (m_emergencyManager && m_emergencyManager->isViewingArchive())
            {
                break;
            }
            QString taskIdStr = index.data(TeamsTreeModel::TaskIdRole).toString();
            QString familyIdStr = index.data(TeamsTreeModel::FamilyIdRole).toString();
            if (taskIdStr.isEmpty() || familyIdStr.isEmpty())
            {
                break;
            }

            TaskId taskId = TaskId::fromString(taskIdStr);
            FamilyId familyId = FamilyId::fromString(familyIdStr);
            m_contextFamilyId = familyId;
            m_contextTaskId = taskId;

            // Check if task is assigned to a team
            std::optional<TeamId> taskTeamId = m_model->teamIdAt(index);

            if (!taskTeamId)
            {
                // Unassigned task — offer Assign action
                menu.addAction(tr("Assign to Team..."), this,
                               &TeamsView::assignTaskFromContextMenu);
            }

            // Notify action (only if assigned and not yet notified)
            const FamilyResponseRecord* record = m_emergencyManager->recordForFamily(familyId);
            if (record)
            {
                for (const ResponseTask& task : record->tasks())
                {
                    if (task.id() == taskId)
                    {
                        if (task.isAssigned() && !task.isNotified())
                        {
                            menu.addAction(tr("Notify..."), this,
                                           &TeamsView::notifyTaskFromContextMenu);
                        }
                        if (!task.isResolved())
                        {
                            menu.addAction(tr("Resolve..."), this,
                                           &TeamsView::resolveTaskFromContextMenu);
                        }
                        break;
                    }
                }
            }
            break;
        }

        case ItemType::UnassignedTasksHeader:
        case ItemType::ContactDetail:
        case ItemType::Invalid:
        default:
            break;
        }
    }

    if (!menu.isEmpty())
    {
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }

    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
    m_contextFamilyId = std::nullopt;
    m_contextTaskId = std::nullopt;
}

void TeamsView::expandTeams()
{
    m_tree->expandToDepth(0);
}

void TeamsView::editTeamFromContextMenu()
{
    std::optional<TeamId> teamId = m_contextTeamId;
    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (teamId)
    {
        showTeamDialog(teamId);
    }
}

void TeamsView::setLeaderFromContextMenu()
{
    std::optional<TeamId> teamId = m_contextTeamId;
    std::optional<PersonId> personId = m_contextPersonId;
    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (teamId && personId)
    {
        setLeader(*teamId, *personId);
    }
}

void TeamsView::clearLeaderFromContextMenu()
{
    std::optional<TeamId> teamId = m_contextTeamId;
    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (teamId)
    {
        clearLeader(*teamId);
    }
}

void TeamsView::removeMemberFromContextMenu()
{
    std::optional<TeamId> teamId = m_contextTeamId;
    std::optional<PersonId> personId = m_contextPersonId;
    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
    if (teamId && personId)
    {
        removeMemberFromTeam(*teamId, *personId);
    }
}

void TeamsView::updateButtonStates()
{
    bool hasTeamSelected = selectedTeamId().has_value();
    m_editButton->setEnabled(hasTeamSelected);
    m_deleteButton->setEnabled(hasTeamSelected);
}

std::optional<TeamId> TeamsView::selectedTeamId() const
{
    QModelIndex current = m_tree->currentIndex();
    if (!current.isValid())
    {
        return std::nullopt;
    }
    return m_model->teamIdAt(current);
}

HighlightInfo TeamsView::highlightInfo() const
{
    FamilyAssociation assoc = m_model->relatedFamiliesAt(m_tree->currentIndex());
    return {assoc.relatedFamilyIds, assoc.contactPointFamilyIds};
}

QSet<FamilyId> TeamsView::visibleFamilyIds() const
{
    return {};
}

void TeamsView::clearSelection()
{
    m_tree->clearSelection();
}

void TeamsView::selectFamily(const FamilyId& familyId)
{
    QModelIndex idx = m_model->indexForFamilyId(familyId);
    if (idx.isValid())
    {
        m_tree->setCurrentIndex(idx);
        m_tree->scrollTo(idx);
    }
}

void TeamsView::addTeam()
{
    showTeamDialog(std::nullopt);
}

void TeamsView::editSelectedTeam()
{
    std::optional<TeamId> teamId = selectedTeamId();
    if (teamId)
    {
        showTeamDialog(teamId);
    }
}

void TeamsView::deleteTeam()
{
    std::optional<TeamId> teamId = selectedTeamId();
    if (!teamId)
    {
        return;
    }

    const Document& doc = DocumentManager::instance()->document();
    std::optional<Team> teamOpt = doc.findTeamById(*teamId);
    if (!teamOpt)
    {
        return;
    }

    QString message = tr("Delete team \"%1\"?").arg(teamOpt->name());
    if (QMessageBox::question(this, tr("Delete Team"), message) == QMessageBox::Yes)
    {
        DocumentManager::instance()->executeCommand(
            std::make_unique<DeleteTeamCommand>(*teamOpt));
    }
}

void TeamsView::showTeamDialog(const std::optional<TeamId>& teamId)
{
    QString initialName;
    QList<PersonId> initialIds;

    if (teamId)
    {
        const Document& doc = DocumentManager::instance()->document();
        std::optional<Team> teamOpt = doc.findTeamById(*teamId);
        if (!teamOpt)
        {
            return;
        }
        initialName = teamOpt->name();
        initialIds = teamOpt->memberIds().values();
    }

    std::optional<PersonSelectionResult> result = WardListDialog::selectPersons(
        tr("Team"), initialName, initialIds, this);

    if (!result || result->name.isEmpty())
    {
        return;
    }

    QSet<PersonId> newMembers(result->personIds.begin(), result->personIds.end());

    if (teamId)
    {
        // Edit existing team
        const Document& doc = DocumentManager::instance()->document();
        std::optional<Team> teamOpt = doc.findTeamById(*teamId);
        if (!teamOpt)
        {
            return;
        }

        Team updated = *teamOpt;
        updated.setName(result->name);
        updated.setMemberIds(newMembers);

        // Clear leader if they were removed
        if (updated.leaderId() && !newMembers.contains(*updated.leaderId()))
        {
            updated.setLeaderId(std::nullopt);
        }

        if (updated != *teamOpt)
        {
            DocumentManager::instance()->executeCommand(
                std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
        }
    }
    else
    {
        // Create new team
        Team team = Team::create(result->name, QColor(), std::nullopt);
        team.setMemberIds(newMembers);
        DocumentManager::instance()->executeCommand(
            std::make_unique<AddTeamCommand>(team));
    }
}

void TeamsView::setLeader(const TeamId& teamId, const PersonId& personId)
{
    const Document& doc = DocumentManager::instance()->document();
    std::optional<Team> teamOpt = doc.findTeamById(teamId);
    if (!teamOpt)
    {
        return;
    }

    Team updated = *teamOpt;
    updated.setLeaderId(personId);
    DocumentManager::instance()->executeCommand(
        std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
}

void TeamsView::clearLeader(const TeamId& teamId)
{
    const Document& doc = DocumentManager::instance()->document();
    std::optional<Team> teamOpt = doc.findTeamById(teamId);
    if (!teamOpt)
    {
        return;
    }

    Team updated = *teamOpt;
    updated.setLeaderId(std::nullopt);
    DocumentManager::instance()->executeCommand(
        std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
}

void TeamsView::removeMemberFromTeam(const TeamId& teamId, const PersonId& personId)
{
    DocumentManager::instance()->executeCommand(
        std::make_unique<RemoveTeamMemberCommand>(teamId, personId));
}

void TeamsView::assignTaskFromContextMenu()
{
    std::optional<FamilyId> familyId = m_contextFamilyId;
    std::optional<TaskId> taskId = m_contextTaskId;
    m_contextFamilyId = std::nullopt;
    m_contextTaskId = std::nullopt;
    if (familyId && taskId)
    {
        assignTaskToTeam(*familyId, *taskId);
    }
}

void TeamsView::notifyTaskFromContextMenu()
{
    std::optional<FamilyId> familyId = m_contextFamilyId;
    std::optional<TaskId> taskId = m_contextTaskId;
    m_contextFamilyId = std::nullopt;
    m_contextTaskId = std::nullopt;
    if (familyId && taskId)
    {
        notifyTask(*familyId, *taskId);
    }
}

void TeamsView::resolveTaskFromContextMenu()
{
    std::optional<FamilyId> familyId = m_contextFamilyId;
    std::optional<TaskId> taskId = m_contextTaskId;
    m_contextFamilyId = std::nullopt;
    m_contextTaskId = std::nullopt;
    if (familyId && taskId)
    {
        resolveTask(*familyId, *taskId);
    }
}

void TeamsView::assignTaskToTeam(const FamilyId& familyId, const TaskId& taskId)
{
    const Document& doc = DocumentManager::instance()->document();
    QList<Team> teams = doc.teams().values();

    if (teams.isEmpty())
    {
        QMessageBox::information(this, tr("Assign Task"), tr("No teams available."));
        return;
    }

    // Build team name list
    std::sort(teams.begin(), teams.end(),
              [](const Team& a, const Team& b)
              { return a.name().toLower() < b.name().toLower(); });

    QStringList teamNames;
    for (const Team& team : teams)
    {
        teamNames.append(team.name());
    }

    bool ok = false;
    QString selected = QInputDialog::getItem(
        this, tr("Assign Task"), tr("Select team:"),
        teamNames, 0, false, &ok);

    if (!ok || selected.isEmpty())
    {
        return;
    }

    // Find selected team
    for (const Team& team : teams)
    {
        if (team.name() == selected)
        {
            m_emergencyManager->assignTaskToTeam(familyId, taskId, team.id(), QString());
            return;
        }
    }
}

void TeamsView::notifyTask(const FamilyId& familyId, const TaskId& taskId)
{
    NotifyDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    std::optional<TaskNotification> notification = dialog.result();
    if (notification)
    {
        m_emergencyManager->notifyAssignee(familyId, taskId, *notification);
    }
}

void TeamsView::resolveTask(const FamilyId& familyId, const TaskId& taskId)
{
    bool ok = false;
    QString notes = QInputDialog::getText(
        this, tr("Resolve Task"), tr("Resolution notes (optional):"),
        QLineEdit::Normal, QString(), &ok);

    if (!ok)
    {
        return;
    }

    m_emergencyManager->resolveTask(familyId, taskId, notes.trimmed());
}

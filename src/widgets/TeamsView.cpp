#include "TeamsView.h"
#include "BaseTreeModel.h"
#include "ChoiceDialog.h"
#include "PeoplePickerDialog.h"
#include "DocumentManager.h"
#include "Document.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"
#include "EmergencyActions.h"
#include "EmergencyCommands.h"
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

TeamsView::TeamsView(QWidget* parent)
    : QWidget(parent)
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

    m_setLeaderButton = new QPushButton(tr("Set Leader"));
    m_removeMemberButton = new QPushButton(tr("Remove"));
    m_removeMemberButton->setStyleSheet("color: #c0392b;");

    m_setLeaderButton->setEnabled(false);
    m_removeMemberButton->setEnabled(false);

    toolbar->addWidget(m_addButton);
    toolbar->addWidget(m_editButton);
    toolbar->addWidget(m_deleteButton);
    toolbar->addWidget(m_setLeaderButton);
    toolbar->addWidget(m_removeMemberButton);
    toolbar->addStretch();

    layout->addLayout(toolbar);

    // Create model with filter from FilterBar
    m_model = new TeamsTreeModel(m_filterBar->filter(), this);

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
    connect(m_setLeaderButton, &QPushButton::clicked, this, &TeamsView::onSetLeaderClicked);
    connect(m_removeMemberButton, &QPushButton::clicked, this, &TeamsView::onRemoveMemberClicked);

    connect(m_tree, &SelectionPreservingTreeView::selectionChanged,
            this, &TeamsView::onSelectionChanged);
    connect(m_tree, &QTreeView::doubleClicked,
            this, &TeamsView::onTreeDoubleClicked);
    connect(m_tree, &QTreeView::customContextMenuRequested,
            this, &TeamsView::onContextMenu);

    // Expand top-level items when model is reset, and the Unassigned Tasks section when it
    // comes back
    connect(m_model, &QAbstractItemModel::modelReset, this, &TeamsView::expandTeams);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &TeamsView::onRowsInserted);

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

    if (!index.isValid())
    {
        menu.addAction(tr("Add Team..."), this, &TeamsView::addTeam);
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
            QString taskIdStr = index.data(TeamsTreeModel::TaskIdRole).toString();
            if (taskIdStr.isEmpty())
            {
                break;
            }

            TaskId taskId = TaskId::fromString(taskIdStr);
            m_contextTaskId = taskId;

            // Check if task is assigned to a team
            std::optional<TeamId> taskTeamId = m_model->teamIdAt(index);

            if (!taskTeamId)
            {
                // Unassigned task: offer the Assign action
                menu.addAction(tr("Assign to Team..."), this,
                               &TeamsView::assignTaskFromContextMenu);
            }

            // Notify action (only if assigned and not yet notified). Task rows exist only
            // while an emergency is in progress.
            const ResponseTask* task = EmergencyManager::instance()->response().findTask(taskId);
            if (task)
            {
                if (task->isAssigned() && !task->isNotified())
                {
                    menu.addAction(tr("Notify..."), this,
                                   &TeamsView::notifyTaskFromContextMenu);
                }
                if (!task->isResolved())
                {
                    menu.addAction(tr("Resolve..."), this,
                                   &TeamsView::resolveTaskFromContextMenu);
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
    m_contextTaskId = std::nullopt;
}

void TeamsView::expandTeams()
{
    m_tree->expandToDepth(0);
}

void TeamsView::onRowsInserted(const QModelIndex& parent, int first, int last)
{
    if (parent.isValid())
    {
        return;
    }
    for (int row = first; row <= last; ++row)
    {
        m_tree->expand(m_model->index(row, 0));
    }
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

void TeamsView::onSetLeaderClicked()
{
    QModelIndex index = m_tree->currentIndex();
    if (!index.isValid())
    {
        return;
    }

    ItemType type = m_model->itemTypeAt(index);
    if (type != ItemType::TeamMember)
    {
        return;
    }

    std::optional<TeamId> teamId = m_model->teamIdAt(index);
    std::optional<PersonId> personId = m_model->personIdAt(index);
    if (!teamId || !personId)
    {
        return;
    }

    const Document& doc = DocumentManager::instance()->document();
    std::optional<Team> teamOpt = doc.findTeamById(*teamId);
    if (!teamOpt)
    {
        return;
    }

    bool isLeader = teamOpt->leaderId() && *teamOpt->leaderId() == *personId;
    if (isLeader)
    {
        clearLeader(*teamId);
    }
    else
    {
        setLeader(*teamId, *personId);
    }
}

void TeamsView::onRemoveMemberClicked()
{
    QModelIndex index = m_tree->currentIndex();
    if (!index.isValid())
    {
        return;
    }

    ItemType type = m_model->itemTypeAt(index);
    if (type != ItemType::TeamMember)
    {
        return;
    }

    std::optional<TeamId> teamId = m_model->teamIdAt(index);
    std::optional<PersonId> personId = m_model->personIdAt(index);
    if (teamId && personId)
    {
        removeMemberFromTeam(*teamId, *personId);
    }
}

void TeamsView::updateButtonStates()
{
    QModelIndex index = m_tree->currentIndex();
    ItemType type = ItemType::Invalid;
    if (index.isValid())
    {
        type = m_model->itemTypeAt(index);
    }

    // Team-level buttons
    bool hasTeamSelected = (type == ItemType::Team);
    m_addButton->setEnabled(true);
    m_editButton->setEnabled(hasTeamSelected);
    m_deleteButton->setEnabled(hasTeamSelected);

    // Member-level buttons
    bool hasMemberSelected = (type == ItemType::TeamMember);
    m_removeMemberButton->setEnabled(hasMemberSelected);

    if (hasMemberSelected)
    {
        // Toggle label between Set Leader / Clear Leader
        std::optional<TeamId> teamId = m_model->teamIdAt(index);
        std::optional<PersonId> personId = m_model->personIdAt(index);
        if (teamId && personId)
        {
            const Document& doc = DocumentManager::instance()->document();
            std::optional<Team> teamOpt = doc.findTeamById(*teamId);
            bool isLeader = teamOpt && teamOpt->leaderId() && *teamOpt->leaderId() == *personId;
            m_setLeaderButton->setText(isLeader ? tr("Clear Leader") : tr("Set Leader"));
            m_setLeaderButton->setEnabled(true);
        }
        else
        {
            m_setLeaderButton->setEnabled(false);
        }
    }
    else
    {
        m_setLeaderButton->setText(tr("Set Leader"));
        m_setLeaderButton->setEnabled(false);
    }
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
        DocumentManager::instance()->executeCommand(DeleteTeamCommand::fromSeen(*teamOpt, doc));
    }
}

void TeamsView::showTeamDialog(const std::optional<TeamId>& teamId)
{
    // The team as the dialog opens on it: the edit's start condition.
    std::optional<Team> opened;
    if (teamId)
    {
        opened = DocumentManager::instance()->document().findTeamById(*teamId);
        if (!opened)
        {
            return;
        }
    }

    QString initialName = opened ? opened->name() : QString();
    QList<PersonId> initialIds = opened ? opened->memberIds().values() : QList<PersonId>();
    std::optional<PersonSelectionResult> result = PeoplePickerDialog::selectPersons(
        tr("Team"), initialName, initialIds, this);

    if (!result || result->name.isEmpty())
    {
        return;
    }

    QSet<PersonId> members(result->personIds.begin(), result->personIds.end());
    const Document& doc = DocumentManager::instance()->document();
    if (opened)
    {
        Team saved = *opened;
        saved.setName(result->name);
        saved.setMemberIds(members);
        DocumentManager::instance()->executeCommand(UpdateTeamCommand::fromEdit(*opened, saved, doc));
    }
    else
    {
        Team team = Team::create(result->name);
        team.setMemberIds(members);
        DocumentManager::instance()->executeCommand(AddTeamCommand::fromNew(team, doc));
    }
}

void TeamsView::setLeader(const TeamId& teamId, const PersonId& personId)
{
    const Document& doc = DocumentManager::instance()->document();
    std::optional<Team> team = doc.findTeamById(teamId);
    if (!team)
    {
        return;
    }

    Team saved = *team;
    saved.setLeaderId(personId);
    DocumentManager::instance()->executeCommand(UpdateTeamCommand::fromEdit(*team, saved, doc));
}

void TeamsView::clearLeader(const TeamId& teamId)
{
    const Document& doc = DocumentManager::instance()->document();
    std::optional<Team> team = doc.findTeamById(teamId);
    if (!team)
    {
        return;
    }

    Team saved = *team;
    saved.setLeaderId(std::nullopt);
    DocumentManager::instance()->executeCommand(UpdateTeamCommand::fromEdit(*team, saved, doc));
}

void TeamsView::removeMemberFromTeam(const TeamId& teamId, const PersonId& personId)
{
    const Document& doc = DocumentManager::instance()->document();
    std::optional<Team> team = doc.findTeamById(teamId);
    if (!team)
    {
        return;
    }

    Team saved = *team;
    QSet<PersonId> members = team->memberIds();
    members.remove(personId);
    saved.setMemberIds(members);
    DocumentManager::instance()->executeCommand(UpdateTeamCommand::fromEdit(*team, saved, doc));
}

void TeamsView::assignTaskFromContextMenu()
{
    std::optional<TaskId> taskId = m_contextTaskId;
    m_contextTaskId = std::nullopt;
    if (taskId)
    {
        assignTaskToTeam(*taskId);
    }
}

void TeamsView::notifyTaskFromContextMenu()
{
    std::optional<TaskId> taskId = m_contextTaskId;
    m_contextTaskId = std::nullopt;
    if (taskId)
    {
        EmergencyActions::notifyAssignee(*taskId, this);
    }
}

void TeamsView::resolveTaskFromContextMenu()
{
    std::optional<TaskId> taskId = m_contextTaskId;
    m_contextTaskId = std::nullopt;
    if (taskId)
    {
        EmergencyActions::resolveTask(*taskId, this);
    }
}

namespace
{

bool byName(const QPair<QString, QString>& a, const QPair<QString, QString>& b)
{
    return a.first.toLower() < b.first.toLower();
}

}  // namespace

void TeamsView::assignTaskToTeam(const TaskId& taskId)
{
    EmergencyManager* manager = EmergencyManager::instance();
    const ResponseTask* found = manager->isActive() ? manager->response().findTask(taskId) : nullptr;
    if (!found)
    {
        return;
    }
    // The task as the dialog opens on it: the edit's start condition
    ResponseTask opened = *found;

    QList<QPair<QString, QString>> choices;
    const QHash<TeamId, Team>& teams = DocumentManager::instance()->document().teams();
    for (auto it = teams.constBegin(); it != teams.constEnd(); ++it)
    {
        choices.append({it.value().name(), it.key().toString()});
    }
    if (choices.isEmpty())
    {
        QMessageBox::information(this, tr("Assign Task"), tr("No teams available."));
        return;
    }
    std::sort(choices.begin(), choices.end(), &byName);

    std::optional<QString> teamId = ChoiceDialog::choose(this, tr("Assign Task"), tr("Select team:"), choices);
    std::optional<Team> team = teamId ? DocumentManager::instance()->document().findTeamById(TeamId::fromString(*teamId))
                                      : std::nullopt;
    if (!team)
    {
        return;
    }
    ResponseTask saved = opened;
    saved.setAssignment(TaskAssignment::toTeam(*team));
    DocumentManager::instance()->executeCommand(UpdateTaskCommand::fromEdit(opened, saved));
}

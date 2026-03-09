#include "TeamsView.h"
#include "BaseTreeModel.h"
#include "WardListDialog.h"
#include "DocumentManager.h"
#include "Document.h"
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

TeamsView::TeamsView(DocumentManager* documentManager,
                     QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // FilterBar owns Filter
    m_filterBar = new FilterBar(documentManager, this);
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
    m_model = new TeamsTreeModel(documentManager, m_filterBar->filter(), this);

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
    case ItemType::Invalid:
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
            const Document& doc = m_documentManager->document();
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

        case ItemType::ContactDetail:
        case ItemType::Invalid:
            break;
        }
    }

    if (!menu.isEmpty())
    {
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }

    m_contextTeamId = std::nullopt;
    m_contextPersonId = std::nullopt;
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

    const Document& doc = m_documentManager->document();
    std::optional<Team> teamOpt = doc.findTeamById(*teamId);
    if (!teamOpt)
    {
        return;
    }

    QString message = tr("Delete team \"%1\"?").arg(teamOpt->name());
    if (QMessageBox::question(this, tr("Delete Team"), message) == QMessageBox::Yes)
    {
        m_documentManager->executeCommand(
            std::make_unique<DeleteTeamCommand>(*teamOpt));
    }
}

void TeamsView::showTeamDialog(const std::optional<TeamId>& teamId)
{
    QString initialName;
    QList<PersonId> initialIds;

    if (teamId)
    {
        const Document& doc = m_documentManager->document();
        std::optional<Team> teamOpt = doc.findTeamById(*teamId);
        if (!teamOpt)
        {
            return;
        }
        initialName = teamOpt->name();
        initialIds = teamOpt->memberIds().values();
    }

    std::optional<PersonSelectionResult> result = WardListDialog::selectPersons(
        m_documentManager, tr("Team"), initialName, initialIds, this);

    if (!result || result->name.isEmpty())
    {
        return;
    }

    QSet<PersonId> newMembers(result->personIds.begin(), result->personIds.end());

    if (teamId)
    {
        // Edit existing team
        const Document& doc = m_documentManager->document();
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
            m_documentManager->executeCommand(
                std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
        }
    }
    else
    {
        // Create new team
        Team team = Team::create(result->name);
        team.setMemberIds(newMembers);
        m_documentManager->executeCommand(
            std::make_unique<AddTeamCommand>(team));
    }
}

void TeamsView::setLeader(const TeamId& teamId, const PersonId& personId)
{
    const Document& doc = m_documentManager->document();
    std::optional<Team> teamOpt = doc.findTeamById(teamId);
    if (!teamOpt)
    {
        return;
    }

    Team updated = *teamOpt;
    updated.setLeaderId(personId);
    m_documentManager->executeCommand(
        std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
}

void TeamsView::clearLeader(const TeamId& teamId)
{
    const Document& doc = m_documentManager->document();
    std::optional<Team> teamOpt = doc.findTeamById(teamId);
    if (!teamOpt)
    {
        return;
    }

    Team updated = *teamOpt;
    updated.setLeaderId(std::nullopt);
    m_documentManager->executeCommand(
        std::make_unique<UpdateTeamCommand>(*teamOpt, updated));
}

void TeamsView::removeMemberFromTeam(const TeamId& teamId, const PersonId& personId)
{
    m_documentManager->executeCommand(
        std::make_unique<RemoveTeamMemberCommand>(teamId, personId));
}

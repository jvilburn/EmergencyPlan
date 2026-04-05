#pragma once

#include "FamilyMarkerProvider.h"
#include "Id.h"

#include <QWidget>

#include <optional>

class FilterBar;
class QPushButton;
class QModelIndex;
class SelectionPreservingTreeView;
class TeamsTreeModel;

/// TeamsView displays a 2-level tree of teams and their members.
/// Owns FilterBar (which owns Filter) and TeamsTreeModel internally.
/// Includes a toolbar with Add, Edit, Delete buttons.
/// During emergencies, shows task rows under each team with context menu actions.
class TeamsView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit TeamsView(QWidget* parent);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;
    void clearSelection() override;
    void selectFamily(const FamilyId& familyId) override;

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged();
    void onTreeDoubleClicked(const QModelIndex& index);
    void onTreeExpanded(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);
    void expandTeams();
    void editTeamFromContextMenu();
    void setLeaderFromContextMenu();
    void clearLeaderFromContextMenu();
    void removeMemberFromContextMenu();

private:
    void updateButtonStates();

    void addTeam();
    void editSelectedTeam();
    void deleteTeam();
    void showTeamDialog(const std::optional<TeamId>& teamId);
    void setLeader(const TeamId& teamId, const PersonId& personId);
    void clearLeader(const TeamId& teamId);
    void removeMemberFromTeam(const TeamId& teamId, const PersonId& personId);

    // Task context menu slots
    void assignTaskFromContextMenu();
    void notifyTaskFromContextMenu();
    void resolveTaskFromContextMenu();

    // Task actions
    void assignTaskToTeam(const FamilyId& familyId, const TaskId& taskId);
    void notifyTask(const FamilyId& familyId, const TaskId& taskId);
    void resolveTask(const FamilyId& familyId, const TaskId& taskId);

    std::optional<TeamId> selectedTeamId() const;

    FilterBar* m_filterBar;
    TeamsTreeModel* m_model;
    SelectionPreservingTreeView* m_tree;

    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;

    // Context menu state
    std::optional<TeamId> m_contextTeamId;
    std::optional<PersonId> m_contextPersonId;
    std::optional<FamilyId> m_contextFamilyId;
    std::optional<TaskId> m_contextTaskId;
};

#pragma once

#include "Id.h"

#include <QAbstractTableModel>
#include <optional>

class TaskListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        ResolvedCol = 0,
        AssignedToCol,
        CategoryCol,
        FamilyCol,
        DescriptionCol,
        ColumnCount
    };

    explicit TaskListModel(QObject* parent);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    FamilyId familyIdForRow(int row) const;
    TaskId taskIdForRow(int row) const;
    bool isAssignedForRow(int row) const;
    bool isNotifiedForRow(int row) const;

    void rebuild();

private:
    struct TaskEntry
    {
        FamilyId familyId;
        QString familyName;
        TaskId taskId;
        QString category;
        QString description;
        std::optional<TeamId> assignedTeamId;
        std::optional<PersonId> assignedPersonId;
        bool resolved = false;
        bool notified = false;
    };

    static bool taskEntryLessThan(const TaskEntry& a, const TaskEntry& b);
    static QString resolveAssignedName(const std::optional<TeamId>& teamId,
                                      const std::optional<PersonId>& personId);

    QList<TaskEntry> m_entries;
};

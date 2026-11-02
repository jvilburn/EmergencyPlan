#pragma once

#include "DocumentChange.h"
#include "Id.h"

#include <QAbstractTableModel>

class ResponseTask;

/// The table of every task in the emergency in progress, sorted unresolved first, then by
/// assignee (assigned before unassigned), category and family. A change to one family's tasks
/// replaces only that family's rows, so selection follows the rows that stay; an emergency
/// starting or ending, or a full document change, rebuilds the table.
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

    /// Resolves or reopens the row's task from its check box. True when the edit applied.
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    FamilyId familyIdForRow(int row) const;
    TaskId taskIdForRow(int row) const;
    bool isAssignedForRow(int row) const;
    bool isNotifiedForRow(int row) const;

private slots:
    void onDocumentChanged(const DocumentChange& change);
    void rebuild();
    void replaceFamilyRows(const FamilyId& familyId);

private:
    /// One task's row. The names are the family's and the assignee's as the document held
    /// them when the row was made, for display and sorting.
    struct TaskEntry
    {
        FamilyId familyId;
        QString familyName;
        TaskId taskId;
        QString category;
        QString description;
        bool assigned = false;
        QString assigneeName;
        bool resolved = false;
        bool notified = false;
    };

    static TaskEntry entryFor(const ResponseTask& task);
    static bool taskEntryLessThan(const TaskEntry& a, const TaskEntry& b);

    /// The row where entry belongs among the others, which are in order.
    int sortedRow(const TaskEntry& entry) const;

    QList<TaskEntry> m_entries;
};

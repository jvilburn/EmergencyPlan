#include "TreeViewStateTest.h"
#include "BaseTreeModel.h"
#include "SelectionPreservingTreeView.h"

#include <QStringList>
#include <QTest>

namespace
{

/// A two-level tree: each top-level row named in groups, with two child rows under it,
/// "<name>1" and "<name>2". A row's key is its text.
class GroupsModel : public BaseTreeModel
{
public:
    explicit GroupsModel(const QStringList& groups)
        : BaseTreeModel(nullptr)
        , m_groups(groups)
    {
    }

    /// Replaces the groups in one reset, as a model's rebuild does.
    void rebuild(const QStringList& groups)
    {
        beginResetModel();
        m_groups = groups;
        endResetModel();
    }

    QModelIndex index(int row, int column, const QModelIndex& parent) const override
    {
        if (!hasIndex(row, column, parent))
        {
            return QModelIndex();
        }
        // A child's internal id is its group's row plus one; a group's is 0.
        return createIndex(row, column, parent.isValid() ? quintptr(parent.row() + 1) : quintptr(0));
    }

    QModelIndex parent(const QModelIndex& child) const override
    {
        if (!child.isValid() || child.internalId() == 0)
        {
            return QModelIndex();
        }
        return createIndex(int(child.internalId() - 1), 0, quintptr(0));
    }

    int rowCount(const QModelIndex& parent) const override
    {
        if (!parent.isValid())
        {
            return int(m_groups.size());
        }
        return parent.internalId() == 0 ? 2 : 0;
    }

    int columnCount(const QModelIndex&) const override { return 1; }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (role != Qt::DisplayRole)
        {
            return QVariant();
        }
        return textAt(index);
    }

    ItemType itemTypeAt(const QModelIndex& index) const override
    {
        return index.internalId() == 0 ? ItemType::Team : ItemType::TeamMember;
    }

    SelectionKey selectionKeyAt(const QModelIndex& index) const override
    {
        return SelectionKey::literal(textAt(index));
    }

private:
    QString textAt(const QModelIndex& index) const
    {
        if (index.internalId() == 0)
        {
            return m_groups.at(index.row());
        }
        return m_groups.at(int(index.internalId() - 1)) + QString::number(index.row() + 1);
    }

    QStringList m_groups;
};

/// The keys of view's expanded top-level rows, in order.
QStringList expandedGroups(const SelectionPreservingTreeView& view, const GroupsModel& model)
{
    QStringList groups;
    for (int row = 0; row < model.rowCount(QModelIndex()); ++row)
    {
        QModelIndex index = model.index(row, 0, QModelIndex());
        if (view.isExpanded(index))
        {
            groups.append(index.data().toString());
        }
    }
    return groups;
}

}  // namespace

TreeViewStateTest::TreeViewStateTest(QObject* parent)
    : QObject(parent)
{
}

void TreeViewStateTest::rebuildKeepsTheSelectedAndExpandedRows()
{
    GroupsModel model({"Bravo", "Delta"});
    SelectionPreservingTreeView view(&model, nullptr);
    view.expand(model.index(1, 0, QModelIndex()));
    view.setCurrentIndex(model.index(1, 0, model.index(1, 0, QModelIndex())));
    QCOMPARE(view.currentIndex().data().toString(), QString("Delta2"));

    // Alpha comes in above both, so every row moves down one.
    model.rebuild({"Alpha", "Bravo", "Delta"});

    QCOMPARE(expandedGroups(view, model), QStringList({"Delta"}));
    QCOMPARE(view.currentIndex().data().toString(), QString("Delta2"));
}

void TreeViewStateTest::rebuildExpandsNewTopLevelRowsOnlyWhenAsked()
{
    GroupsModel model({"Bravo", "Delta"});
    SelectionPreservingTreeView view(&model, nullptr);
    view.expand(model.index(0, 0, QModelIndex()));

    model.rebuild({"Bravo", "Charlie", "Delta"});
    QCOMPARE(expandedGroups(view, model), QStringList({"Bravo"}));

    view.setExpandsNewTopLevelRows(true);
    model.rebuild({"Alpha", "Bravo", "Charlie", "Delta"});
    QCOMPARE(expandedGroups(view, model), QStringList({"Alpha", "Bravo"}));
}

#pragma once

#include "Id.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QTreeView>
#include <optional>

class BaseTreeModel;

/// QTreeView subclass that keeps what the user sees across model rebuilds: before a reset it
/// saves the keys of the selected row, of every expanded row and of the row at the top, and
/// after it selects, expands and scrolls to the rows with those keys, so a model can rebuild
/// on any change.
///
/// Detail row behavior: Clicking a detail row (ContactDetail, Address, etc.)
/// automatically selects the parent row instead. This is determined by
/// itemTypeAt() == ContactDetail check - detail rows are redirected to parent.
///
/// Requires a model that implements BaseTreeModel interface.
/// Views can query the model directly for itemTypeAt(currentIndex()).
class SelectionPreservingTreeView : public QTreeView
{
    Q_OBJECT

public:
    /// Construct with a model implementing BaseTreeModel.
    /// The model must also inherit from QAbstractItemModel.
    explicit SelectionPreservingTreeView(BaseTreeModel* model, QWidget* parent);

    /// Clear tree selection and emit selectionChanged
    void clearSelection();

    /// When set, a rebuild also expands each top-level row that was not there before it, as
    /// for a team or asset just added to a list shown expanded.
    void setExpandsNewTopLevelRows(bool expands) { m_expandsNewTopLevelRows = expands; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

signals:
    /// Emitted when selection changes (after any detail-to-parent redirect)
    void selectionChanged();

private slots:
    void onCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
    void onModelAboutToBeReset();
    void onModelReset();

private:
    /// Find index matching selection key, or invalid index if not found
    QModelIndex findIndex(const SelectionKey& key) const;

    /// Recursively search for matching index
    QModelIndex findIndexRecursive(const QModelIndex& parent, const SelectionKey& key) const;

    /// Appends the keys of the expanded rows under parent, each before those under it.
    void collectExpandedKeys(const QModelIndex& parent, QList<SelectionKey>& keys) const;

    BaseTreeModel* m_typedModel;

    // Saved before a model reset and restored after
    std::optional<SelectionKey> m_savedKey;
    QList<SelectionKey> m_expandedKeys;
    std::optional<SelectionKey> m_topKey;
    QList<SelectionKey> m_topLevelKeys;
    bool m_expandsNewTopLevelRows = false;
};

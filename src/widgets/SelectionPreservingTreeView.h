#pragma once

#include <QTreeView>

class BaseTreeModel;

/// QTreeView subclass that preserves selection across model rebuilds.
/// Saves selected item's key before model reset, restores after.
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
    explicit SelectionPreservingTreeView(BaseTreeModel* model, QWidget* parent = nullptr);

signals:
    /// Emitted when selection changes (after any detail-to-parent redirect)
    void selectionChanged();

private slots:
    void onCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
    void onModelAboutToBeReset();
    void onModelReset();

private:
    /// Find index matching selection key, or invalid index if not found
    QModelIndex findIndex(const QString& key) const;

    /// Recursively search for matching index
    QModelIndex findIndexRecursive(const QModelIndex& parent, const QString& key) const;

    BaseTreeModel* m_typedModel;

    // Saved selection for restoration after model reset
    QString m_savedKey;
};

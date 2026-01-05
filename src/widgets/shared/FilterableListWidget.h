#pragma once

#include <QWidget>
#include <QStringList>

class QListView;
class QPushButton;
class QAbstractItemModel;
class QItemSelectionModel;
class SearchField;

/// Core reusable widget for search + filter + list functionality.
/// Used by WardListView (sidebar) and SelectionDialog (dialogs).
class FilterableListWidget : public QWidget
{
    Q_OBJECT

public:
    enum SelectionMode
    {
        SingleSelect,   ///< Click selects one item (for browsing)
        MultiSelect     ///< Click toggles selection (for dialogs)
    };
    Q_ENUM(SelectionMode)

    explicit FilterableListWidget(QWidget* parent = nullptr);
    ~FilterableListWidget();

    /// Set the model to display. The model must have an IdRole.
    void setModel(QAbstractItemModel* model);
    QAbstractItemModel* model() const;

    /// Set the role that provides the item ID (default: Qt::UserRole + 1).
    void setIdRole(int role);

    /// Set selection mode (single vs multi-select with checkboxes).
    void setSelectionMode(SelectionMode mode);
    SelectionMode selectionMode() const;

    /// Pre-select items by ID (mainly for MultiSelect mode).
    void setSelectedIds(const QStringList& ids);

    /// Get all selected IDs (for MultiSelect mode).
    QStringList selectedIds() const;

    /// Get the current single selection (for SingleSelect mode).
    QString currentId() const;

    /// Select a specific item by ID and scroll to it.
    void selectById(const QString& id);

    /// Get the search field for external connections.
    SearchField* searchField() const;

signals:
    /// Emitted when selection changes (provides all selected IDs).
    void selectionChanged(const QStringList& ids);

    /// Emitted when search text changes (debounced).
    void searchTextChanged(const QString& text);

private slots:
    void onSearchTextChanged(const QString& text);
    void onSelectionChanged();

private:
    void setupUi();
    void setupConnections();
    int rowForId(const QString& id) const;

    // UI components
    SearchField* m_searchField;
    QPushButton* m_addFilterButton;
    QListView* m_listView;

    // State
    QAbstractItemModel* m_model = nullptr;
    int m_idRole = Qt::UserRole + 1;
    SelectionMode m_selectionMode = SingleSelect;
};

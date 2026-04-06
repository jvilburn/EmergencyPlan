#pragma once

#include <QWidget>
#include <functional>

class Filter;
class FilterChip;
class SearchField;
class QHBoxLayout;
class QMenu;
class QPushButton;
enum class ResponseArea;

/// Unified filter UI component with search field, filter chips, and add filter menu.
/// Owns its Filter object internally - access via filter().
class FilterBar : public QWidget
{
    Q_OBJECT

public:
    explicit FilterBar(QWidget* parent);

    /// Get the Filter object for connecting to models.
    Filter* filter() const { return m_filter; }

private slots:
    void onSearchTextChanged(const QString& text);
    void onFilterChanged();
    void onAddFilterClicked();
    void onClearAllClicked();
    void onContactFilterTriggered(bool checked);
    void onUnmappedFilterTriggered(bool checked);

private:
    void rebuildChips();
    void showAddFilterMenu();

    // Add filter menu helpers
    void addTagSubmenu(QMenu* menu);
    void addTeamSubmenu(QMenu* menu);
    void addCallingSubmenu(QMenu* menu);
    void addGenderSubmenu(QMenu* menu);
    void addAgeSubmenu(QMenu* menu);
    void addSpecialNeedsSubmenu(QMenu* menu);
    void addResponseAreaSubmenu(QMenu* menu);
    void addResponseAreaItems(QMenu* menu, ResponseArea area);

    void addChip(const QString& label, const QString& value,
                 std::function<void()> removeCallback);

    Filter* m_filter;
    SearchField* m_searchField;
    QWidget* m_chipsContainer;
    QHBoxLayout* m_chipsLayout;
    QPushButton* m_addFilterButton;
    QPushButton* m_clearAllButton;

    QList<FilterChip*> m_chips;
};

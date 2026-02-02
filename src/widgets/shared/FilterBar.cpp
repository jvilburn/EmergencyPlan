#include "FilterBar.h"
#include "DocumentManager.h"
#include "Document.h"
#include "Filter.h"
#include "FilterChip.h"
#include "SearchField.h"
#include "AppStyles.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QPushButton>

FilterBar::FilterBar(DocumentManager* docMgr, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(docMgr)
    , m_filter(new Filter(this))
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    // Search field
    m_searchField = new SearchField(this);
    mainLayout->addWidget(m_searchField);

    // Button row: [+ Add Filter] [× Clear All (N)]
    QHBoxLayout* buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(8);

    m_addFilterButton = new QPushButton(tr("+ Add Filter"), this);
    m_addFilterButton->setStyleSheet(AppStyles::beveledButton());
    buttonRow->addWidget(m_addFilterButton);

    m_clearAllButton = new QPushButton(tr("× Clear All"), this);
    m_clearAllButton->setStyleSheet(AppStyles::beveledButton());
    m_clearAllButton->setVisible(false);
    buttonRow->addWidget(m_clearAllButton);

    buttonRow->addStretch();
    mainLayout->addLayout(buttonRow);

    // Chips container
    m_chipsContainer = new QWidget(this);
    m_chipsLayout = new QHBoxLayout(m_chipsContainer);
    m_chipsLayout->setContentsMargins(0, 0, 0, 0);
    m_chipsLayout->setSpacing(4);
    m_chipsLayout->addStretch();
    m_chipsContainer->setVisible(false);
    mainLayout->addWidget(m_chipsContainer);

    // Connections
    connect(m_searchField, &SearchField::searchTextChanged,
            this, &FilterBar::onSearchTextChanged);
    connect(m_filter, &Filter::changed,
            this, &FilterBar::onFilterChanged);
    connect(m_addFilterButton, &QPushButton::clicked,
            this, &FilterBar::onAddFilterClicked);
    connect(m_clearAllButton, &QPushButton::clicked,
            this, &FilterBar::onClearAllClicked);
}

void FilterBar::onSearchTextChanged(const QString& text)
{
    m_filter->setSearchText(text);
}

void FilterBar::onFilterChanged()
{
    rebuildChips();
}

void FilterBar::onAddFilterClicked()
{
    showAddFilterMenu();
}

void FilterBar::onClearAllClicked()
{
    m_filter->clear();
    m_searchField->clear();
}

void FilterBar::rebuildChips()
{
    // Clear existing chips
    for (FilterChip* chip : m_chips)
    {
        m_chipsLayout->removeWidget(chip);
        chip->deleteLater();
    }
    m_chips.clear();

    // TODO: Add chips for each active filter
    // This will be implemented in Task 6

    // Update visibility
    bool hasChips = !m_chips.isEmpty();
    m_chipsContainer->setVisible(hasChips);

    // Update clear all button
    int filterCount = m_chips.size();
    if (!m_filter->searchText().isEmpty())
    {
        filterCount++;
    }
    m_clearAllButton->setVisible(filterCount > 0);
    m_clearAllButton->setText(tr("× Clear All (%1)").arg(filterCount));
}

void FilterBar::showAddFilterMenu()
{
    QMenu menu(this);

    addTagSubmenu(&menu);
    addTeamSubmenu(&menu);
    addCallingSubmenu(&menu);

    menu.addSeparator();

    addGenderSubmenu(&menu);
    addAgeSubmenu(&menu);

    menu.addSeparator();

    addSpecialNeedsSubmenu(&menu);
    addResponseAreaSubmenu(&menu);

    menu.addSeparator();

    // Boolean filters
    QAction* contactAction = menu.addAction(tr("Has Contact Info"));
    contactAction->setCheckable(true);
    contactAction->setChecked(m_filter->onlyWithContact());
    connect(contactAction, &QAction::triggered, this, [this](bool checked)
    {
        m_filter->setOnlyWithContact(checked);
    });

    QAction* unmappedAction = menu.addAction(tr("Unmapped Only"));
    unmappedAction->setCheckable(true);
    unmappedAction->setChecked(m_filter->mappedFilter() == MappedFilter::Unmapped);
    connect(unmappedAction, &QAction::triggered, this, [this](bool checked)
    {
        m_filter->setMappedFilter(checked ? MappedFilter::Unmapped : MappedFilter::All);
    });

    menu.exec(m_addFilterButton->mapToGlobal(
        QPoint(0, m_addFilterButton->height())));
}

// Submenu stubs - will be implemented in Task 6
void FilterBar::addTagSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Tag"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addTeamSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Team"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addCallingSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Calling"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addGenderSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Gender"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addAgeSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Age"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addSpecialNeedsSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Special Needs"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addResponseAreaSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Response Area"));
    sub->addAction(tr("(Coming soon)"))->setEnabled(false);
}

void FilterBar::addResponseAreaItems(QMenu* /*menu*/, ResponseArea /*area*/)
{
    // Will be implemented in Task 6
}

void FilterBar::addChip(const QString& label, const QString& value,
                        std::function<void()> removeCallback)
{
    FilterChip* chip = new FilterChip(label, value, m_chipsContainer);
    connect(chip, &FilterChip::removeClicked, this, removeCallback);

    // Insert before the stretch
    m_chipsLayout->insertWidget(m_chipsLayout->count() - 1, chip);
    m_chips.append(chip);
}

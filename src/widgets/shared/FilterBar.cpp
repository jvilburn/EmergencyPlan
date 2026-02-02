#include "FilterBar.h"
#include "DocumentManager.h"
#include "Document.h"
#include "EmergencyResource.h"
#include "Family.h"
#include "Filter.h"
#include "FilterChip.h"
#include "Gender.h"
#include "Person.h"
#include "ResponseArea.h"
#include "SearchField.h"
#include "Tag.h"
#include "Team.h"
#include "AppStyles.h"

#include <algorithm>
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

void FilterBar::onContactFilterTriggered(bool checked)
{
    m_filter->setOnlyWithContact(checked);
}

void FilterBar::onUnmappedFilterTriggered(bool checked)
{
    m_filter->setMappedFilter(checked ? MappedFilter::Unmapped : MappedFilter::All);
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

    const Document& doc = m_documentManager->document();

    // Tags
    for (const QString& tagId : m_filter->tagIds())
    {
        std::optional<Tag> tag = doc.findTagById(tagId);
        if (tag)
        {
            addChip(tr("Tag"), tag->name(), [this, tagId]() {
                QSet<QString> ids = m_filter->tagIds();
                ids.remove(tagId);
                m_filter->setTagIds(ids);
            });
        }
    }

    // Teams
    for (const QString& teamId : m_filter->teamIds())
    {
        std::optional<Team> team = doc.findTeamById(teamId);
        if (team)
        {
            addChip(tr("Team"), team->name(), [this, teamId]() {
                QSet<QString> ids = m_filter->teamIds();
                ids.remove(teamId);
                m_filter->setTeamIds(ids);
            });
        }
    }

    // Callings
    for (const QString& calling : m_filter->callings())
    {
        addChip(tr("Calling"), calling, [this, calling]() {
            QSet<QString> callings = m_filter->callings();
            callings.remove(calling);
            m_filter->setCallings(callings);
        });
    }

    // Gender
    if (m_filter->gender().has_value())
    {
        Gender gender = m_filter->gender().value();
        addChip("", gender.toString(), [this]() {
            m_filter->setGender(std::nullopt);
        });
    }

    // Age filter
    if (m_filter->ageFilter() == AgeFilter::Adults)
    {
        addChip("", tr("Adults"), [this]() {
            m_filter->setAgeFilter(AgeFilter::All);
        });
    }
    else if (m_filter->ageFilter() == AgeFilter::Children)
    {
        addChip("", tr("Children"), [this]() {
            m_filter->setAgeFilter(AgeFilter::All);
        });
    }

    // Special needs - "any" flag
    if (m_filter->hasAnySpecialNeed())
    {
        addChip(tr("Needs"), tr("(All)"), [this]() {
            m_filter->setHasAnySpecialNeed(false);
        });
    }

    // Special needs - specific
    for (const QString& need : m_filter->specialNeeds())
    {
        addChip(tr("Need"), need, [this, need]() {
            m_filter->removeSpecialNeed(need);
        });
    }

    // Response areas
    for (const ResponseArea& area : m_filter->responseAreas())
    {
        QString areaName;
        switch (area)
        {
            case ResponseArea::Medical:
                areaName = tr("Medical");
                break;
            case ResponseArea::Communications:
                areaName = tr("Communications");
                break;
            case ResponseArea::Recovery:
                areaName = tr("Recovery");
                break;
            default:
                continue;
        }
        addChip(tr("Area"), areaName, [this, area]() {
            m_filter->removeResponseArea(area);
        });
    }

    // Resource IDs
    for (const QString& resourceId : m_filter->resourceTypeIds())
    {
        std::optional<EmergencyResource> resource = doc.findEmergencyResourceById(resourceId);
        if (resource)
        {
            addChip(tr("Resource"), resource->name(), [this, resourceId]() {
                QSet<QString> ids = m_filter->resourceTypeIds();
                ids.remove(resourceId);
                m_filter->setResourceTypeIds(ids);
            });
        }
    }

    // Has contact info
    if (m_filter->onlyWithContact())
    {
        addChip("", tr("Has Contact"), [this]() {
            m_filter->setOnlyWithContact(false);
        });
    }

    // Unmapped only
    if (m_filter->mappedFilter() == MappedFilter::Unmapped)
    {
        addChip("", tr("Unmapped"), [this]() {
            m_filter->setMappedFilter(MappedFilter::All);
        });
    }

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
    connect(contactAction, &QAction::triggered,
            this, &FilterBar::onContactFilterTriggered);

    QAction* unmappedAction = menu.addAction(tr("Unmapped Only"));
    unmappedAction->setCheckable(true);
    unmappedAction->setChecked(m_filter->mappedFilter() == MappedFilter::Unmapped);
    connect(unmappedAction, &QAction::triggered,
            this, &FilterBar::onUnmappedFilterTriggered);

    menu.exec(m_addFilterButton->mapToGlobal(
        QPoint(0, m_addFilterButton->height())));
}

void FilterBar::addTagSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Tag"));

    const Document& doc = m_documentManager->document();
    QList<Tag> tags = doc.tags().values();
    std::sort(tags.begin(), tags.end(), [](const Tag& a, const Tag& b) {
        return a.name().toLower() < b.name().toLower();
    });

    if (tags.isEmpty())
    {
        sub->addAction(tr("(No tags)"))->setEnabled(false);
        return;
    }

    for (const Tag& tag : tags)
    {
        QAction* action = sub->addAction(tag.name());
        action->setCheckable(true);
        action->setChecked(m_filter->tagIds().contains(tag.id()));
        connect(action, &QAction::triggered, this, [this, tagId = tag.id()](bool checked) {
            QSet<QString> ids = m_filter->tagIds();
            if (checked)
            {
                ids.insert(tagId);
            }
            else
            {
                ids.remove(tagId);
            }
            m_filter->setTagIds(ids);
        });
    }
}

void FilterBar::addTeamSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Team"));

    const Document& doc = m_documentManager->document();
    QList<Team> teams = doc.teams().values();
    std::sort(teams.begin(), teams.end(), [](const Team& a, const Team& b) {
        return a.name().toLower() < b.name().toLower();
    });

    if (teams.isEmpty())
    {
        sub->addAction(tr("(No teams)"))->setEnabled(false);
        return;
    }

    for (const Team& team : teams)
    {
        QAction* action = sub->addAction(team.name());
        action->setCheckable(true);
        action->setChecked(m_filter->teamIds().contains(team.id()));
        connect(action, &QAction::triggered, this, [this, teamId = team.id()](bool checked) {
            QSet<QString> ids = m_filter->teamIds();
            if (checked)
            {
                ids.insert(teamId);
            }
            else
            {
                ids.remove(teamId);
            }
            m_filter->setTeamIds(ids);
        });
    }
}

void FilterBar::addCallingSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Calling"));

    // Gather all unique callings from document
    const Document& doc = m_documentManager->document();
    QSet<QString> allCallings;
    for (const Family& family : doc.families())
    {
        for (const QString& calling : family.allCallings())
        {
            allCallings.insert(calling);
        }
    }

    QStringList callingList = allCallings.values();
    callingList.sort(Qt::CaseInsensitive);

    if (callingList.isEmpty())
    {
        sub->addAction(tr("(No callings)"))->setEnabled(false);
        return;
    }

    for (const QString& calling : callingList)
    {
        QString displayName = calling;
        if (displayName.length() > 30)
        {
            displayName = displayName.left(27) + "...";
        }

        QAction* action = sub->addAction(displayName);
        action->setCheckable(true);
        action->setChecked(m_filter->callings().contains(calling));
        connect(action, &QAction::triggered, this, [this, calling](bool checked) {
            QSet<QString> callings = m_filter->callings();
            if (checked)
            {
                callings.insert(calling);
            }
            else
            {
                callings.remove(calling);
            }
            m_filter->setCallings(callings);
        });
    }
}

void FilterBar::addGenderSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Gender"));

    std::optional<Gender> currentGender = m_filter->gender();

    QAction* maleAction = sub->addAction(tr("Male"));
    maleAction->setCheckable(true);
    maleAction->setChecked(currentGender.has_value() && currentGender.value() == Gender::Male);
    connect(maleAction, &QAction::triggered, this, [this](bool checked) {
        m_filter->setGender(checked ? std::optional<Gender>(Gender::Male) : std::nullopt);
    });

    QAction* femaleAction = sub->addAction(tr("Female"));
    femaleAction->setCheckable(true);
    femaleAction->setChecked(currentGender.has_value() && currentGender.value() == Gender::Female);
    connect(femaleAction, &QAction::triggered, this, [this](bool checked) {
        m_filter->setGender(checked ? std::optional<Gender>(Gender::Female) : std::nullopt);
    });
}

void FilterBar::addAgeSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Age"));

    AgeFilter currentFilter = m_filter->ageFilter();

    QAction* adultsAction = sub->addAction(tr("Adults"));
    adultsAction->setCheckable(true);
    adultsAction->setChecked(currentFilter == AgeFilter::Adults);
    connect(adultsAction, &QAction::triggered, this, [this](bool checked) {
        m_filter->setAgeFilter(checked ? AgeFilter::Adults : AgeFilter::All);
    });

    QAction* childrenAction = sub->addAction(tr("Children"));
    childrenAction->setCheckable(true);
    childrenAction->setChecked(currentFilter == AgeFilter::Children);
    connect(childrenAction, &QAction::triggered, this, [this](bool checked) {
        m_filter->setAgeFilter(checked ? AgeFilter::Children : AgeFilter::All);
    });
}

void FilterBar::addSpecialNeedsSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Special Needs"));

    // "(All)" option for any special need
    QAction* allAction = sub->addAction(tr("(All)"));
    allAction->setCheckable(true);
    allAction->setChecked(m_filter->hasAnySpecialNeed());
    connect(allAction, &QAction::triggered, this, [this](bool checked) {
        m_filter->setHasAnySpecialNeed(checked);
    });

    sub->addSeparator();

    // Gather all unique special need notes
    const Document& doc = m_documentManager->document();
    QSet<QString> allNeeds;
    for (const Family& family : doc.families())
    {
        for (const Person& person : family.members())
        {
            if (person.hasSpecialNeed())
            {
                allNeeds.insert(person.specialNeedNote());
            }
        }
    }

    QStringList needsList = allNeeds.values();
    needsList.sort(Qt::CaseInsensitive);

    if (needsList.isEmpty())
    {
        sub->addAction(tr("(No special needs)"))->setEnabled(false);
        return;
    }

    for (const QString& need : needsList)
    {
        QString displayName = need;
        if (displayName.length() > 30)
        {
            displayName = displayName.left(27) + "...";
        }

        QAction* action = sub->addAction(displayName);
        action->setCheckable(true);
        action->setChecked(m_filter->specialNeeds().contains(need));
        connect(action, &QAction::triggered, this, [this, need](bool checked) {
            if (checked)
            {
                m_filter->addSpecialNeed(need);
            }
            else
            {
                m_filter->removeSpecialNeed(need);
            }
        });
    }
}

void FilterBar::addResponseAreaSubmenu(QMenu* menu)
{
    QMenu* sub = menu->addMenu(tr("Response Area"));

    // Medical submenu
    QMenu* medicalSub = sub->addMenu(tr("Medical"));
    addResponseAreaItems(medicalSub, ResponseArea::Medical);

    // Communications submenu
    QMenu* commSub = sub->addMenu(tr("Communications"));
    addResponseAreaItems(commSub, ResponseArea::Communications);

    // Recovery submenu
    QMenu* recoverySub = sub->addMenu(tr("Recovery"));
    addResponseAreaItems(recoverySub, ResponseArea::Recovery);
}

void FilterBar::addResponseAreaItems(QMenu* menu, ResponseArea area)
{
    // "(All)" option for any resource in this area
    QAction* allAction = menu->addAction(tr("(All)"));
    allAction->setCheckable(true);
    allAction->setChecked(m_filter->responseAreas().contains(area));
    connect(allAction, &QAction::triggered, this, [this, area](bool checked) {
        if (checked)
        {
            m_filter->addResponseArea(area);
        }
        else
        {
            m_filter->removeResponseArea(area);
        }
    });

    menu->addSeparator();

    // Get resources in this area
    const Document& doc = m_documentManager->document();
    QList<EmergencyResource> resources;
    for (const EmergencyResource& resource : doc.emergencyResources())
    {
        if (resource.responseArea() == area)
        {
            resources.append(resource);
        }
    }

    std::sort(resources.begin(), resources.end(),
              [](const EmergencyResource& a, const EmergencyResource& b) {
        return a.name().toLower() < b.name().toLower();
    });

    for (const EmergencyResource& resource : resources)
    {
        QAction* action = menu->addAction(resource.name());
        action->setCheckable(true);
        action->setChecked(m_filter->resourceTypeIds().contains(resource.id()));
        connect(action, &QAction::triggered,
                this, [this, resourceId = resource.id()](bool checked) {
            QSet<QString> ids = m_filter->resourceTypeIds();
            if (checked)
            {
                ids.insert(resourceId);
            }
            else
            {
                ids.remove(resourceId);
            }
            m_filter->setResourceTypeIds(ids);
        });
    }
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

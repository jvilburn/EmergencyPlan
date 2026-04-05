#include "TaskDialog.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "Family.h"
#include "Person.h"
#include "Team.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRadioButton>
#include <QVBoxLayout>

TaskDialog::TaskDialog(EmergencyManager* emergencyManager,
                       QWidget* parent)
    : QDialog(parent)
    , m_emergencyManager(emergencyManager)
{
    setWindowTitle(tr("Add Task"));
    setMinimumWidth(400);

    QVBoxLayout* layout = new QVBoxLayout(this);

    // Category
    QLabel* categoryLabel = new QLabel(tr("Category:"), this);
    layout->addWidget(categoryLabel);

    m_categoryCombo = new QComboBox(this);
    QStringList categories = m_emergencyManager->taskCategories();
    for (const QString& cat : categories)
    {
        m_categoryCombo->addItem(cat);
    }
    m_categoryCombo->addItem(tr("+ Add new..."));
    layout->addWidget(m_categoryCombo);

    connect(m_categoryCombo, &QComboBox::activated,
            this, &TaskDialog::onCategoryActivated);

    // Description
    QLabel* descLabel = new QLabel(tr("Description:"), this);
    layout->addWidget(descLabel);

    m_descriptionEdit = new QLineEdit(this);
    layout->addWidget(m_descriptionEdit);

    // Assignment
    QLabel* assignLabel = new QLabel(tr("Assign to:"), this);
    layout->addWidget(assignLabel);

    m_assignGroup = new QButtonGroup(this);

    m_unassignedRadio = new QRadioButton(tr("Unassigned"), this);
    m_teamRadio = new QRadioButton(tr("Team:"), this);
    m_personRadio = new QRadioButton(tr("Person:"), this);

    m_assignGroup->addButton(m_unassignedRadio, 0);
    m_assignGroup->addButton(m_teamRadio, 1);
    m_assignGroup->addButton(m_personRadio, 2);
    m_unassignedRadio->setChecked(true);

    layout->addWidget(m_unassignedRadio);

    // Team row
    QHBoxLayout* teamRow = new QHBoxLayout();
    teamRow->addWidget(m_teamRadio);
    m_teamCombo = new QComboBox(this);
    m_teamCombo->setEnabled(false);

    const QHash<TeamId, Team>& teams = DocumentManager::instance()->document().teams();
    QList<QPair<QString, TeamId>> teamList;
    for (auto it = teams.constBegin(); it != teams.constEnd(); ++it)
    {
        teamList.append({it.value().name(), it.key()});
    }
    std::sort(teamList.begin(), teamList.end(),
              [](const QPair<QString, TeamId>& a, const QPair<QString, TeamId>& b) { return a.first.toLower() < b.first.toLower(); });
    for (const QPair<QString, TeamId>& entry : teamList)
    {
        m_teamCombo->addItem(entry.first, entry.second.toString());
    }

    teamRow->addWidget(m_teamCombo);
    layout->addLayout(teamRow);

    // Person row
    QHBoxLayout* personRow = new QHBoxLayout();
    personRow->addWidget(m_personRadio);
    m_personCombo = new QComboBox(this);
    m_personCombo->setEditable(true);
    m_personCombo->setInsertPolicy(QComboBox::NoInsert);
    m_personCombo->setEnabled(false);

    struct PersonEntry
    {
        QString displayName;
        PersonId id;
    };
    QList<PersonEntry> personEntries;

    const QHash<FamilyId, Family>& families = DocumentManager::instance()->document().families();
    for (auto it = families.constBegin(); it != families.constEnd(); ++it)
    {
        for (const Person& person : it.value().members())
        {
            personEntries.append({person.displayName(), person.id()});
        }
    }
    std::sort(personEntries.begin(), personEntries.end(),
              [](const PersonEntry& a, const PersonEntry& b) { return a.displayName.toLower() < b.displayName.toLower(); });
    for (const PersonEntry& entry : personEntries)
    {
        m_personCombo->addItem(entry.displayName, entry.id.toString());
    }

    personRow->addWidget(m_personCombo);
    layout->addLayout(personRow);

    connect(m_assignGroup, &QButtonGroup::idClicked,
            this, &TaskDialog::onAssignmentChanged);

    // Buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted,
            this, &TaskDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

void TaskDialog::setTask(const ResponseTask& task)
{
    setWindowTitle(tr("Edit Task"));
    m_originalTask = task;

    // Set category
    int catIndex = m_categoryCombo->findText(task.category());
    if (catIndex >= 0)
    {
        m_categoryCombo->setCurrentIndex(catIndex);
    }

    // Set description
    m_descriptionEdit->setText(task.description());

    // Set assignment
    if (task.assignedTeamId())
    {
        m_teamRadio->setChecked(true);
        int teamIndex = m_teamCombo->findData(task.assignedTeamId()->toString());
        if (teamIndex >= 0)
        {
            m_teamCombo->setCurrentIndex(teamIndex);
        }
        m_teamCombo->setEnabled(true);
    }
    else if (task.assignedPersonId())
    {
        m_personRadio->setChecked(true);
        int personIndex = m_personCombo->findData(task.assignedPersonId()->toString());
        if (personIndex >= 0)
        {
            m_personCombo->setCurrentIndex(personIndex);
        }
        m_personCombo->setEnabled(true);
    }
    else
    {
        m_unassignedRadio->setChecked(true);
    }
}

std::optional<ResponseTask> TaskDialog::result() const
{
    return m_result;
}

void TaskDialog::onAccepted()
{
    QString category = m_categoryCombo->currentText();
    QString description = m_descriptionEdit->text().trimmed();

    if (category.isEmpty() || category == tr("+ Add new..."))
    {
        QMessageBox::warning(this, tr("Task"),
                             tr("Please select a category."));
        return;
    }

    if (description.isEmpty())
    {
        QMessageBox::warning(this, tr("Task"),
                             tr("Please enter a description."));
        m_descriptionEdit->setFocus();
        return;
    }

    // When editing, mutate the original task to preserve ID, notification, and resolution state
    ResponseTask task = m_originalTask
        ? *m_originalTask
        : ResponseTask::create(category, description);

    if (m_originalTask)
    {
        task.setCategory(category);
        task.setDescription(description);
    }

    // Apply assignment
    if (m_teamRadio->isChecked() && m_teamCombo->currentIndex() >= 0)
    {
        TeamId teamId = TeamId::fromString(m_teamCombo->currentData().toString());
        task.assignToTeam(teamId, QString());
    }
    else if (m_personRadio->isChecked() && m_personCombo->currentIndex() >= 0)
    {
        PersonId personId = PersonId::fromString(m_personCombo->currentData().toString());
        task.assignToPerson(personId, QString());
    }
    else
    {
        task.clearAssignment();
    }

    m_result = task;
    accept();
}

void TaskDialog::onCategoryActivated(int index)
{
    // Last item is "+ Add new..."
    if (index != m_categoryCombo->count() - 1)
    {
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(
        this, tr("New Category"), tr("Category name:"),
        QLineEdit::Normal, QString(), &ok);

    if (!ok || name.trimmed().isEmpty())
    {
        // Revert to first item
        if (m_categoryCombo->count() > 1)
        {
            m_categoryCombo->setCurrentIndex(0);
        }
        return;
    }

    name = name.trimmed();
    m_emergencyManager->addTaskCategory(name);

    // Insert before the "+ Add new..." item
    int insertIndex = m_categoryCombo->count() - 1;
    m_categoryCombo->insertItem(insertIndex, name);
    m_categoryCombo->setCurrentIndex(insertIndex);
}

void TaskDialog::onAssignmentChanged()
{
    m_teamCombo->setEnabled(m_teamRadio->isChecked());
    m_personCombo->setEnabled(m_personRadio->isChecked());
}

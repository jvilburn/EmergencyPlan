#include "TaskDialog.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyCommands.h"
#include "EmergencyManager.h"
#include "PersonCombo.h"
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

TaskDialog::TaskDialog(const FamilyId& familyId, QWidget* parent)
    : QDialog(parent)
    , m_familyId(familyId)
{
    setWindowTitle(tr("Add Task"));
    setMinimumWidth(400);

    QVBoxLayout* layout = new QVBoxLayout(this);

    // Category
    QLabel* categoryLabel = new QLabel(tr("Category:"), this);
    layout->addWidget(categoryLabel);

    m_categoryCombo = new QComboBox(this);
    if (EmergencyManager::instance()->isActive())
    {
        for (const QString& category : EmergencyManager::instance()->response().taskCategories())
        {
            m_categoryCombo->addItem(category);
        }
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
    PersonCombo::fill(m_personCombo, DocumentManager::instance()->document(), std::nullopt);
    m_personCombo->setEnabled(false);

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

    // The task's own category, which the emergency may not list
    int catIndex = m_categoryCombo->findText(task.category());
    if (catIndex < 0)
    {
        catIndex = addCategory(task.category());
    }
    m_categoryCombo->setCurrentIndex(catIndex);

    m_descriptionEdit->setText(task.description());

    // The task's assignee, which the ward may no longer hold: saving it unchanged keeps the
    // assignment as it is
    const std::optional<TaskAssignment>& assignment = task.assignment();
    if (assignment && assignment->teamId())
    {
        m_teamRadio->setChecked(true);
        selectAssignee(m_teamCombo, assignment->teamId()->toString());
        m_teamCombo->setEnabled(true);
    }
    else if (assignment && assignment->personId())
    {
        m_personRadio->setChecked(true);
        selectAssignee(m_personCombo, assignment->personId()->toString());
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

    // An edit starts from the original task, keeping its id, creation time and resolution
    ResponseTask task = m_originalTask
        ? *m_originalTask
        : ResponseTask::create(m_familyId, category, description);
    task.setCategory(category);
    task.setDescription(description);

    std::optional<TaskAssignment> assignment;
    if (m_teamRadio->isChecked() && m_teamCombo->currentIndex() >= 0)
    {
        assignment = TaskAssignment::toTeam(TeamId::fromString(m_teamCombo->currentData().toString()));
    }
    else if (m_personRadio->isChecked())
    {
        std::optional<PersonId> person = PersonCombo::chosen(m_personCombo);
        if (!person)
        {
            QMessageBox::warning(this, tr("Task"),
                                 tr("Please choose a person from the list."));
            m_personCombo->setFocus();
            return;
        }
        assignment = TaskAssignment::toPerson(*person);
    }

    // The same assignee keeps the assignment as it was, notification included
    const std::optional<TaskAssignment>& original = task.assignment();
    if (assignment && original
        && assignment->teamId() == original->teamId()
        && assignment->personId() == original->personId())
    {
        assignment = original;
    }
    task.setAssignment(assignment);

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

    // A name already listed is chosen rather than added again
    name = name.trimmed();
    int existing = m_categoryCombo->findText(name, Qt::MatchFixedString);
    if (existing >= 0 && existing != m_categoryCombo->count() - 1)
    {
        m_categoryCombo->setCurrentIndex(existing);
        return;
    }

    CommitResult result = DocumentManager::instance()->executeCommand(AddTaskCategoryCommand::fromNew(name));
    if (result.status != CommitStatus::Applied)
    {
        m_categoryCombo->setCurrentIndex(0);
        return;
    }
    m_categoryCombo->setCurrentIndex(addCategory(name));
}

int TaskDialog::addCategory(const QString& category)
{
    // Before the "+ Add new..." item
    int index = m_categoryCombo->count() - 1;
    m_categoryCombo->insertItem(index, category);
    return index;
}

void TaskDialog::selectAssignee(QComboBox* combo, const QString& id)
{
    int index = combo->findData(id);
    if (index < 0)
    {
        combo->insertItem(0, tr("(removed)"), id);
        index = 0;
    }
    combo->setCurrentIndex(index);
}

void TaskDialog::onAssignmentChanged()
{
    m_teamCombo->setEnabled(m_teamRadio->isChecked());
    m_personCombo->setEnabled(m_personRadio->isChecked());
}

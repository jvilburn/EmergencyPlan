#include "ActionButtonsWidget.h"
#include "EmergencyActions.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"

#include <QGridLayout>
#include <QPushButton>

ActionButtonsWidget::ActionButtonsWidget(const FamilyId& familyId,
                                         QWidget* parent)
    : QWidget(parent)
    , m_familyId(familyId)
{
    QGridLayout* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    int row = 0;

    // Emergency action buttons (only while an emergency is in progress)
    if (EmergencyManager::instance()->isActive())
    {
        QPushButton* okButton = new QPushButton(tr("OK"), this);
        okButton->setStyleSheet("color: #4CAF50; font-weight: bold;");

        QPushButton* unreachableButton = new QPushButton(tr("Unable to Reach"), this);
        unreachableButton->setStyleSheet("color: #E65100;");

        layout->addWidget(okButton, row, 0);
        layout->addWidget(unreachableButton, row, 1);
        ++row;

        QPushButton* addTaskButton = new QPushButton(tr("Add Task"), this);
        QPushButton* logContactButton = new QPushButton(tr("Log Contact"), this);

        layout->addWidget(addTaskButton, row, 0);
        layout->addWidget(logContactButton, row, 1);
        ++row;

        connect(okButton, &QPushButton::clicked,
                this, &ActionButtonsWidget::onOkClicked);
        connect(unreachableButton, &QPushButton::clicked,
                this, &ActionButtonsWidget::onUnableToReachClicked);
        connect(addTaskButton, &QPushButton::clicked,
                this, &ActionButtonsWidget::onAddTaskClicked);
        connect(logContactButton, &QPushButton::clicked,
                this, &ActionButtonsWidget::onLogContactClicked);
    }

    m_editButton = new QPushButton(tr("Edit"), this);
    m_deleteButton = new QPushButton(tr("Delete"), this);
    m_deleteButton->setStyleSheet("color: #c0392b;");

    layout->addWidget(m_editButton, row, 0);
    layout->addWidget(m_deleteButton, row, 1);

    connect(m_editButton, &QPushButton::clicked,
            this, &ActionButtonsWidget::onEditClicked);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &ActionButtonsWidget::onDeleteClicked);
}

void ActionButtonsWidget::onEditClicked()
{
    emit editRequested(m_familyId);
}

void ActionButtonsWidget::onDeleteClicked()
{
    emit deleteRequested(m_familyId);
}

void ActionButtonsWidget::onOkClicked()
{
    EmergencyActions::markContact(m_familyId, ContactOutcome::OK);
}

void ActionButtonsWidget::onUnableToReachClicked()
{
    EmergencyActions::markContact(m_familyId, ContactOutcome::UnableToReach);
}

// The dialogs take the window as their parent, since this widget goes with its row when the
// family's rows change while a dialog is open
void ActionButtonsWidget::onAddTaskClicked()
{
    EmergencyActions::addTask(m_familyId, window());
}

void ActionButtonsWidget::onLogContactClicked()
{
    EmergencyActions::logContact(m_familyId, window());
}

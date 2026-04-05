#include "ActionButtonsWidget.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"

#include <QGridLayout>
#include <QPushButton>

ActionButtonsWidget::ActionButtonsWidget(const FamilyId& familyId,
                                         EmergencyManager* emergencyManager,
                                         QWidget* parent)
    : QWidget(parent)
    , m_familyId(familyId)
    , m_emergencyManager(emergencyManager)
{
    QGridLayout* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    int row = 0;

    // Emergency action buttons (only during active emergency, not archive viewing).
    // Note: isActive() returns true during archive view because m_response holds the archive's
    // response data, so the isViewingArchive() check is needed to distinguish live vs. archive.
    if (m_emergencyManager && m_emergencyManager->isActive() && !m_emergencyManager->isViewingArchive())
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
    m_emergencyManager->setContactStatus(m_familyId, ContactStatus::OK);
}

void ActionButtonsWidget::onUnableToReachClicked()
{
    m_emergencyManager->setContactStatus(m_familyId, ContactStatus::UnableToReach);
}

void ActionButtonsWidget::onAddTaskClicked()
{
    emit addTaskRequested(m_familyId);
}

void ActionButtonsWidget::onLogContactClicked()
{
    emit logContactRequested(m_familyId);
}

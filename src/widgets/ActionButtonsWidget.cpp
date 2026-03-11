#include "ActionButtonsWidget.h"
#include "EmergencyManager.h"
#include "EmergencyResponse.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>

ActionButtonsWidget::ActionButtonsWidget(const FamilyId& familyId,
                                         EmergencyManager* emergencyManager,
                                         const QString& phoneNumber,
                                         QWidget* parent)
    : QWidget(parent)
    , m_familyId(familyId)
    , m_emergencyManager(emergencyManager)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(8);

    // Emergency action buttons (only during active emergency)
    if (m_emergencyManager && m_emergencyManager->isActive())
    {
        // Phone number for quick calling
        if (!phoneNumber.isEmpty())
        {
            QLabel* phoneLabel = new QLabel(phoneNumber, this);
            phoneLabel->setStyleSheet("color: #1976D2; font-weight: bold;");
            layout->addWidget(phoneLabel);
        }

        QPushButton* okButton = new QPushButton(tr("OK"), this);
        okButton->setStyleSheet("color: #4CAF50; font-weight: bold;");
        layout->addWidget(okButton);

        QPushButton* unreachableButton = new QPushButton(tr("Unable to Reach"), this);
        unreachableButton->setStyleSheet("color: #FFC107;");
        layout->addWidget(unreachableButton);

        QPushButton* addTaskButton = new QPushButton(tr("Add Task"), this);
        layout->addWidget(addTaskButton);

        connect(okButton, &QPushButton::clicked,
                this, &ActionButtonsWidget::onOkClicked);
        connect(unreachableButton, &QPushButton::clicked,
                this, &ActionButtonsWidget::onUnableToReachClicked);
        connect(addTaskButton, &QPushButton::clicked,
                this, &ActionButtonsWidget::onAddTaskClicked);

        layout->addSpacing(16);
    }

    m_editButton = new QPushButton(tr("Edit"), this);
    m_deleteButton = new QPushButton(tr("Delete"), this);

    // Style delete button to indicate destructive action
    m_deleteButton->setStyleSheet("color: #c0392b;");

    layout->addWidget(m_editButton);
    layout->addWidget(m_deleteButton);
    layout->addStretch();

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

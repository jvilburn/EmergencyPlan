#include "ActionButtonsWidget.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSizePolicy>

ActionButtonsWidget::ActionButtonsWidget(const QString& familyId, QWidget* parent)
    : QWidget(parent)
    , m_familyId(familyId)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(8);

    m_editButton = new QPushButton(tr("Edit"), this);
    m_deleteButton = new QPushButton(tr("Delete"), this);

    // Style delete button to indicate destructive action
    m_deleteButton->setStyleSheet("color: #c0392b;");

    layout->addWidget(m_editButton);
    layout->addWidget(m_deleteButton);
    layout->addStretch();

    connect(m_editButton, &QPushButton::clicked, this, [this]() {
        emit editRequested(m_familyId);
    });

    connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        emit deleteRequested(m_familyId);
    });
}

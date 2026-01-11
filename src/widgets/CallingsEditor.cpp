#include "CallingsEditor.h"

#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>

CallingsEditor::CallingsEditor(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(4);

    m_addButton = new QPushButton(tr("+ Add Calling"), this);
    m_addButton->setFlat(true);
    connect(m_addButton, &QPushButton::clicked, this, &CallingsEditor::onAddCalling);

    m_layout->addWidget(m_addButton);
    m_layout->addStretch();
}

void CallingsEditor::setCallings(const QStringList& callings)
{
    // Clear existing rows by finding parent widgets
    for (QLineEdit* edit : m_callingEdits)
    {
        QWidget* row = edit->parentWidget();
        if (row && row != this)
        {
            row->deleteLater();  // This will also delete the child QLineEdit
        }
    }
    m_callingEdits.clear();

    // Add rows for each calling
    for (const QString& calling : callings)
    {
        addCallingRow(calling);
    }
}

QStringList CallingsEditor::callings() const
{
    QStringList result;
    for (QLineEdit* edit : m_callingEdits)
    {
        QString text = edit->text().trimmed();
        if (!text.isEmpty())
        {
            result.append(text);
        }
    }
    return result;
}

void CallingsEditor::onAddCalling()
{
    addCallingRow();
    if (!m_callingEdits.isEmpty())
    {
        m_callingEdits.last()->setFocus();
    }
    emit callingsChanged();
}

void CallingsEditor::onRemoveCalling()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button)
    {
        return;
    }

    // Find the QLineEdit in the same row
    QWidget* row = button->parentWidget();
    QLineEdit* edit = row->findChild<QLineEdit*>();
    if (edit)
    {
        m_callingEdits.removeOne(edit);
    }
    row->deleteLater();
    emit callingsChanged();
}

void CallingsEditor::onCallingTextChanged()
{
    emit callingsChanged();
}

void CallingsEditor::addCallingRow(const QString& text)
{
    QWidget* row = new QWidget(this);
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);

    QLineEdit* edit = new QLineEdit(text, row);
    edit->setPlaceholderText(tr("Calling name"));
    connect(edit, &QLineEdit::textChanged, this, &CallingsEditor::onCallingTextChanged);

    QPushButton* removeBtn = new QPushButton(tr("X"), row);
    removeBtn->setFixedWidth(24);
    removeBtn->setToolTip(tr("Remove calling"));
    connect(removeBtn, &QPushButton::clicked, this, &CallingsEditor::onRemoveCalling);

    rowLayout->addWidget(edit, 1);
    rowLayout->addWidget(removeBtn);

    m_callingEdits.append(edit);

    // Insert before the Add button
    int insertIndex = m_layout->indexOf(m_addButton);
    m_layout->insertWidget(insertIndex, row);
}

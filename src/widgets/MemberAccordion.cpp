#include "MemberAccordion.h"
#include "MemberEditor.h"
#include "Gender.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>
#include <QEvent>
#include <QMouseEvent>

MemberAccordion::MemberAccordion(QWidget* parent)
    : QWidget(parent)
    , m_contentLayout(new QVBoxLayout(this))
{
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(4);

    // Add member button
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->addWidget(new QLabel(tr("Members"), this));
    headerLayout->addStretch();
    m_addButton = new QPushButton(tr("+ Add"), this);
    m_addButton->setFlat(true);
    connect(m_addButton, &QPushButton::clicked, this, &MemberAccordion::onAddMember);
    headerLayout->addWidget(m_addButton);
    m_contentLayout->addLayout(headerLayout);

    m_contentLayout->addStretch();
}

void MemberAccordion::setMembers(const QList<Person>& members)
{
    // Clear existing rows
    for (const MemberRow& row : m_rows)
    {
        row.header->deleteLater();
        row.editor->deleteLater();
    }
    m_rows.clear();

    // Add rows for each member
    for (const Person& person : members)
    {
        addMemberRow(person, false);
    }
}

QList<Person> MemberAccordion::members() const
{
    QList<Person> result;
    for (const MemberRow& row : m_rows)
    {
        result.append(row.editor->person());
    }
    return result;
}

void MemberAccordion::onAddMember()
{
    Person newPerson = Person::create(
        Name(QString(), QString()),
        false,  // isParent
        Phone(),
        Phone(),
        Email(),
        std::nullopt,
        Birthday(),
        QStringList()
    );
    addMemberRow(newPerson, true);
    emit membersChanged();
}

void MemberAccordion::onRemoveMember()
{
    MemberEditor* editor = qobject_cast<MemberEditor*>(sender());
    if (!editor)
    {
        return;
    }

    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i].editor == editor)
        {
            m_rows[i].header->deleteLater();
            m_rows[i].editor->deleteLater();
            m_rows.removeAt(i);

            // Update rowIndex properties for remaining rows
            for (int j = i; j < m_rows.size(); ++j)
            {
                m_rows[j].header->setProperty("rowIndex", j);
            }

            emit membersChanged();
            return;
        }
    }
}

void MemberAccordion::onMemberDataChanged()
{
    // Update header text for the changed member
    MemberEditor* editor = qobject_cast<MemberEditor*>(sender());
    if (!editor)
    {
        return;
    }

    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i].editor == editor)
        {
            updateHeaderText(i);
            break;
        }
    }
    emit membersChanged();
}

void MemberAccordion::addMemberRow(const Person& person, bool expanded)
{
    MemberRow row;
    row.expanded = expanded;

    // Create header (clickable bar)
    row.header = new QFrame(this);
    row.header->setFrameShape(QFrame::StyledPanel);
    row.header->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* headerLayout = new QHBoxLayout(row.header);
    headerLayout->setContentsMargins(8, 4, 8, 4);

    // Parent indicator and name
    QLabel* nameLabel = new QLabel(row.header);
    nameLabel->setObjectName("nameLabel");
    headerLayout->addWidget(nameLabel);
    headerLayout->addStretch();

    // Expand indicator
    QLabel* expandLabel = new QLabel(expanded ? QString::fromUtf8("\u25BC") : QString::fromUtf8("\u25B6"), row.header);
    expandLabel->setObjectName("expandLabel");
    headerLayout->addWidget(expandLabel);

    // Create editor
    row.editor = new MemberEditor(this);
    row.editor->setPerson(person);
    row.editor->setVisible(expanded);
    connect(row.editor, &MemberEditor::dataChanged, this, &MemberAccordion::onMemberDataChanged);
    connect(row.editor, &MemberEditor::removeRequested, this, &MemberAccordion::onRemoveMember);

    // Click handler for header
    int rowIndex = m_rows.size();
    row.header->installEventFilter(this);
    row.header->setProperty("rowIndex", rowIndex);

    m_rows.append(row);
    updateHeaderText(rowIndex);

    // Insert before stretch
    int insertPos = m_contentLayout->count() - 1;  // Before stretch
    m_contentLayout->insertWidget(insertPos, row.header);
    m_contentLayout->insertWidget(insertPos + 1, row.editor);
}

void MemberAccordion::toggleRow(int index)
{
    if (index < 0 || index >= m_rows.size())
    {
        return;
    }

    MemberRow& row = m_rows[index];
    row.expanded = !row.expanded;
    row.editor->setVisible(row.expanded);

    // Update expand indicator
    QLabel* expandLabel = row.header->findChild<QLabel*>("expandLabel");
    if (expandLabel)
    {
        expandLabel->setText(row.expanded ? QString::fromUtf8("\u25BC") : QString::fromUtf8("\u25B6"));
    }
}

void MemberAccordion::updateHeaderText(int index)
{
    if (index < 0 || index >= m_rows.size())
    {
        return;
    }

    const MemberRow& row = m_rows[index];
    Person p = row.editor->person();

    QString indicator = p.isParent() ? QString::fromUtf8("\u25CF ") : QString::fromUtf8("\u25CB ");
    QString name = p.displayName();
    if (name.isEmpty())
    {
        name = tr("(New Member)");
    }

    QString details;
    if (p.gender().has_value())
    {
        details += p.gender()->toString().left(1);
    }
    if (!p.birthday().dateDisplay().isEmpty())
    {
        if (!details.isEmpty())
        {
            details += QString::fromUtf8(" \u00B7 ");
        }
        details += p.birthday().dateDisplay();
        if (!p.birthday().ageDisplay().isEmpty())
        {
            details += " (" + p.birthday().ageDisplay() + ")";
        }
    }
    if (!p.displayPhone().isEmpty())
    {
        if (!details.isEmpty())
        {
            details += QString::fromUtf8(" \u00B7 ");
        }
        details += p.displayPhone();
    }

    QString text = indicator + name;
    if (!details.isEmpty())
    {
        text += "\n   " + details;
    }

    QLabel* nameLabel = row.header->findChild<QLabel*>("nameLabel");
    if (nameLabel)
    {
        nameLabel->setText(text);
    }
}

bool MemberAccordion::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease)
    {
        QWidget* header = qobject_cast<QWidget*>(obj);
        if (header)
        {
            int index = header->property("rowIndex").toInt();
            toggleRow(index);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

#pragma once

#include <QWidget>
#include <QList>
#include "Person.h"

class QVBoxLayout;
class QScrollArea;
class QFrame;
class MemberEditor;
class QPushButton;

/// Accordion widget showing family members with expandable editors.
class MemberAccordion : public QWidget
{
    Q_OBJECT

public:
    explicit MemberAccordion(QWidget* parent = nullptr);

    void setMembers(const QList<Person>& members);
    QList<Person> members() const;

signals:
    void membersChanged();

private slots:
    void onAddMember();
    void onRemoveMember();
    void onMemberDataChanged();

private:
    struct MemberRow
    {
        QFrame* header;
        MemberEditor* editor;
        bool expanded;
    };

    void addMemberRow(const Person& person, bool expanded = false);
    void toggleRow(int index);
    void updateHeaderText(int index);

    QVBoxLayout* m_contentLayout;
    QList<MemberRow> m_rows;
    QPushButton* m_addButton;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
};

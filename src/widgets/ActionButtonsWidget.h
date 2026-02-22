#pragma once

#include "Id.h"

#include <QWidget>

class QPushButton;

/// Simple widget with Edit and Delete buttons for family actions.
/// Used in the tree view via setIndexWidget().
class ActionButtonsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ActionButtonsWidget(const FamilyId& familyId, QWidget* parent = nullptr);

    const FamilyId& familyId() const { return m_familyId; }

signals:
    void editRequested(const FamilyId& familyId);
    void deleteRequested(const FamilyId& familyId);

private:
    FamilyId m_familyId;
    QPushButton* m_editButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
};

#pragma once

#include "Id.h"

#include <QWidget>

class QLabel;
class QPushButton;
class EmergencyManager;

/// Widget with Edit/Delete buttons and optional emergency action buttons.
/// Used in the tree view via setIndexWidget().
class ActionButtonsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ActionButtonsWidget(const FamilyId& familyId,
                                 EmergencyManager* emergencyManager,
                                 const QString& phoneNumber,
                                 QWidget* parent);

    const FamilyId& familyId() const { return m_familyId; }

signals:
    void editRequested(const FamilyId& familyId);
    void deleteRequested(const FamilyId& familyId);
    void addTaskRequested(const FamilyId& familyId);

private:
    FamilyId m_familyId;
    QPushButton* m_editButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
};

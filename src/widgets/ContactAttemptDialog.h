#pragma once

#include "EmergencyResponse.h"

#include <QDialog>
#include <optional>

class QButtonGroup;
class QComboBox;
class QLineEdit;
class DocumentManager;

/// Dialog for logging a contact attempt during an emergency.
class ContactAttemptDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ContactAttemptDialog(DocumentManager* documentManager,
                                  QWidget* parent);

    std::optional<ContactAttempt> result() const;

private slots:
    void onAccepted();

private:
    DocumentManager* m_documentManager;
    QButtonGroup* m_methodGroup = nullptr;
    QComboBox* m_whoCombo = nullptr;
    QLineEdit* m_notesEdit = nullptr;
    std::optional<ContactAttempt> m_result;
};

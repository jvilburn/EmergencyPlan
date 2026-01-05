#pragma once

#include <QWidget>

class QPushButton;

/// Simple widget with Edit and Delete buttons for family actions.
/// Used in the tree view via setIndexWidget().
class ActionButtonsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ActionButtonsWidget(const QString& familyId, QWidget* parent = nullptr);

    QString familyId() const { return m_familyId; }

signals:
    void editRequested(const QString& familyId);
    void deleteRequested(const QString& familyId);

private:
    QString m_familyId;
    QPushButton* m_editButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
};

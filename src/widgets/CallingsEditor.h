#pragma once

#include <QWidget>
#include <QStringList>
#include <QVBoxLayout>

class QLineEdit;
class QPushButton;

/// Widget for editing a list of callings with add/remove functionality.
class CallingsEditor : public QWidget
{
    Q_OBJECT

public:
    explicit CallingsEditor(QWidget* parent = nullptr);

    void setCallings(const QStringList& callings);
    QStringList callings() const;

signals:
    void callingsChanged();

private slots:
    void onAddCalling();
    void onRemoveCalling();
    void onCallingTextChanged();

private:
    void addCallingRow(const QString& text = QString());

    QVBoxLayout* m_layout;
    QList<QLineEdit*> m_callingEdits;
    QPushButton* m_addButton;
};

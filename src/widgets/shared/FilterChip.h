#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

/// A removable filter chip showing filter type and value.
/// Displays as "[Label: Value ×]" with remove button.
class FilterChip : public QFrame
{
    Q_OBJECT

public:
    explicit FilterChip(const QString& label,
                        const QString& value,
                        QWidget* parent = nullptr);

    QString label() const { return m_label; }
    QString value() const { return m_value; }

signals:
    void removeClicked();

private:
    QString m_label;
    QString m_value;
    QLabel* m_textLabel;
    QPushButton* m_removeButton;
};

#pragma once

#include <QFrame>

class QLabel;

class EmergencyBanner : public QFrame
{
    Q_OBJECT

public:
    explicit EmergencyBanner(QWidget* parent);
    void setEmergencyName(const QString& name);

private:
    QLabel* m_label;
};

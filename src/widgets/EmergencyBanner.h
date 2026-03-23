#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

class EmergencyBanner : public QFrame
{
    Q_OBJECT

public:
    explicit EmergencyBanner(QWidget* parent);
    void setEmergencyName(const QString& name);
    void setArchiveName(const QString& name);
    void clearBanner();

signals:
    void closeArchiveRequested();

private:
    QLabel* m_label = nullptr;
    QPushButton* m_closeArchiveButton = nullptr;
};

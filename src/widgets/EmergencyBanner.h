#pragma once

#include <QFrame>

class QLabel;

/// The bar across the window while an emergency is in progress, naming it.
class EmergencyBanner : public QFrame
{
    Q_OBJECT

public:
    explicit EmergencyBanner(QWidget* parent);

    /// Shows the banner naming the emergency in progress.
    void showEmergency(const QString& name);
    void clearBanner();

private:
    QLabel* m_label = nullptr;
};

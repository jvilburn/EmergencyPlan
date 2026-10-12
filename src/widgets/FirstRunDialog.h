#pragma once

#include <QDialog>

class IdentityService;
class QLineEdit;
class QPushButton;

/// The first screen, while no person exists on this device: a name for the person
/// and a name for the device, which others with access to their wards will see. A
/// second device of someone who already uses Ward Planning gets its wards by opening
/// the invite links their other device makes.
class FirstRunDialog : public QDialog
{
    Q_OBJECT

public:
    FirstRunDialog(IdentityService& identity, QWidget* parent);

private slots:
    void onCreate();
    void onNamesChanged();

private:
    IdentityService& m_identity;
    QLineEdit* m_displayName = nullptr;
    QLineEdit* m_deviceName = nullptr;
    QPushButton* m_createButton = nullptr;
};

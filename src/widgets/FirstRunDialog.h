#pragma once

#include <QDialog>

class IdentityService;
class QLineEdit;
class QPushButton;
class QStackedWidget;

/// The first screen, while no person exists on this device. It asks one question -
/// does this person already use Ward Planning on another device - before anything
/// else, so one person never becomes two by accident.
///
/// No asks for a display name and a device name and creates the person. Yes links
/// this device to the person on the other one.
class FirstRunDialog : public QDialog
{
    Q_OBJECT

public:
    FirstRunDialog(IdentityService& identity, QWidget* parent);

private slots:
    void onNewPerson();
    void onLinkDevice();
    void onCreate();
    void onNamesChanged();

private:
    IdentityService& m_identity;
    QStackedWidget* m_pages = nullptr;
    QLineEdit* m_displayName = nullptr;
    QLineEdit* m_deviceName = nullptr;
    QPushButton* m_createButton = nullptr;
};

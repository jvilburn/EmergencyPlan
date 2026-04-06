#pragma once

#include <QFrame>
#include "Family.h"

class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class MemberAccordion;
struct GeocodingResult;

/// Side panel for editing a family's address, location, and members.
class FamilyEditPanel : public QFrame
{
    Q_OBJECT

public:
    explicit FamilyEditPanel(QWidget* parent);

    void setFamily(const Family& family);
    Family family() const;

    const FamilyId& familyId() const { return m_familyId; }
    bool isDirty() const;

signals:
    void saveRequested();
    void cancelRequested();
    void closeRequested();
    void dataChanged();

private slots:
    void onLookUpCoordinates();
    void onGeocodingComplete(const GeocodingResult& result);
    void onAddressChanged();
    void onLatChanged();
    void onLonChanged();
    void onMembersChanged();

private:
    void setupUi();
    void updateTitle();

    FamilyId m_familyId;
    Family m_originalFamily;
    Family m_editedFamily;

    QLabel* m_titleLabel;
    QPushButton* m_closeButton;

    // Address
    QTextEdit* m_addressEdit;

    // Location
    QLineEdit* m_latEdit;
    QLineEdit* m_lonEdit;
    QPushButton* m_lookupButton;

    // Members
    MemberAccordion* m_memberAccordion;

    // Actions
    QPushButton* m_cancelButton;
    QPushButton* m_saveButton;
};

#pragma once

#include <QFrame>
#include "Family.h"

class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class MemberAccordion;
class DocumentManager;
struct GeocodingResult;

/// Side panel for editing a family's address, location, and members.
class FamilyEditPanel : public QFrame
{
    Q_OBJECT

public:
    explicit FamilyEditPanel(DocumentManager* docManager, QWidget* parent = nullptr);

    void setFamily(const Family& family);
    Family family() const;

    QString familyId() const { return m_familyId; }
    bool isDirty() const;

signals:
    void saveRequested();
    void cancelRequested();
    void closeRequested();
    void dataChanged();

private slots:
    void onLookUpCoordinates();
    void onGeocodingComplete(const GeocodingResult& result);

private:
    void setupUi();
    void updateTitle();

    DocumentManager* m_docManager;
    QString m_familyId;
    Family m_originalFamily;

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

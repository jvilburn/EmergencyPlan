#pragma once

#include <QFrame>
#include <QHash>
#include <optional>
#include "Coordinates.h"
#include "Family.h"

class Document;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class MemberAccordion;
class AutomaticUpdates;

/// Side panel for editing a family's address, location, note, and members.
class FamilyEditPanel : public QFrame
{
    Q_OBJECT

public:
    /// The coordinate fields show document's entry for the address shown, and Look Up asks
    /// automaticUpdates for it; both outlive the panel.
    FamilyEditPanel(const Document& document, AutomaticUpdates& automaticUpdates, QWidget* parent);

    /// Opens the panel on family.
    void setFamily(const Family& family);

    /// Shows the document's current entry for the address shown while the coordinate fields
    /// still read what the panel showed; typed coordinates are left as they are.
    void refreshCoordinates();
    Family family() const;

    /// The family as setFamily gave it: the start condition of the edit.
    const Family& openedFamily() const { return m_originalFamily; }

    /// The coordinates the user typed or looked up, for the address the panel shows. None
    /// when they did neither, when the address is empty, or when a field holds no number.
    std::optional<Coordinates> typedCoordinates() const;

    const FamilyId& familyId() const { return m_familyId; }

    /// True when the family differs from the one opened, or the typed coordinates differ
    /// from the entry for the address the panel shows.
    bool isDirty() const;

signals:
    void saveRequested();
    void cancelRequested();
    void closeRequested();
    void dataChanged();

private slots:
    void onLookUpCoordinates();

    /// Fills the coordinate fields with the answer to this panel's Look Up, while the panel
    /// still shows the address it looked up; any other answer is not this panel's.
    void onAnswered(const QString& address, const std::optional<Coordinates>& coordinates);
    void onAddressChanged();
    void onCoordinatesTyped();
    void onMembersChanged();
    void onNoteChanged();

private:
    void setupUi();
    void updateTitle();

    /// Shows the entry for address in the coordinate fields, or clears them, and remembers
    /// the text shown.
    void showEntryFor(const Address& address);

    /// True when the coordinate fields hold text other than what showEntryFor last wrote.
    bool coordinatesTyped() const;

    /// Enables the coordinate fields while the panel shows an address, and Look Up while it
    /// shows one it is not looking up already.
    void updateCoordinatesEnabled();

    /// Disables Save while the coordinate fields are typed but do not hold a latitude and a
    /// longitude.
    void updateSaveEnabled();

    FamilyId m_familyId;
    Family m_originalFamily;
    Family m_editedFamily;
    QString m_shownLatitude;
    QString m_shownLongitude;

    QLabel* m_titleLabel;
    QPushButton* m_closeButton;

    // Address
    QTextEdit* m_addressEdit;

    // Location
    QLineEdit* m_latEdit;
    QLineEdit* m_lonEdit;
    QPushButton* m_lookupButton;
    const Document& m_document;
    AutomaticUpdates& m_automaticUpdates;
    QString m_lookingUp;  ///< the address this panel's Look Up awaits; empty when none

    // Note
    QTextEdit* m_noteEdit;

    // Members
    MemberAccordion* m_memberAccordion;

    // Actions
    QPushButton* m_cancelButton;
    QPushButton* m_saveButton;
};

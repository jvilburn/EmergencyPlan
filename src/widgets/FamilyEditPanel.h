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
class GeocodingService;
struct GeocodingResult;

/// Side panel for editing a family's address, location, and members.
class FamilyEditPanel : public QFrame
{
    Q_OBJECT

public:
    explicit FamilyEditPanel(QWidget* parent);

    /// Opens the panel on family. The coordinate fields show the entry that the document's
    /// table of address coordinates holds for the address the panel shows.
    void setFamily(const Family& family, const Document& document);

    /// Takes the document's current table of address coordinates. While the coordinate
    /// fields still read what the panel showed, they show the entry for the address shown;
    /// typed coordinates are left as they are.
    void refreshCoordinates(const Document& document);
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
    void onGeocodingComplete(const GeocodingResult& result);
    void onAddressChanged();
    void onCoordinatesTyped();
    void onMembersChanged();
    void onNoteChanged();

private:
    void setupUi();
    void updateTitle();

    /// The entry the table the panel opened with holds for address, or none.
    std::optional<Coordinates> entryFor(const Address& address) const;

    /// Shows the entry for address in the coordinate fields, or clears them, and remembers
    /// the text shown.
    void showEntryFor(const Address& address);

    /// True when the coordinate fields hold text other than what showEntryFor last wrote.
    bool coordinatesTyped() const;

    /// Enables the coordinate fields and the lookup while the panel shows an address.
    void updateCoordinatesEnabled();

    /// Disables Save while the coordinate fields are typed but do not hold a latitude and a
    /// longitude.
    void updateSaveEnabled();

    FamilyId m_familyId;
    Family m_originalFamily;
    Family m_editedFamily;
    QHash<QString, Coordinates> m_addressCoordinates;
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
    GeocodingService* m_geocoder;

    // Note
    QTextEdit* m_noteEdit;

    // Members
    MemberAccordion* m_memberAccordion;

    // Actions
    QPushButton* m_cancelButton;
    QPushButton* m_saveButton;
};

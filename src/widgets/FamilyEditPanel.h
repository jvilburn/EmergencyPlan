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

private:
    void setupUi();
    void updateTitle();

    /// The entry the table the panel opened with holds for address, or none.
    std::optional<Coordinates> entryFor(const Address& address) const;

    /// Shows the entry for address in the coordinate fields, or clears them.
    void showEntryFor(const Address& address);

    /// Enables the coordinate fields and the lookup while the panel shows an address.
    void updateCoordinatesEnabled();

    FamilyId m_familyId;
    Family m_originalFamily;
    Family m_editedFamily;
    QHash<QString, Coordinates> m_addressCoordinates;
    bool m_coordinatesTyped = false;

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

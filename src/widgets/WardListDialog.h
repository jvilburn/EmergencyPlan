#pragma once

#include "Family.h"
#include "FamilyMarkerProvider.h"
#include "Id.h"

#include <QDialog>
#include <optional>

class FilterBar;
class FamilyTreeModel;
class PersonTreeModel;
class MapWidget;
class QLabel;
class QLineEdit;
class QSplitter;
class QDialogButtonBox;
class SelectionPreservingTreeView;

/// Result from multi-select person dialog.
struct PersonSelectionResult
{
    QString name;
    QList<PersonId> personIds;
};

/// Result from multi-select family dialog.
struct FamilySelectionResult
{
    QString name;
    QList<FamilyId> familyIds;
};

/// What the "Special Need" dialog saved: the person's family as the dialog read it when the
/// person was chosen, and that family with the person's note set to what the user entered.
struct SpecialNeedEdit
{
    Family opened;
    Family saved;
};

/// Modal dialog for selecting families or persons from the ward list.
/// Features a split view with tree on left and map on right.
/// Uses FilterBar for search/filtering and tree models for display.
class WardListDialog : public QDialog, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    enum Mode
    {
        FamilyMode,
        PersonMode
    };
    Q_ENUM(Mode)

    explicit WardListDialog(Mode mode,
                            bool checkable,
                            QWidget* parent);

    /// Get the name field text (trimmed).
    QString name() const;

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;
    void selectFamily(const FamilyId& familyId) override;

    // Static convenience methods for common use cases

    /// Show dialog to select multiple families. Returns nullopt if cancelled.
    static std::optional<FamilySelectionResult> selectFamilies(
        const QString& nameLabel,
        const QString& initialName,
        const QList<FamilyId>& initialIds,
        QWidget* parent);

    /// Show the "Special Need" dialog. With lockedPerson set, it edits that person's need and
    /// the person cannot be changed; otherwise the user picks the person, and each pick reads
    /// that person's family and fills in their current note. Returns nullopt if cancelled or
    /// no person was picked.
    static std::optional<SpecialNeedEdit> editSpecialNeed(
        const std::optional<PersonId>& lockedPerson,
        QWidget* parent);

    /// Show dialog to select multiple persons. Returns nullopt if cancelled.
    static std::optional<PersonSelectionResult> selectPersons(
        const QString& nameLabel,
        const QString& initialName,
        const QList<PersonId>& initialIds,
        QWidget* parent);

private slots:
    void onSelectionChanged();

private:
    void setupUi();
    QSet<FamilyId> highlightedFamilyIds() const;

    /// The person whose need the "Special Need" dialog edits, and their family as read when
    /// that person was chosen: the edit's start condition.
    struct OpenedNeed
    {
        PersonId person;
        Family family;
    };

    /// Takes the special-need edit's start condition: reads person's family from the
    /// document into m_openedNeed and fills the note field with the person's current note.
    void openSpecialNeed(const PersonId& personId);

    Mode m_mode;
    FilterBar* m_filterBar = nullptr;
    SelectionPreservingTreeView* m_treeView = nullptr;
    MapWidget* m_mapWidget = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
    QSplitter* m_splitter = nullptr;
    bool m_checkable;
    QLabel* m_nameLabel = nullptr;
    QLineEdit* m_nameEdit = nullptr;

    // Model - only one is non-null depending on mode
    FamilyTreeModel* m_familyModel = nullptr;
    PersonTreeModel* m_personModel = nullptr;

    // Set when this is the "Special Need" dialog; then its opened need, empty while no
    // person is chosen.
    bool m_editsSpecialNeed = false;
    std::optional<OpenedNeed> m_openedNeed;
};

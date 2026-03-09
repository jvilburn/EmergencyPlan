#pragma once

#include "FamilyMarkerProvider.h"
#include "Id.h"

#include <QDialog>
#include <optional>

class DocumentManager;
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

    explicit WardListDialog(DocumentManager* documentManager,
                            Mode mode,
                            bool checkable = false,
                            QWidget* parent = nullptr);

    /// Get the name field text (trimmed).
    QString name() const;

    /// Pre-select items by ID.
    void setPreselectedFamilyIds(const QList<FamilyId>& ids);
    void setPreselectedPersonIds(const QList<PersonId>& ids);

    /// Get selected IDs after dialog is accepted.
    QList<FamilyId> selectedFamilyIds() const;
    QList<PersonId> selectedPersonIds() const;

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;
    void selectFamily(const FamilyId& familyId) override;

    // Static convenience methods for common use cases

    /// Show dialog to select a single family. Returns nullopt if cancelled.
    static std::optional<FamilyId> selectFamily(DocumentManager* documentManager,
                                const std::optional<FamilyId>& initialId = std::nullopt,
                                QWidget* parent = nullptr);

    /// Show dialog to select multiple families. Returns nullopt if cancelled.
    static std::optional<FamilySelectionResult> selectFamilies(
        DocumentManager* documentManager,
        const QString& nameLabel,
        const QString& initialName,
        const QList<FamilyId>& initialIds = {},
        QWidget* parent = nullptr);

    /// Show dialog to select a single person. Returns nullopt if cancelled.
    static std::optional<PersonId> selectPerson(DocumentManager* documentManager,
                                const std::optional<PersonId>& initialId = std::nullopt,
                                QWidget* parent = nullptr);

    /// Show dialog to select a single person with a name field.
    /// Returns nullopt if cancelled.
    static std::optional<PersonSelectionResult> selectPersonWithName(
        DocumentManager* documentManager,
        const QString& nameLabel,
        const QString& initialName = {},
        const std::optional<PersonId>& initialId = std::nullopt,
        QWidget* parent = nullptr);

    /// Show dialog to select multiple persons. Returns nullopt if cancelled.
    static std::optional<PersonSelectionResult> selectPersons(
        DocumentManager* documentManager,
        const QString& nameLabel,
        const QString& initialName,
        const QList<PersonId>& initialIds = {},
        QWidget* parent = nullptr);

private slots:
    void onSelectionChanged();

private:
    void setupUi();
    QSet<FamilyId> highlightedFamilyIds() const;

    Mode m_mode;
    DocumentManager* m_documentManager;
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
};

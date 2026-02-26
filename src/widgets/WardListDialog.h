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
class QSplitter;
class QDialogButtonBox;
class SelectionPreservingTreeView;

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

    enum SelectionMode
    {
        SingleSelect,
        MultiSelect
    };
    Q_ENUM(SelectionMode)

    explicit WardListDialog(DocumentManager* documentManager,
                            Mode mode,
                            QWidget* parent = nullptr);

    /// Set selection mode (single vs multi-select).
    void setSelectionMode(SelectionMode mode);

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

    /// Show dialog to select multiple families. Returns empty list if cancelled.
    static QList<FamilyId> selectFamilies(DocumentManager* documentManager,
                                      const QList<FamilyId>& initialIds = {},
                                      QWidget* parent = nullptr);

    /// Show dialog to select a single person. Returns nullopt if cancelled.
    static std::optional<PersonId> selectPerson(DocumentManager* documentManager,
                                const std::optional<PersonId>& initialId = std::nullopt,
                                QWidget* parent = nullptr);

    /// Show dialog to select multiple persons. Returns empty list if cancelled.
    static QList<PersonId> selectPersons(DocumentManager* documentManager,
                                     const QList<PersonId>& initialIds = {},
                                     QWidget* parent = nullptr);

private slots:
    void onSelectionChanged();

private:
    void setupUi();
    std::optional<FamilyId> familyIdForCurrentSelection() const;

    Mode m_mode;
    DocumentManager* m_documentManager;
    FilterBar* m_filterBar = nullptr;
    SelectionPreservingTreeView* m_treeView = nullptr;
    MapWidget* m_mapWidget = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
    QSplitter* m_splitter = nullptr;

    // Model - only one is non-null depending on mode
    FamilyTreeModel* m_familyModel = nullptr;
    PersonTreeModel* m_personModel = nullptr;
};

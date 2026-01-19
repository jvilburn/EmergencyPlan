#pragma once

#include "FamilyMarkerProvider.h"

#include <QDialog>
#include <QStringList>

class DocumentManager;
class Filter;
class FilterableListWidget;
class FamilyListModel;
class PersonListModel;
class MapWidget;
class QSplitter;
class QDialogButtonBox;
class QAbstractListModel;

/// Modal dialog for selecting families or persons from the ward list.
/// Features a split view with list on left and map on right.
/// Owns its own Filter and model instances.
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
    void setPreselectedIds(const QStringList& ids);

    /// Get selected IDs after dialog is accepted.
    QStringList selectedIds() const;

    /// Access the filter for external configuration.
    Filter* filter() const { return m_filter; }

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

    // Static convenience methods for common use cases

    /// Show dialog to select a single family. Returns empty string if cancelled.
    static QString selectFamily(DocumentManager* documentManager,
                                const QString& initialId = QString(),
                                QWidget* parent = nullptr);

    /// Show dialog to select multiple families. Returns empty list if cancelled.
    static QStringList selectFamilies(DocumentManager* documentManager,
                                      const QStringList& initialIds = {},
                                      QWidget* parent = nullptr);

    /// Show dialog to select a single person. Returns empty string if cancelled.
    static QString selectPerson(DocumentManager* documentManager,
                                const QString& initialId = QString(),
                                QWidget* parent = nullptr);

    /// Show dialog to select multiple persons. Returns empty list if cancelled.
    static QStringList selectPersons(DocumentManager* documentManager,
                                     const QStringList& initialIds = {},
                                     QWidget* parent = nullptr);

private slots:
    void onMapFamilyClicked(const QString& id);
    void onSearchTextChanged(const QString& text);
    void onModelReset();

private:
    void setupUi();
    QString familyIdForCurrentSelection() const;

    Mode m_mode;
    DocumentManager* m_documentManager;
    Filter* m_filter = nullptr;
    FilterableListWidget* m_listWidget = nullptr;
    MapWidget* m_mapWidget = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
    QSplitter* m_splitter = nullptr;

    // Model - only one is non-null depending on mode
    FamilyListModel* m_familyModel = nullptr;
    PersonListModel* m_personModel = nullptr;
};

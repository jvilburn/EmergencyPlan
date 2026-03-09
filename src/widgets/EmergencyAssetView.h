#pragma once

#include "FamilyMarkerProvider.h"
#include "ResponseArea.h"

#include <QWidget>

#include <optional>

class DocumentManager;
class EmergencyAssetModel;
class FilterBar;
class QModelIndex;
class QPushButton;
class SelectionPreservingTreeView;

/// EmergencyAssetView displays a 3-level tree of assets.
/// Owns FilterBar (which owns Filter) and EmergencyAssetModel internally.
/// Includes a toolbar with Add, Edit, Delete buttons.
class EmergencyAssetView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit EmergencyAssetView(DocumentManager* documentManager,
                                    ResponseArea area,
                                    QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<FamilyId> visibleFamilyIds() const override;
    void clearSelection() override;
    void selectFamily(const FamilyId& familyId) override;

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged();
    void onTreeDoubleClicked(const QModelIndex& index);
    void onTreeExpanded(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);
    void expandAssets();
    void editAssetFromContextMenu();
    void removePersonFromContextMenu();

private:
    void updateButtonStates();

    void addAsset();
    void editSelectedAsset();
    void deleteAsset();
    void showAssetDialog(const std::optional<EmergencyAssetId>& assetId);
    QString nameLabel() const;
    void removePersonFromAsset(const EmergencyAssetId& assetId, const PersonId& personId);

    std::optional<EmergencyAssetId> selectedAssetId() const;

    DocumentManager* m_documentManager;
    FilterBar* m_filterBar;
    EmergencyAssetModel* m_model;
    SelectionPreservingTreeView* m_tree;

    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;

    std::optional<EmergencyAssetId> m_contextAssetId;
    std::optional<PersonId> m_contextPersonId;
};

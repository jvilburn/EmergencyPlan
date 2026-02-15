#pragma once

#include "FamilyMarkerProvider.h"
#include "ResponseArea.h"

#include <QWidget>

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
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onSelectionChanged();
    void onTreeDoubleClicked(const QModelIndex& index);
    void onTreeExpanded(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);
    void expandAssets();
    void selectPeopleFromContextMenu();
    void removePersonFromContextMenu();

private:
    void updateButtonStates();

    void addAsset();
    void editAsset();
    void deleteAsset();
    void showSelectPeopleDialog(const QString& assetId);
    void removePersonFromAsset(const QString& assetId, const QString& personId);

    QString selectedAssetId() const;

    DocumentManager* m_documentManager;
    FilterBar* m_filterBar;
    EmergencyAssetModel* m_model;
    SelectionPreservingTreeView* m_tree;

    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;

    QString m_contextAssetId;
    QString m_contextPersonId;
};

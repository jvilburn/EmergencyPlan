#pragma once

#include <QWidget>
#include <QString>
#include <QList>
#include "Id.h"
#include <optional>

class DocumentManager;
class FamilyTreeModel;
class Filter;
class FamilyMarkerProvider;
class MapViewModel;

/// Panel showing unmapped families with half-size markers.
/// Displays in a flow layout: 2 columns, expands up, then adds columns.
class UnmappedPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UnmappedPanel(DocumentManager* docManager, MapViewModel* viewModel,
                           QWidget* parent);

    /// Returns true if there are unmapped families to show
    bool hasUnmappedFamilies() const;

    /// Set the marker provider (owned by caller)
    void setMarkerProvider(FamilyMarkerProvider* provider);

    /// Calculate the preferred size for the panel
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /// Set expanded/collapsed state
    void setExpanded(bool expanded);
    bool isExpanded() const { return m_isExpanded; }

signals:
    /// Emitted when a family marker is clicked
    void familyClicked(const FamilyId& familyId);

    /// Emitted when the header is clicked (for expand/collapse toggle)
    void headerClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void onModelChanged();

private:
    struct MarkerLayout
    {
        FamilyId familyId;
        QString name;
        QPointF markerPos;
        QRectF labelRect;
    };

    void recalculateLayout();
    std::optional<FamilyId> markerAtPoint(const QPoint& pos) const;

    MapViewModel* m_viewModel;
    Filter* m_filter;
    FamilyTreeModel* m_model;
    FamilyMarkerProvider* m_markerProvider = nullptr;
    bool m_isExpanded = true;

    QList<MarkerLayout> m_layout;
    int m_numColumns = 0;
    int m_rowsPerColumn = 0;

    // Layout constants
    static constexpr int HEADER_HEIGHT = 24;
    static constexpr int COLUMN_WIDTH = 100;
    static constexpr int ROW_HEIGHT = 24;
    static constexpr int MARKER_SIZE = 14;  // Half of normal 26px diameter
    static constexpr int H_PADDING = 8;
    static constexpr int V_PADDING = 4;
    static constexpr int MIN_COLUMNS = 2;
};

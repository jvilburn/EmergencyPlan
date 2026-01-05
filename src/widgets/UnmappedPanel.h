#pragma once

#include <QWidget>
#include <QString>
#include <QList>

class DocumentManager;
class FamilyListModel;
class Filter;

/// Panel showing unmapped families with half-size markers.
/// Displays in a flow layout: 2 columns, expands up, then adds columns.
class UnmappedPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UnmappedPanel(DocumentManager* docManager, QWidget* parent = nullptr);

    /// Returns true if there are unmapped families to show
    bool hasUnmappedFamilies() const;

    /// Get the currently selected family ID (empty if none)
    QString selectedFamilyId() const { return m_selectedId; }

    /// Set the selected family
    void setSelectedFamilyId(const QString& id);

    /// Calculate the preferred size for the panel
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// Emitted when a family marker is clicked
    void familyClicked(const QString& familyId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void onModelChanged();

private:
    struct MarkerLayout
    {
        QString familyId;
        QString name;
        QPointF markerPos;
        QRectF labelRect;
    };

    void recalculateLayout();
    QString markerAtPoint(const QPoint& pos) const;

    DocumentManager* m_docManager;
    Filter* m_filter;
    FamilyListModel* m_model;

    QString m_selectedId;
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

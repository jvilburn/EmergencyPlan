#include "UnmappedPanel.h"
#include "Document.h"
#include "DocumentManager.h"
#include "FamilyMarkerProvider.h"
#include "FamilyTreeModel.h"
#include "Filter.h"
#include "MarkerRenderer.h"

#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>

UnmappedPanel::UnmappedPanel(DocumentManager* docManager, QWidget* parent)
    : QWidget(parent)
    , m_docManager(docManager)
    , m_filter(new Filter(this))
    , m_model(nullptr)
{
    // Set up filter for unmapped families only
    m_filter->setMappedFilter(MappedFilter::Unmapped);

    // Create model with unmapped filter
    m_model = new FamilyTreeModel(docManager, m_filter, this);

    // Rebuild layout when model changes
    connect(m_model, &QAbstractItemModel::modelReset,
            this, &UnmappedPanel::onModelChanged);

    recalculateLayout();
}

bool UnmappedPanel::hasUnmappedFamilies() const
{
    return m_model->rowCount() > 0;
}

void UnmappedPanel::setMarkerProvider(FamilyMarkerProvider* provider)
{
    m_markerProvider = provider;
    update();
}

void UnmappedPanel::setExpanded(bool expanded)
{
    if (m_isExpanded != expanded)
    {
        m_isExpanded = expanded;
        updateGeometry();
        update();
    }
}

void UnmappedPanel::onModelChanged()
{
    recalculateLayout();
    updateGeometry();
    update();
}

void UnmappedPanel::recalculateLayout()
{
    m_layout.clear();

    int count = m_model->rowCount();
    if (count == 0)
    {
        return;
    }

    // Get available height (parent's height or our max height)
    int availableHeight = parentWidget() ? parentWidget()->height() : 600;

    // Calculate how many rows fit in available height (no header for bottom panel)
    int contentHeight = availableHeight - HEADER_HEIGHT - V_PADDING * 2;
    int rowsPerColumn = qMax(1, contentHeight / ROW_HEIGHT);

    // Calculate number of columns needed
    int numColumns = qMax(MIN_COLUMNS, (count + rowsPerColumn - 1) / rowsPerColumn);

    // Recalculate rows per column to distribute evenly
    rowsPerColumn = (count + numColumns - 1) / numColumns;

    // Store for sizeHint
    m_numColumns = numColumns;
    m_rowsPerColumn = rowsPerColumn;

    QFontMetrics fm(font());

    for (int i = 0; i < count; ++i)
    {
        int col = i / rowsPerColumn;
        int row = i % rowsPerColumn;

        MarkerLayout item;
        QModelIndex idx = m_model->index(i, 0);
        item.familyId = m_model->familyIdAt(idx);
        item.name = m_model->data(idx, Qt::DisplayRole).toString();

        // Position: columns from left to right for natural reading order
        double x = H_PADDING + col * COLUMN_WIDTH + MARKER_SIZE / 2.0;
        double y = HEADER_HEIGHT + V_PADDING + row * ROW_HEIGHT + ROW_HEIGHT / 2.0;

        item.markerPos = QPointF(x, y);

        // Label rect starts after marker
        int labelX = static_cast<int>(x + MARKER_SIZE / 2.0 + 4);
        int labelY = static_cast<int>(y - fm.height() / 2.0);
        int labelWidth = COLUMN_WIDTH - MARKER_SIZE - 8;
        item.labelRect = QRectF(labelX, labelY, labelWidth, fm.height());

        m_layout.append(item);
    }
}

QSize UnmappedPanel::sizeHint() const
{
    if (m_layout.isEmpty())
    {
        return QSize(0, 0);
    }

    int width = m_numColumns * COLUMN_WIDTH + H_PADDING * 2;

    if (!m_isExpanded)
    {
        return QSize(width, HEADER_HEIGHT);
    }

    int height = HEADER_HEIGHT + V_PADDING * 2 + m_rowsPerColumn * ROW_HEIGHT;

    return QSize(width, height);
}

QSize UnmappedPanel::minimumSizeHint() const
{
    return QSize(MIN_COLUMNS * COLUMN_WIDTH + H_PADDING * 2, HEADER_HEIGHT + ROW_HEIGHT);
}

void UnmappedPanel::paintEvent(QPaintEvent* /*event*/)
{
    if (m_layout.isEmpty())
    {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw semi-transparent background
    painter.fillRect(rect(), QColor(255, 255, 255, 230));

    // Draw border on top edge
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.drawLine(0, 0, width(), 0);

    // Draw header with expand/collapse icon and count
    QFont headerFont = font();
    headerFont.setBold(true);
    painter.setFont(headerFont);
    painter.setPen(QColor(100, 100, 100));

    // Expand/collapse icon
    QString icon = m_isExpanded ? QStringLiteral("\u25BE") : QStringLiteral("\u25B8");
    int iconWidth = 16;
    painter.drawText(H_PADDING, 0, iconWidth, HEADER_HEIGHT,
                     Qt::AlignLeft | Qt::AlignVCenter, icon);

    // Header text with count
    QString headerText = tr("Unknown Location (%1)").arg(m_model->rowCount());
    painter.drawText(H_PADDING + iconWidth, 0, width() - H_PADDING * 2 - iconWidth, HEADER_HEIGHT,
                     Qt::AlignLeft | Qt::AlignVCenter, headerText);

    // If collapsed, stop here
    if (!m_isExpanded)
    {
        return;
    }

    // Draw separator line below header
    painter.setPen(QPen(QColor(220, 220, 220), 1));
    painter.drawLine(H_PADDING, HEADER_HEIGHT, width() - H_PADDING, HEADER_HEIGHT);

    // Get highlight info from provider
    HighlightInfo highlight;
    if (m_markerProvider)
    {
        highlight = m_markerProvider->highlightInfo();
    }

    // Draw markers and labels
    QFont labelFont = font();
    labelFont.setPointSize(labelFont.pointSize() - 1);
    painter.setFont(labelFont);

    for (const MarkerLayout& item : m_layout)
    {
        // Draw marker at half size with highlight state
        MarkerRenderer::State state;
        state.scale = 0.5;
        state.isHighlighted = highlight.allHighlightedIds().contains(item.familyId);
        state.showPip = highlight.contactPointFamilyIds.contains(item.familyId);
        state.icons = MarkerRenderer::computeFamilyIcons(item.familyId, m_docManager->document());

        MarkerRenderer::draw(painter, item.markerPos, QVariantMap(), state);

        // Draw label (elided if too long)
        painter.setPen(QColor(60, 60, 60));
        QFontMetrics fm(labelFont);
        QString elidedName = fm.elidedText(item.name, Qt::ElideRight,
                                            static_cast<int>(item.labelRect.width()));
        painter.drawText(item.labelRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);
    }
}

void UnmappedPanel::mousePressEvent(QMouseEvent* event)
{
    // Accept all mouse presses to prevent propagation to parent
    event->accept();

    if (event->button() == Qt::LeftButton)
    {
        // Check if click is in header area
        if (event->pos().y() < HEADER_HEIGHT)
        {
            emit headerClicked();
            return;
        }

        // If expanded, handle marker clicks
        if (m_isExpanded)
        {
            emit familyClicked(markerAtPoint(event->pos()));
        }
    }
}

void UnmappedPanel::mouseReleaseEvent(QMouseEvent* event)
{
    // Accept to prevent propagation to parent
    event->accept();
}

QString UnmappedPanel::markerAtPoint(const QPoint& pos) const
{
    // Check in reverse order (topmost last)
    for (int i = m_layout.size() - 1; i >= 0; --i)
    {
        const MarkerLayout& item = m_layout[i];

        MarkerRenderer::State state;
        state.scale = 0.5;

        if (MarkerRenderer::hitTest(item.markerPos, pos, QVariantMap(), state))
        {
            return item.familyId;
        }

        // Also check label rect for easier clicking
        if (item.labelRect.contains(pos))
        {
            return item.familyId;
        }
    }

    return QString();
}

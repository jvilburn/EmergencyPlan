#include "SculptedItemDelegate.h"
#include "BaseTreeModel.h"
#include "ItemType.h"

#include <QPainter>
#include <QLinearGradient>

SculptedItemDelegate::SculptedItemDelegate(BaseTreeModel* model, QWidget* parent)
    : QStyledItemDelegate(parent)
    , m_model(model)
{
}

void SculptedItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    ItemType type = m_model->itemTypeAt(index);

    if (type == ItemType::ContactDetail)
    {
        paintPlain(painter, option, index);
    }
    else
    {
        paintSculpted(painter, option, index);
    }
}

QSize SculptedItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    // Add vertical space for border + margin
    size.setHeight(qMax(size.height(), 22) + 4);
    return size;
}

void SculptedItemDelegate::paintSculpted(QPainter* painter, const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QRect rect = option.rect.adjusted(0, 1, 0, -1);

    // Determine colors based on state
    QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
    QColor borderColor;
    QColor textColor = option.palette.color(QPalette::Text);

    bool selected = option.state & QStyle::State_Selected;
    bool hovered = option.state & QStyle::State_MouseOver;
    bool active = option.state & QStyle::State_Active;

    if (selected && active)
    {
        gradient.setColorAt(0, QColor(0x3e, 0xa1, 0xff));
        gradient.setColorAt(1, QColor(0x1a, 0x7e, 0xe6));
        borderColor = QColor(0x15, 0x65, 0xc0);
        textColor = Qt::white;
    }
    else if (selected && !active)
    {
        gradient.setColorAt(0, QColor(0xe0, 0xe0, 0xe0));
        gradient.setColorAt(1, QColor(0xc8, 0xc8, 0xc8));
        borderColor = QColor(0xb0, 0xb0, 0xb0);
        textColor = Qt::black;
    }
    else if (hovered)
    {
        gradient.setColorAt(0, QColor(0x3e, 0xa1, 0xff));
        gradient.setColorAt(1, QColor(0x1a, 0x7e, 0xe6));
        borderColor = QColor(0x15, 0x65, 0xc0);
        textColor = Qt::white;
    }
    else
    {
        // Resting state: subtle raised gradient
        gradient.setColorAt(0, QColor(0xf7, 0xf7, 0xf7));
        gradient.setColorAt(1, QColor(0xe8, 0xe8, 0xe8));
        borderColor = QColor(0xd0, 0xd0, 0xd0);
    }

    // Draw sculpted background
    painter->setPen(QPen(borderColor, 1));
    painter->setBrush(gradient);
    painter->drawRoundedRect(rect, 2, 2);

    // Draw text
    QString text = index.data(Qt::DisplayRole).toString();
    QRect textRect = rect.adjusted(4, 0, -4, 0);
    painter->setPen(textColor);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

    painter->restore();
}

void SculptedItemDelegate::paintPlain(QPainter* painter, const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const
{
    // Plain text rendering for contact details - no sculpted background
    QStyledItemDelegate::paint(painter, option, index);
}

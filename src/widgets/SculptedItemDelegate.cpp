#include "SculptedItemDelegate.h"
#include "BaseTreeModel.h"
#include "ItemType.h"

#include <QPainter>
#include <QApplication>
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

    if (selected)
    {
        gradient.setColorAt(0, QColor(0x5b, 0xa3, 0xd9));    // #5BA3D9
        gradient.setColorAt(1, QColor(0x3d, 0x87, 0xc4));    // #3D87C4
        borderColor = QColor(0x2e, 0x6f, 0xa8);               // #2E6FA8
        textColor = Qt::white;
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

    // Draw checkbox if item is checkable
    QRect textRect = rect.adjusted(4, 0, -4, 0);
    Qt::ItemFlags itemFlags = index.flags();
    if (itemFlags & Qt::ItemIsUserCheckable)
    {
        QStyleOptionButton checkOpt;
        checkOpt.rect = QRect(textRect.left(), textRect.top(),
                              textRect.height(), textRect.height());
        checkOpt.rect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                                            QSize(16, 16), checkOpt.rect);
        QVariant checkData = index.data(Qt::CheckStateRole);
        checkOpt.state = QStyle::State_Enabled;
        if (checkData.isValid() && checkData.toInt() == Qt::Checked)
        {
            checkOpt.state |= QStyle::State_On;
        }
        else
        {
            checkOpt.state |= QStyle::State_Off;
        }
        QApplication::style()->drawControl(QStyle::CE_CheckBox, &checkOpt, painter);
        textRect.setLeft(textRect.left() + textRect.height() + 2);
    }

    // Draw text
    QString text = index.data(Qt::DisplayRole).toString();
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

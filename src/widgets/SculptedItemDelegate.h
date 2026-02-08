#pragma once

#include <QStyledItemDelegate>

class BaseTreeModel;

/// Delegate that draws tree items with a sculpted, button-like appearance.
/// ContactDetail items are drawn as plain text (no sculpted background).
/// Rows with index widgets are skipped by Qt, so they remain flat.
class SculptedItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit SculptedItemDelegate(BaseTreeModel* model, QWidget* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    void paintSculpted(QPainter* painter, const QStyleOptionViewItem& option,
                       const QModelIndex& index) const;
    void paintPlain(QPainter* painter, const QStyleOptionViewItem& option,
                    const QModelIndex& index) const;

    BaseTreeModel* m_model;
};

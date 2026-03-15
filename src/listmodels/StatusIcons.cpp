#include "StatusIcons.h"

#include <QApplication>
#include <QFont>
#include <QHash>
#include <QPainter>
#include <QPixmap>

namespace StatusIcons
{

static QIcon createIcon(EffectiveContactStatus status)
{
    constexpr int SIZE = 16;
    double dpr = qApp->devicePixelRatio();
    int pixelSize = static_cast<int>(SIZE * dpr);

    QPixmap pixmap(pixelSize, pixelSize);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bgColor;
    QString symbol;

    switch (status)
    {
    case EffectiveContactStatus::OK:
        bgColor = QColor(76, 175, 80);    // Green
        symbol = Checkmark;
        break;
    case EffectiveContactStatus::NeedsHelp:
        bgColor = QColor(255, 152, 0);    // Orange
        symbol = Flag;
        break;
    case EffectiveContactStatus::UnableToReach:
        bgColor = QColor(255, 235, 59);   // Yellow
        symbol = "?";
        break;
    case EffectiveContactStatus::NotContacted:
        return QIcon();
    }

    // Draw colored circle
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawEllipse(1, 1, SIZE - 2, SIZE - 2);

    // Draw symbol
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(10);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRect(0, 0, SIZE, SIZE), Qt::AlignCenter, symbol);

    painter.end();
    return QIcon(pixmap);
}

QIcon iconForStatus(EffectiveContactStatus status)
{
    static QHash<EffectiveContactStatus, QIcon> cache;
    if (!cache.contains(status))
    {
        cache.insert(status, createIcon(status));
    }
    return cache.value(status);
}

}  // namespace StatusIcons

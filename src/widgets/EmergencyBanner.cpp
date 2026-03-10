#include "EmergencyBanner.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

EmergencyBanner::EmergencyBanner(QWidget* parent)
    : QFrame(parent)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);

    QLabel* icon = new QLabel(this);
    icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(16, 16));
    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);
    layout->addWidget(m_label, 1);

    setStyleSheet(
        "QFrame {"
        "  background-color: #FFA726;"
        "  color: #333;"
        "  font-weight: bold;"
        "}");
    hide();
}

void EmergencyBanner::setEmergencyName(const QString& name)
{
    m_label->setText(tr("EMERGENCY: %1").arg(name));
    show();
}

#include "EmergencyBanner.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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

    m_closeArchiveButton = new QPushButton(tr("Close Archive"), this);
    m_closeArchiveButton->setFixedHeight(24);
    m_closeArchiveButton->hide();
    connect(m_closeArchiveButton, &QPushButton::clicked,
            this, &EmergencyBanner::closeArchiveRequested);

    layout->addWidget(icon);
    layout->addWidget(m_label, 1);
    layout->addWidget(m_closeArchiveButton);

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
    m_closeArchiveButton->hide();
    setStyleSheet(
        "QFrame {"
        "  background-color: #FFA726;"
        "  color: #333;"
        "  font-weight: bold;"
        "}");
    show();
}

void EmergencyBanner::setArchiveName(const QString& name)
{
    m_label->setText(tr("ARCHIVED - READ ONLY: %1").arg(name));
    m_closeArchiveButton->show();
    setStyleSheet(
        "QFrame {"
        "  background-color: #78909C;"
        "  color: #fff;"
        "  font-weight: bold;"
        "}");
    show();
}

void EmergencyBanner::clearBanner()
{
    m_closeArchiveButton->hide();
    hide();
}

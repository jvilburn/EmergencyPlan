#include "EmergencyBanner.h"

#include <QComboBox>
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
    m_switcher = new QComboBox(this);
    m_switcher->hide();

    layout->addWidget(icon);
    layout->addWidget(m_label, 1);
    layout->addWidget(m_switcher);

    connect(m_switcher, &QComboBox::activated, this, &EmergencyBanner::onSwitcherActivated);

    setStyleSheet(
        "QFrame {"
        "  background-color: #FFA726;"
        "  color: #333;"
        "  font-weight: bold;"
        "}");
    hide();
}

void EmergencyBanner::setResponses(const QList<EmergencyResponse>& inProgress, const EmergencyResponseId& shownId)
{
    m_switcher->clear();
    for (const EmergencyResponse& response : inProgress)
    {
        // Two responders who each started one offline may have given them the same name
        QString started = response.startedAt().toLocalTime().toString(tr("MMM d, h:mm AP"));
        m_switcher->addItem(tr("%1 (started %2)").arg(response.name(), started), response.id().toString());
        if (response.id() == shownId)
        {
            m_switcher->setCurrentIndex(m_switcher->count() - 1);
            m_label->setText(tr("EMERGENCY: %1").arg(response.name()));
        }
    }

    bool several = inProgress.size() > 1;
    m_switcher->setVisible(several);
    if (several)
    {
        m_label->setText(tr("EMERGENCIES IN PROGRESS: %1").arg(inProgress.size()));
    }
    show();
}

void EmergencyBanner::clearBanner()
{
    m_switcher->clear();
    hide();
}

void EmergencyBanner::onSwitcherActivated(int index)
{
    emit responseChosen(EmergencyResponseId::fromString(m_switcher->itemData(index).toString()));
}

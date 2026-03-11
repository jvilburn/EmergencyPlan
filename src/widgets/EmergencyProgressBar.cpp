#include "EmergencyProgressBar.h"
#include "EmergencyManager.h"

#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

EmergencyProgressBar::EmergencyProgressBar(EmergencyManager* emergencyManager, QWidget* parent)
    : QWidget(parent)
    , m_emergencyManager(emergencyManager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(2);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_summaryLabel);

    // Fixed height for the bar area
    setMinimumHeight(30);
    setMaximumHeight(40);

    connect(m_emergencyManager, &EmergencyManager::responseDataChanged,
            this, &EmergencyProgressBar::updateCounts);

    updateCounts();
}

void EmergencyProgressBar::updateCounts()
{
    m_totalCount = m_emergencyManager->totalFamilies();
    m_okCount = m_emergencyManager->countByStatus(EffectiveContactStatus::OK);
    m_needsHelpCount = m_emergencyManager->countByStatus(EffectiveContactStatus::NeedsHelp);
    m_unableToReachCount = m_emergencyManager->countByStatus(EffectiveContactStatus::UnableToReach);
    m_notContactedCount = m_emergencyManager->countByStatus(EffectiveContactStatus::NotContacted);

    QString summary = tr("%1 families: %2 OK")
        .arg(m_totalCount)
        .arg(m_okCount);

    if (m_needsHelpCount > 0)
    {
        summary += tr(" \xc2\xb7 %1 need help").arg(m_needsHelpCount);
    }
    if (m_unableToReachCount > 0)
    {
        summary += tr(" \xc2\xb7 %1 unable to reach").arg(m_unableToReachCount);
    }
    summary += tr(" \xc2\xb7 %1 remaining").arg(m_notContactedCount);

    m_summaryLabel->setText(summary);
    update();
}

void EmergencyProgressBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    if (m_totalCount <= 0)
    {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Bar area below the label
    int barY = m_summaryLabel->geometry().bottom() + 2;
    int barHeight = 8;
    int barWidth = width() - 4;
    int barX = 2;

    double scale = static_cast<double>(barWidth) / m_totalCount;

    // Draw segments: OK (green), NeedsHelp (orange), UnableToReach (yellow), NotContacted (gray)
    int x = barX;

    if (m_okCount > 0)
    {
        int w = static_cast<int>(m_okCount * scale);
        painter.fillRect(x, barY, w, barHeight, QColor(76, 175, 80));
        x += w;
    }
    if (m_needsHelpCount > 0)
    {
        int w = static_cast<int>(m_needsHelpCount * scale);
        painter.fillRect(x, barY, w, barHeight, QColor(255, 152, 0));
        x += w;
    }
    if (m_unableToReachCount > 0)
    {
        int w = static_cast<int>(m_unableToReachCount * scale);
        painter.fillRect(x, barY, w, barHeight, QColor(255, 235, 59));
        x += w;
    }
    if (m_notContactedCount > 0)
    {
        // Fill remaining space for gray
        painter.fillRect(x, barY, barX + barWidth - x, barHeight, QColor(200, 200, 200));
    }
}

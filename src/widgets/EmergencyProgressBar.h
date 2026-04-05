#pragma once

#include <QWidget>

class QLabel;

/// Segmented progress bar showing emergency contact status counts.
/// Paints colored segments proportional to status counts.
class EmergencyProgressBar : public QWidget
{
    Q_OBJECT

public:
    explicit EmergencyProgressBar(QWidget* parent);

public slots:
    void updateCounts();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* m_summaryLabel = nullptr;

    int m_okCount = 0;
    int m_needsHelpCount = 0;
    int m_unableToReachCount = 0;
    int m_notContactedCount = 0;
    int m_totalCount = 0;
};

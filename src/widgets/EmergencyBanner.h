#pragma once

#include "EmergencyResponse.h"

#include <QFrame>
#include <QList>

class QComboBox;
class QLabel;

/// The bar across the window while an emergency is in progress. It names the shown
/// emergency, and when more than one is in progress it offers a choice between them.
class EmergencyBanner : public QFrame
{
    Q_OBJECT

public:
    explicit EmergencyBanner(QWidget* parent);

    /// Shows the banner for the response with shownId among inProgress, which holds it.
    void setResponses(const QList<EmergencyResponse>& inProgress, const EmergencyResponseId& shownId);
    void clearBanner();

signals:
    /// The user chose to show the response with that id.
    void responseChosen(const EmergencyResponseId& id);

private slots:
    void onSwitcherActivated(int index);

private:
    QLabel* m_label = nullptr;
    QComboBox* m_switcher = nullptr;
};

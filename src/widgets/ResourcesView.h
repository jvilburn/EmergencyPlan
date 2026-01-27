#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class QTabWidget;
class DocumentManager;
class EmergencyResourceView;

/// ResourcesView displays emergency resources organized into three sub-tabs:
/// Medical, Communications, and Recovery. Each sub-tab uses an EmergencyResourceView
/// filtered by ResponseArea.
class ResourcesView : public QWidget, public FamilyMarkerProvider
{
    Q_OBJECT

public:
    explicit ResourcesView(DocumentManager* documentManager, QWidget* parent = nullptr);

    // FamilyMarkerProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private:
    QTabWidget* m_subTabs;
    EmergencyResourceView* m_medicalView;
    EmergencyResourceView* m_commsView;
    EmergencyResourceView* m_recoveryView;

    EmergencyResourceView* currentSubView() const;
};

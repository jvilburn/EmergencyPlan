#pragma once

#include "FamilyMarkerProvider.h"

#include <QWidget>

class QTabWidget;
class DocumentManager;
class EmergencyResourceModel;
class EmergencyResourceView;

/// ResourcesView displays emergency resources organized into three sub-tabs:
/// Medical, Communications, and Recovery. Creates models and passes to views.
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
    EmergencyResourceView* currentSubView() const;

    QTabWidget* m_subTabs;

    // Models owned by ResourcesView
    EmergencyResourceModel* m_medicalModel;
    EmergencyResourceModel* m_commsModel;
    EmergencyResourceModel* m_recoveryModel;

    // Views take model pointers
    EmergencyResourceView* m_medicalView;
    EmergencyResourceView* m_commsView;
    EmergencyResourceView* m_recoveryView;
};

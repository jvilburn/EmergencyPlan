#pragma once

#include <QWidget>
#include "MapHighlightProvider.h"

class QTabWidget;
class DocumentManager;
class SkillsSubView;
class EquipmentSubView;
class NeedsSubView;

/// EmergencyView displays emergency preparedness data organized into three sub-tabs:
/// Skills, Equipment, and Needs. Each sub-tab shows a 3-level tree hierarchy
/// (Category -> Item -> Person/Family) and integrates with the map for highlighting.
class EmergencyView : public QWidget, public MapHighlightProvider
{
    Q_OBJECT

public:
    explicit EmergencyView(DocumentManager* documentManager, QWidget* parent = nullptr);

    // MapHighlightProvider interface
    HighlightInfo highlightInfo() const override;
    QSet<QString> visibleFamilyIds() const override;

signals:
    void highlightChanged();

private slots:
    void onSubTabChanged(int index);

private:
    DocumentManager* m_documentManager;
    QTabWidget* m_subTabs;
    SkillsSubView* m_skillsView;
    EquipmentSubView* m_equipmentView;
    NeedsSubView* m_needsView;

    MapHighlightProvider* currentSubProvider() const;
};

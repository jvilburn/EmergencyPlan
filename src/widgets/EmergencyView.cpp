#include "EmergencyView.h"
#include "SkillsSubView.h"
#include "EquipmentSubView.h"
#include "NeedsSubView.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLabel>

EmergencyView::EmergencyView(DocumentManager* documentManager, QWidget* parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_subTabs = new QTabWidget();

    // Placeholder for Teams tab (future)
    QWidget* teamsPlaceholder = new QWidget();
    QVBoxLayout* teamsLayout = new QVBoxLayout(teamsPlaceholder);
    QLabel* teamsLabel = new QLabel(tr("Teams functionality coming soon"));
    teamsLabel->setAlignment(Qt::AlignCenter);
    teamsLayout->addWidget(teamsLabel);
    m_subTabs->addTab(teamsPlaceholder, tr("Teams"));

    m_skillsView = new SkillsSubView(documentManager);
    m_subTabs->addTab(m_skillsView, tr("Skills"));

    m_equipmentView = new EquipmentSubView(documentManager);
    m_subTabs->addTab(m_equipmentView, tr("Equipment"));

    m_needsView = new NeedsSubView(documentManager);
    m_subTabs->addTab(m_needsView, tr("Needs"));

    layout->addWidget(m_subTabs);

    // Forward highlight signals from sub-views
    connect(m_skillsView, &SkillsSubView::highlightChanged,
            this, &EmergencyView::highlightChanged);
    connect(m_equipmentView, &EquipmentSubView::highlightChanged,
            this, &EmergencyView::highlightChanged);
    connect(m_needsView, &NeedsSubView::highlightChanged,
            this, &EmergencyView::highlightChanged);

    connect(m_subTabs, &QTabWidget::currentChanged,
            this, &EmergencyView::onSubTabChanged);

    // Start on Skills tab (index 1, after Teams placeholder)
    m_subTabs->setCurrentIndex(1);
}

FamilyMarkerProvider* EmergencyView::currentSubProvider() const
{
    QWidget* current = m_subTabs->currentWidget();
    return dynamic_cast<FamilyMarkerProvider*>(current);
}

HighlightInfo EmergencyView::highlightInfo() const
{
    FamilyMarkerProvider* provider = currentSubProvider();
    if (provider)
    {
        return provider->highlightInfo();
    }
    return HighlightInfo{};
}

QSet<QString> EmergencyView::visibleFamilyIds() const
{
    FamilyMarkerProvider* provider = currentSubProvider();
    if (provider)
    {
        return provider->visibleFamilyIds();
    }
    return {};
}

void EmergencyView::onSubTabChanged(int index)
{
    Q_UNUSED(index);
    emit highlightChanged();
}

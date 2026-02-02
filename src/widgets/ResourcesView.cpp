#include "ResourcesView.h"
#include "EmergencyResourceModel.h"
#include "EmergencyResourceView.h"
#include "ResponseArea.h"

#include <QTabWidget>
#include <QVBoxLayout>

ResourcesView::ResourcesView(DocumentManager* documentManager, QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_subTabs = new QTabWidget();

    // Create models with ResponseArea
    m_medicalModel = new EmergencyResourceModel(documentManager, ResponseArea::Medical, this);
    m_commsModel = new EmergencyResourceModel(documentManager, ResponseArea::Communications, this);
    m_recoveryModel = new EmergencyResourceModel(documentManager, ResponseArea::Recovery, this);

    // Pass models to views
    m_medicalView = new EmergencyResourceView(m_medicalModel, documentManager, this);
    m_subTabs->addTab(m_medicalView, tr("Medical"));

    m_commsView = new EmergencyResourceView(m_commsModel, documentManager, this);
    m_subTabs->addTab(m_commsView, tr("Communications"));

    m_recoveryView = new EmergencyResourceView(m_recoveryModel, documentManager, this);
    m_subTabs->addTab(m_recoveryView, tr("Recovery"));

    layout->addWidget(m_subTabs);

    // Forward highlight signals from sub-views
    connect(m_medicalView, &EmergencyResourceView::highlightChanged,
            this, &ResourcesView::highlightChanged);
    connect(m_commsView, &EmergencyResourceView::highlightChanged,
            this, &ResourcesView::highlightChanged);
    connect(m_recoveryView, &EmergencyResourceView::highlightChanged,
            this, &ResourcesView::highlightChanged);

    // Also update map when switching sub-tabs
    connect(m_subTabs, &QTabWidget::currentChanged,
            this, &ResourcesView::highlightChanged);
}

EmergencyResourceView* ResourcesView::currentSubView() const
{
    return qobject_cast<EmergencyResourceView*>(m_subTabs->currentWidget());
}

HighlightInfo ResourcesView::highlightInfo() const
{
    EmergencyResourceView* current = currentSubView();
    if (current)
    {
        return current->highlightInfo();
    }
    return HighlightInfo{};
}

QSet<QString> ResourcesView::visibleFamilyIds() const
{
    EmergencyResourceView* current = currentSubView();
    if (current)
    {
        return current->visibleFamilyIds();
    }
    return {};
}

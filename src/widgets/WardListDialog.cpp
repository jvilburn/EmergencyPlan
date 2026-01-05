#include "WardListDialog.h"
#include "FilterableListWidget.h"
#include "FamilyListModel.h"
#include "PersonListModel.h"
#include "Filter.h"
#include "MapWidget.h"
#include "DocumentManager.h"
#include "Document.h"

#include <QVBoxLayout>
#include <QSplitter>
#include <QDialogButtonBox>
#include <QPushButton>

WardListDialog::WardListDialog(DocumentManager* documentManager,
                               Mode mode,
                               QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_documentManager(documentManager)
{
    setupUi();
}

void WardListDialog::setupUi()
{
    // Set dialog title based on mode
    if (m_mode == FamilyMode)
    {
        setWindowTitle(tr("Select Family"));
    }
    else
    {
        setWindowTitle(tr("Select Person"));
    }

    resize(1000, 700);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Create splitter with list and map
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Create filter (owned by dialog)
    m_filter = new Filter(this);

    // Create list widget
    m_listWidget = new FilterableListWidget(this);

    // Create appropriate model based on mode
    if (m_mode == FamilyMode)
    {
        m_familyModel = new FamilyListModel(m_documentManager, m_filter, this);
        m_listWidget->setModel(m_familyModel);
        m_listWidget->setIdRole(FamilyListModel::IdRole);

        // Connect model reset
        connect(m_familyModel, &QAbstractItemModel::modelReset,
                this, &WardListDialog::onModelReset);
    }
    else
    {
        m_personModel = new PersonListModel(m_documentManager, m_filter, this);
        m_listWidget->setModel(m_personModel);
        m_listWidget->setIdRole(PersonListModel::IdRole);

        // Connect model reset
        connect(m_personModel, &QAbstractItemModel::modelReset,
                this, &WardListDialog::onModelReset);
    }

    // Create map widget
    m_mapWidget = new MapWidget(m_documentManager, this);
    m_mapWidget->setMinimumWidth(400);

    // Add to splitter
    m_splitter->addWidget(m_listWidget);
    m_splitter->addWidget(m_mapWidget);
    m_splitter->setSizes({350, 650});
    m_splitter->setStretchFactor(0, 0);  // List doesn't stretch
    m_splitter->setStretchFactor(1, 1);  // Map stretches

    mainLayout->addWidget(m_splitter, 1);

    // Button box
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    // Connections
    connect(m_listWidget, &FilterableListWidget::selectionChanged,
            this, &WardListDialog::onListSelectionChanged);
    connect(m_listWidget, &FilterableListWidget::searchTextChanged,
            this, &WardListDialog::onSearchTextChanged);
    connect(m_mapWidget, &MapWidget::familyClicked,
            this, &WardListDialog::onMapFamilyClicked);
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    // Fit map to show all families
    m_mapWidget->fitAllFamilies();
}

void WardListDialog::setSelectionMode(SelectionMode mode)
{
    if (mode == MultiSelect)
    {
        m_listWidget->setSelectionMode(FilterableListWidget::MultiSelect);
    }
    else
    {
        m_listWidget->setSelectionMode(FilterableListWidget::SingleSelect);
    }
}

void WardListDialog::setPreselectedIds(const QStringList& ids)
{
    m_listWidget->setSelectedIds(ids);

    // Center map on first selection
    if (!ids.isEmpty())
    {
        QString familyId = familyIdForCurrentSelection();
        if (!familyId.isEmpty())
        {
            centerMapOnFamily(familyId);
        }
    }
}

QStringList WardListDialog::selectedIds() const
{
    return m_listWidget->selectedIds();
}

void WardListDialog::onListSelectionChanged(const QStringList& ids)
{
    if (!ids.isEmpty())
    {
        QString familyId = familyIdForCurrentSelection();
        if (!familyId.isEmpty())
        {
            centerMapOnFamily(familyId);
        }
    }
}

void WardListDialog::onMapFamilyClicked(const QString& familyId)
{
    if (m_mode == FamilyMode)
    {
        // Direct selection
        m_listWidget->setSelectedIds({familyId});
    }
    else
    {
        // In person mode, select the first person from this family
        std::optional<Family> familyOpt =
            m_documentManager->document().findFamilyById(familyId);
        if (familyOpt && !familyOpt->members().isEmpty())
        {
            QString personId = familyOpt->members().first().id();
            m_listWidget->setSelectedIds({personId});
        }
    }
}

void WardListDialog::onSearchTextChanged(const QString& text)
{
    m_filter->setSearchText(text);
}

void WardListDialog::onModelReset()
{
    // Could update map highlighting here if needed
}

void WardListDialog::centerMapOnFamily(const QString& familyId)
{
    if (!familyId.isEmpty())
    {
        m_mapWidget->centerOnFamily(familyId);
    }
}

QString WardListDialog::familyIdForCurrentSelection() const
{
    QStringList ids = m_listWidget->selectedIds();
    if (ids.isEmpty())
    {
        return QString();
    }

    if (m_mode == FamilyMode)
    {
        return ids.first();
    }
    else
    {
        // Look up family for selected person
        return m_documentManager->document().familyIdForPerson(ids.first());
    }
}

// Static convenience methods

QString WardListDialog::selectFamily(DocumentManager* documentManager,
                                     const QString& initialId,
                                     QWidget* parent)
{
    WardListDialog dialog(documentManager, FamilyMode, parent);
    dialog.setSelectionMode(SingleSelect);
    if (!initialId.isEmpty())
    {
        dialog.setPreselectedIds({initialId});
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        QStringList ids = dialog.selectedIds();
        return ids.isEmpty() ? QString() : ids.first();
    }
    return QString();
}

QStringList WardListDialog::selectFamilies(DocumentManager* documentManager,
                                           const QStringList& initialIds,
                                           QWidget* parent)
{
    WardListDialog dialog(documentManager, FamilyMode, parent);
    dialog.setSelectionMode(MultiSelect);
    if (!initialIds.isEmpty())
    {
        dialog.setPreselectedIds(initialIds);
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        return dialog.selectedIds();
    }
    return {};
}

QString WardListDialog::selectPerson(DocumentManager* documentManager,
                                     const QString& initialId,
                                     QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, parent);
    dialog.setSelectionMode(SingleSelect);
    if (!initialId.isEmpty())
    {
        dialog.setPreselectedIds({initialId});
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        QStringList ids = dialog.selectedIds();
        return ids.isEmpty() ? QString() : ids.first();
    }
    return QString();
}

QStringList WardListDialog::selectPersons(DocumentManager* documentManager,
                                          const QStringList& initialIds,
                                          QWidget* parent)
{
    WardListDialog dialog(documentManager, PersonMode, parent);
    dialog.setSelectionMode(MultiSelect);
    if (!initialIds.isEmpty())
    {
        dialog.setPreselectedIds(initialIds);
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        return dialog.selectedIds();
    }
    return {};
}

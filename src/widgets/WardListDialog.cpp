#include "WardListDialog.h"
#include "FilterBar.h"
#include "FamilyTreeModel.h"
#include "PersonTreeModel.h"
#include "ItemType.h"
#include "MapWidget.h"
#include "DocumentManager.h"
#include "Document.h"
#include "SelectionPreservingTreeView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDialogButtonBox>
#include <QItemSelectionModel>
#include <QPushButton>
#include <QSet>

WardListDialog::WardListDialog(Mode mode,
                               bool checkable,
                               QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_checkable(checkable)
{
    setupUi();
}

QString WardListDialog::name() const
{
    return m_nameEdit->text().trimmed();
}

void WardListDialog::setupUi()
{
    resize(1000, 700);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Name field row
    QHBoxLayout* nameLayout = new QHBoxLayout();
    m_nameLabel = new QLabel(this);
    m_nameEdit = new QLineEdit(this);
    nameLayout->addWidget(m_nameLabel);
    nameLayout->addWidget(m_nameEdit, 1);
    mainLayout->addLayout(nameLayout);

    // Create FilterBar (owns Filter internally)
    m_filterBar = new FilterBar(this);
    mainLayout->addWidget(m_filterBar);

    // Create splitter with tree and map
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Create appropriate model based on mode, then tree view with model
    if (m_mode == FamilyMode)
    {
        m_familyModel = new FamilyTreeModel(m_filterBar->filter(), m_checkable, this);
        m_treeView = new SelectionPreservingTreeView(m_familyModel, this);
    }
    else
    {
        m_personModel = new PersonTreeModel(m_filterBar->filter(), m_checkable, this);
        m_treeView = new SelectionPreservingTreeView(m_personModel, this);
    }
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setIndentation(16);

    // Create map widget
    m_mapWidget = new MapWidget(this);
    m_mapWidget->setMinimumWidth(400);

    // Add to splitter
    m_splitter->addWidget(m_treeView);
    m_splitter->addWidget(m_mapWidget);
    m_splitter->setSizes({350, 650});
    m_splitter->setStretchFactor(0, 0);  // Tree doesn't stretch
    m_splitter->setStretchFactor(1, 1);  // Map stretches

    mainLayout->addWidget(m_splitter, 1);

    // Button box
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    // Set this dialog as the highlight provider for the map
    m_mapWidget->setMarkerProvider(this);

    // Connections
    connect(m_treeView, &SelectionPreservingTreeView::selectionChanged,
            this, &WardListDialog::onSelectionChanged);
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    // Center on chapel; defer fit until widget has size
    m_mapWidget->centerOnChapel();
    m_mapWidget->requestFitAllFamilies();
}

void WardListDialog::selectFamily(const FamilyId& familyId)
{
    if (m_mode == FamilyMode && m_familyModel)
    {
        QModelIndex index = m_familyModel->indexForFamilyId(familyId);
        if (index.isValid())
        {
            m_treeView->selectionModel()->select(
                index, QItemSelectionModel::ClearAndSelect);
            m_treeView->scrollTo(index);
        }
    }
    else if (m_personModel && m_treeView->isEnabled())
    {
        // In person mode, pick the first person from this family, as a click in the tree
        // does. A disabled tree holds a locked person, whom a map click leaves in place.
        std::optional<Family> familyOpt =
            DocumentManager::instance()->document().findFamilyById(familyId);
        if (familyOpt && !familyOpt->members().isEmpty())
        {
            PersonId personId = familyOpt->members().first().id();
            QModelIndex index = m_personModel->indexForPersonId(personId);
            if (index.isValid())
            {
                m_treeView->setCurrentIndex(index);
                m_treeView->scrollTo(index);
            }
        }
    }
}

void WardListDialog::onSelectionChanged()
{
    // Update map highlighting
    m_mapWidget->updateHighlights();

    if (!m_editsSpecialNeed)
    {
        return;
    }

    // Picking a different person takes a new start condition. The tree reselecting the
    // same person after a rebuild keeps the one already taken.
    QModelIndex current = m_treeView->currentIndex();
    std::optional<PersonId> picked;
    if (current.isValid() && m_personModel->itemTypeAt(current) == ItemType::Person)
    {
        picked = m_personModel->personIdAt(current);
    }
    if (!picked.has_value())
    {
        m_openedNeed.reset();
    }
    else if (!m_openedNeed.has_value() || *picked != m_openedNeed->person)
    {
        openSpecialNeed(*picked);
    }
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_openedNeed.has_value());
}

void WardListDialog::openSpecialNeed(const PersonId& personId)
{
    const Document& document = DocumentManager::instance()->document();
    std::optional<FamilyId> familyId = document.familyIdForPerson(personId);
    std::optional<Person> person = document.findPersonById(personId);
    std::optional<Family> family;
    if (familyId.has_value())
    {
        family = document.findFamilyById(*familyId);
    }
    if (!family.has_value() || !person.has_value())
    {
        m_openedNeed.reset();
        return;
    }
    m_openedNeed = OpenedNeed{personId, *family};
    m_nameEdit->setText(person->specialNeedNote());
}

QSet<FamilyId> WardListDialog::highlightedFamilyIds() const
{
    if (m_checkable)
    {
        // In checkbox mode, highlight all checked items' families
        QSet<FamilyId> familyIds;
        if (m_mode == FamilyMode && m_familyModel)
        {
            familyIds = m_familyModel->checkedFamilyIds();
        }
        else if (m_personModel)
        {
            const Document& doc = DocumentManager::instance()->document();
            for (const PersonId& personId : m_personModel->checkedPersonIds())
            {
                auto familyId = doc.familyIdForPerson(personId);
                if (familyId)
                {
                    familyIds.insert(*familyId);
                }
            }
        }
        return familyIds;
    }

    // Single-select: highlight current selection
    QModelIndex current = m_treeView->currentIndex();
    if (!current.isValid())
    {
        return {};
    }

    if (m_mode == FamilyMode && m_familyModel)
    {
        auto id = m_familyModel->familyIdAt(current);
        if (id)
        {
            return {*id};
        }
    }
    else if (m_personModel)
    {
        auto personId = m_personModel->personIdAt(current);
        if (personId)
        {
            auto familyId = DocumentManager::instance()->document().familyIdForPerson(*personId);
            if (familyId)
            {
                return {*familyId};
            }
        }
    }
    return {};
}

HighlightInfo WardListDialog::highlightInfo() const
{
    HighlightInfo info;
    info.highlightedFamilyIds = highlightedFamilyIds();
    return info;
}

QSet<FamilyId> WardListDialog::visibleFamilyIds() const
{
    // Show all families in the dialog
    return {};
}

// Static convenience methods

std::optional<FamilySelectionResult> WardListDialog::selectFamilies(
    const QString& nameLabel,
    const QString& initialName,
    const QList<FamilyId>& initialIds,
    QWidget* parent)
{
    WardListDialog dialog(FamilyMode, true, parent);
    dialog.setWindowTitle(tr("Select Families \u2014 %1").arg(nameLabel));
    dialog.m_nameLabel->setText(nameLabel + tr(":"));
    dialog.m_nameEdit->setText(initialName);
    connect(dialog.m_familyModel, &FamilyTreeModel::dataChanged,
            dialog.m_mapWidget, &MapWidget::updateHighlights);
    if (!initialIds.isEmpty())
    {
        QSet<FamilyId> idSet(initialIds.begin(), initialIds.end());
        dialog.m_familyModel->setCheckedFamilyIds(idSet);

        // Scroll to first checked family
        QModelIndex firstIndex = dialog.m_familyModel->indexForFamilyId(initialIds.first());
        if (firstIndex.isValid())
        {
            dialog.m_treeView->scrollTo(firstIndex);
        }
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    QSet<FamilyId> checked = dialog.m_familyModel->checkedFamilyIds();
    FamilySelectionResult result;
    result.name = dialog.name();
    result.familyIds = QList<FamilyId>(checked.begin(), checked.end());
    return result;
}

std::optional<SpecialNeedEdit> WardListDialog::editSpecialNeed(
    const std::optional<PersonId>& lockedPerson,
    QWidget* parent)
{
    WardListDialog dialog(PersonMode, false, parent);
    dialog.setWindowTitle(tr("Special Need"));
    dialog.m_nameLabel->setText(tr("Need:"));
    dialog.m_editsSpecialNeed = true;
    if (lockedPerson)
    {
        // Making the person current runs onSelectionChanged, which takes the start
        // condition as a pick does.
        QModelIndex index = dialog.m_personModel->indexForPersonId(*lockedPerson);
        dialog.m_treeView->setCurrentIndex(index);
        dialog.m_treeView->scrollTo(index);
        dialog.m_treeView->setEnabled(false);
    }
    dialog.m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(dialog.m_openedNeed.has_value());

    if (dialog.exec() != QDialog::Accepted || !dialog.m_openedNeed.has_value())
    {
        return std::nullopt;
    }

    // An empty note clears the special need.
    const OpenedNeed& opened = *dialog.m_openedNeed;
    QList<Person> members = opened.family.members();
    for (Person& member : members)
    {
        if (member.id() == opened.person)
        {
            member.setSpecialNeedNote(dialog.name());
        }
    }
    Family saved = opened.family;
    saved.setMembers(members);
    return SpecialNeedEdit{opened.family, saved};
}

std::optional<PersonSelectionResult> WardListDialog::selectPersons(
    const QString& nameLabel,
    const QString& initialName,
    const QList<PersonId>& initialIds,
    QWidget* parent)
{
    WardListDialog dialog(PersonMode, true, parent);
    dialog.setWindowTitle(tr("Select People \u2014 %1").arg(nameLabel));
    dialog.m_nameLabel->setText(nameLabel + tr(":"));
    dialog.m_nameEdit->setText(initialName);
    connect(dialog.m_personModel, &PersonTreeModel::dataChanged,
            dialog.m_mapWidget, &MapWidget::updateHighlights);
    if (!initialIds.isEmpty())
    {
        QSet<PersonId> idSet(initialIds.begin(), initialIds.end());
        dialog.m_personModel->setCheckedPersonIds(idSet);

        // Scroll to first checked person
        QModelIndex firstIndex = dialog.m_personModel->indexForPersonId(initialIds.first());
        if (firstIndex.isValid())
        {
            dialog.m_treeView->scrollTo(firstIndex);
        }
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    QSet<PersonId> checked = dialog.m_personModel->checkedPersonIds();
    PersonSelectionResult result;
    result.name = dialog.name();
    result.personIds = QList<PersonId>(checked.begin(), checked.end());
    return result;
}

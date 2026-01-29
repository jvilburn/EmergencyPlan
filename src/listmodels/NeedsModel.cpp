#include "NeedsModel.h"
#include "DocumentManager.h"
#include "Document.h"
#include "Family.h"
#include "Person.h"

#include <algorithm>

NeedsModel::NeedsModel(DocumentManager* documentManager, QObject* parent)
    : QAbstractItemModel(parent)
    , m_documentManager(documentManager)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &NeedsModel::onDocumentChanged);
    rebuild();
}

bool NeedsModel::shouldRebuild(const DocumentChange& change) const
{
    switch (change.scope)
    {
    case ChangeScope::Full:
    case ChangeScope::Family:  // Persons are in families
        return true;
    default:
        return false;
    }
}

void NeedsModel::onDocumentChanged(const DocumentChange& change)
{
    if (shouldRebuild(change))
    {
        rebuild();
    }
}

void NeedsModel::rebuild()
{
    beginResetModel();

    m_entries.clear();

    const Document& doc = m_documentManager->document();

    // Collect all persons with special needs
    for (const Family& family : doc.families())
    {
        for (const Person& person : family.members())
        {
            if (person.hasSpecialNeed())
            {
                NeedEntry entry;
                entry.personId = person.id();
                entry.familyId = family.id();
                entry.sortKey = person.displayName().toLower();

                QString text = person.displayName();
                if (!person.specialNeedNote().isEmpty())
                {
                    text += QString(" - %1").arg(person.specialNeedNote());
                }
                entry.displayText = text;

                m_entries.append(entry);
            }
        }
    }

    // Sort alphabetically by display name
    std::sort(m_entries.begin(), m_entries.end(),
              [](const NeedEntry& a, const NeedEntry& b) { return a.sortKey < b.sortKey; });

    endResetModel();
}

QModelIndex NeedsModel::index(int row, int column, const QModelIndex& parent) const
{
    // Flat list - only valid if no parent
    if (parent.isValid() || column != 0)
    {
        return QModelIndex();
    }

    if (row >= 0 && row < m_entries.size())
    {
        return createIndex(row, 0, nullptr);
    }

    return QModelIndex();
}

QModelIndex NeedsModel::parent(const QModelIndex& child) const
{
    Q_UNUSED(child)
    // Flat list - no parents
    return QModelIndex();
}

int NeedsModel::rowCount(const QModelIndex& parent) const
{
    // Flat list - only root has children
    if (parent.isValid())
    {
        return 0;
    }
    return m_entries.size();
}

int NeedsModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant NeedsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
    {
        return QVariant();
    }

    const NeedEntry& entry = m_entries.at(index.row());

    switch (role)
    {
    case Qt::DisplayRole:
        return entry.displayText;
    case PersonIdRole:
        return entry.personId;
    case FamilyIdRole:
        return entry.familyId;
    default:
        return QVariant();
    }
}

QString NeedsModel::personIdAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
    {
        return QString();
    }
    return m_entries.at(index.row()).personId;
}

QString NeedsModel::familyIdAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
    {
        return QString();
    }
    return m_entries.at(index.row()).familyId;
}

QModelIndex NeedsModel::indexForPersonId(const QString& personId) const
{
    for (int i = 0; i < m_entries.size(); ++i)
    {
        if (m_entries.at(i).personId == personId)
        {
            return createIndex(i, 0, nullptr);
        }
    }
    return QModelIndex();
}

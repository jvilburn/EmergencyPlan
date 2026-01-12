#include "PersonListModel.h"
#include "DocumentManager.h"
#include "Filter.h"
#include "Document.h"
#include "Family.h"
#include "Person.h"

#include <algorithm>

PersonListModel::PersonListModel(DocumentManager* documentManager,
                                 Filter* filter,
                                 QObject* parent)
    : QAbstractListModel(parent)
    , m_documentManager(documentManager)
    , m_filter(filter)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &PersonListModel::onDocumentChanged);
    connect(m_filter, &Filter::changed,
            this, [this]() { rebuild(); });
    rebuild();
}

void PersonListModel::onDocumentChanged(const DocumentChange& change)
{
    Q_UNUSED(change)
    // TODO: implement surgical updates for Family scope changes
    rebuild();
}

void PersonListModel::rebuild()
{
    const Document& doc = m_documentManager->document();
    const QHash<QString, Family>& families = doc.families();

    QList<QString> ids;

    // Iterate all persons in all families
    for (auto it = families.begin(); it != families.end(); ++it)
    {
        const Family& family = it.value();
        for (const Person& person : family.members())
        {
            if (m_filter->passes(doc, person))
            {
                ids.append(person.id());
            }
        }
    }

    // Sort by display name
    std::sort(ids.begin(), ids.end(), [&doc](const QString& a, const QString& b) {
        std::optional<Person> personA = doc.findPersonById(a);
        std::optional<Person> personB = doc.findPersonById(b);
        if (!personA || !personB)
        {
            return a < b;
        }
        return personA->displayName().toLower() < personB->displayName().toLower();
    });

    beginResetModel();
    m_personIds = ids;
    endResetModel();
}

QStringList PersonListModel::personIds() const
{
    return m_personIds;
}

int PersonListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return m_personIds.size();
}

QVariant PersonListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_personIds.size())
    {
        return QVariant();
    }

    if (!m_documentManager)
    {
        return QVariant();
    }

    const QString& id = m_personIds.at(index.row());
    std::optional<Person> opt = m_documentManager->document().findPersonById(id);
    if (!opt)
    {
        return QVariant();
    }

    const Person& person = *opt;

    switch (role)
    {
        case Qt::DisplayRole:
        case DisplayNameRole:
            return person.displayName();

        case IdRole:
            return person.id();

        case GivenNamesRole:
            return person.givenNames();

        case SurnameRole:
            return person.surname();

        case PhoneRole:
            return person.phone();

        case AltPhoneRole:
            return person.altPhone();

        case EmailRole:
            return person.email();

        case GenderRole:
            if (person.gender().has_value())
            {
                return person.gender()->toString();
            }
            return QVariant();

        case AgeRole:
        {
            std::optional<int> age = person.age();
            return age.has_value() ? QVariant(age.value()) : QVariant();
        }

        case IsChildRole:
            return person.isChild();

        case BirthDateDisplayRole:
            return person.birthDateDisplay();

        case AgeDisplayRole:
            return person.ageDisplay();

        case CallingsRole:
            return person.callings();

        case HasCallingsRole:
            return person.hasCallings();

        case HasContactRole:
            return person.hasContact();

        case FamilyIdRole:
            return m_documentManager->document().familyIdForPerson(id);

        default:
            return QVariant();
    }
}

QHash<int, QByteArray> PersonListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[DisplayNameRole] = "displayName";
    roles[GivenNamesRole] = "givenNames";
    roles[SurnameRole] = "surname";
    roles[PhoneRole] = "phone";
    roles[AltPhoneRole] = "altPhone";
    roles[EmailRole] = "email";
    roles[GenderRole] = "gender";
    roles[AgeRole] = "age";
    roles[IsChildRole] = "isChild";
    roles[BirthDateDisplayRole] = "birthDateDisplay";
    roles[AgeDisplayRole] = "ageDisplay";
    roles[CallingsRole] = "callings";
    roles[HasCallingsRole] = "hasCallings";
    roles[HasContactRole] = "hasContact";
    roles[FamilyIdRole] = "familyId";
    return roles;
}

QString PersonListModel::idAt(int row) const
{
    if (row >= 0 && row < m_personIds.size())
    {
        return m_personIds.at(row);
    }
    return QString();
}

int PersonListModel::rowForId(const QString& id) const
{
    return m_personIds.indexOf(id);
}

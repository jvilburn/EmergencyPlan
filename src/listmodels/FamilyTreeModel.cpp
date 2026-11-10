#include "FamilyTreeModel.h"
#include "Document.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "Family.h"
#include "Filter.h"
#include "Person.h"
#include "StatusIcons.h"

#include <QColor>
#include <QFont>
#include <QIcon>

#include <algorithm>

namespace
{

const Document& document()
{
    return DocumentManager::instance()->document();
}

}  // namespace

FamilyTreeModel::FamilyTreeModel(Filter* filter,
                                 bool checkable,
                                 QObject* parent)
    : BaseTreeModel(parent)
    , m_filter(filter)
    , m_checkable(checkable)
{
    // Rebuilt on every change; the view keeps its selection, expansion and scroll.
    connect(DocumentManager::instance(), &DocumentManager::documentChanged,
            this, &FamilyTreeModel::rebuild);
    connect(m_filter, &Filter::changed,
            this, &FamilyTreeModel::rebuild);

    rebuild();
}

FamilyTreeModel::~FamilyTreeModel()
{
    clearNodes();
}

void FamilyTreeModel::clearNodes()
{
    qDeleteAll(m_familyNodes);
    m_familyNodes.clear();
}

void FamilyTreeModel::rebuild()
{
    beginResetModel();

    clearNodes();
    m_familyIds.clear();

    const Document& doc = document();
    const QHash<FamilyId, Family>& families = doc.families();

    // Collect and filter family IDs
    QList<FamilyId> ids;
    ids.reserve(families.size());
    for (auto it = families.begin(); it != families.end(); ++it)
    {
        if (m_filter->passes(doc, it.value()))
        {
            ids.append(it.key());
        }
    }

    // Sort by display name
    std::sort(ids.begin(), ids.end(), [&families](const FamilyId& a, const FamilyId& b) { return families.value(a).displayName().toLower() < families.value(b).displayName().toLower(); });

    m_familyIds = ids;
    for (int i = 0; i < m_familyIds.size(); ++i)
    {
        m_familyNodes.append(buildFamilyNode(i, families.value(m_familyIds.at(i))));
    }

    endResetModel();
}

FamilyTreeModel::TreeNode* FamilyTreeModel::newNode(RowType type, TreeNode* parent, int familyIndex, const QString& text)
{
    TreeNode* node = new TreeNode();
    node->type = type;
    node->parent = parent;
    node->familyIndex = familyIndex;
    node->displayText = text;
    return node;
}

void FamilyTreeModel::appendDetail(TreeNode* memberNode, DetailType detailType, const QString& text)
{
    TreeNode* detailNode = newNode(RowType::MemberDetail, memberNode, memberNode->familyIndex, text);
    detailNode->memberIndex = memberNode->memberIndex;
    detailNode->detailType = detailType;
    memberNode->children.append(detailNode);
}

FamilyTreeModel::TreeNode* FamilyTreeModel::buildFamilyNode(int familyIndex, const Family& family) const
{
    TreeNode* node = newNode(RowType::Family, nullptr, familyIndex, family.displayName());
    node->children = childNodes(node, family);
    return node;
}

QList<FamilyTreeModel::TreeNode*> FamilyTreeModel::childNodes(TreeNode* familyNode, const Family& family) const
{
    int familyIndex = familyNode->familyIndex;
    QList<TreeNode*> children;

    const QList<Person>& members = family.members();
    for (int memberIdx = 0; memberIdx < members.size(); ++memberIdx)
    {
        const Person& person = members.at(memberIdx);

        // Show given name if same surname, full name otherwise
        QString displayName = person.givenNames();
        if (!person.surname().isEmpty() && person.surname() != family.surname())
        {
            displayName = person.displayName();
        }
        TreeNode* memberNode = newNode(RowType::Member, familyNode, familyIndex, displayName);
        memberNode->memberIndex = memberIdx;

        // Member detail nodes, only for values the member has
        if (!person.phone().isEmpty())
        {
            appendDetail(memberNode, DetailType::Phone, tr("Phone: %1").arg(QString(person.phone())));
        }
        if (!person.altPhone().isEmpty())
        {
            appendDetail(memberNode, DetailType::AltPhone, tr("Alt: %1").arg(QString(person.altPhone())));
        }
        if (!person.email().isEmpty())
        {
            appendDetail(memberNode, DetailType::Email, tr("Email: %1").arg(person.email()));
        }
        if (!person.callings().isEmpty())
        {
            appendDetail(memberNode, DetailType::Callings, tr("Callings: %1").arg(person.callings().join(", ")));
        }

        // Birthday: show date for everyone, append age for children
        QString birthDisplay = person.birthday().display();
        if (!birthDisplay.isEmpty())
        {
            QString ageStr = person.ageDisplay();
            if (!ageStr.isEmpty())
            {
                birthDisplay += " " + ageStr;
            }
            appendDetail(memberNode, DetailType::Age, birthDisplay);
        }

        if (memberNode->children.isEmpty())
        {
            appendDetail(memberNode, DetailType::Phone, tr("No contact info"));
        }

        children.append(memberNode);
    }

    QString addressText = family.address().multiLine();
    children.append(newNode(RowType::Address, familyNode, familyIndex,
                            addressText.isEmpty() ? tr("No address") : addressText));

    Phone familyPhone = family.displayPhone();
    children.append(newNode(RowType::Phone, familyNode, familyIndex,
                            familyPhone.isEmpty() ? tr("No phone") : QString(familyPhone)));

    children.append(emergencyNodes(familyNode, family.id()));

    // The Actions row's text is empty; the view puts its buttons there
    children.append(newNode(RowType::Actions, familyNode, familyIndex, QString()));
    return children;
}

QList<FamilyTreeModel::TreeNode*> FamilyTreeModel::emergencyNodes(TreeNode* familyNode, const FamilyId& familyId) const
{
    QList<TreeNode*> rows;
    EmergencyManager* manager = EmergencyManager::instance();
    if (!manager->isActive())
    {
        return rows;
    }
    int familyIndex = familyNode->familyIndex;

    QList<ContactAttempt> attempts = manager->response().contactAttemptsFor(familyId);
    if (!attempts.isEmpty())
    {
        rows.append(newNode(RowType::ContactAttempt, familyNode, familyIndex, tr("Contact Attempts:")));
        for (const ContactAttempt& attempt : attempts)
        {
            QString time = attempt.timestamp().toLocalTime().toString(tr("MMM d, h:mm AP"));
            rows.append(newNode(RowType::ContactAttempt, familyNode, familyIndex,
                                "  " + contactAttemptParts(attempt, time).join(" - ")));
        }
    }

    QList<ResponseTask> tasks = manager->response().tasksFor(familyId);
    if (!tasks.isEmpty())
    {
        rows.append(newNode(RowType::Task, familyNode, familyIndex, tr("Tasks:")));
        for (const ResponseTask& task : tasks)
        {
            // Line 1: a bullet and "Category - Description", with a check mark in place of
            // the bullet if resolved
            QString prefix = task.isResolved() ? StatusIcons::Checkmark : StatusIcons::Bullet;
            TreeNode* taskNode = newNode(RowType::Task, familyNode, familyIndex,
                                         tr("  %1 %2 - \"%3\"").arg(prefix, task.category(), task.description()));
            taskNode->taskId = task.id();
            taskNode->taskResolved = task.isResolved();
            rows.append(taskNode);

            // Line 2: who it is assigned to, as the assignment recorded them, and whether
            // they have been told
            if (!task.isAssigned())
            {
                continue;
            }
            QString assignmentText = tr("    Assigned to %1").arg(task.assignment()->assigneeName());
            if (task.isNotified())
            {
                assignmentText += " " + StatusIcons::Checkmark + " " + tr("notified");
            }
            else
            {
                assignmentText += " " + StatusIcons::Circle + " " + tr("not notified");
            }
            TreeNode* assignNode = newNode(RowType::Task, familyNode, familyIndex, assignmentText);
            assignNode->taskId = task.id();
            assignNode->taskResolved = task.isResolved();
            rows.append(assignNode);
        }
    }
    return rows;
}

FamilyTreeModel::TreeNode* FamilyTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return nullptr;
    }
    return static_cast<TreeNode*>(index.internalPointer());
}

QModelIndex FamilyTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0)
    {
        return QModelIndex();
    }

    if (!parent.isValid())
    {
        // Top-level: family rows
        if (row >= 0 && row < m_familyNodes.size())
        {
            return createIndex(row, 0, m_familyNodes.at(row));
        }
        return QModelIndex();
    }

    TreeNode* parentNode = nodeFromIndex(parent);
    if (!parentNode)
    {
        return QModelIndex();
    }

    if (row >= 0 && row < parentNode->children.size())
    {
        return createIndex(row, 0, parentNode->children.at(row));
    }

    return QModelIndex();
}

QModelIndex FamilyTreeModel::parent(const QModelIndex& child) const
{
    TreeNode* node = nodeFromIndex(child);
    if (!node || !node->parent)
    {
        return QModelIndex();
    }

    TreeNode* parentNode = node->parent;

    // Find parent's row in its parent's children (or in m_familyNodes if root)
    if (parentNode->parent)
    {
        int row = parentNode->parent->children.indexOf(parentNode);
        return createIndex(row, 0, parentNode);
    }
    else
    {
        // Parent is a family node (top-level)
        int row = m_familyNodes.indexOf(parentNode);
        return createIndex(row, 0, parentNode);
    }
}

int FamilyTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return m_familyNodes.size();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return node->children.size();
    }

    return 0;
}

int FamilyTreeModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

bool FamilyTreeModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return !m_familyNodes.isEmpty();
    }

    TreeNode* node = nodeFromIndex(parent);
    if (node)
    {
        return !node->children.isEmpty();
    }

    return false;
}

QVariant FamilyTreeModel::data(const QModelIndex& index, int role) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node)
    {
        return QVariant();
    }

    switch (role)
    {
        case Qt::DisplayRole:
            // Actions row doesn't display text (widget handles it)
            if (node->type == RowType::Actions)
            {
                return QVariant();
            }
            return node->displayText;

        case Qt::CheckStateRole:
            if (m_checkable && node->type == RowType::Family)
            {
                FamilyId familyId = m_familyIds.at(node->familyIndex);
                return m_checkedIds.contains(familyId)
                    ? Qt::Checked : Qt::Unchecked;
            }
            return QVariant();

        case RowTypeRole:
            return QVariant::fromValue(node->type);

        case FamilyIdRole:
            if (node->familyIndex >= 0 && node->familyIndex < m_familyIds.size())
            {
                return m_familyIds.at(node->familyIndex).toString();
            }
            return QVariant();

        case MemberIndexRole:
            return node->memberIndex;

        case DetailTypeRole:
            return QVariant::fromValue(node->detailType);

        case Qt::DecorationRole:
            if (node->type == RowType::Family)
            {
                FamilyId familyId = m_familyIds.at(node->familyIndex);
                EffectiveContactStatus status = DocumentManager::instance()->document().emergencyStatus(familyId);
                if (status != EffectiveContactStatus::NotContacted)
                {
                    return StatusIcons::iconForStatus(status);
                }
            }
            return QVariant();

        case ResponseStatusRole:
            if (node->type == RowType::Family && EmergencyManager::instance()->isActive())
            {
                FamilyId familyId = m_familyIds.at(node->familyIndex);
                return QVariant::fromValue(
                    static_cast<int>(DocumentManager::instance()->document().emergencyStatus(familyId)));
            }
            return QVariant();

        case TaskIdRole:
            if (node->type == RowType::Task && node->taskId)
            {
                return node->taskId->toString();
            }
            return QVariant();

        case Qt::FontRole:
            if (node->type == RowType::ContactAttempt)
            {
                QFont font;
                font.setItalic(true);
                return font;
            }
            if (node->type == RowType::Task && node->taskResolved)
            {
                QFont font;
                font.setStrikeOut(true);
                return font;
            }
            return QVariant();

        case Qt::ForegroundRole:
            if (node->type == RowType::ContactAttempt)
            {
                return QColor(100, 100, 100);
            }
            if (node->type == RowType::Task)
            {
                return QColor(80, 80, 80);
            }
            // Gray out placeholder text
            if (node->displayText == tr("No contact info")
                || node->displayText == tr("No address")
                || node->displayText == tr("No phone"))
            {
                return QColor(128, 128, 128);
            }
            return QVariant();

        default:
            return QVariant();
    }
}

QHash<int, QByteArray> FamilyTreeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[RowTypeRole] = "rowType";
    roles[FamilyIdRole] = "familyId";
    roles[MemberIndexRole] = "memberIndex";
    roles[DetailTypeRole] = "detailType";
    roles[ResponseStatusRole] = "responseStatus";
    roles[TaskIdRole] = "taskId";
    return roles;
}

QList<FamilyId> FamilyTreeModel::familyIds() const
{
    return m_familyIds;
}

std::optional<FamilyId> FamilyTreeModel::familyIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node && node->familyIndex >= 0 && node->familyIndex < m_familyIds.size())
    {
        return m_familyIds.at(node->familyIndex);
    }
    return std::nullopt;
}

std::optional<PersonId> FamilyTreeModel::personIdAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (!node || node->type != RowType::Member)
    {
        return std::nullopt;
    }

    std::optional<FamilyId> famId = familyIdAt(index);
    if (!famId)
    {
        return std::nullopt;
    }

    const QHash<FamilyId, Family>& families = document().families();
    auto it = families.find(*famId);
    if (it == families.end())
    {
        return std::nullopt;
    }

    const Family& family = it.value();
    if (node->memberIndex >= 0 && node->memberIndex < family.members().size())
    {
        return family.members().at(node->memberIndex).id();
    }
    return std::nullopt;
}

QModelIndex FamilyTreeModel::indexForFamilyId(const FamilyId& id) const
{
    int idx = m_familyIds.indexOf(id);
    if (idx >= 0 && idx < m_familyNodes.size())
    {
        return createIndex(idx, 0, m_familyNodes.at(idx));
    }
    return QModelIndex();
}

FamilyTreeModel::RowType FamilyTreeModel::rowTypeAt(const QModelIndex& index) const
{
    TreeNode* node = nodeFromIndex(index);
    if (node)
    {
        return node->type;
    }
    return RowType::Family;
}

ItemType FamilyTreeModel::itemTypeAt(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return ItemType::Invalid;
    }

    switch (rowTypeAt(index))
    {
    case RowType::Family:
        return ItemType::Family;
    case RowType::Member:
        return ItemType::Person;
    case RowType::MemberDetail:
    case RowType::Address:
    case RowType::Phone:
    case RowType::ContactAttempt:
    case RowType::Task:
    case RowType::Actions:
        return ItemType::ContactDetail;
    default:
        return ItemType::Invalid;
    }
}

SelectionKey FamilyTreeModel::selectionKeyAt(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return SelectionKey::literal(QString());
    }

    switch (rowTypeAt(index))
    {
    case RowType::Family:
    {
        std::optional<FamilyId> famId = familyIdAt(index);
        if (famId)
        {
            return SelectionKey::from(*famId);
        }
        return SelectionKey::literal(QString());
    }
    case RowType::Member:
    {
        std::optional<PersonId> personId = personIdAt(index);
        if (personId)
        {
            return SelectionKey::from(*personId);
        }
        return SelectionKey::literal(QString());
    }
    case RowType::MemberDetail:
    case RowType::Address:
    case RowType::Phone:
    case RowType::ContactAttempt:
    case RowType::Task:
    case RowType::Actions:
        // Delegate to parent
        return selectionKeyAt(index.parent());
    default:
        return SelectionKey::literal(QString());
    }
}

void FamilyTreeModel::setCheckedFamilyIds(const QSet<FamilyId>& ids)
{
    m_checkedIds = ids;
    if (!m_familyNodes.isEmpty())
    {
        emit dataChanged(
            index(0, 0),
            index(m_familyNodes.size() - 1, 0),
            {Qt::CheckStateRole});
    }
}

QSet<FamilyId> FamilyTreeModel::checkedFamilyIds() const
{
    return m_checkedIds;
}

Qt::ItemFlags FamilyTreeModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = BaseTreeModel::flags(index);
    if (m_checkable && index.isValid())
    {
        TreeNode* node = nodeFromIndex(index);
        if (node && node->type == RowType::Family)
        {
            f |= Qt::ItemIsUserCheckable;
        }
    }
    return f;
}

bool FamilyTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!m_checkable || role != Qt::CheckStateRole || !index.isValid())
    {
        return false;
    }

    TreeNode* node = nodeFromIndex(index);
    if (!node || node->type != RowType::Family)
    {
        return false;
    }

    Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
    FamilyId familyId = m_familyIds.at(node->familyIndex);
    if (state == Qt::Checked)
    {
        m_checkedIds.insert(familyId);
    }
    else
    {
        m_checkedIds.remove(familyId);
    }

    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}

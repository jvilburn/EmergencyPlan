#include "FamilyTreeModel.h"
#include "DocumentManager.h"
#include "EmergencyManager.h"
#include "Family.h"
#include "Filter.h"
#include "Person.h"

#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QScreen>

#include <algorithm>

static QIcon createStatusIcon(EffectiveContactStatus status)
{
    constexpr int SIZE = 16;
    double dpr = qApp->devicePixelRatio();
    int pixelSize = static_cast<int>(SIZE * dpr);

    QPixmap pixmap(pixelSize, pixelSize);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bgColor;
    QString symbol;

    switch (status)
    {
        case EffectiveContactStatus::OK:
            bgColor = QColor(76, 175, 80);    // Green
            symbol = QString::fromUtf8("\xe2\x9c\x93");  // ✓
            break;
        case EffectiveContactStatus::NeedsHelp:
            bgColor = QColor(255, 152, 0);    // Orange
            symbol = QString::fromUtf8("\xe2\x9a\x91");  // ⚑
            break;
        case EffectiveContactStatus::UnableToReach:
            bgColor = QColor(255, 235, 59);   // Yellow
            symbol = "?";
            break;
        case EffectiveContactStatus::NotContacted:
            return QIcon();
    }

    // Draw colored circle
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawEllipse(1, 1, SIZE - 2, SIZE - 2);

    // Draw symbol
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(10);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRect(0, 0, SIZE, SIZE), Qt::AlignCenter, symbol);

    painter.end();
    return QIcon(pixmap);
}

static QIcon statusIcon(EffectiveContactStatus status)
{
    static QHash<EffectiveContactStatus, QIcon> cache;
    if (!cache.contains(status))
    {
        cache.insert(status, createStatusIcon(status));
    }
    return cache.value(status);
}

FamilyTreeModel::FamilyTreeModel(DocumentManager* documentManager,
                                 EmergencyManager* emergencyManager,
                                 Filter* filter,
                                 bool checkable,
                                 QObject* parent)
    : BaseTreeModel(parent)
    , m_documentManager(documentManager)
    , m_emergencyManager(emergencyManager)
    , m_filter(filter)
    , m_checkable(checkable)
{
    connect(m_documentManager, &DocumentManager::documentChanged,
            this, &FamilyTreeModel::onDocumentChanged);
    connect(m_filter, &Filter::changed,
            this, &FamilyTreeModel::rebuild);

    if (m_emergencyManager)
    {
        connect(m_emergencyManager, &EmergencyManager::familyStatusChanged,
                this, &FamilyTreeModel::onFamilyStatusChanged);
        connect(m_emergencyManager, &EmergencyManager::emergencyStarted,
                this, &FamilyTreeModel::onEmergencyStateChanged);
        connect(m_emergencyManager, &EmergencyManager::emergencyEnded,
                this, &FamilyTreeModel::onEmergencyStateChanged);
    }

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

void FamilyTreeModel::onDocumentChanged(const DocumentChange& change)
{
    // Full document reload
    if (change.action == ChangeAction::Full)
    {
        rebuild();
        return;
    }

    // Only care about family scope
    if (!change.familyId)
    {
        return;
    }

    // Surgical updates - preserve expanded state
    switch (change.action)
    {
        case ChangeAction::Updated:
            updateFamilyRow(*change.familyId);
            break;
        case ChangeAction::Added:
            insertFamilyRow(*change.familyId);
            break;
        case ChangeAction::Removed:
            removeFamilyRow(*change.familyId);
            break;
        default:
            rebuild();
            break;
    }
}

void FamilyTreeModel::onFamilyStatusChanged(const FamilyId& familyId)
{
    int row = m_familyIds.indexOf(familyId);
    if (row < 0)
    {
        return;
    }
    QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {Qt::DecorationRole, ResponseStatusRole});
}

void FamilyTreeModel::onEmergencyStateChanged()
{
    if (m_familyNodes.isEmpty())
    {
        return;
    }
    QModelIndex first = index(0, 0);
    QModelIndex last = index(m_familyNodes.size() - 1, 0);
    emit dataChanged(first, last, {Qt::DecorationRole, ResponseStatusRole});
}

void FamilyTreeModel::updateFamilyRow(const FamilyId& familyId)
{
    // Find the row index for this family
    int row = m_familyIds.indexOf(familyId);
    if (row < 0)
    {
        // Family not currently shown (might be filtered out or new)
        // Check if family exists and passes filter
        auto family = m_documentManager->document().findFamilyById(familyId);
        if (family.has_value() && m_filter->passes(m_documentManager->document(), *family))
        {
            // Family should now be visible - insert it
            insertFamilyRow(familyId);
        }
        return;
    }

    // Check if family still passes filter
    auto family = m_documentManager->document().findFamilyById(familyId);
    if (!family.has_value() || !m_filter->passes(m_documentManager->document(), *family))
    {
        // Family no longer passes filter - remove it
        removeFamilyRow(familyId);
        return;
    }

    // Rebuild just this family's subtree
    TreeNode* oldNode = m_familyNodes.at(row);

    // Remove old children
    if (!oldNode->children.isEmpty())
    {
        QModelIndex familyIndex = index(row, 0);
        beginRemoveRows(familyIndex, 0, oldNode->children.size() - 1);
        qDeleteAll(oldNode->children);
        oldNode->children.clear();
        endRemoveRows();
    }

    // Update the family node display text
    oldNode->displayText = family->displayName();

    // Rebuild children for this family
    const QList<Person>& members = family->members();
    QModelIndex familyIndex = index(row, 0);

    // We need to add all children at once
    QList<TreeNode*> newChildren;

    // Build member nodes
    for (int memberIdx = 0; memberIdx < members.size(); ++memberIdx)
    {
        const Person& person = members.at(memberIdx);

        TreeNode* memberNode = new TreeNode();
        memberNode->type = RowType::Member;
        memberNode->familyIndex = row;
        memberNode->memberIndex = memberIdx;
        memberNode->parent = oldNode;

        QString displayName = person.givenNames();
        if (!person.surname().isEmpty() && person.surname() != family->surname())
        {
            displayName = person.displayName();
        }
        memberNode->displayText = displayName;

        // Add member detail nodes
        Phone phone = person.displayPhone();
        if (!phone.isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = row;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Phone;
            detailNode->displayText = tr("Phone: %1").arg(QString(phone));
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        if (!person.altPhone().isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = row;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::AltPhone;
            detailNode->displayText = tr("Alt: %1").arg(QString(person.altPhone()));
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        if (!person.email().isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = row;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Email;
            detailNode->displayText = tr("Email: %1").arg(person.email());
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        if (!person.callings().isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = row;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Callings;
            detailNode->displayText = tr("Callings: %1").arg(person.callings().join(", "));
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        QString birthDisplay = person.birthDateDisplay();
        if (!birthDisplay.isEmpty())
        {
            QString ageStr = person.ageDisplay();
            if (!ageStr.isEmpty())
            {
                birthDisplay += " " + ageStr;
            }

            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = row;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Age;
            detailNode->displayText = birthDisplay;
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        if (memberNode->children.isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = row;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Phone;
            detailNode->displayText = tr("No contact info");
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        newChildren.append(memberNode);
    }

    // Add address node
    TreeNode* addressNode = new TreeNode();
    addressNode->type = RowType::Address;
    addressNode->familyIndex = row;
    addressNode->parent = oldNode;
    QString addressText = family->address().multiLine();
    addressNode->displayText = addressText.isEmpty() ? tr("No address") : addressText;
    newChildren.append(addressNode);

    // Add phone node
    TreeNode* phoneNode = new TreeNode();
    phoneNode->type = RowType::Phone;
    phoneNode->familyIndex = row;
    phoneNode->parent = oldNode;
    Phone familyPhone = family->displayPhone();
    phoneNode->displayText = familyPhone.isEmpty() ? tr("No phone") : QString(familyPhone);
    newChildren.append(phoneNode);

    // Add actions node
    TreeNode* actionsNode = new TreeNode();
    actionsNode->type = RowType::Actions;
    actionsNode->familyIndex = row;
    actionsNode->parent = oldNode;
    newChildren.append(actionsNode);

    // Insert all children
    if (!newChildren.isEmpty())
    {
        beginInsertRows(familyIndex, 0, newChildren.size() - 1);
        oldNode->children = newChildren;
        endInsertRows();
    }

    // Emit dataChanged for the family row itself
    emit dataChanged(familyIndex, familyIndex);
}

void FamilyTreeModel::insertFamilyRow(const FamilyId& familyId)
{
    // Check if family exists and passes filter
    auto family = m_documentManager->document().findFamilyById(familyId);
    if (!family.has_value() || !m_filter->passes(m_documentManager->document(), *family))
    {
        return;
    }

    // Already exists?
    if (m_familyIds.contains(familyId))
    {
        updateFamilyRow(familyId);
        return;
    }

    // Find insertion point (sorted by display name)
    QString newName = family->displayName().toLower();
    int insertRow = 0;
    for (int i = 0; i < m_familyIds.size(); ++i)
    {
        auto existingFamily = m_documentManager->document().findFamilyById(m_familyIds.at(i));
        if (existingFamily.has_value()
            && existingFamily->displayName().toLower() > newName)
        {
            break;
        }
        insertRow = i + 1;
    }

    // Insert into the model
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_familyIds.insert(insertRow, familyId);

    // Build the family node
    TreeNode* familyNode = new TreeNode();
    familyNode->type = RowType::Family;
    familyNode->familyIndex = insertRow;
    familyNode->displayText = family->displayName();
    m_familyNodes.insert(insertRow, familyNode);

    // Update familyIndex for all nodes after this one
    for (int i = insertRow + 1; i < m_familyNodes.size(); ++i)
    {
        m_familyNodes.at(i)->familyIndex = i;
        // Also update all children
        for (TreeNode* child : m_familyNodes.at(i)->children)
        {
            child->familyIndex = i;
            for (TreeNode* grandchild : child->children)
            {
                grandchild->familyIndex = i;
            }
        }
    }

    endInsertRows();

    // Now build the children for this family
    updateFamilyRow(familyId);

    emit familyListChanged();
}

void FamilyTreeModel::removeFamilyRow(const FamilyId& familyId)
{
    int row = m_familyIds.indexOf(familyId);
    if (row < 0)
    {
        return;
    }

    beginRemoveRows(QModelIndex(), row, row);

    // Delete the node
    delete m_familyNodes.at(row);
    m_familyNodes.removeAt(row);
    m_familyIds.removeAt(row);

    // Update familyIndex for all nodes after this one
    for (int i = row; i < m_familyNodes.size(); ++i)
    {
        m_familyNodes.at(i)->familyIndex = i;
        for (TreeNode* child : m_familyNodes.at(i)->children)
        {
            child->familyIndex = i;
            for (TreeNode* grandchild : child->children)
            {
                grandchild->familyIndex = i;
            }
        }
    }

    endRemoveRows();

    emit familyListChanged();
}

void FamilyTreeModel::rebuild()
{
    beginResetModel();

    clearNodes();
    m_familyIds.clear();

    const Document& doc = m_documentManager->document();
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
    std::sort(ids.begin(), ids.end(), [&families](const FamilyId& a, const FamilyId& b) {
        return families.value(a).displayName().toLower()
             < families.value(b).displayName().toLower();
    });

    m_familyIds = ids;

    // Build tree nodes for each family
    for (int i = 0; i < m_familyIds.size(); ++i)
    {
        buildFamilyNode(i);
    }

    endResetModel();

    emit familyListChanged();
}

void FamilyTreeModel::buildFamilyNode(int familyIndex)
{
    const FamilyId& familyId = m_familyIds.at(familyIndex);
    std::optional<Family> opt = m_documentManager->document().findFamilyById(familyId);
    if (!opt)
    {
        return;
    }

    const Family& family = *opt;

    // Create family node
    TreeNode* familyNode = new TreeNode();
    familyNode->type = RowType::Family;
    familyNode->familyIndex = familyIndex;
    familyNode->displayText = family.displayName();
    m_familyNodes.append(familyNode);

    // Add member nodes
    const QList<Person>& members = family.members();
    for (int memberIdx = 0; memberIdx < members.size(); ++memberIdx)
    {
        const Person& person = members.at(memberIdx);

        TreeNode* memberNode = new TreeNode();
        memberNode->type = RowType::Member;
        memberNode->familyIndex = familyIndex;
        memberNode->memberIndex = memberIdx;
        memberNode->parent = familyNode;

        // Show given name if same surname, full name otherwise
        QString displayName = person.givenNames();
        if (!person.surname().isEmpty() && person.surname() != family.surname())
        {
            displayName = person.displayName();
        }
        memberNode->displayText = displayName;

        familyNode->children.append(memberNode);

        // Add member detail nodes (only for non-empty values)
        Phone phone = person.displayPhone();
        if (!phone.isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Phone;
            detailNode->displayText = tr("Phone: %1").arg(QString(phone));
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        if (!person.altPhone().isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::AltPhone;
            detailNode->displayText = tr("Alt: %1").arg(QString(person.altPhone()));
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        if (!person.email().isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Email;
            detailNode->displayText = tr("Email: %1").arg(person.email());
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        if (!person.callings().isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Callings;
            detailNode->displayText = tr("Callings: %1").arg(person.callings().join(", "));
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        // Birthday: show date for everyone, append age for children
        QString birthDisplay = person.birthDateDisplay();
        if (!birthDisplay.isEmpty())
        {
            QString ageStr = person.ageDisplay();
            if (!ageStr.isEmpty())
            {
                birthDisplay += " " + ageStr;
            }

            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Age;
            detailNode->displayText = birthDisplay;
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }

        // If member has no details, add a "No contact info" placeholder
        if (memberNode->children.isEmpty())
        {
            TreeNode* detailNode = new TreeNode();
            detailNode->type = RowType::MemberDetail;
            detailNode->familyIndex = familyIndex;
            detailNode->memberIndex = memberIdx;
            detailNode->detailType = DetailType::Phone;
            detailNode->displayText = tr("No contact info");
            detailNode->parent = memberNode;
            memberNode->children.append(detailNode);
        }
    }

    // Add address node
    TreeNode* addressNode = new TreeNode();
    addressNode->type = RowType::Address;
    addressNode->familyIndex = familyIndex;
    addressNode->parent = familyNode;
    QString addressText = family.address().multiLine();
    addressNode->displayText = addressText.isEmpty() ? tr("No address") : addressText;
    familyNode->children.append(addressNode);

    // Add phone node
    TreeNode* phoneNode = new TreeNode();
    phoneNode->type = RowType::Phone;
    phoneNode->familyIndex = familyIndex;
    phoneNode->parent = familyNode;
    Phone familyPhone = family.displayPhone();
    phoneNode->displayText = familyPhone.isEmpty() ? tr("No phone") : QString(familyPhone);
    familyNode->children.append(phoneNode);

    // Add actions node
    TreeNode* actionsNode = new TreeNode();
    actionsNode->type = RowType::Actions;
    actionsNode->familyIndex = familyIndex;
    actionsNode->parent = familyNode;
    familyNode->children.append(actionsNode);
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
            if (node->type == RowType::Family && m_emergencyManager
                && m_emergencyManager->isActive())
            {
                FamilyId familyId = m_familyIds.at(node->familyIndex);
                EffectiveContactStatus status = m_emergencyManager->familyStatus(familyId);
                if (status != EffectiveContactStatus::NotContacted)
                {
                    return statusIcon(status);
                }
            }
            return QVariant();

        case ResponseStatusRole:
            if (node->type == RowType::Family && m_emergencyManager
                && m_emergencyManager->isActive())
            {
                FamilyId familyId = m_familyIds.at(node->familyIndex);
                return QVariant::fromValue(
                    static_cast<int>(m_emergencyManager->familyStatus(familyId)));
            }
            return QVariant();

        case Qt::ForegroundRole:
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

    const auto& families = m_documentManager->document().families();
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
        auto famId = familyIdAt(index);
        if (famId)
        {
            return SelectionKey::from(*famId);
        }
        return SelectionKey::literal(QString());
    }
    case RowType::Member:
    {
        auto personId = personIdAt(index);
        if (personId)
        {
            return SelectionKey::from(*personId);
        }
        return SelectionKey::literal(QString());
    }
    case RowType::MemberDetail:
    case RowType::Address:
    case RowType::Phone:
    case RowType::Actions:
        // Delegate to parent
        return selectionKeyAt(index.parent());
    default:
        return SelectionKey::literal(QString());
    }
}

void FamilyTreeModel::setCheckable(bool checkable)
{
    m_checkable = checkable;
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

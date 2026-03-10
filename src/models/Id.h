#pragma once

#include <QString>
#include <QUuid>
#include <QDebug>
#include <QMetaType>

template<typename Derived>
class IdBase
{
    QString m_value;

protected:
    IdBase() = default;
    explicit IdBase(const QString& value) : m_value(value) {}

public:
    static Derived generate()
    {
        return Derived(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }

    static Derived fromString(const QString& value) { return Derived(value); }

    const QString& toString() const
    {
        Q_ASSERT(!m_value.isEmpty());
        return m_value;
    }

    friend bool operator==(const Derived& a, const Derived& b)
    {
        return a.m_value == b.m_value;
    }

    friend bool operator!=(const Derived& a, const Derived& b) { return !(a == b); }

    friend bool operator<(const Derived& a, const Derived& b)
    {
        return a.m_value < b.m_value;
    }

    friend size_t qHash(const Derived& id, size_t seed = 0)
    {
        return ::qHash(id.m_value, seed);
    }

    friend QDebug operator<<(QDebug dbg, const Derived& id)
    {
        QDebugStateSaver saver(dbg);
        dbg.nospace() << Derived::typeName() << '(' << id.m_value << ')';
        return dbg;
    }
};

// === Derived ID types ===

class PersonId : public IdBase<PersonId>
{
    friend class IdBase<PersonId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "PersonId"; }
};

class FamilyId : public IdBase<FamilyId>
{
    friend class IdBase<FamilyId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "FamilyId"; }
};

class TeamId : public IdBase<TeamId>
{
    friend class IdBase<TeamId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "TeamId"; }
};

class TagId : public IdBase<TagId>
{
    friend class IdBase<TagId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "TagId"; }
};

class EmergencyAssetId : public IdBase<EmergencyAssetId>
{
    friend class IdBase<EmergencyAssetId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "EmergencyAssetId"; }
};

class MinisteringGroupId : public IdBase<MinisteringGroupId>
{
    friend class IdBase<MinisteringGroupId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "MinisteringGroupId"; }
};

class MinisteringDistrictId : public IdBase<MinisteringDistrictId>
{
    friend class IdBase<MinisteringDistrictId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "MinisteringDistrictId"; }
};

class TaskId : public IdBase<TaskId>
{
    friend class IdBase<TaskId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "TaskId"; }
};

class ContactAttemptId : public IdBase<ContactAttemptId>
{
    friend class IdBase<ContactAttemptId>;
    using IdBase::IdBase;
public:
    static constexpr const char* typeName() { return "ContactAttemptId"; }
};

// === SelectionKey ===

class SelectionKey
{
    QString m_value;
    explicit SelectionKey(const QString& value) : m_value(value) {}

public:
    template<typename Id>
    static SelectionKey from(const Id& id) { return SelectionKey(id.toString()); }

    static SelectionKey literal(const QString& value) { return SelectionKey(value); }

    friend bool operator==(const SelectionKey& a, const SelectionKey& b) { return a.m_value == b.m_value; }
    friend bool operator!=(const SelectionKey& a, const SelectionKey& b) { return !(a == b); }
};

// === Qt metatype registration ===
// Required so ID types can be stored in QVariant (used by data() roles in tree models)
// and passed through signal/slot connections.

Q_DECLARE_METATYPE(PersonId)
Q_DECLARE_METATYPE(FamilyId)
Q_DECLARE_METATYPE(TeamId)
Q_DECLARE_METATYPE(TagId)
Q_DECLARE_METATYPE(EmergencyAssetId)
Q_DECLARE_METATYPE(MinisteringGroupId)
Q_DECLARE_METATYPE(MinisteringDistrictId)
Q_DECLARE_METATYPE(TaskId)
Q_DECLARE_METATYPE(ContactAttemptId)

#pragma once

#include <QSet>
#include <optional>

/// One value a command changes, as the user saw it and as they set it.
///
/// The value holds for the command when it is at either end: at before, the command sets
/// it; at after, the command's result already holds there. Any other value means the
/// ground moved, and the command conflicts.
template<typename T>
class Change
{
public:
    Change(const T& before, const T& after) : m_before(before), m_after(after) {}

    const T& before() const { return m_before; }
    const T& after() const { return m_after; }

    bool holds(const T& current) const { return current == m_before || current == m_after; }
    Change swapped() const { return Change(m_after, m_before); }

private:
    T m_before;
    T m_after;
};

/// The change from before to after, or none when they are equal.
template<typename T>
std::optional<Change<T>> changeBetween(const T& before, const T& after) { return before == after ? std::nullopt : std::optional<Change<T>>(Change<T>(before, after)); }

/// True when there is no change to this value, or the value holds for its change.
template<typename T>
bool changeHolds(const std::optional<Change<T>>& change, const T& current) { return !change.has_value() || change->holds(current); }

/// The value the change sets, or current when there is no change to it.
template<typename T>
T changedValue(const std::optional<Change<T>>& change, const T& current) { return change.has_value() ? change->after() : current; }

template<typename T>
std::optional<Change<T>> changeSwapped(const std::optional<Change<T>>& change) { return change.has_value() ? std::optional<Change<T>>(change->swapped()) : std::nullopt; }

/// A change to a set of ids, held as the ids added and the ids removed. An addition is done
/// when the id is present and a removal when it is absent, so a set change holds whatever
/// the set holds, and two users adding different ids to one set both apply.
template<typename Id>
class SetChange
{
public:
    SetChange(const QSet<Id>& added, const QSet<Id>& removed) : m_added(added), m_removed(removed) {}

    static SetChange between(const QSet<Id>& before, const QSet<Id>& after) { return SetChange(after - before, before - after); }

    const QSet<Id>& added() const { return m_added; }
    const QSet<Id>& removed() const { return m_removed; }
    bool isEmpty() const { return m_added.isEmpty() && m_removed.isEmpty(); }

    QSet<Id> appliedTo(const QSet<Id>& current) const { return (current | m_added) - m_removed; }
    SetChange swapped() const { return SetChange(m_removed, m_added); }

private:
    QSet<Id> m_added;
    QSet<Id> m_removed;
};

#include "SessionTimers.h"

void QtSessionTimers::start(Timer timer, int milliseconds)
{
    timerFor(timer)->start(milliseconds);
}

void QtSessionTimers::stop(Timer timer)
{
    timerFor(timer)->stop();
}

QTimer* QtSessionTimers::timerFor(Timer timer)
{
    const int key = int(timer);
    QTimer* existing = m_timers.value(key, nullptr);
    if (existing != nullptr)
    {
        return existing;
    }

    QTimer* created = new QTimer(this);
    created->setSingleShot(true);
    connect(created, &QTimer::timeout, this, [this, timer]() { fire(timer); });
    m_timers.insert(key, created);
    return created;
}

void QtSessionTimers::fire(Timer timer)
{
    if (onTimeout)
    {
        onTimeout(timer);
    }
}

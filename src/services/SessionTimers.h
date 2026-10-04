#pragma once

#include <QHash>
#include <QObject>
#include <QTimer>

#include <functional>

/// A session's timers, or the roster change queue's. The app runs them on
/// QTimer; tests fire them by hand, so no test waits on the clock and no
/// assertion depends on event-loop timing.
class SessionTimers
{
public:
    enum class Timer
    {
        Save,       ///< the debounce before an edit is written
        Retry,      ///< the backoff before an unreachable publish is tried again
        Heartbeat,  ///< refreshes the lease
        HeadCheck   ///< asks the server whether it has moved
    };

    /// The Retry backoff: from 5 seconds, doubling to at most 5 minutes, reset by
    /// any success.
    static constexpr int FirstRetryMilliseconds = 5 * 1000;
    static constexpr int LastRetryMilliseconds = 5 * 60 * 1000;

    virtual ~SessionTimers() = default;

    /// Single-shot. Restarts one already running.
    virtual void start(Timer timer, int milliseconds) = 0;
    virtual void stop(Timer timer) = 0;

    /// Set by the session or queue that owns these timers.
    std::function<void(Timer)> onTimeout;
};

/// SessionTimers on QTimer, for the app. Owned by its session or queue, so it
/// has no Qt parent.
class QtSessionTimers : public QObject, public SessionTimers
{
public:
    QtSessionTimers() = default;

    void start(Timer timer, int milliseconds) override;
    void stop(Timer timer) override;

private:
    QTimer* timerFor(Timer timer);
    void fire(Timer timer);

    QHash<int, QTimer*> m_timers;
};

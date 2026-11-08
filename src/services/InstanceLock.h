#pragma once

#include <QLockFile>
#include <QString>

/// The per-user lock that keeps one Ward Planning running, so two processes never write the
/// same ward file and journal. Only a dead holder makes it stale, never its age.
class InstanceLock
{
public:
    explicit InstanceLock(const QString& path);

    /// Takes the lock, making its folder when there is none. Answers false when another process
    /// holds the lock or the lock cannot be taken.
    bool take();

private:
    QLockFile m_lock;
};

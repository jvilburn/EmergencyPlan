#include "InstanceLock.h"

#include <QDir>
#include <QFileInfo>

InstanceLock::InstanceLock(const QString& path)
    : m_lock(path)
{
    m_lock.setStaleLockTime(0);
}

bool InstanceLock::take()
{
    QDir().mkpath(QFileInfo(m_lock.fileName()).absolutePath());
    return m_lock.tryLock(0);
}

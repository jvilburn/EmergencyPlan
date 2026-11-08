// WardPlanningLockTests
//
// Runs LockTest, which starts copies of this executable.
//
// WardPlanningLockTests hold <path>
//
// Takes the instance lock at path through InstanceLock and prints held or refused on a line.
// Held, it keeps the lock until its standard input closes, then exits 0; refused, it exits 1.

#include "InstanceLock.h"
#include "LockTest.h"

#include <QCoreApplication>
#include <QStringList>
#include <QTest>

#include <iostream>
#include <limits>

namespace
{

int hold(const QString& path)
{
    InstanceLock lock(path);
    if (!lock.take())
    {
        std::cout << "refused" << std::endl;
        return 1;
    }
    std::cout << "held" << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max());
    return 0;
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QStringList arguments = app.arguments();
    if (arguments.size() == 3 && arguments[1] == "hold")
    {
        return hold(arguments[2]);
    }

    LockTest lockTest(nullptr);
    return QTest::qExec(&lockTest, argc, argv);
}

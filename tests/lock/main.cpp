// WardPlanningLockTests
//
// Runs LockTest, which starts copies of this executable.
//
// WardPlanningLockTests hold <path>
//
// Takes the instance lock at path through InstanceLock and prints held or refused on a line.
// Held, it listens for handovers, prints "received <message>" for the first one and exits 0;
// refused, it exits 1.
//
// WardPlanningLockTests hand <path> <message>
//
// Hands message to the holder of the lock at path and prints handed or not handed; prints held
// and exits 1 when it takes the lock itself.

#include "InstanceLock.h"
#include "LockTest.h"

#include <QCoreApplication>
#include <QStringList>
#include <QTest>

#include <iostream>

namespace
{

int hold(QCoreApplication& app, const QString& path)
{
    InstanceLock lock(path, nullptr);
    if (!lock.take())
    {
        std::cout << "refused" << std::endl;
        return 1;
    }
    QObject::connect(&lock, &InstanceLock::handedOver, &app,
                     [](const QString& message)
                     {
                         std::cout << "received " << message.toStdString() << std::endl;
                         QCoreApplication::exit(0);
                     });
    lock.listen();
    std::cout << "held" << std::endl;
    return app.exec();
}

int hand(const QString& path, const QString& message)
{
    InstanceLock lock(path, nullptr);
    if (lock.take())
    {
        std::cout << "held" << std::endl;
        return 1;
    }
    bool handed = lock.handOver(message);
    std::cout << (handed ? "handed" : "not handed") << std::endl;
    return handed ? 0 : 1;
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QStringList arguments = app.arguments();
    if (arguments.size() == 3 && arguments[1] == "hold")
    {
        return hold(app, arguments[2]);
    }
    if (arguments.size() == 4 && arguments[1] == "hand")
    {
        return hand(arguments[2], arguments[3]);
    }

    LockTest lockTest(nullptr);
    return QTest::qExec(&lockTest, argc, argv);
}

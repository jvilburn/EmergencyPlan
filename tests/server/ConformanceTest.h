#pragma once

#include "Server.h"

#include <QObject>

class FakeServer;
class QSignalSpy;

/// What a server must do, written against Server and run on any Transport: on the fake server
/// in every test run, and on the real one over the network when WARD_PLANNING_SERVER_URL is
/// set. A disagreement is fixed in whichever server is wrong.
///
/// It makes four wards and six create calls, inside the real server's limit of ten creates a
/// minute from one address, and leaves those wards on the server, one of them deleted.
class ConformanceTest : public QObject
{
    Q_OBJECT

public:
    /// Runs against transport. fake is that transport when it is the fake server, which
    /// answers only when settled; otherwise null, and answers are awaited on the event loop.
    ConformanceTest(Transport* transport, FakeServer* fake, QObject* parent);

private slots:
    void createAndRead();
    void whoMayWriteAndOnlyAHigherNumberIsKept();
    void removalAndDeletion();
    void theChannel();

private:
    /// Waits until spy has heard count signals; false when they do not come.
    bool heard(QSignalSpy& spy, int count);

    bool stored(ServerWrite* write);
    ServerRead answered(ServerReading* reading);

    Server m_server;
    FakeServer* m_fake;

    static constexpr int WaitMilliseconds = 10000;
};

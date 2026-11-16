#pragma once

#include "Server.h"
#include "Transport.h"

#include <QObject>
#include <QSet>

class FakeServer;
class Identity;
class QSignalSpy;

/// A Transport that passes everything to another and remembers the status of the last reply,
/// so a test written against Server can also check which status refused a write.
class StatusRecorder : public QObject, public Transport
{
    Q_OBJECT

public:
    StatusRecorder(Transport* transport, QObject* parent);

    TransportCall* send(const TransportRequest& request, QObject* parent) override;
    TransportChannel* open(const TransportRequest& request, QObject* parent) override;

    /// The status of the reply that arrived last; NoAnswer before any has.
    int lastStatus() const { return m_lastStatus; }

private slots:
    void onAnswered(const TransportReply& reply);

private:
    Transport* m_transport;
    int m_lastStatus = TransportReply::NoAnswer;
};

/// What a server must do, written against Server and run on any Transport: on the fake server
/// in every test run, and on the real one over the network when WARD_PLANNING_SERVER_URL is
/// set. Each refused write is checked for the status server/README.md gives it, and by reading
/// the ward back. A disagreement is fixed in whichever server is wrong.
///
/// It makes three wards with eight create calls, inside the real server's limit of ten creates
/// a minute from one address, and leaves those wards on the server, one of them deleted.
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
    void theChannelRemovalAndDeletion();

private:
    /// Waits until spy has heard count signals; false when they do not come.
    bool heard(QSignalSpy& spy, int count);

    /// Waits until the last message told is present and number; false when it does not come.
    /// A server may say the same thing more than once, so the count of messages is not held.
    bool toldAtLast(QSignalSpy& told, const QSet<QByteArray>& present, qint64 number);

    /// The status that answered write; NoAnswer when nothing did.
    int statusOf(ServerWrite* write);

    /// The status that answered request, sent as it stands; NoAnswer when nothing did.
    int statusOf(const TransportRequest& request);

    ServerRead answered(ServerReading* reading);

    /// The Ward-Signer and Ward-Signature headers of signer signing line.
    static QList<QPair<QByteArray, QByteArray>> signedBy(const Identity& signer, const QByteArray& line);

    StatusRecorder m_recorder;
    Server m_server;
    FakeServer* m_fake;

    static constexpr int WaitMilliseconds = 10000;
};

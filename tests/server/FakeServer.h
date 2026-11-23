#pragma once

#include "Id.h"
#include "Transport.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QString>

class FakeCall;
class FakeChannel;

/// The Ward Planning server, in process: a Transport that keeps each ward's current version by
/// the rules server/src/ward.ts keeps, answers the statuses server/README.md lists, and holds
/// each ward's channels. A test can make it misbehave.
///
/// A request changes what the server holds as it is sent, and nothing is answered inside send
/// or open: each reply and each channel event (a message, a close) joins one queue, and
/// deliverNext, deliverNextReply or settle hands them over. So a test decides what has arrived
/// when, with no timer and no event loop. The real server sends a write's reply and the channel
/// events that write causes on separate connections, so either may arrive first:
/// setRepliesFirst chooses.
///
/// It reads headers with the app's WardHeader, where the real server has its own reader; the
/// conformance suite is what holds the two servers to one answer.
class FakeServer : public Transport
{
public:
    FakeServer() = default;
    ~FakeServer() override;

    TransportCall* send(const TransportRequest& request, QObject* parent) override;
    TransportChannel* open(const TransportRequest& request, QObject* parent) override;

    /// While unreachable, every request gets no answer and stores nothing, and no channel
    /// opens. Becoming unreachable closes every channel.
    void setReachable(bool reachable);

    /// The next create or push is handled as usual, stored and told to the channels when the
    /// rules keep it, and gets no answer.
    void dropNextWriteReply() { m_dropNextWriteReply = true; }

    /// A write's reply joins the queue ahead of the channel events the write causes when set,
    /// and behind them otherwise, which is how it starts.
    void setRepliesFirst(bool first) { m_repliesFirst = first; }

    /// Hands over the oldest reply or channel event that is waiting; false when none is.
    bool deliverNext();

    /// Hands over the oldest reply that is waiting, ahead of any channel event queued before
    /// it; false when none is.
    bool deliverNextReply();

    /// Hands over every waiting reply and channel event, and those they lead to, until none
    /// is left.
    void settle();

    /// What GET /releases/latest answers with; until it is set the answer is 404.
    void setReleases(const QByteArray& list) { m_releases = list; }

    /// The ward's current version becomes bytes, whatever the rules say, as a restored or
    /// hostile server would serve it. Tells no channel.
    void serve(const DocumentId& document, const QByteArray& bytes);

    /// Forgets the ward and closes its channels.
    void forget(const DocumentId& document);

    /// The bytes of the ward's current version; empty when it holds none.
    QByteArray version(const DocumentId& document) const;

    /// The number of the ward's current version; 0 when it holds none.
    qint64 number(const DocumentId& document) const;

    /// How many requests it has received, channel opens among them.
    int calls() const { return m_calls.size(); }

    /// How many requests of method it has received; a channel open counts as OPEN.
    int calls(const QByteArray& method) const { return m_calls.count(method); }

    /// The fingerprint of every device with a channel open to the ward.
    QSet<QByteArray> present(const DocumentId& document) const;

    /// A channel was closed from its client's side, or destroyed.
    void channelGone(FakeChannel* channel);

private:
    enum class EventKind
    {
        Reply,
        Message,
        Closed
    };

    /// One thing on its way to a client.
    struct Event
    {
        EventKind kind;
        QPointer<QObject> target;  ///< the call or the channel, when it still exists
        TransportReply reply;      ///< for a Reply
        QByteArray message;        ///< for a Message
    };

    /// Hands event over, when what it is for still exists; false when it does not.
    bool deliver(const Event& event);

    /// One ward: its current version and its open channels.
    struct Ward
    {
        QByteArray bytes;
        QList<FakeChannel*> channels;
    };

    TransportReply handle(const TransportRequest& request);
    TransportReply create(const TransportRequest& request);
    TransportReply push(const TransportRequest& request, const QString& document);
    TransportReply read(const QString& document, bool whole) const;

    /// Queues a close for channel and takes it out of its ward.
    void closeFromServer(FakeChannel* channel);

    /// Queues, for every open channel of the ward, who is here and the current version's number.
    void tellEveryone(const QString& document);

    QHash<QString, Ward> m_wards;
    QList<Event> m_events;
    QList<QByteArray> m_calls;
    QByteArray m_releases;
    bool m_reachable = true;
    bool m_dropNextWriteReply = false;
    bool m_repliesFirst = false;
};

#pragma once

#include "Server.h"
#include "WardSession.h"

#include <QObject>
#include <QSet>
#include <QTimer>
#include <optional>

/// What stops the open ward's changes from being shared, when something does.
enum class SyncProblem
{
    None,
    Unreachable,  ///< the server did not answer
    NewerFormat,  ///< a newer Ward Planning wrote the version on the server
    Unreadable,   ///< the version on the server fails a check, so it was not taken
    PushFault     ///< the server did not store this device's version, and its head has not moved
};

/// Shares the session's open ward through the server. Its one operation is the look: it reads
/// the server's head and acts on how the head ranks against the version this device holds.
///
/// - The server has no such ward: one this device has seen there is gone, so the ward is no
///   longer this device's; any other is created there from this device's next version.
/// - The head ranks above: its version is fetched and taken, and the journal replays onto it.
/// - The head is the version this device holds: with changes to share, this device's next
///   version is pushed; otherwise there is nothing to do.
/// - The head ranks below: this device's next version is pushed over it.
///
/// A write the server stores is held. After any other answer, or none, it looks again, and the
/// head says what happened: a newer head is taken and replayed onto, and a head that has not
/// moved is a fault, left for the next retry. One look runs at a time, with one call to the
/// server on its way; asking for a look while one runs makes it look once more when it ends.
/// A write is built only from what is saved: a save that is pending is tried first, and while
/// it fails nothing is written. A look that finds the ward is no longer this device's stops
/// the sharing.
///
/// It holds the ward's live channel while it runs. Each message says who is here and the
/// server's version number, and a number above this device's asks for a look. A channel that
/// closes asks for one too, which finds a removal when that is why it closed.
class WardSync : public QObject
{
    Q_OBJECT

public:
    /// Shares session's ward through server. It owns neither.
    WardSync(WardSession* session, Server* server, QObject* parent);

    /// Starts sharing the session's open ward: opens its channel and looks. A ward that is no
    /// longer this device's is not started.
    void start();

    /// Stops: the channel closes, and the calls on their way are dropped with their answers.
    /// Before the session opens another ward, and when the server is turned off.
    void stop();

    /// Opens the channel when it is closed, and looks, which tries again whatever is pending.
    void retry();

    /// Looks, as the class comment describes. Does nothing while stopped.
    void look();

    /// Asks for an invitation for invite, one of the session's invitesMadeHere, that carries
    /// the ward's version as the server has it, so whoever joins through it starts from a
    /// version the server holds. The look under way, with the pushes it makes, ends first.
    /// invitationFetched answers.
    void fetchInvitation(const QByteArray& invite);

    bool isRunning() const { return m_running; }

    /// True while the channel is open: the server has answered on it and it has not closed.
    bool isConnected() const { return m_connected; }

    SyncProblem problem() const { return m_problem; }

    /// A ward is syncing while it is connected and nothing stops its pushes: no problem, no
    /// save pending, and the ward still this device's.
    bool isSyncing() const;

    /// The fingerprint of every other device with the ward open, as the channel last said;
    /// empty while it is closed. This device, which the server lists under the fingerprint
    /// its channel was signed with, is left out.
    const QSet<QByteArray>& present() const { return m_present; }

signals:
    /// isConnected, problem or isSyncing changed.
    void stateChanged();

    void presenceChanged();

    /// A version fetched from the server was put to the session, which answered result.
    void versionFetched(const TakeResult& result);

    /// The answer to fetchInvitation: the invitation's bytes, or none when the server did not
    /// answer, sharing stopped, or its version does not name the invite.
    void invitationFetched(const QByteArray& invite, const QByteArray& invitation);

private slots:
    void onLocalChange();
    void onHead(const ServerRead& head);
    void onFetched(const ServerRead& version);
    void onWritten(bool stored);
    void onInvitationVersionFetched(const ServerRead& version);
    void onTold(const QSet<QByteArray>& present, qint64 version);
    void onChannelClosed();
    void onRetryTimer();

private:
    /// A write on its way: the version sent, the version this device held when it was built,
    /// the head it went over, and whether it carries a change the journal does not hold.
    struct Writing
    {
        NextVersion next;
        VersionRank base;
        VersionRank over;
        bool unjournalledChange;
    };

    /// Opens the channel, signed by the session's writer.
    void openChannel();

    /// Closes the channel and forgets who is here.
    void closeChannel();

    /// Opens the channel anew when it is not connected or was signed by another writer than
    /// the session's now. Called when the server may have just come to accept one: the ward is
    /// first seen there, a write is stored, or a version is taken. The server refuses a
    /// channel to a ward it does not have, and closes one signed by an invite once a version
    /// names the device in the invite's place.
    void freshenChannel();

    /// Sends this device's next version over the head over: a create when over is NoHead, the
    /// server having no such ward, otherwise a push. A write the server did not store over
    /// this same head is not sent again; that is the fault.
    void write(const VersionRank& over);

    /// Ends the look with problem. Then looks once more when one was asked for meanwhile, and
    /// otherwise fetches the invitation that was asked for.
    void finish(SyncProblem problem);

    /// Ends the look and starts another at once.
    void lookAgain();

    /// Fetches the server's version for the invitation asked for, or answers none when
    /// something stops the sharing.
    void startInvitationFetch();

    void setConnected(bool connected);

    WardSession* m_session;
    Server* m_server;
    ServerChannel* m_channel = nullptr;
    QByteArray m_channelSigner;   ///< the fingerprint the channel was opened under
    QObject* m_call = nullptr;    ///< the look's call on its way, which deletes itself once answered
    QObject* m_invitationCall = nullptr;  ///< the invitation's fetch on its way
    QTimer m_retryTimer;
    bool m_running = false;
    bool m_connected = false;
    bool m_looking = false;
    bool m_lookAsked = false;     ///< a look was asked for while one ran
    /// An automatic update changed the ward since the last write was built. Nothing in the
    /// journal shows such a change, so this is what says it is still to share.
    bool m_unjournalledChange = false;
    SyncProblem m_problem = SyncProblem::None;
    QSet<QByteArray> m_present;
    std::optional<Writing> m_writing;

    /// The head a write went over that the server did not store.
    std::optional<VersionRank> m_unstoredOver;

    /// The invite an invitation was asked for and not yet answered.
    std::optional<QByteArray> m_invitationWanted;

    static constexpr int RetryMilliseconds = 30000;
};

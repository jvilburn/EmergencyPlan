#pragma once

#include "Ward.h"
#include "Stake.h"

#include <QObject>
#include <QProcess>

/// Async service for looking up ward/stake metadata from the church
/// meetinghouse locator website.
///
/// Uses headless Edge browser to render the JavaScript-heavy pages and
/// extract the resulting HTML. This is necessary because the site is a
/// Next.js React app that requires JavaScript execution.
///
/// Two-step lookup process:
/// 1. lookupWard(wardUnitNumber) -> returns ward info + stake unit number
/// 2. lookupStake(stakeUnitNumber) -> returns stake info
///
/// The lookup is triggered after PDF import, using the ward unit number
/// extracted from the PDF header. DocumentManager coordinates the two-step
/// process automatically.
class UnitLookupService : public QObject
{
    Q_OBJECT

public:
    explicit UnitLookupService(QObject* parent);
    ~UnitLookupService() override;

    /// Initiate async lookup for ward metadata.
    /// @param wardUnitNumber The ward unit number to look up (e.g., "123456")
    void lookupWard(const QString& wardUnitNumber);

    /// Initiate async lookup for stake metadata.
    /// @param stakeUnitNumber The stake unit number to look up (e.g., "654321")
    void lookupStake(const QString& stakeUnitNumber);

    /// Cancel any pending lookup request.
    void cancel();

    /// Check if a lookup is currently in progress.
    bool isLookupInProgress() const { return m_pendingProcess != nullptr; }

signals:
    /// Emitted when ward lookup completes successfully.
    /// Ward will have chapel info and stakeUnitNumber populated.
    void wardLookupComplete(const QString& wardUnitNumber, const Ward& ward);

    /// Emitted when stake lookup completes successfully.
    /// Stake will have stake center info populated.
    void stakeLookupComplete(const QString& stakeUnitNumber, const Stake& stake);

    /// Emitted when any lookup fails.
    void lookupFailed(const QString& unitNumber, const QString& error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum class LookupType
    {
        Ward,
        Stake
    };

    static QString findEdgePath();
    void startLookup(const QString& unitNumber, LookupType type);
    void parseWardResponse(const QString& wardUnitNumber, const QByteArray& data);
    void parseStakeResponse(const QString& stakeUnitNumber, const QByteArray& data);

    static QString s_edgePath;
    QProcess* m_pendingProcess = nullptr;
    QString m_currentUnitNumber;
    LookupType m_currentLookupType = LookupType::Ward;
};

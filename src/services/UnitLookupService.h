#pragma once

#include "UnitLookup.h"

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
/// WardSession runs the lookups one at a time, as automatic updates.
class UnitLookupService : public UnitLookup
{
    Q_OBJECT

public:
    explicit UnitLookupService(QObject* parent);
    ~UnitLookupService() override;

    /// Initiate async lookup for ward metadata.
    /// @param wardUnitNumber The ward unit number to look up (e.g., "123456")
    void lookupWard(const QString& wardUnitNumber) override;

    /// Initiate async lookup for stake metadata.
    /// @param stakeUnitNumber The stake unit number to look up (e.g., "654321")
    void lookupStake(const QString& stakeUnitNumber) override;

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

    /// Stops the lookup under way, if any, which then reports nothing.
    void cancel();
    void parseWardResponse(const QString& wardUnitNumber, const QByteArray& data);
    void parseStakeResponse(const QString& stakeUnitNumber, const QByteArray& data);

    static QString s_edgePath;
    QProcess* m_pendingProcess = nullptr;
    QString m_currentUnitNumber;
    LookupType m_currentLookupType = LookupType::Ward;
};

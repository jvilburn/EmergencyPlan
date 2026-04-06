#include "UnitLookupService.h"

#include <QDir>
#include <QRegularExpression>

// Church meetinghouse locator base URL
static const QString BASE_URL = "https://maps.churchofjesuschrist.org";

QString UnitLookupService::s_edgePath;

UnitLookupService::UnitLookupService(QObject* parent)
    : QObject(parent)
{
    if (s_edgePath.isEmpty())
    {
        s_edgePath = findEdgePath();
    }
}

UnitLookupService::~UnitLookupService()
{
    cancel();
}

QString UnitLookupService::findEdgePath()
{
    // Check standard Edge install locations
    QStringList candidates = {
        "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe",
        "C:/Program Files/Microsoft/Edge/Application/msedge.exe",
    };

    for (const QString& path : candidates)
    {
        if (QFile::exists(path))
        {
            return path;
        }
    }

    qWarning() << "Microsoft Edge not found - ward/stake lookup will be unavailable";
    return {};
}

void UnitLookupService::lookupWard(const QString& wardUnitNumber)
{
    if (wardUnitNumber.isEmpty())
    {
        emit lookupFailed(wardUnitNumber, tr("Ward unit number is empty"));
        return;
    }

    startLookup(wardUnitNumber, LookupType::Ward);
}

void UnitLookupService::lookupStake(const QString& stakeUnitNumber)
{
    if (stakeUnitNumber.isEmpty())
    {
        emit lookupFailed(stakeUnitNumber, tr("Stake unit number is empty"));
        return;
    }

    startLookup(stakeUnitNumber, LookupType::Stake);
}

void UnitLookupService::startLookup(const QString& unitNumber, LookupType type)
{
    cancel();

    if (s_edgePath.isEmpty())
    {
        emit lookupFailed(unitNumber, tr("Microsoft Edge not found"));
        return;
    }

    m_currentUnitNumber = unitNumber;
    m_currentLookupType = type;

    QString pathSegment = (type == LookupType::Ward) ? "wards" : "stakes";
    QString url = BASE_URL + "/" + pathSegment + "/" + unitNumber;

    m_pendingProcess = new QProcess(this);
    connect(m_pendingProcess, &QProcess::finished,
            this, &UnitLookupService::onProcessFinished);

    QStringList args = {
        "--headless",
        "--dump-dom",
        "--disable-gpu",
        "--no-sandbox",
        url
    };

    m_pendingProcess->start(s_edgePath, args);
}

void UnitLookupService::cancel()
{
    if (m_pendingProcess)
    {
        m_pendingProcess->kill();
        m_pendingProcess->waitForFinished(3000);
        m_pendingProcess->deleteLater();
        m_pendingProcess = nullptr;
    }
    m_currentUnitNumber.clear();
}

void UnitLookupService::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_pendingProcess == nullptr)
    {
        return;
    }

    QString unitNumber = m_currentUnitNumber;
    LookupType lookupType = m_currentLookupType;
    m_currentUnitNumber.clear();

    QByteArray data = m_pendingProcess->readAllStandardOutput();
    QByteArray errorOutput = m_pendingProcess->readAllStandardError();

    m_pendingProcess->deleteLater();
    m_pendingProcess = nullptr;

    if (exitStatus != QProcess::NormalExit || exitCode != 0)
    {
        QString error = tr("Edge process failed (exit code %1)").arg(exitCode);
        if (!errorOutput.isEmpty())
        {
            error += ": " + QString::fromUtf8(errorOutput).left(200);
        }
        emit lookupFailed(unitNumber, error);
        return;
    }

    if (data.isEmpty())
    {
        emit lookupFailed(unitNumber, tr("Edge returned empty output"));
        return;
    }

    if (lookupType == LookupType::Ward)
    {
        parseWardResponse(unitNumber, data);
    }
    else
    {
        parseStakeResponse(unitNumber, data);
    }
}

void UnitLookupService::parseWardResponse(const QString& wardUnitNumber, const QByteArray& data)
{
    QString html = QString::fromUtf8(data);
    Ward ward;
    ward.setUnitNumber(wardUnitNumber);

    // Ward name: <h1 class="location-header__name">Tanglewood Ward</h1>
    QRegularExpression nameRe(R"(<h1[^>]*>([^<]+)</h1>)");
    QRegularExpressionMatch match = nameRe.match(html);
    if (match.hasMatch())
    {
        ward.setName(match.captured(1).trimmed());
    }

    // Stake: <a ... href="/stakes/510033"> ... <span class="location-link__name">...</span>
    QRegularExpression stakeRe(
        R"RE(href="/stakes/(\d+)"[^>]*>.*?<span class="location-link__name">([^<]+)</span>)RE",
        QRegularExpression::DotMatchesEverythingOption);
    match = stakeRe.match(html);
    if (match.hasMatch())
    {
        ward.setStakeUnitNumber(match.captured(1));
    }

    // Address: <span class="address__text">4260 Clinard Road<br>Clemmons, North Carolina 27012-8485<br>United States</span>
    QRegularExpression addressRe(R"RE(<span class="address__text">(.+?)</span>)RE",
        QRegularExpression::DotMatchesEverythingOption);
    match = addressRe.match(html);
    if (match.hasMatch())
    {
        QString addr = match.captured(1);
        addr.replace(QRegularExpression(R"(<br\s*/?>)"), ", ");  // Handle <br> or <br/>
        addr.replace(QRegularExpression(R"(<[^>]+>)"), "");      // Remove any other HTML tags
        ward.setChapelAddress(addr.trimmed());
    }

    // Coordinates: <div class="address__coordinates">36.014172, -80.393048</div>
    QRegularExpression coordRe(R"RE(<div class="address__coordinates">(.+?)</div>)RE",
        QRegularExpression::DotMatchesEverythingOption);
    match = coordRe.match(html);
    if (match.hasMatch())
    {
        QString coords = match.captured(1).trimmed();
        QRegularExpression latLngRe(R"((-?\d+\.\d+),\s*(-?\d+\.\d+))");
        QRegularExpressionMatch coordMatch = latLngRe.match(coords);
        if (coordMatch.hasMatch())
        {
            ward.setChapelLat(coordMatch.captured(1).toDouble());
            ward.setChapelLng(coordMatch.captured(2).toDouble());
        }
    }

    // Phone: <a ... href="tel:+1 336-766-3607">...</a>
    QRegularExpression phoneRe(R"RE(href="tel:([^"]+)")RE");
    match = phoneRe.match(html);
    if (match.hasMatch())
    {
        ward.setChapelPhone(match.captured(1).trimmed());
    }

    // Meeting time: <div class="hours__line">Sunday 9:00 AM...Sacrament meets first</div>
    QRegularExpression meetingRe(R"(<div class="hours__line">([^<]+(?:<!--[^>]*-->[^<]*)*)</div>)");
    match = meetingRe.match(html);
    if (match.hasMatch())
    {
        QString time = match.captured(1);
        time.replace(QRegularExpression(R"(<!--[^>]*-->)"), "");  // Remove HTML comments
        ward.setMeetingTime(time.trimmed());
    }

    if (!ward.chapelLat().has_value())
    {
        qWarning() << "Ward lookup for" << wardUnitNumber << "- no coordinates found in response";
    }

    emit wardLookupComplete(wardUnitNumber, ward);
}

void UnitLookupService::parseStakeResponse(const QString& stakeUnitNumber, const QByteArray& data)
{
    QString html = QString::fromUtf8(data);
    Stake stake;
    stake.setUnitNumber(stakeUnitNumber);

    // Stake name: <h1 class="location-header__name">Winston-Salem North Carolina Stake</h1>
    QRegularExpression nameRe(R"(<h1 class="location-header__name">([^<]+)</h1>)");
    QRegularExpressionMatch match = nameRe.match(html);
    if (match.hasMatch())
    {
        stake.setName(match.captured(1).trimmed());
    }

    // Address: <span class="address__text">4260 Clinard Road<br>...</span>
    QRegularExpression addressRe(R"RE(<span class="address__text">(.+?)</span>)RE",
        QRegularExpression::DotMatchesEverythingOption);
    match = addressRe.match(html);
    if (match.hasMatch())
    {
        QString addr = match.captured(1);
        addr.replace(QRegularExpression(R"(<br\s*/?>)"), ", ");
        addr.replace(QRegularExpression(R"(<[^>]+>)"), "");
        stake.setStakeCenterAddress(addr.trimmed());
    }

    // Coordinates: <div class="address__coordinates">36.014172, -80.393048</div>
    QRegularExpression coordRe(R"RE(<div class="address__coordinates">(.+?)</div>)RE",
        QRegularExpression::DotMatchesEverythingOption);
    match = coordRe.match(html);
    if (match.hasMatch())
    {
        QString coords = match.captured(1).trimmed();
        QRegularExpression latLngRe(R"((-?\d+\.\d+),\s*(-?\d+\.\d+))");
        QRegularExpressionMatch coordMatch = latLngRe.match(coords);
        if (coordMatch.hasMatch())
        {
            stake.setStakeCenterLat(coordMatch.captured(1).toDouble());
            stake.setStakeCenterLng(coordMatch.captured(2).toDouble());
        }
    }

    // Ward unit numbers: <a ... href="/wards/45004">...
    QSet<QString> wardUnits;
    QRegularExpression wardRe(R"RE(href="/wards/(\d+)")RE");
    QRegularExpressionMatchIterator wardIter = wardRe.globalMatch(html);
    while (wardIter.hasNext())
    {
        QRegularExpressionMatch wardMatch = wardIter.next();
        wardUnits.insert(wardMatch.captured(1));
    }
    stake.setWardUnitNumbers(wardUnits);

    emit stakeLookupComplete(stakeUnitNumber, stake);
}

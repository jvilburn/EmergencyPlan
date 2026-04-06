#include "UnitLookupService.h"

#include <QNetworkReply>
#include <QRegularExpression>

// Church meetinghouse locator APIs
static const QString WARD_API_URL = "https://maps.churchofjesuschrist.org/wards";
static const QString STAKE_API_URL = "https://maps.churchofjesuschrist.org/stakes";

UnitLookupService::UnitLookupService(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &UnitLookupService::onRequestFinished);
}

UnitLookupService::~UnitLookupService()
{
    cancel();
}

void UnitLookupService::lookupWard(const QString& wardUnitNumber)
{
    if (wardUnitNumber.isEmpty())
    {
        emit lookupFailed(wardUnitNumber, tr("Ward unit number is empty"));
        return;
    }

    // Cancel any pending request
    cancel();

    m_currentUnitNumber = wardUnitNumber;
    m_currentLookupType = LookupType::Ward;

    // Check if API is configured
    if (WARD_API_URL.isEmpty())
    {
        return;
    }

    // Build request URL
    QString url = WARD_API_URL + "/" + wardUnitNumber;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EmergencyPlan/1.0");
    request.setRawHeader("Accept", "text/html");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");

    m_pendingReply = m_networkManager->get(request);
}

void UnitLookupService::lookupStake(const QString& stakeUnitNumber)
{
    if (stakeUnitNumber.isEmpty())
    {
        emit lookupFailed(stakeUnitNumber, tr("Stake unit number is empty"));
        return;
    }

    // Cancel any pending request
    cancel();

    m_currentUnitNumber = stakeUnitNumber;
    m_currentLookupType = LookupType::Stake;

    // Check if API is configured
    if (STAKE_API_URL.isEmpty())
    {
        return;
    }

    // Build request URL
    QString url = STAKE_API_URL + "/" + stakeUnitNumber;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EmergencyPlan/1.0");
    request.setRawHeader("Accept", "text/html");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");

    m_pendingReply = m_networkManager->get(request);
}

void UnitLookupService::cancel()
{
    if (m_pendingReply)
    {
        m_pendingReply->abort();
        m_pendingReply->deleteLater();
        m_pendingReply = nullptr;
    }
    m_currentUnitNumber.clear();
}

void UnitLookupService::onRequestFinished(QNetworkReply* reply)
{
    if (reply != m_pendingReply)
    {
        reply->deleteLater();
        return;
    }

    m_pendingReply = nullptr;
    QString unitNumber = m_currentUnitNumber;
    LookupType lookupType = m_currentLookupType;
    m_currentUnitNumber.clear();

    if (reply->error() != QNetworkReply::NoError)
    {
        emit lookupFailed(unitNumber, reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

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

    // Ward name: first h1 tag
    QRegularExpression nameRe(R"(<h1[^>]*>([^<]+)</h1>)");
    QRegularExpressionMatch match = nameRe.match(html);
    if (match.hasMatch())
    {
        ward.setName(match.captured(1).trimmed());
    }

    // Stake: <div class="location-link"><a class="location-link__anchor" href="/stakes/510033">
    //        <span class="location-link__text"><span class="location-link__name">Winston-Salem North Carolina Stake</span></span></a></div>
    QRegularExpression stakeRe(
        R"RE(href="/stakes/(\d+)"[^>]*>.*?<span class="location-link__name">([^<]+)</span>)RE",
        QRegularExpression::DotMatchesEverythingOption);
    match = stakeRe.match(html);
    if (match.hasMatch())
    {
        ward.setStakeUnitNumber(match.captured(1));
        // TODO: pass stake name to DocumentMetadata: match.captured(2).trimmed()
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

    // Phone: <a class="anchor" href="tel:+1 336-766-3607">...<svg>...</svg></span>+1 336-766-3607</a>
    QRegularExpression phoneRe(R"RE(href="tel:([^"]+)"[^>]*>.*?</svg></span>([^<]+)</a>)RE",
        QRegularExpression::DotMatchesEverythingOption);
    match = phoneRe.match(html);
    if (match.hasMatch())
    {
        ward.setChapelPhone(match.captured(2).trimmed());
    }

    // Meeting time: <div class="hours__line">Sunday 9:00 AM<!-- --> - <!-- -->Sacrament meets first</div>
    QRegularExpression meetingRe(R"(<div class="hours__line">([^<]+(?:<!--[^>]*-->[^<]*)*)</div>)");
    match = meetingRe.match(html);
    if (match.hasMatch())
    {
        QString time = match.captured(1);
        time.replace(QRegularExpression(R"(<!--[^>]*-->)"), "");  // Remove HTML comments
        ward.setMeetingTime(time.trimmed());
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

    // Ward unit numbers: <a class="location-link__anchor" href="/wards/45004">...
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

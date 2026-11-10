#include "Birthday.h"

#include <QDate>
#include <QLocale>
#include <QObject>
#include <QRegularExpression>
#include <QStringList>

namespace
{
    // Matches "7 Oct", "23 May", "7 Oct (16)", "23 May (8)": the day, the month and the age
    const QRegularExpression BIRTHDAY_PATTERN(
        R"(^(\d{1,2})\s+(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)(?:\s+\((\d+)\))?$)");

    const QStringList MONTHS = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    /// The part under key, or none when its writer left it out.
    std::optional<int> readPart(JsonReader& json, const QString& key)
    {
        std::optional<qint64> part = json.optionalInteger(key);
        if (!part.has_value())
        {
            return std::nullopt;
        }
        return static_cast<int>(*part);
    }
}

Birthday Birthday::create(std::optional<int> year,
                          std::optional<int> month,
                          std::optional<int> day)
{
    Birthday birthday;
    birthday.m_year = year;
    birthday.m_month = month;
    birthday.m_day = day;
    return birthday;
}

std::optional<int> Birthday::age() const
{
    if (!m_year.has_value() || !m_month.has_value() || !m_day.has_value())
    {
        return std::nullopt;
    }

    QDate birthDate(m_year.value(), m_month.value(), m_day.value());
    if (!birthDate.isValid())
    {
        return std::nullopt;
    }

    QDate today = QDate::currentDate();
    int years = 0;

    // Simple iteration is fine for ~100 years max
    while (birthDate.addYears(years + 1) <= today)
    {
        years++;
    }

    return years;
}

bool Birthday::isChild() const
{
    std::optional<int> personAge = age();
    if (!personAge.has_value())
    {
        return false;
    }
    return personAge.value() < 18;
}

QString Birthday::display() const
{
    QString monthAndDay;
    if (m_month.has_value() && m_day.has_value())
    {
        monthAndDay = QObject::tr("%1 %2").arg(QLocale::c().monthName(*m_month, QLocale::ShortFormat)).arg(*m_day);
    }
    else if (m_month.has_value())
    {
        monthAndDay = QLocale::c().monthName(*m_month, QLocale::ShortFormat);
    }
    else if (m_day.has_value())
    {
        monthAndDay = QObject::tr("day %1").arg(*m_day);
    }

    if (!m_year.has_value())
    {
        return monthAndDay;
    }
    if (monthAndDay.isEmpty())
    {
        return QString::number(*m_year);
    }
    if (m_day.has_value())
    {
        return QObject::tr("%1, %2").arg(monthAndDay).arg(*m_year);
    }
    return QObject::tr("%1 %2").arg(monthAndDay).arg(*m_year);
}

QString Birthday::ageDisplay() const
{
    std::optional<int> personAge = age();
    if (!personAge.has_value())
    {
        return QString();
    }

    if (personAge.value() >= 18)
    {
        return QString();
    }

    return QString("(%1)").arg(personAge.value());
}

QJsonObject Birthday::toJson() const
{
    QJsonObject json;
    if (m_year.has_value())
    {
        json["year"] = m_year.value();
    }
    if (m_month.has_value())
    {
        json["month"] = m_month.value();
    }
    if (m_day.has_value())
    {
        json["day"] = m_day.value();
    }
    return json;
}

Birthday Birthday::fromJson(JsonReader json)
{
    Birthday birthday;
    birthday.m_year = readPart(json, "year");
    birthday.m_month = readPart(json, "month");
    birthday.m_day = readPart(json, "day");
    json.finish();
    return birthday;
}

bool Birthday::operator==(const Birthday& other) const
{
    return m_year == other.m_year
        && m_month == other.m_month
        && m_day == other.m_day;
}

bool Birthday::isBirthday(const QString& text)
{
    return BIRTHDAY_PATTERN.match(text.trimmed()).hasMatch();
}

Birthday Birthday::fromPdfText(const QString& text, const QDate& documentDate)
{
    QRegularExpressionMatch match = BIRTHDAY_PATTERN.match(text.trimmed());
    if (!match.hasMatch())
    {
        return Birthday();
    }
    int day = match.captured(1).toInt();
    int month = MONTHS.indexOf(match.captured(2)) + 1;
    if (match.capturedLength(3) == 0)
    {
        return create(std::nullopt, month, day);
    }

    // The age is as of documentDate, so a birthday still to come that year is one year
    // further back.
    int year = documentDate.year() - match.captured(3).toInt();
    if (month > documentDate.month() || (month == documentDate.month() && day > documentDate.day()))
    {
        --year;
    }
    return create(year, month, day);
}

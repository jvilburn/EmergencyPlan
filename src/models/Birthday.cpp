#include "Birthday.h"
#include "StrictJson.h"

#include <QDate>
#include <QLocale>
#include <QRegularExpression>

namespace
{
    // Matches "7 Oct", "23 May", "7 Oct (16)", "23 May (8)"
    const QRegularExpression BIRTHDAY_PATTERN(
        R"(^\d{1,2}\s+(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)(\s+\(\d+\))?$)");
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

QString Birthday::dateDisplay() const
{
    if (!m_month.has_value() || !m_day.has_value())
    {
        return QString();
    }

    // Use a reference year to get month name
    QDate displayDate(2000, m_month.value(), m_day.value());
    if (!displayDate.isValid())
    {
        return QString();
    }

    QString monthName = QLocale::c().monthName(m_month.value(), QLocale::ShortFormat);

    if (m_year.has_value())
    {
        return QString("%1 %2 %3")
            .arg(m_day.value())
            .arg(monthName)
            .arg(m_year.value());
    }

    return QString("%1 %2").arg(m_day.value()).arg(monthName);
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

std::optional<Birthday> Birthday::fromJson(const QJsonObject& json)
{
    // toJson is written only for a birthday with a date, so an empty object is
    // never one of its outputs.
    if (json.isEmpty()
        || !StrictJson::hasOnlyKeys(json,
                                    {QLatin1String("year"), QLatin1String("month"), QLatin1String("day")}))
    {
        return std::nullopt;
    }

    Birthday birthday;
    if (!StrictJson::readOptionalInt(json, QLatin1String("year"), birthday.m_year)
        || !StrictJson::readOptionalInt(json, QLatin1String("month"), birthday.m_month)
        || !StrictJson::readOptionalInt(json, QLatin1String("day"), birthday.m_day))
    {
        return std::nullopt;
    }

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

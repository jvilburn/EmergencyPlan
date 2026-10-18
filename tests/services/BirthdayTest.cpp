#include "BirthdayTest.h"
#include "Birthday.h"

#include <QTest>

namespace
{

/// The part, or none when the test row gives 0.
std::optional<int> part(int value)
{
    if (value == 0)
    {
        return std::nullopt;
    }
    return value;
}

}  // namespace

BirthdayTest::BirthdayTest(QObject* parent)
    : QObject(parent)
{
}

void BirthdayTest::displayShowsWhicheverPartsAreSet_data()
{
    QTest::addColumn<int>("year");
    QTest::addColumn<int>("month");
    QTest::addColumn<int>("day");
    QTest::addColumn<QString>("expected");

    QTest::newRow("month day year") << 1980 << 1 << 4 << QString("Jan 4, 1980");
    QTest::newRow("month day") << 0 << 1 << 4 << QString("Jan 4");
    QTest::newRow("month year") << 1980 << 1 << 0 << QString("Jan 1980");
    QTest::newRow("year") << 1980 << 0 << 0 << QString("1980");
    QTest::newRow("month") << 0 << 1 << 0 << QString("Jan");
    QTest::newRow("day year") << 1980 << 0 << 4 << QString("day 4, 1980");
    QTest::newRow("day") << 0 << 0 << 4 << QString("day 4");
    QTest::newRow("none") << 0 << 0 << 0 << QString();
}

void BirthdayTest::displayShowsWhicheverPartsAreSet()
{
    QFETCH(int, year);
    QFETCH(int, month);
    QFETCH(int, day);
    QFETCH(QString, expected);

    QCOMPARE(Birthday::create(part(year), part(month), part(day)).display(), expected);
}

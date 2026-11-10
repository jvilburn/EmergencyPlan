#pragma once

#include <QObject>

/// The filter's text search matches a person's note and a family's note, and a team or asset
/// deleted since it was chosen filters nothing.
class FilterTest : public QObject
{
    Q_OBJECT

public:
    explicit FilterTest(QObject* parent);

private slots:
    void searchMatchesAPersonsNote();
    void searchMatchesAFamilysNote();
    void deletedTeamOrAssetFiltersNothing();
};

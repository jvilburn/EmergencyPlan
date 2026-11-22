#pragma once

#include <QObject>

/// The update check offers the release the server lists for a platform exactly when that
/// release's number is above the running app's.
class UpdateCheckTest : public QObject
{
    Q_OBJECT

public:
    explicit UpdateCheckTest(QObject* parent);

private slots:
    void aReleaseWithAHigherNumberIsOffered();
    void aReleaseThatIsNotLaterIsNotOffered();
    void aListWithoutAReleaseForThePlatformOffersNothing();
};

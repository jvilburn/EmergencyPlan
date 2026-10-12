#pragma once

#include <QObject>

/// The wards this device asked for, from the request to the ward arriving or the
/// request being declined.
class PendingRequestsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aGrantedRequestArrivesOnceTheWardIsSealedToThisDevice();
    void aDeclinedRequestIsDroppedAndSaysSo();
    void aRequestOutlivesTheApp();
    void aRequestForNoSuchWardFails();
};

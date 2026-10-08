#pragma once

#include <QObject>

/// The server's wire format and the HTTP transport that carries it.
class HttpServerApiTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void requestsAreWrittenInTheWireFormat();
    void answersAreReadFromTheWireFormat();
    void illFormedAnswersAreRefused_data();
    void illFormedAnswersAreRefused();
    void errorsAreReadByNameThenStatus_data();
    void errorsAreReadByNameThenStatus();

    void aCallTravelsWithItsTokenSignatureAndBody();
    void everyCallTakesItsRoute_data();
    void everyCallTakesItsRoute();
    void aFailedAnswerIsItsNamedError();
    void anUnreadableOrFailingServerIsUnreachable();
    void noAddressIsUnreachableAfterTheCallReturns();
    void aRefusedConnectionIsUnreachable();
};

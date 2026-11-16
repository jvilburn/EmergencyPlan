#pragma once

#include <QObject>

/// Who else has the ward open, as the window names them.
class PresenceTest : public QObject
{
    Q_OBJECT

public:
    explicit PresenceTest(QObject* parent);

private slots:
    void othersNamesEachKindAndAUserOnce();
    void lineJoinsNames();
};

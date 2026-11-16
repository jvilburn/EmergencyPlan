#pragma once

#include <QObject>

/// The signed lines' known answers, which server/test/wire.test.ts pins for the same inputs.
class SignedLineTest : public QObject
{
    Q_OBJECT

public:
    explicit SignedLineTest(QObject* parent);

private slots:
    void createLine();
    void pushLine();
    void updateLine();
    void channelLine();
};

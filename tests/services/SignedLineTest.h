#pragma once

#include <QObject>

/// The signed lines' known answers. server/test/wire.test.ts pins the create, push and channel
/// lines for the same inputs; the update line is the app's alone.
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

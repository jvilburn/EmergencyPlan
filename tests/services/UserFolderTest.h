#pragma once

#include <QObject>

/// The folder of files handed to the user: an update file replaces the one of its name, and a
/// to-redo file never does.
class UserFolderTest : public QObject
{
    Q_OBJECT

public:
    explicit UserFolderTest(QObject* parent);

private slots:
    void writeMakesTheFolderAndReplacesAFileOfItsName();
    void writeNewNumbersEachFileWhoseNameIsTaken();
};

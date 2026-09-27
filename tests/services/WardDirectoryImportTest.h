#pragma once

#include <QObject>

/// Keeping existing ids when a ward directory is imported again.
class WardDirectoryImportTest : public QObject
{
    Q_OBJECT

private slots:
    void aMatchedMemberKeepsTheirId();
    void twoMembersSharingAFirstNameNeverShareAnId();
};

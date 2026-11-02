#pragma once

#include <QObject>

/// Imports work out every change at construction and replay like any command: they
/// conflict only with an edit to something they change, keep what the app holds that no
/// PDF prints, and an older ministering PDF adds no one it cannot match.
class ImportCommandsTest : public QObject
{
    Q_OBJECT

public:
    explicit ImportCommandsTest(QObject* parent);

private slots:
    void importReplayedAfterSomeoneAddedAFamilyKeepsThatFamily();
    void importReplayedAfterAnEditToAFamilyItChangesConflicts();
    void reimportingTheSameDirectoryKeepsNotesAndCoordinatesAndHoldsNoChange();
    void olderMinisteringPdfAddsNoUnknownFamilyOrMemberAndNoGroupNamesAMissingId();
    void attemptOnAFamilyLandsAfterAnImportRemovedIt();
    void importDescriptionIsOneLineNamingTheFile();
};

#include "AccessTest.h"
#include "AppDataTest.h"
#include "AppSettingsTest.h"
#include "AutomaticUpdatesTest.h"
#include "BirthdayTest.h"
#include "Crypto.h"
#include "DocumentMetadataTest.h"
#include "FamilyEditPanelTest.h"
#include "FamilyOrderTest.h"
#include "FieldChangeTest.h"
#include "FileSharingTest.h"
#include "FileWardStoreTest.h"
#include "FilterTest.h"
#include "IdentityTest.h"
#include "ImportCommandsTest.h"
#include "JsonReaderTest.h"
#include "CommandRoundTripTest.h"
#include "EmergencyAssetCommandsTest.h"
#include "EmergencyCommandsTest.h"
#include "MemberEditorTest.h"
#include "MinisteringPdfParserTest.h"
#include "PersonJsonTest.h"
#include "ReferenceCleanupTest.h"
#include "SettingsDialogTest.h"
#include "SpecReaderTest.h"
#include "StrictReadersTest.h"
#include "TeamCommandsTest.h"
#include "UpdateFamilyCommandTest.h"
#include "TileCacheTest.h"
#include "TreeViewStateTest.h"
#include "UpdateFileTest.h"
#include "UserFolderTest.h"
#include "WardFileTest.h"
#include "WardListTest.h"
#include "WardSessionSavingTest.h"
#include "WardSessionTest.h"
#include "WhoHasAccessDialogTest.h"

#include <QApplication>
#include <QTest>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    if (!Crypto::initialize())
    {
        return 1;
    }

    PersonJsonTest personJsonTest(nullptr);
    BirthdayTest birthdayTest(nullptr);
    FamilyOrderTest familyOrderTest(nullptr);
    FieldChangeTest changeTest(nullptr);
    UpdateFamilyCommandTest updateFamilyCommandTest(nullptr);
    CommandRoundTripTest commandRoundTripTest(nullptr);
    WardSessionTest wardSessionTest(nullptr);
    AutomaticUpdatesTest automaticUpdatesTest(nullptr);
    TeamCommandsTest teamCommandsTest(nullptr);
    EmergencyAssetCommandsTest emergencyAssetCommandsTest(nullptr);
    EmergencyCommandsTest emergencyCommandsTest(nullptr);
    ImportCommandsTest importCommandsTest(nullptr);
    ReferenceCleanupTest referenceCleanupTest(nullptr);
    FilterTest filterTest(nullptr);
    MemberEditorTest memberEditorTest(nullptr);
    FamilyEditPanelTest familyEditPanelTest(nullptr);
    JsonReaderTest jsonReaderTest(nullptr);
    IdentityTest identityTest(nullptr);
    WardFileTest wardFileTest(nullptr);
    FileWardStoreTest fileWardStoreTest(nullptr);
    WardSessionSavingTest wardSessionSavingTest(nullptr);
    StrictReadersTest strictReadersTest(nullptr);
    AppDataTest appDataTest(nullptr);
    AppSettingsTest appSettingsTest(nullptr);
    SettingsDialogTest settingsDialogTest(nullptr);
    WhoHasAccessDialogTest whoHasAccessDialogTest(nullptr);
    WardListTest wardListTest(nullptr);
    DocumentMetadataTest documentMetadataTest(nullptr);
    SpecReaderTest specReaderTest(nullptr);
    UpdateFileTest updateFileTest(nullptr);
    AccessTest accessTest(nullptr);
    FileSharingTest fileSharingTest(nullptr);
    UserFolderTest userFolderTest(nullptr);
    TreeViewStateTest treeViewStateTest(nullptr);
    MinisteringPdfParserTest ministeringPdfParserTest(nullptr);
    TileCacheTest tileCacheTest(nullptr);

    int failures = 0;
    failures += QTest::qExec(&personJsonTest, argc, argv);
    failures += QTest::qExec(&birthdayTest, argc, argv);
    failures += QTest::qExec(&familyOrderTest, argc, argv);
    failures += QTest::qExec(&changeTest, argc, argv);
    failures += QTest::qExec(&updateFamilyCommandTest, argc, argv);
    failures += QTest::qExec(&commandRoundTripTest, argc, argv);
    failures += QTest::qExec(&wardSessionTest, argc, argv);
    failures += QTest::qExec(&automaticUpdatesTest, argc, argv);
    failures += QTest::qExec(&teamCommandsTest, argc, argv);
    failures += QTest::qExec(&emergencyAssetCommandsTest, argc, argv);
    failures += QTest::qExec(&emergencyCommandsTest, argc, argv);
    failures += QTest::qExec(&importCommandsTest, argc, argv);
    failures += QTest::qExec(&referenceCleanupTest, argc, argv);
    failures += QTest::qExec(&filterTest, argc, argv);
    failures += QTest::qExec(&memberEditorTest, argc, argv);
    failures += QTest::qExec(&familyEditPanelTest, argc, argv);
    failures += QTest::qExec(&jsonReaderTest, argc, argv);
    failures += QTest::qExec(&identityTest, argc, argv);
    failures += QTest::qExec(&wardFileTest, argc, argv);
    failures += QTest::qExec(&fileWardStoreTest, argc, argv);
    failures += QTest::qExec(&wardSessionSavingTest, argc, argv);
    failures += QTest::qExec(&strictReadersTest, argc, argv);
    failures += QTest::qExec(&appDataTest, argc, argv);
    failures += QTest::qExec(&appSettingsTest, argc, argv);
    failures += QTest::qExec(&settingsDialogTest, argc, argv);
    failures += QTest::qExec(&whoHasAccessDialogTest, argc, argv);
    failures += QTest::qExec(&wardListTest, argc, argv);
    failures += QTest::qExec(&documentMetadataTest, argc, argv);
    failures += QTest::qExec(&specReaderTest, argc, argv);
    failures += QTest::qExec(&updateFileTest, argc, argv);
    failures += QTest::qExec(&accessTest, argc, argv);
    failures += QTest::qExec(&fileSharingTest, argc, argv);
    failures += QTest::qExec(&userFolderTest, argc, argv);
    failures += QTest::qExec(&treeViewStateTest, argc, argv);
    failures += QTest::qExec(&ministeringPdfParserTest, argc, argv);
    failures += QTest::qExec(&tileCacheTest, argc, argv);
    return failures;
}

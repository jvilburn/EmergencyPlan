#pragma once

#include "Identity.h"
#include "WardPayload.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <optional>

/// The reader in tests/spec, run as its own process against files the app writes.
class SpecReaderTest : public QObject
{
    Q_OBJECT

public:
    explicit SpecReaderTest(QObject* parent);

private slots:
    void initTestCase();
    void init();

    void specReaderRecoversEveryFieldTheAppWrites();
    void specReaderRefusesAFieldTheSpecLacksInEveryTable();
    void specReaderRefusesAHeaderThatDiffersFromTheAccessList();
    void specReaderRefusesAFileNamedForAnotherWard();
    void specReaderReportsANewerFormat();
    void specReaderRefusesAHeaderWithoutAVersion();
    void specReaderReadsAnUpdateFileTheAppWrites();
    void specReaderRefusesAnUpdateWhoseSignatureOrWriterFails();
    void specReaderReadsAnInvitationTheAppWrites();
    void specReaderRefusesAnInvitationWhoseSeedOpensNothing();

private:
    /// What one run of the reader gave.
    struct Run
    {
        int exitCode;
        QByteArray output;
        QString problem;  ///< its standard error
    };

    /// Puts file in the app data folder as the ward file of document.
    bool putWard(const DocumentId& document, const QByteArray& file) const;

    /// Runs the reader on the app data folder's ward of document.
    Run read(const DocumentId& document) const;

    /// Runs the reader on the update file holding update, against the app data folder's
    /// ward of document.
    Run readUpdate(const DocumentId& document, const QByteArray& update) const;

    /// Runs the reader on the invitation file holding invitation.
    Run readInvitation(const QByteArray& invitation) const;

    /// The every-field payload without its device part, as version 5 written by the phone.
    QByteArray phoneVersion() const;

    /// Runs the reader with arguments.
    static Run runReader(const QStringList& arguments);

    /// The file of the every-field payload sealed to recipients, built as the app builds one.
    QByteArray sealedTo(const QList<RecipientKeys>& recipients, const QJsonObject& payload) const;

    QTemporaryDir m_appData;
    std::optional<Identity> m_laptop;  ///< the device whose key file the app made in m_appData
    Identity m_phone;
    DocumentId m_document;
    QByteArray m_dataKey;
    WardPayload m_payload;
};

#pragma once

#include "Identity.h"
#include "WardPayload.h"

#include <QByteArray>
#include <QObject>
#include <QString>
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
    void specReaderRefusesAFieldTheSpecLacks();
    void specReaderRefusesAHeaderThatDiffersFromTheAccessList();
    void specReaderRefusesAFileNamedForAnotherWard();
    void specReaderReportsANewerFormat();

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

    /// The file of the every-field payload sealed to recipients, built as the app builds one.
    QByteArray sealedTo(const QList<RecipientKeys>& recipients, const QJsonObject& payload) const;

    QTemporaryDir m_appData;
    std::optional<Identity> m_laptop;  ///< the device whose key file the app made in m_appData
    Identity m_phone;
    DocumentId m_document;
    QByteArray m_dataKey;
    WardPayload m_payload;
};

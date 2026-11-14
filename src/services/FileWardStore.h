#pragma once

#include "WardStore.h"

#include <QCoreApplication>
#include <QString>

/// The wards folder in app data: each ward is the file named by its document id with the
/// .wardplanning extension, written through QSaveFile.
class FileWardStore : public WardStore
{
    Q_DECLARE_TR_FUNCTIONS(FileWardStore)

public:
    explicit FileWardStore(const QString& directory);

    bool write(const DocumentId& document, const QByteArray& bytes, QString* problem) override;
    StoreRead read(const DocumentId& document) const override;
    bool remove(const DocumentId& document, QString* problem) override;

    /// Each file in the folder with the .wardplanning extension whose base name is a document
    /// id.
    QList<DocumentId> documents() const override;

    QString pathOf(const DocumentId& document) const;

    /// The wards folder.
    const QString& directory() const { return m_directory; }

private:
    QString m_directory;
};

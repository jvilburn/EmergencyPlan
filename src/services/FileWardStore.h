#pragma once

#include "WardStore.h"

#include <QString>

/// The wards folder in app data: each ward is the file named by its document id with the
/// .wardplanning extension, written through QSaveFile.
class FileWardStore : public WardStore
{
public:
    explicit FileWardStore(const QString& directory);

    bool write(const DocumentId& document, const QByteArray& bytes, QString* problem) override;
    StoreRead read(const DocumentId& document) const override;

    QString pathOf(const DocumentId& document) const;

private:
    QString m_directory;
};

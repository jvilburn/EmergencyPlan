#pragma once

#include "AppSettings.h"
#include "DeviceIdentity.h"
#include "Document.h"
#include "DocumentRecords.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

/// The wards on this device. The app owns their files, in local app data, named by
/// document id; the person never chooses a path. "Open" is this list, not a file
/// dialog.
///
/// Each ward's entry - its name for the list, when it was last opened, and the
/// copies written from it - is kept in AppSettings beside the ward's record.
class DocumentLibrary
{
public:
    struct Entry
    {
        QString documentId;
        QString wardName;
        std::optional<QDateTime> lastOpened;
        bool shared = false;
    };

    /// An exported copy, opened read-only: never the live document, never in the
    /// library.
    struct Copy
    {
        Document document;
        QString originDocumentId;   ///< the ward it was written from; empty for a ward's own file
    };

    DocumentLibrary(AppSettings& settings, const QString& directory, const DeviceIdentity& identity);

    /// Sorted by ward name, then id, so two wards sharing a name keep their order.
    QList<Entry> entries() const;

    /// Where the wards' files are, for Show in Folder.
    const QString& directory() const { return m_directory; }

    /// Empty for a ward not in the library, rather than a guess.
    DocumentPaths pathsFor(const QString& documentId) const;

    /// 128 random bits, as hex.
    static QString newDocumentId();

    /// Adds a ward under the name the list shows until the document names itself.
    void add(const QString& documentId);

    /// Names the ward from the document once it can - its suggested file name, the
    /// ward it holds - so the list never has to decrypt a document to show it.
    void noteDocument(const QString& documentId, const Document& document);

    /// Marks the ward opened now, and as the one to reopen at startup.
    void markOpened(const QString& documentId);

    /// The ward to reopen at startup; empty for none.
    QString lastOpenedId() const { return m_settings.lastWardId(); }

    /// An unshared ward the person deletes: its files, record, and entry. Copies
    /// written from it are theirs to keep.
    void forget(const QString& documentId);

    /// After this device was removed from a ward, or left it: every recorded copy
    /// still at its path goes too, then the files, record, and entry. A file now at a
    /// copy's path that is not that copy is left alone.
    void forgetRemoved(const QString& documentId);

    /// Writes a copy to targetPath under a fresh document id and a fresh data key,
    /// with documentId as its origin, sealed to the same roster - a fresh key, so no
    /// one who once held the ward's can read it, and a fresh id, so it can never
    /// publish itself into the ward. Its path is recorded under the ward.
    bool exportCopy(const QString& documentId,
                    const Document& document,
                    const QString& targetPath,
                    QString* errorMessage);

    /// Opens a copy read-only; nullopt when this device cannot open it.
    std::optional<Copy> openCopy(const QString& sourcePath, QString* errorMessage) const;

private:
    AppSettings::WardEntry entryOf(const QString& documentId) const;

    AppSettings& m_settings;
    QString m_directory;
    DeviceIdentity m_identity;
};

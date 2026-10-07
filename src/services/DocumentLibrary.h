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
        QString wardName;           ///< Untitled until the document names its ward
        std::optional<QDateTime> lastOpened;
        bool shared = false;
    };

    /// An exported copy, opened read-only: never the live document, never in the
    /// library.
    struct Copy
    {
        Document document;
        QString originDocumentId;   ///< the ward it was written from
    };

    DocumentLibrary(AppSettings& settings, const QString& directory, const DeviceIdentity& identity);

    /// Sorted by ward name, then id, so two wards sharing a name keep their order.
    QList<Entry> entries() const;

    bool contains(const QString& documentId) const { return m_settings.ward(documentId).has_value(); }

    /// Where the wards' files are, for Show in Folder.
    const QString& directory() const { return m_directory; }

    /// Empty for a ward not in the library, rather than a guess.
    DocumentPaths pathsFor(const QString& documentId) const;

    /// 128 random bits, as hex.
    static QString newDocumentId();

    /// Adds a ward, listed as Untitled until the document names itself. A ward
    /// already in the library keeps its entry.
    void add(const QString& documentId);

    /// Adds a ward from a file written for this device, found outside the library -
    /// its entry lost with the settings, say - by copying the file in as the ward's
    /// own. The source is left where it is.
    bool addFromFile(const QString& documentId, const QString& sourcePath, QString* errorMessage);

    /// Names the ward from the document once it can - its suggested file name, the
    /// ward it holds - so the list never has to decrypt a document to show it.
    void noteDocument(const QString& documentId, const Document& document);

    /// Marks the ward opened now, and as the one to reopen at startup.
    void markOpened(const QString& documentId);

    /// The ward to reopen at startup; empty for none, or when it has left the library.
    QString lastOpenedId() const;

    /// An unshared ward the person deletes: its files, record, and entry. Copies
    /// written from it are theirs to keep.
    void forget(const QString& documentId);

    /// After this device was removed from a ward, or left it: every recorded copy
    /// still at its path goes too, then the files, record, and entry. A file now at a
    /// copy's path that is not that copy, by its document id, is left alone.
    void forgetRemoved(const QString& documentId);

    /// Writes a copy to targetPath under a fresh document id and a fresh data key,
    /// with documentId as its origin, sealed to the same roster - a fresh key, so no
    /// one who once held the ward's can read it, and a fresh id, so it can never
    /// publish itself into the ward. Its path and id are recorded under the ward.
    /// Refused for a ward not in the library, and for a target in the library's own
    /// directory, where it could replace a ward's file.
    bool exportCopy(const QString& documentId,
                    const Document& document,
                    const QString& targetPath,
                    QString* errorMessage);

    /// Opens a copy read-only; nullopt, with the reason, when the file is not a copy
    /// this device can open. A ward's own file is not a copy.
    std::optional<Copy> openCopy(const QString& sourcePath, QString* errorMessage) const;

private:
    AppSettings& m_settings;
    QString m_directory;
    DeviceIdentity m_identity;
};

#pragma once

#include "DocumentRecords.h"

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

/// Everything this device keeps between runs, in one JSON file.
///
/// The file is loaded once into memory, and every change rewrites it through
/// QSaveFile, so a crash mid-write leaves the previous file whole - the
/// high-water mark in each document record is part of the rollback defence.
/// It lives in local app data, which the OS keeps and which does not roam, beside
/// the identity and documents it describes.
///
/// Document records are keyed by document id: documents move between machines
/// and are named by id in app data, and an id outlives any path.
class AppSettings : public DocumentRecords
{
public:
    /// The settings at path: empty when no file is there yet; nullptr with
    /// errorMessage set when the file is there and cannot be read or understood.
    static std::unique_ptr<AppSettings> open(const QString& path, QString* errorMessage);

    // --- DocumentRecords ---

    DocumentRecord record(const QString& documentId) const override;
    void setRecord(const QString& documentId, const DocumentRecord& record) override;
    void removeRecord(const QString& documentId) override;
    QStringList documentIds() const override;
    QList<DeviceKeys> linkedDevices() const override { return m_linkedDevices; }
    void setLinkedDevices(const QList<DeviceKeys>& devices) override;

    // --- The document library's wards ---

    /// A copy written from a ward: where it was written, and its own document id, so
    /// the file at that path can be told from anything else put there since.
    struct ExportedCopy
    {
        QString path;
        QString documentId;

        bool operator==(const ExportedCopy& other) const { return path == other.path && documentId == other.documentId; }
        bool operator!=(const ExportedCopy& other) const { return !(*this == other); }
    };

    /// One ward in this device's library: what the ward list shows of it, and the
    /// copies written from it. Kept beside the ward's record with a lifetime of its
    /// own, so a removal that deletes the record still leaves the copies for the
    /// library to delete.
    struct WardEntry
    {
        std::optional<QString> wardName;    ///< absent until the document names its ward
        std::optional<QDateTime> lastOpened;
        QList<ExportedCopy> copies;

        bool operator==(const WardEntry& other) const;
        bool operator!=(const WardEntry& other) const { return !(*this == other); }
    };

    QStringList wardIds() const { return m_wards.keys(); }
    std::optional<WardEntry> ward(const QString& documentId) const;
    void setWard(const QString& documentId, const WardEntry& entry);

    /// Also clears the last ward when it is this one.
    void removeWard(const QString& documentId);

    /// The ward opened last, which the app reopens at startup; empty for none.
    QString lastWardId() const { return m_lastWardId; }
    void setLastWardId(const QString& documentId);

    /// The wards this device asked for access to and has not yet received.
    QStringList requestedWards() const { return m_requestedWards; }
    void setRequestedWards(const QStringList& documentIds);

    // --- This device ---

    bool developerExport() const { return m_developerExport; }
    void setDeveloperExport(bool enabled);

    /// The last answer from the release check: when, and the newest version it named.
    struct UpdateCheck
    {
        QDateTime when;
        QString latestVersion;
    };

    std::optional<UpdateCheck> updateCheck() const { return m_updateCheck; }
    void setUpdateCheck(const UpdateCheck& check);

    /// The newer version the person chose not to hear about again; empty for none.
    QString dismissedUpdate() const { return m_dismissedUpdate; }
    void setDismissedUpdate(const QString& version);

    /// The override when one is set, the built-in address otherwise.
    QString serverAddress() const;
    void setServerAddressOverride(const QString& address);

    // --- The map ---

    struct Location
    {
        double latitude = 0.0;
        double longitude = 0.0;
    };

    std::optional<Location> defaultLocation() const { return m_defaultLocation; }
    void setDefaultLocation(const Location& location);

    /// True while the last change could not be written: the values in memory
    /// stand, and the file holds the ones before it until a later write succeeds.
    bool writeFailed() const { return m_writeFailed; }

private:
    explicit AppSettings(const QString& path);

    bool read(const QByteArray& bytes);
    QByteArray write() const;
    void save();

    QString m_path;
    QHash<QString, DocumentRecord> m_records;
    QList<DeviceKeys> m_linkedDevices;
    QHash<QString, WardEntry> m_wards;
    QString m_lastWardId;
    QStringList m_requestedWards;
    bool m_developerExport = false;
    std::optional<UpdateCheck> m_updateCheck;
    QString m_dismissedUpdate;
    QString m_serverAddressOverride;
    std::optional<Location> m_defaultLocation;
    bool m_writeFailed = false;
};

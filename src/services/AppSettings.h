#pragma once

#include "DocumentRecords.h"

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

    // --- This device ---

    QString displayName() const { return m_displayName; }
    void setDisplayName(const QString& displayName);

    bool developerExport() const { return m_developerExport; }
    void setDeveloperExport(bool enabled);

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
    QString m_displayName;
    bool m_developerExport = false;
    QString m_serverAddressOverride;
    std::optional<Location> m_defaultLocation;
    bool m_writeFailed = false;
};

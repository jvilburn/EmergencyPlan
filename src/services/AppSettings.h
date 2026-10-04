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
/// It lives in AppDataLocation, which the OS keeps.
///
/// Per-document state is keyed by document id: documents move between machines
/// and are named by id in app data, and an id outlives any path.
class AppSettings : public DocumentRecords
{
public:
    /// The settings at path: empty when no file is there yet; nullptr with
    /// errorMessage set when the file is there and does not read.
    static std::unique_ptr<AppSettings> open(const QString& path, QString* errorMessage);

    // --- DocumentRecords ---

    DocumentRecord record(const QString& documentId) const override;
    void setRecord(const QString& documentId, const DocumentRecord& record) override;
    void removeRecord(const QString& documentId) override;
    QStringList documentIds() const override;

    // --- The document library's entries, beside each record ---

    QString wardName(const QString& documentId) const;
    void setWardName(const QString& documentId, const QString& wardName);
    std::optional<QDateTime> lastOpened(const QString& documentId) const;
    void setLastOpened(const QString& documentId, const QDateTime& lastOpened);

    /// Copies written from this document, so its removal can delete them.
    QStringList copyPaths(const QString& documentId) const;
    void setCopyPaths(const QString& documentId, const QStringList& paths);

    QString lastDocumentId() const { return m_lastDocumentId; }
    void setLastDocumentId(const QString& documentId);

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

private:
    /// One document's state: its record and its library entry.
    struct DocumentState
    {
        DocumentRecord record;
        QString wardName;
        std::optional<QDateTime> lastOpened;
        QStringList copyPaths;
    };

    explicit AppSettings(const QString& path);

    bool read(const QByteArray& bytes);
    QByteArray write() const;

    /// Rewrites the file. A failure is logged; the settings in memory stay as set.
    void save() const;

    QString m_path;
    QHash<QString, DocumentState> m_documents;
    QString m_lastDocumentId;
    QString m_displayName;
    bool m_developerExport = false;
    QString m_serverAddressOverride;
    std::optional<Location> m_defaultLocation;
};

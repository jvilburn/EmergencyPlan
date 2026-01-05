#pragma once

#include "Family.h"
#include "WardDirectoryPdfParser.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>

/// Result of ward directory import operation
struct WardDirectoryImportResult
{
    bool success = false;
    QHash<QString, Family> families;
    QStringList errors;
    QString wardName;
    QString wardUnitNumber;
};

/// Service for importing ward directory data from PDF files
class WardDirectoryImportService : public QObject
{
    Q_OBJECT

public:
    explicit WardDirectoryImportService(QObject* parent = nullptr);
    ~WardDirectoryImportService();

    /// Import families from a PDF file
    WardDirectoryImportResult importFromPdf(const QString& pdfPath);
};

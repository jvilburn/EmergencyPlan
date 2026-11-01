#pragma once

#include <QString>

class Document;
class EmergencyResponse;
class QDateTime;

/// Generates PDF summary reports for emergency responses.
/// Uses QPrinter/QPainter for PDF output.
class ReportGenerator
{
public:
    /// Writes the summary report of response, the record of the emergency: its statistics,
    /// counting every family in document without a record as Not Contacted, and each
    /// family record's attempts and tasks, oldest first. People and teams are named as
    /// document holds them. Returns false when the file cannot be written.
    /// @param writtenAt When the report is written; the header's date range ends here
    static bool generateReport(const EmergencyResponse& response,
                               const Document& document,
                               const QDateTime& writtenAt,
                               const QString& filePath);
};

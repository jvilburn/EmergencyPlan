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
    /// Writes the summary report of response, which is what remains of the emergency once it
    /// ends: its statistics, counting every directory family, and each family's attempts
    /// and tasks, oldest first. Families, people and teams are named as document holds them,
    /// and as "(removed)" when document lacks them. Returns false when the file cannot be
    /// written.
    /// @param writtenAt When the report is written; the header's date range ends here
    static bool generateReport(const EmergencyResponse& response,
                               const Document& document,
                               const QDateTime& writtenAt,
                               const QString& filePath);
};

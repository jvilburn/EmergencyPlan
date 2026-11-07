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
    /// and tasks, oldest first. Families, people and teams are named as the attempts, tasks
    /// and assignments recorded them, a family as its newest attempt or task did; a family
    /// with none is named as document holds it. Returns false when the file cannot be
    /// written.
    /// @param writtenAt When the report is written; the header's date range ends here
    static bool generateReport(const EmergencyResponse& response,
                               const Document& document,
                               const QDateTime& writtenAt,
                               const QString& filePath);
};

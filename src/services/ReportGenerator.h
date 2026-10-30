#pragma once

#include <QString>

class EmergencyResponse;
class QDateTime;

/// Generates PDF summary reports for emergency responses.
/// Uses QPrinter/QPainter for PDF output.
class ReportGenerator
{
public:
    /// Generate a PDF report for the given emergency response.
    /// Returns true on success, false on failure.
    /// @param response The emergency response data
    /// @param wardName Ward name to display in the report header
    /// @param writtenAt When the report is written; the header's date range ends here
    /// @param filePath Output PDF file path
    static bool generateReport(const EmergencyResponse& response,
                               const QString& wardName,
                               const QDateTime& writtenAt,
                               const QString& filePath);
};

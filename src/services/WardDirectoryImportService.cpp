#include "WardDirectoryImportService.h"

// ============================================================================
// Constructor / Destructor
// ============================================================================

WardDirectoryImportService::WardDirectoryImportService(QObject* parent)
    : QObject(parent)
{
}

WardDirectoryImportService::~WardDirectoryImportService()
{
}

// ============================================================================
// Public API
// ============================================================================

WardDirectoryImportResult WardDirectoryImportService::importFromPdf(const QString& pdfPath)
{
    WardDirectoryImportResult result;

    WardDirectoryPdfParser::ParseResult parseResult = WardDirectoryPdfParser::parse(pdfPath);

    result.success = parseResult.success;
    result.errors = parseResult.errors;
    result.wardName = parseResult.wardName;
    result.wardUnitNumber = parseResult.wardUnitNumber;
    result.families = parseResult.families;

    return result;
}

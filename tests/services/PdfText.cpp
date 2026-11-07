#include "PdfText.h"
#include "Document.h"
#include "PdfExtractor.h"
#include "ReportGenerator.h"

#include <QDateTime>
#include <QStringList>
#include <QTemporaryDir>

QString pdfText(const QString& path)
{
    PdfExtractor extractor;
    if (!extractor.open(path))
    {
        return QString();
    }
    QStringList lines;
    for (int page = 0; page < extractor.pageCount(); ++page)
    {
        for (const PdfTextField& field : extractor.textFields(page))
        {
            lines.append(field.text());
        }
    }
    return lines.join("\n");
}

QString reportText(const Document& document)
{
    QTemporaryDir dir;
    QString path = dir.filePath("report.pdf");
    if (!dir.isValid()
        || !ReportGenerator::generateReport(document.emergencyResponse().value(), document,
                                            QDateTime::currentDateTimeUtc(), path))
    {
        return QString();
    }
    return pdfText(path);
}

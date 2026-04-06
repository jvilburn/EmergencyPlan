// Standalone tool to dump PDF text extraction data for analysis
// Build: cl /EHsc pdf_dump.cpp /I<qt_include> /I<mupdf_include> <libs>
// Or add to CMakeLists.txt as a separate executable

#include "PdfExtractor.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <memory>

void usage(const char* progName)
{
    QTextStream err(stderr);
    err << "Usage: " << progName << " [options] <pdf_file>\n"
        << "\n"
        << "Dumps PDF text extraction data for analysis.\n"
        << "Fields are sorted by position (top-to-bottom, left-to-right).\n"
        << "\n"
        << "Options:\n"
        << "  -o <file>    Write output to file (default: stdout)\n"
        << "  -h, --help   Show this help\n"
        << "\n"
        << "Output includes:\n"
        << "  - Font sizes found on each page\n"
        << "  - Text fields with position (X, Y), font size, bold flag\n"
        << "\n"
        << "Examples:\n"
        << "  pdf_dump directory.pdf\n"
        << "  pdf_dump directory.pdf > analysis.txt\n"
        << "  pdf_dump -o analysis.txt directory.pdf\n";
}

void dumpFields(const QString& pdfPath, QTextStream& out)
{
    PdfExtractor extractor;
    if (!extractor.open(pdfPath))
    {
        qWarning() << "ERROR: Failed to open PDF:" << pdfPath;
        return;
    }

    QTextStream err(stderr);
    err << "Processing " << extractor.pageCount() << " pages...\n";
    err.flush();

    int pages = extractor.pageCount();
    out << "PDF: " << pdfPath << "\n";
    out << "Pages: " << pages << "\n";
    out << "========================================\n\n";

    for (int page = 0; page < pages; ++page)
    {
        out << "=== PAGE " << (page + 1) << " ===\n\n";

        // Get text fields (sorted by Y then X)
        QList<PdfTextField> fields = extractor.textFields(page);

        // Collect unique font sizes for analysis
        QSet<float> fontSizes;
        for (const auto& field : fields)
        {
            fontSizes.insert(field.fontSize());
        }

        QList<float> sortedSizes = fontSizes.values();
        std::sort(sortedSizes.begin(), sortedSizes.end(), std::greater<float>());

        out << "Font sizes found (largest to smallest):\n";
        for (float size : sortedSizes)
        {
            out << "  " << size << "\n";
        }
        out << "\n";

        out << "Text fields:\n";
        out << QString("%1 | %2 | %3 | %4 | %5 | %6\n")
                   .arg("Y", 6)
                   .arg("Left", 6)
                   .arg("Right", 6)
                   .arg("Font", 5)
                   .arg("Bold", 4)
                   .arg("Text");
        out << QString("-").repeated(90) << "\n";

        for (const auto& field : fields)
        {
            out << QString("%1 | %2 | %3 | %4 | %5 | %6\n")
                       .arg(field.y(), 6, 'f', 1)
                       .arg(field.left(), 6, 'f', 1)
                       .arg(field.right(), 6, 'f', 1)
                       .arg(field.fontSize(), 5, 'f', 1)
                       .arg(field.bold() ? "Y" : "N", 4)
                       .arg(field.text().left(60));
        }

        out << "\n";
    }

    out.flush();
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QString pdfPath;
    QString outputPath;

    // Parse arguments
    for (int i = 1; i < argc; ++i)
    {
        QString arg = QString::fromLocal8Bit(argv[i]);

        if (arg == "-h" || arg == "--help")
        {
            usage(argv[0]);
            return 0;
        }
        else if (arg == "-o")
        {
            if (i + 1 >= argc)
            {
                qWarning() << "ERROR: -o requires a filename";
                return 1;
            }
            outputPath = QString::fromLocal8Bit(argv[++i]);
        }
        else if (arg.startsWith("-"))
        {
            qWarning() << "ERROR: Unknown option:" << arg;
            usage(argv[0]);
            return 1;
        }
        else
        {
            pdfPath = arg;
        }
    }

    if (pdfPath.isEmpty())
    {
        usage(argv[0]);
        return 1;
    }

    // Set up output stream
    QFile outFile;
    std::unique_ptr<QTextStream> out;

    if (outputPath.isEmpty())
    {
        out = std::make_unique<QTextStream>(stdout);
    }
    else
    {
        outFile.setFileName(outputPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qWarning() << "ERROR: Failed to open output file:" << outputPath;
            return 1;
        }
        out = std::make_unique<QTextStream>(&outFile);
    }
    out->setEncoding(QStringConverter::Utf8);

    dumpFields(pdfPath, *out);

    return 0;
}

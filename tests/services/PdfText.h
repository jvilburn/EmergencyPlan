#pragma once

#include <QString>

class Document;

/// The text of every page of the PDF at path, a line per text field, in reading order; empty
/// when it cannot be opened.
QString pdfText(const QString& path);

/// The text of the summary report of document's emergency in progress, written now, as
/// pdfText reads it; empty when the report cannot be written.
QString reportText(const Document& document);

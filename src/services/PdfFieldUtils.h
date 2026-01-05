#pragma once

#include "PdfExtractor.h"

#include <QList>
#include <functional>
#include <utility>

/// Utility functions for processing PDF text fields

/// Group fields by Y position within a tolerance.
/// Fields within yTolerance of each other are considered on the same line.
/// Returns list of rows, each row containing fields at similar Y positions.
QList<QList<PdfTextField>> groupFieldsByY(
    const QList<PdfTextField>& fields,
    double yTolerance);

/// Find content boundaries in a page's fields using custom header/footer detection.
/// @param fields The text fields from a PDF page
/// @param isHeaderMarker Predicate for header marker (null = no header, start at 0)
/// @param isFooterMarker Predicate for footer marker (null = no footer, end at size)
/// @param headerFieldsAfterMarker Header fields after the marker (content starts after these)
/// @param footerFieldsBeforeMarker Footer fields before the marker (content ends before these)
/// @return (contentStart, contentEnd) indices for use with half-open range [start, end)
std::pair<int, int> findContentBounds(
    const QList<PdfTextField>& fields,
    const std::function<bool(const QString&)>& isHeaderMarker,
    const std::function<bool(const QString&)>& isFooterMarker,
    int headerFieldsAfterMarker = 0,
    int footerFieldsBeforeMarker = 0);

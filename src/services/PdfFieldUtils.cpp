#include "PdfFieldUtils.h"

#include <cmath>

QList<QList<PdfTextField>> groupFieldsByY(
    const QList<PdfTextField>& fields,
    double yTolerance)
{
    if (fields.isEmpty())
    {
        return {};
    }

    // Fields are already sorted by Y from PdfExtractor
    QList<QList<PdfTextField>> groups;
    QList<PdfTextField> currentGroup;
    currentGroup.append(fields[0]);

    for (int i = 1; i < fields.size(); ++i)
    {
        if (std::abs(fields[i].y() - currentGroup[0].y()) <= yTolerance)
        {
            currentGroup.append(fields[i]);
        }
        else
        {
            groups.append(currentGroup);
            currentGroup.clear();
            currentGroup.append(fields[i]);
        }
    }

    if (!currentGroup.isEmpty())
    {
        groups.append(currentGroup);
    }

    return groups;
}

std::pair<int, int> findContentBounds(
    const QList<PdfTextField>& fields,
    const std::function<bool(const QString&)>& isHeaderMarker,
    const std::function<bool(const QString&)>& isFooterMarker,
    int headerFieldsAfterMarker,
    int footerFieldsBeforeMarker)
{
    int contentStart = 0;
    int contentEnd = fields.size();
    bool foundHeader = false;

    for (int i = 0; i < fields.size(); ++i)
    {
        QString text = fields[i].text().trimmed();

        if (!foundHeader && isHeaderMarker && isHeaderMarker(text))
        {
            foundHeader = true;
            contentStart = i + 1 + headerFieldsAfterMarker;
        }

        if (isFooterMarker && isFooterMarker(text))
        {
            contentEnd = i - footerFieldsBeforeMarker;
        }
    }

    return {contentStart, contentEnd};
}

#include "ReportGenerator.h"
#include "EmergencyResponse.h"

#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QPainter>
#include <QPrinter>
#include <QDateTime>
#include <QHash>

#include <algorithm>

namespace
{

const int kPageMargin = 50;
const int kSectionSpacing = 12;
// Font sizes in the report's 96 DPI layout units
const int kHeaderFontPixels = 21;
const int kSubheaderFontPixels = 16;
const int kBodyFontPixels = 13;

struct ReportContext
{
    QPainter* painter;
    int y;
    int pageWidth;
    int pageHeight;
    QPrinter* printer;
    QString fontFamily;
};

QString selectFontFamily()
{
    QStringList preferred = {"Segoe UI", "Helvetica Neue", "Helvetica", "Arial"};
    QStringList available = QFontDatabase::families();
    for (const QString& name : preferred)
    {
        if (available.contains(name))
        {
            return name;
        }
    }
    return QFont().family();
}

void ensureSpace(ReportContext& ctx, int needed)
{
    if (ctx.y + needed > ctx.pageHeight - kPageMargin)
    {
        ctx.printer->newPage();
        ctx.y = kPageMargin;
    }
}

/// Sets the painter's font and returns its metrics, both in the report's 96 DPI
/// layout units. A pixel size keeps the glyphs in the same units as the layout;
/// a point size would resolve against the printer's own DPI and then be scaled again.
QFontMetrics setReportFont(ReportContext& ctx, int pixelSize, bool bold)
{
    QFont font(ctx.fontFamily);
    font.setPixelSize(pixelSize);
    font.setBold(bold);
    ctx.painter->setFont(font);
    return ctx.painter->fontMetrics();
}

void drawText(ReportContext& ctx, const QString& text, int pixelSize, bool bold)
{
    QFontMetrics fm = setReportFont(ctx, pixelSize, bold);
    int textHeight = fm.height();
    ensureSpace(ctx, textHeight);

    ctx.painter->drawText(kPageMargin, ctx.y + fm.ascent(), text);
    ctx.y += textHeight + 2;
}

void drawLine(ReportContext& ctx)
{
    ensureSpace(ctx, 4);
    ctx.painter->drawLine(kPageMargin, ctx.y, ctx.pageWidth - kPageMargin, ctx.y);
    ctx.y += 6;
}

void drawSpacer(ReportContext& ctx, int height)
{
    ctx.y += height;
}

void drawKeyValue(ReportContext& ctx, const QString& key, const QString& value)
{
    QFontMetrics fm = setReportFont(ctx, kBodyFontPixels, false);
    int textHeight = fm.height();
    ensureSpace(ctx, textHeight);

    ctx.painter->drawText(kPageMargin + 20, ctx.y + fm.ascent(),
                          key + ": " + value);
    ctx.y += textHeight + 2;
}

void drawBulletItem(ReportContext& ctx, const QString& text)
{
    QFontMetrics fm = setReportFont(ctx, kBodyFontPixels, false);
    int textHeight = fm.height();
    ensureSpace(ctx, textHeight);

    ctx.painter->drawText(kPageMargin + 20, ctx.y + fm.ascent(),
                          "\u2022 " + text);
    ctx.y += textHeight + 2;
}

QString percentString(int count, int total)
{
    if (total == 0)
    {
        return "0%";
    }
    int pct = qRound(100.0 * count / total);
    return QString::number(pct) + "%";
}

QString contactMethodName(ContactMethod method)
{
    switch (method)
    {
    case ContactMethod::Phone:
        return QObject::tr("Phone");
    case ContactMethod::Text:
        return QObject::tr("Text");
    case ContactMethod::Email:
        return QObject::tr("Email");
    case ContactMethod::Visit:
        return QObject::tr("Visit");
    case ContactMethod::Other:
        return QObject::tr("Other");
    }
    return QString();
}

// Fixed order for consistent reports
const QList<ContactMethod> kMethodOrder =
{
    ContactMethod::Phone,
    ContactMethod::Text,
    ContactMethod::Email,
    ContactMethod::Visit,
    ContactMethod::Other
};

}  // namespace

bool ReportGenerator::generateReport(const EmergencyResponse& response,
                                     const std::optional<QDateTime>& endedAt,
                                     const QString& wardName,
                                     const QString& filePath)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::Letter));

    QPainter painter;
    if (!painter.begin(&printer))
    {
        return false;
    }

    // Scale to logical coordinates (printer DPI is high, we want ~600px width feel)
    int dpi = printer.resolution();
    double scale = dpi / 96.0;
    painter.scale(scale, scale);

    int pageWidth = static_cast<int>(printer.width() / scale);
    int pageHeight = static_cast<int>(printer.height() / scale);

    ReportContext ctx;
    ctx.painter = &painter;
    ctx.y = kPageMargin;
    ctx.pageWidth = pageWidth;
    ctx.pageHeight = pageHeight;
    ctx.printer = &printer;
    ctx.fontFamily = selectFontFamily();

    // === 1. Header ===
    drawText(ctx, QObject::tr("Emergency Response Report"), kHeaderFontPixels, true);
    drawSpacer(ctx, 4);
    drawText(ctx, response.name(), kSubheaderFontPixels, false);

    if (!wardName.isEmpty())
    {
        drawText(ctx, wardName, kBodyFontPixels, false);
    }

    QString dateRange = response.startedAt().toLocalTime().toString("MMM d, yyyy h:mm AP");
    if (endedAt.has_value())
    {
        dateRange += " - " + endedAt->toLocalTime().toString("MMM d, yyyy h:mm AP");
    }
    else
    {
        dateRange += QObject::tr(" - Ongoing");
    }
    drawText(ctx, dateRange, kBodyFontPixels, false);
    drawLine(ctx);

    // === 2. Overview Statistics ===
    int total = response.totalFamilies();
    int okCount = response.countByStatus(EffectiveContactStatus::OK);
    int needsHelpCount = response.countByStatus(EffectiveContactStatus::NeedsHelp);
    int unableCount = response.countByStatus(EffectiveContactStatus::UnableToReach);
    int notContactedCount = response.countByStatus(EffectiveContactStatus::NotContacted);
    int contactedCount = total - notContactedCount;

    drawText(ctx, QObject::tr("Overview"), kSubheaderFontPixels, true);
    drawKeyValue(ctx, QObject::tr("Total families"), QString::number(total));
    drawKeyValue(ctx, QObject::tr("Contacted"),
                 QString::number(contactedCount) + " (" + percentString(contactedCount, total) + ")");
    drawSpacer(ctx, kSectionSpacing);

    // === 3. Status Breakdown ===
    drawText(ctx, QObject::tr("Status Breakdown"), kSubheaderFontPixels, true);
    drawKeyValue(ctx, QObject::tr("OK"),
                 QString::number(okCount) + " (" + percentString(okCount, total) + ")");
    drawKeyValue(ctx, QObject::tr("Needs Help"),
                 QString::number(needsHelpCount) + " (" + percentString(needsHelpCount, total) + ")");
    drawKeyValue(ctx, QObject::tr("Unable to Reach"),
                 QString::number(unableCount) + " (" + percentString(unableCount, total) + ")");
    drawKeyValue(ctx, QObject::tr("Not Contacted"),
                 QString::number(notContactedCount) + " (" + percentString(notContactedCount, total) + ")");
    drawSpacer(ctx, kSectionSpacing);

    // === 4. Task Summary ===
    const QHash<FamilyId, FamilyResponseRecord>& records = response.familyRecords();

    int totalTasks = 0;
    int resolvedTasks = 0;
    int assignedTasks = 0;
    int totalAttempts = 0;
    QHash<QString, int> tasksByCategory;
    QHash<ContactMethod, int> attemptsByMethod;

    for (auto it = records.constBegin(); it != records.constEnd(); ++it)
    {
        const FamilyResponseRecord& record = it.value();

        for (const ResponseTask& task : record.tasks())
        {
            totalTasks++;
            if (task.isResolved())
            {
                resolvedTasks++;
            }
            if (task.isAssigned())
            {
                assignedTasks++;
            }
            tasksByCategory[task.category()]++;
        }

        for (const ContactAttempt& attempt : record.contactAttempts())
        {
            totalAttempts++;
            attemptsByMethod[attempt.method()]++;
        }
    }

    drawText(ctx, QObject::tr("Task Summary"), kSubheaderFontPixels, true);
    drawKeyValue(ctx, QObject::tr("Total tasks"), QString::number(totalTasks));
    drawKeyValue(ctx, QObject::tr("Resolved"),
                 QString::number(resolvedTasks) + " (" + percentString(resolvedTasks, totalTasks) + ")");
    drawKeyValue(ctx, QObject::tr("Unresolved"), QString::number(totalTasks - resolvedTasks));
    drawKeyValue(ctx, QObject::tr("Assigned"), QString::number(assignedTasks));
    drawKeyValue(ctx, QObject::tr("Unassigned"), QString::number(totalTasks - assignedTasks));
    drawSpacer(ctx, 4);

    // Tasks by category
    if (!tasksByCategory.isEmpty())
    {
        drawText(ctx, QObject::tr("Tasks by Category"), kBodyFontPixels, true);
        QList<QString> categories = tasksByCategory.keys();
        std::sort(categories.begin(), categories.end());
        for (const QString& cat : categories)
        {
            drawKeyValue(ctx, cat, QString::number(tasksByCategory[cat]));
        }
    }
    drawSpacer(ctx, kSectionSpacing);

    // === 5. Response Activity ===
    drawText(ctx, QObject::tr("Response Activity"), kSubheaderFontPixels, true);
    drawKeyValue(ctx, QObject::tr("Total contact attempts"), QString::number(totalAttempts));

    // List contact methods in fixed order for consistent reports
    for (ContactMethod method : kMethodOrder)
    {
        if (attemptsByMethod.contains(method))
        {
            drawKeyValue(ctx, contactMethodName(method),
                         QString::number(attemptsByMethod[method]));
        }
    }
    drawSpacer(ctx, kSectionSpacing);

    // === 6. Unresolved Items ===
    QList<QString> needsHelpFamilies;
    QList<QString> notContactedFamilies;

    for (auto it = records.constBegin(); it != records.constEnd(); ++it)
    {
        const FamilyResponseRecord& record = it.value();
        EffectiveContactStatus status = record.effectiveStatus();

        if (status == EffectiveContactStatus::NeedsHelp)
        {
            needsHelpFamilies.append(record.displayName());
        }
        else if (status == EffectiveContactStatus::NotContacted)
        {
            notContactedFamilies.append(record.displayName());
        }
    }

    std::sort(needsHelpFamilies.begin(), needsHelpFamilies.end());
    std::sort(notContactedFamilies.begin(), notContactedFamilies.end());

    if (!needsHelpFamilies.isEmpty() || !notContactedFamilies.isEmpty())
    {
        drawText(ctx, QObject::tr("Unresolved Items"), kSubheaderFontPixels, true);

        if (!needsHelpFamilies.isEmpty())
        {
            drawText(ctx, QObject::tr("Families Needing Help (%1)").arg(needsHelpFamilies.size()),
                     kBodyFontPixels, true);
            for (const QString& name : needsHelpFamilies)
            {
                drawBulletItem(ctx, name);
            }
            drawSpacer(ctx, 4);
        }

        if (!notContactedFamilies.isEmpty())
        {
            drawText(ctx, QObject::tr("Families Not Contacted (%1)").arg(notContactedFamilies.size()),
                     kBodyFontPixels, true);
            for (const QString& name : notContactedFamilies)
            {
                drawBulletItem(ctx, name);
            }
        }
    }

    painter.end();
    return true;
}

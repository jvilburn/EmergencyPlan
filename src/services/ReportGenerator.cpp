#include "ReportGenerator.h"
#include "Document.h"
#include "EmergencyResponse.h"

#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QPrinter>
#include <QDateTime>
#include <QHash>

#include <algorithm>

namespace
{

const int kPageMargin = 50;
const int kSectionSpacing = 12;
const int kHeaderFontSize = 16;
const int kSubheaderFontSize = 12;
const int kBodyFontSize = 10;

const QString kDateFormat = "MMM d, yyyy h:mm AP";

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

void drawText(ReportContext& ctx, const QString& text, int fontSize, bool bold)
{
    QFont font(ctx.fontFamily, fontSize);
    font.setBold(bold);
    ctx.painter->setFont(font);

    QFontMetrics fm(font);
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

/// Draws text in the body font, indent past the page margin.
void drawIndented(ReportContext& ctx, const QString& text, int indent)
{
    QFont font(ctx.fontFamily, kBodyFontSize);
    ctx.painter->setFont(font);

    QFontMetrics fm(font);
    int textHeight = fm.height();
    ensureSpace(ctx, textHeight);

    ctx.painter->drawText(kPageMargin + indent, ctx.y + fm.ascent(), text);
    ctx.y += textHeight + 2;
}

void drawKeyValue(ReportContext& ctx, const QString& key, const QString& value)
{
    drawIndented(ctx, key + ": " + value, 20);
}

void drawBulletItem(ReportContext& ctx, const QString& text)
{
    drawIndented(ctx, "\u2022 " + text, 20);
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

// Fixed order for consistent reports
const QList<ContactMethod> kMethodOrder =
{
    ContactMethod::Phone,
    ContactMethod::Text,
    ContactMethod::Email,
    ContactMethod::Visit,
    ContactMethod::Other
};

QString timeText(const QDateTime& time)
{
    return time.toLocalTime().toString(kDateFormat);
}

/// The parts that are not empty, joined by " - ".
QString joined(const QStringList& parts)
{
    QStringList present;
    for (const QString& part : parts)
    {
        if (!part.isEmpty())
        {
            present.append(part);
        }
    }
    return present.join(" - ");
}

/// The person's display name as document holds them, or empty when there is no id or
/// document lacks them.
QString personNameIn(const std::optional<PersonId>& id, const Document& document)
{
    if (!id.has_value())
    {
        return QString();
    }
    std::optional<Person> person = document.findPersonById(*id);
    return person.has_value() ? person->displayName() : QString();
}

/// How an attempt with no method, recorded by a button, reads: "Marked OK" or "Marked
/// Unable to Reach".
QString markedName(ContactOutcome outcome)
{
    switch (outcome)
    {
    case ContactOutcome::OK:
        return QObject::tr("Marked OK");
    case ContactOutcome::UnableToReach:
        return QObject::tr("Marked Unable to Reach");
    }
    return QString();
}

/// One attempt: its time, method, who, outcome and notes, whichever it has.
QString attemptLine(const ContactAttempt& attempt, const Document& document)
{
    QStringList parts = {timeText(attempt.timestamp())};
    std::optional<ContactMethod> method = attempt.method();
    std::optional<ContactOutcome> outcome = attempt.outcome();
    if (method.has_value())
    {
        parts.append(contactMethodName(*method));
    }
    parts.append(personNameIn(attempt.who(), document));
    if (outcome.has_value())
    {
        parts.append(method.has_value() ? contactOutcomeName(*outcome) : markedName(*outcome));
    }
    parts.append(attempt.notes());
    return joined(parts);
}

QString assigneeLine(const TaskAssignment& assignment, const Document& document)
{
    QString assignee;
    if (assignment.teamId().has_value())
    {
        std::optional<Team> team = document.findTeamById(*assignment.teamId());
        assignee = team.has_value() ? team->name() : QString();
    }
    else
    {
        assignee = personNameIn(assignment.personId(), document);
    }
    return joined({QObject::tr("Assigned to: %1").arg(assignee), assignment.notes()});
}

QString notificationLine(const TaskNotification& notification)
{
    return joined({QObject::tr("Notified: %1").arg(timeText(notification.timestamp())),
                   contactMethodName(notification.method()),
                   notification.notes()});
}

QString resolutionLine(const std::optional<TaskResolution>& resolution)
{
    if (!resolution.has_value())
    {
        return QObject::tr("Unresolved");
    }
    return joined({QObject::tr("Resolved: %1").arg(timeText(resolution->resolvedAt())), resolution->notes()});
}

/// The record's name and address, then its attempts and its tasks, each oldest first.
void drawRecord(ReportContext& ctx, const FamilyResponseRecord& record, const Document& document)
{
    drawText(ctx, record.displayName(), kBodyFontSize, true);
    drawIndented(ctx, record.address(), 20);

    const QList<ContactAttempt>& attempts = record.contactAttempts();
    if (!attempts.isEmpty())
    {
        drawIndented(ctx, QObject::tr("Contact attempts"), 20);
        for (auto it = attempts.crbegin(); it != attempts.crend(); ++it)
        {
            drawIndented(ctx, attemptLine(*it, document), 40);
        }
    }

    const QList<ResponseTask>& tasks = record.tasks();
    if (!tasks.isEmpty())
    {
        drawIndented(ctx, QObject::tr("Tasks"), 20);
        for (auto it = tasks.crbegin(); it != tasks.crend(); ++it)
        {
            drawIndented(ctx, joined({timeText(it->createdAt()), it->category(), it->description()}), 40);
            const std::optional<TaskAssignment>& assignment = it->assignment();
            if (assignment.has_value())
            {
                drawIndented(ctx, assigneeLine(*assignment, document), 60);
                if (assignment->notification().has_value())
                {
                    drawIndented(ctx, notificationLine(*assignment->notification()), 60);
                }
            }
            drawIndented(ctx, resolutionLine(it->resolution()), 60);
        }
    }
    drawSpacer(ctx, 6);
}

}  // namespace

bool ReportGenerator::generateReport(const EmergencyResponse& response,
                                     const Document& document,
                                     const QDateTime& writtenAt,
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

    const QHash<FamilyId, Family>& families = document.families();
    const QHash<FamilyId, FamilyResponseRecord>& records = response.familyRecords();

    // === 1. Header ===
    drawText(ctx, QObject::tr("Emergency Response Report"), kHeaderFontSize, true);
    drawSpacer(ctx, 4);
    drawText(ctx, response.name(), kSubheaderFontSize, false);
    drawText(ctx, document.wardName(), kBodyFontSize, false);

    QString dateRange = QObject::tr("%1 - %2").arg(timeText(response.startedAt()), timeText(writtenAt));
    drawText(ctx, dateRange, kBodyFontSize, false);
    drawLine(ctx);

    // === 2. Overview Statistics ===
    int total = response.totalFamilies(families);
    int okCount = response.countByStatus(EffectiveContactStatus::OK, families);
    int needsHelpCount = response.countByStatus(EffectiveContactStatus::NeedsHelp, families);
    int unableCount = response.countByStatus(EffectiveContactStatus::UnableToReach, families);
    int notContactedCount = response.countByStatus(EffectiveContactStatus::NotContacted, families);
    int contactedCount = total - notContactedCount;

    drawText(ctx, QObject::tr("Overview"), kSubheaderFontSize, true);
    drawKeyValue(ctx, QObject::tr("Total families"), QString::number(total));
    drawKeyValue(ctx, QObject::tr("Contacted"),
                 QString::number(contactedCount) + " (" + percentString(contactedCount, total) + ")");
    drawSpacer(ctx, kSectionSpacing);

    // === 3. Status Breakdown ===
    drawText(ctx, QObject::tr("Status Breakdown"), kSubheaderFontSize, true);
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
    int totalTasks = 0;
    int resolvedTasks = 0;
    int assignedTasks = 0;
    int totalAttempts = 0;
    QHash<QString, int> tasksByCategory;
    QHash<ContactMethod, int> attemptsByMethod;
    QHash<ContactOutcome, int> marksByOutcome;

    for (const FamilyResponseRecord& record : records)
    {
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
            if (attempt.method().has_value())
            {
                attemptsByMethod[*attempt.method()]++;
            }
            else if (attempt.outcome().has_value())
            {
                marksByOutcome[*attempt.outcome()]++;
            }
        }
    }

    drawText(ctx, QObject::tr("Task Summary"), kSubheaderFontSize, true);
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
        drawText(ctx, QObject::tr("Tasks by Category"), kBodyFontSize, true);
        QList<QString> categories = tasksByCategory.keys();
        std::sort(categories.begin(), categories.end());
        for (const QString& cat : categories)
        {
            drawKeyValue(ctx, cat, QString::number(tasksByCategory[cat]));
        }
    }
    drawSpacer(ctx, kSectionSpacing);

    // === 5. Response Activity ===
    drawText(ctx, QObject::tr("Response Activity"), kSubheaderFontSize, true);
    drawKeyValue(ctx, QObject::tr("Total contact attempts"), QString::number(totalAttempts));

    // Contact methods in fixed order for consistent reports, then the attempts with no
    // method, each on its own line
    for (ContactMethod method : kMethodOrder)
    {
        if (attemptsByMethod.contains(method))
        {
            drawKeyValue(ctx, contactMethodName(method),
                         QString::number(attemptsByMethod[method]));
        }
    }
    for (ContactOutcome outcome : {ContactOutcome::OK, ContactOutcome::UnableToReach})
    {
        if (marksByOutcome.contains(outcome))
        {
            drawKeyValue(ctx, markedName(outcome), QString::number(marksByOutcome[outcome]));
        }
    }
    drawSpacer(ctx, kSectionSpacing);

    // === 6. Unresolved Items ===
    QList<QString> needsHelpFamilies;
    QList<QString> notContactedFamilies;

    for (const FamilyResponseRecord& record : records)
    {
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
    for (const Family& family : families)
    {
        if (!records.contains(family.id()))
        {
            notContactedFamilies.append(family.displayName());
        }
    }

    std::sort(needsHelpFamilies.begin(), needsHelpFamilies.end());
    std::sort(notContactedFamilies.begin(), notContactedFamilies.end());

    if (!needsHelpFamilies.isEmpty() || !notContactedFamilies.isEmpty())
    {
        drawText(ctx, QObject::tr("Unresolved Items"), kSubheaderFontSize, true);

        if (!needsHelpFamilies.isEmpty())
        {
            drawText(ctx, QObject::tr("Families Needing Help (%1)").arg(needsHelpFamilies.size()),
                     kBodyFontSize, true);
            for (const QString& name : needsHelpFamilies)
            {
                drawBulletItem(ctx, name);
            }
            drawSpacer(ctx, 4);
        }

        if (!notContactedFamilies.isEmpty())
        {
            drawText(ctx, QObject::tr("Families Not Contacted (%1)").arg(notContactedFamilies.size()),
                     kBodyFontSize, true);
            for (const QString& name : notContactedFamilies)
            {
                drawBulletItem(ctx, name);
            }
        }
        drawSpacer(ctx, kSectionSpacing);
    }

    // === 7. Family Records ===
    QList<FamilyResponseRecord> logged;
    for (const FamilyResponseRecord& record : records)
    {
        if (!record.contactAttempts().isEmpty() || !record.tasks().isEmpty())
        {
            logged.append(record);
        }
    }
    std::sort(logged.begin(), logged.end(),
              [](const FamilyResponseRecord& a, const FamilyResponseRecord& b) { return a.displayName() < b.displayName(); });

    if (!logged.isEmpty())
    {
        drawText(ctx, QObject::tr("Family Records"), kSubheaderFontSize, true);
        for (const FamilyResponseRecord& record : logged)
        {
            drawRecord(ctx, record, document);
        }
    }

    return painter.end();
}

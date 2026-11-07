#include "ReportGenerator.h"
#include "Document.h"
#include "EmergencyResponse.h"

#include <QDateTime>
#include <QFont>
#include <QFontDatabase>
#include <QHash>
#include <QPrinter>
#include <QTextDocument>

#include <algorithm>

namespace
{

const int kBodyFontSize = 10;

const QString kDateFormat = "MMM d, yyyy h:mm AP";

// Fixed order for consistent reports
const QList<ContactMethod> kMethodOrder =
{
    ContactMethod::Phone,
    ContactMethod::Text,
    ContactMethod::Email,
    ContactMethod::Visit,
    ContactMethod::Other
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

// --- HTML, each function escaping the text it is given ---

void addHeading(QString& html, const QString& text, int level)
{
    html += QString("<h%1>%2</h%1>").arg(level).arg(text.toHtmlEscaped());
}

void addParagraph(QString& html, const QString& text)
{
    html += QString("<p>%1</p>").arg(text.toHtmlEscaped());
}

void addBoldParagraph(QString& html, const QString& text)
{
    html += QString("<p><b>%1</b></p>").arg(text.toHtmlEscaped());
}

void addKeyValue(QString& html, const QString& key, const QString& value)
{
    addParagraph(html, key + ": " + value);
}

void addList(QString& html, const QStringList& items)
{
    html += "<ul>";
    for (const QString& item : items)
    {
        html += QString("<li>%1</li>").arg(item.toHtmlEscaped());
    }
    html += "</ul>";
}

// --- Text ---

QString percentString(int count, int total)
{
    if (total == 0)
    {
        return "0%";
    }
    int pct = qRound(100.0 * count / total);
    return QString::number(pct) + "%";
}

/// "<count> (<percent>)"
QString countWithPercent(int count, int total)
{
    return QString::number(count) + " (" + percentString(count, total) + ")";
}

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

/// The family's name as the newest of response's attempts and tasks for it recorded it, or
/// its display name as document holds it when response has none for it.
QString familyNameIn(const FamilyId& id, const EmergencyResponse& response, const Document& document)
{
    // Both lists are newest first
    QList<ContactAttempt> attempts = response.contactAttemptsFor(id);
    QList<ResponseTask> tasks = response.tasksFor(id);
    if (!attempts.isEmpty() && (tasks.isEmpty() || attempts.first().timestamp() >= tasks.first().createdAt()))
    {
        return attempts.first().familyName();
    }
    if (!tasks.isEmpty())
    {
        return tasks.first().familyName();
    }
    std::optional<Family> family = document.findFamilyById(id);
    return family.has_value() ? family->displayName() : QString();
}

/// One attempt: its time, method, who, outcome and notes, whichever it has.
QString attemptLine(const ContactAttempt& attempt)
{
    return contactAttemptParts(attempt, timeText(attempt.timestamp())).join(" - ");
}

QString assigneeLine(const TaskAssignment& assignment)
{
    return QObject::tr("Assigned to: %1").arg(assignment.assigneeName());
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

/// The task's line, then its assignment, notification and resolution as a list under it.
QString taskItem(const ResponseTask& task)
{
    QStringList details;
    const std::optional<TaskAssignment>& assignment = task.assignment();
    if (assignment.has_value())
    {
        details.append(assigneeLine(*assignment));
        if (assignment->notification().has_value())
        {
            details.append(notificationLine(*assignment->notification()));
        }
    }
    details.append(resolutionLine(task.resolution()));

    QString html = QString("<li>%1").arg(joined({timeText(task.createdAt()), task.category(), task.description()}).toHtmlEscaped());
    addList(html, details);
    return html + "</li>";
}

/// The family's name as familyNameIn gives it, and its address as document holds it, then
/// its attempts and its tasks in response, each oldest first.
void addFamily(QString& html, const EmergencyResponse& response, const FamilyId& familyId, const Document& document)
{
    addHeading(html, familyNameIn(familyId, response, document), 3);
    std::optional<Family> family = document.findFamilyById(familyId);
    if (family.has_value())
    {
        addParagraph(html, family->address().full());
    }

    QList<ContactAttempt> attempts = response.contactAttemptsFor(familyId);
    if (!attempts.isEmpty())
    {
        QStringList lines;
        for (auto it = attempts.crbegin(); it != attempts.crend(); ++it)
        {
            lines.append(attemptLine(*it));
        }
        addBoldParagraph(html, QObject::tr("Contact attempts"));
        addList(html, lines);
    }

    QList<ResponseTask> tasks = response.tasksFor(familyId);
    if (!tasks.isEmpty())
    {
        addBoldParagraph(html, QObject::tr("Tasks"));
        html += "<ul>";
        for (auto it = tasks.crbegin(); it != tasks.crend(); ++it)
        {
            html += taskItem(*it);
        }
        html += "</ul>";
    }
}

/// The whole report as HTML.
QString reportHtml(const EmergencyResponse& response, const Document& document, const QDateTime& writtenAt)
{
    const QHash<FamilyId, Family>& families = document.families();
    QString html;

    // === 1. Header ===
    addHeading(html, QObject::tr("Emergency Response Report"), 1);
    addHeading(html, response.name(), 2);
    addParagraph(html, document.wardName());
    addParagraph(html, QObject::tr("%1 - %2").arg(timeText(response.startedAt()), timeText(writtenAt)));
    html += "<hr/>";

    // === 2. Overview Statistics ===
    int total = response.totalFamilies(families);
    int okCount = response.countByStatus(EffectiveContactStatus::OK, families);
    int needsHelpCount = response.countByStatus(EffectiveContactStatus::NeedsHelp, families);
    int unableCount = response.countByStatus(EffectiveContactStatus::UnableToReach, families);
    int notContactedCount = response.countByStatus(EffectiveContactStatus::NotContacted, families);
    int contactedCount = total - notContactedCount;

    addHeading(html, QObject::tr("Overview"), 2);
    addKeyValue(html, QObject::tr("Total families"), QString::number(total));
    addKeyValue(html, QObject::tr("Contacted"), countWithPercent(contactedCount, total));

    // === 3. Status Breakdown ===
    addHeading(html, QObject::tr("Status Breakdown"), 2);
    addKeyValue(html, QObject::tr("OK"), countWithPercent(okCount, total));
    addKeyValue(html, QObject::tr("Needs Help"), countWithPercent(needsHelpCount, total));
    addKeyValue(html, QObject::tr("Unable to Reach"), countWithPercent(unableCount, total));
    addKeyValue(html, QObject::tr("Not Contacted"), countWithPercent(notContactedCount, total));

    // === 4. Task Summary ===
    int totalTasks = 0;
    int resolvedTasks = 0;
    int assignedTasks = 0;
    QHash<QString, int> tasksByCategory;
    for (const ResponseTask& task : response.tasks())
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

    addHeading(html, QObject::tr("Task Summary"), 2);
    addKeyValue(html, QObject::tr("Total tasks"), QString::number(totalTasks));
    addKeyValue(html, QObject::tr("Resolved"), countWithPercent(resolvedTasks, totalTasks));
    addKeyValue(html, QObject::tr("Unresolved"), QString::number(totalTasks - resolvedTasks));
    addKeyValue(html, QObject::tr("Assigned"), QString::number(assignedTasks));
    addKeyValue(html, QObject::tr("Unassigned"), QString::number(totalTasks - assignedTasks));

    if (!tasksByCategory.isEmpty())
    {
        addBoldParagraph(html, QObject::tr("Tasks by Category"));
        QList<QString> categories = tasksByCategory.keys();
        std::sort(categories.begin(), categories.end());
        for (const QString& category : categories)
        {
            addKeyValue(html, category, QString::number(tasksByCategory[category]));
        }
    }

    // === 5. Response Activity ===
    QHash<ContactMethod, int> attemptsByMethod;
    QHash<ContactOutcome, int> marksByOutcome;
    for (const ContactAttempt& attempt : response.contactAttempts())
    {
        if (attempt.method().has_value())
        {
            attemptsByMethod[*attempt.method()]++;
        }
        else if (attempt.outcome().has_value())
        {
            marksByOutcome[*attempt.outcome()]++;
        }
    }

    addHeading(html, QObject::tr("Response Activity"), 2);
    addKeyValue(html, QObject::tr("Total contact attempts"), QString::number(response.contactAttempts().size()));

    // Contact methods in fixed order for consistent reports, then the attempts with no
    // method, each on its own line
    for (ContactMethod method : kMethodOrder)
    {
        if (attemptsByMethod.contains(method))
        {
            addKeyValue(html, contactMethodName(method), QString::number(attemptsByMethod[method]));
        }
    }
    for (ContactOutcome outcome : {ContactOutcome::OK, ContactOutcome::UnableToReach})
    {
        if (marksByOutcome.contains(outcome))
        {
            addKeyValue(html, contactMarkName(outcome), QString::number(marksByOutcome[outcome]));
        }
    }

    // === 6. Unresolved Items ===
    QStringList needsHelpFamilies;
    QStringList notContactedFamilies;
    QHash<FamilyId, EffectiveContactStatus> statuses = response.statusByFamily();
    for (const FamilyId& familyId : response.countedFamilies(families))
    {
        EffectiveContactStatus status = statuses.value(familyId, EffectiveContactStatus::NotContacted);
        if (status == EffectiveContactStatus::NeedsHelp)
        {
            needsHelpFamilies.append(familyNameIn(familyId, response, document));
        }
        else if (status == EffectiveContactStatus::NotContacted)
        {
            notContactedFamilies.append(familyNameIn(familyId, response, document));
        }
    }
    needsHelpFamilies.sort();
    notContactedFamilies.sort();

    if (!needsHelpFamilies.isEmpty() || !notContactedFamilies.isEmpty())
    {
        addHeading(html, QObject::tr("Unresolved Items"), 2);
        if (!needsHelpFamilies.isEmpty())
        {
            addBoldParagraph(html, QObject::tr("Families Needing Help (%1)").arg(needsHelpFamilies.size()));
            addList(html, needsHelpFamilies);
        }
        if (!notContactedFamilies.isEmpty())
        {
            addBoldParagraph(html, QObject::tr("Families Not Contacted (%1)").arg(notContactedFamilies.size()));
            addList(html, notContactedFamilies);
        }
    }

    // === 7. By Family ===
    QList<FamilyId> logged = response.familyIds().values();
    std::sort(logged.begin(), logged.end(),
              [&response, &document](const FamilyId& a, const FamilyId& b) { return familyNameIn(a, response, document) < familyNameIn(b, response, document); });
    if (!logged.isEmpty())
    {
        addHeading(html, QObject::tr("By Family"), 2);
        for (const FamilyId& familyId : logged)
        {
            addFamily(html, response, familyId, document);
        }
    }

    return html;
}

}  // namespace

bool ReportGenerator::generateReport(const EmergencyResponse& response,
                                     const Document& document,
                                     const QDateTime& writtenAt,
                                     const QString& filePath)
{
    QTextDocument report;
    report.setDefaultFont(QFont(selectFontFamily(), kBodyFontSize));
    report.setHtml(reportHtml(response, document, writtenAt));

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::Letter));
    report.print(&printer);

    // print() returns nothing; a PDF that cannot be opened for writing leaves the printer in
    // its Error state.
    return printer.printerState() != QPrinter::Error;
}

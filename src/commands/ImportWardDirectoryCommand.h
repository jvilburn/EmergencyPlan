#pragma once

#include "Command.h"
#include "Family.h"
#include "Ward.h"

#include <QDate>
#include <QHash>
#include <QSet>

#include <optional>

/// Command that handles PDF import: sets families AND creates ward if needed.
/// This allows the entire import to be undone as a single operation.
class ImportWardDirectoryCommand : public Command
{
public:
    ImportWardDirectoryCommand(
        const QHash<QString, Family>& mergedFamilies,
        const QSet<QString>& removedFamilyIds,
        const QString& wardUnitNumber,
        const QString& wardName,
        std::optional<QDate> pdfDate = std::nullopt,
        const QString& description = QString());

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QHash<QString, Family> m_newFamilies;
    QHash<QString, Family> m_previousFamilies;
    QSet<QString> m_removedFamilyIds;
    std::optional<QDate> m_pdfDate;
    QString m_wardUnitNumber;
    QString m_wardName;
    QString m_description;
    bool m_wardExisted = false;
    Ward m_previousWard;  // For undo if ward existed (to restore previous state)

    // For undo - track what was removed and previous date
    QHash<QString, Family> m_removedFamilies;
    std::optional<QDate> m_previousWardDirectoryPdfDate;

    void cleanupRemovedFamily(Document& document, const QString& familyId, const Family& family);
};

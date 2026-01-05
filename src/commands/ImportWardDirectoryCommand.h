#pragma once

#include "Command.h"
#include "Family.h"
#include "Ward.h"

#include <QHash>

/// Command that handles PDF import: sets families AND creates ward if needed.
/// This allows the entire import to be undone as a single operation.
class ImportWardDirectoryCommand : public Command
{
public:
    ImportWardDirectoryCommand(const QHash<QString, Family>& mergedFamilies,
                               const QString& wardUnitNumber,
                               const QString& wardName,
                               const QString& description);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;

private:
    QHash<QString, Family> m_newFamilies;
    QHash<QString, Family> m_previousFamilies;
    QString m_wardUnitNumber;
    QString m_wardName;
    QString m_description;
    bool m_wardExisted = false;
    Ward m_previousWard;  // For undo if ward existed (to restore previous state)
};

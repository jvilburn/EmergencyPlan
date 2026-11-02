#include "PersonCombo.h"
#include "Document.h"

#include <QComboBox>

#include <algorithm>

namespace
{

struct PersonEntry
{
    QString name;
    PersonId id;
};

bool byName(const PersonEntry& a, const PersonEntry& b)
{
    return a.name.toLower() < b.name.toLower();
}

void addEntries(QComboBox* combo, const QList<PersonEntry>& entries)
{
    for (const PersonEntry& entry : entries)
    {
        combo->addItem(entry.name, entry.id.toString());
    }
}

}  // namespace

void PersonCombo::fill(QComboBox* combo, const Document& document, const std::optional<FamilyId>& familyFirst)
{
    QList<PersonEntry> members;
    QList<PersonEntry> others;
    const QHash<FamilyId, Family>& families = document.families();
    for (auto it = families.constBegin(); it != families.constEnd(); ++it)
    {
        QList<PersonEntry>& entries = it.key() == familyFirst ? members : others;
        for (const Person& person : it.value().members())
        {
            entries.append({person.displayName(), person.id()});
        }
    }
    std::sort(others.begin(), others.end(), &byName);

    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    addEntries(combo, members);
    addEntries(combo, others);
    combo->setCurrentIndex(-1);
}

std::optional<PersonId> PersonCombo::chosen(const QComboBox* combo)
{
    QString text = combo->currentText().trimmed();
    if (text.isEmpty())
    {
        return std::nullopt;
    }
    int index = combo->currentIndex();
    if (index < 0 || combo->itemText(index).compare(text, Qt::CaseInsensitive) != 0)
    {
        index = combo->findText(text, Qt::MatchFixedString);
    }
    if (index < 0)
    {
        return std::nullopt;
    }
    return PersonId::fromString(combo->itemData(index).toString());
}

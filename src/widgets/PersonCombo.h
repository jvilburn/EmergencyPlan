#pragma once

#include "Id.h"

#include <optional>

class Document;
class QComboBox;

/// The list of people the contact and task dialogs choose from: everyone in the ward by name,
/// or, given a family, its own members first in the family's order and then everyone else
/// by name. Each entry carries the person's id as its data.
namespace PersonCombo
{

/// Fills combo with document's people, the members of familyFirst first when given, and
/// makes it editable so a name can be typed. It starts blank.
void fill(QComboBox* combo, const Document& document, const std::optional<FamilyId>& familyFirst);

/// The person combo names: the entry whose text the combo shows, matched without regard to
/// case. None when the text is blank or matches no entry.
std::optional<PersonId> chosen(const QComboBox* combo);

}  // namespace PersonCombo

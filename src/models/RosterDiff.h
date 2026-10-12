#pragma once

#include "SealingSet.h"

#include <QStringList>

/// What changed between two rosters, in a person's words, one line per change:
/// who joined or was removed, whose role changed, which devices and invites came and
/// went. The access notice reads from it; the server records nothing of the kind.
namespace RosterDiff
{

QStringList describe(const SealingSet& previous, const SealingSet& current);

} // namespace RosterDiff

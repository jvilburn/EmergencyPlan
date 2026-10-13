#pragma once

#include "Document.h"
#include "EmergencyResponse.h"

#include <optional>

/// A document on reconnect, merged three ways against the base copy: the directory
/// decides conflicts, and the emergency response and who has access merge silently.
///
/// An emergency is when several people record work at once and the network is
/// least likely to be there, so responders edit offline in parallel by design.
/// Asking one of them to choose a version would discard their record of who they
/// contacted, so every contact attempt survives and the conflict dialog is kept for
/// the directory changing on both sides.
///
/// Per family record, per task field, per contact attempt:
/// - changed on one side only: that side's value, including a deletion;
/// - changed on both sides to the same value: that value;
/// - changed differently on both sides: mine, because mine publishes second;
/// - contact attempts: the union of both sides, each merged field by field;
/// - taskCategories: a set - added on either side kept, removed on either side removed;
/// - a record or task deleted on one side and edited on the other: the edit.
///
/// A task's assignment (assignee, notes, and the notification sent) is one value,
/// and so is its resolution (notes and time).
///
/// Who has access merges by SealingSet::merge, where a both-sides change is theirs.
namespace DocumentMerge
{

/// True when doc differs from base outside the emergency response and sealing set.
bool directoryChanged(const Document& doc, const Document& base);

/// Three-way merge. Absence is a value: an emergency started on one side is kept;
/// started on both becomes one response, with name and startedAt from theirs; ended
/// on one side while the other changed records keeps the response with those records.
std::optional<EmergencyResponse> merge(const std::optional<EmergencyResponse>& base,
                                       const std::optional<EmergencyResponse>& mine,
                                       const std::optional<EmergencyResponse>& theirs);

/// The whole decision: nullopt when both sides changed the directory differently (the
/// conflict dialog); otherwise the directory from whichever side changed it (theirs
/// when neither did), the roster merged entry by entry, and the response merged.
std::optional<Document> mergeDocuments(const Document& base,
                                       const Document& mine,
                                       const Document& theirs);

} // namespace DocumentMerge

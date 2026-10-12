#pragma once

namespace DocumentConstants
{

/// The extension of a Ward Planning document, leading dot included.
/// Files carrying any other extension are other applications' files.
constexpr const char* FileExtension = ".wardplanning";

/// The same extension as a directory-listing glob.
constexpr const char* FileGlob = "*.wardplanning";

/// What an invite link starts with: wardplanning:open/<document id>.
constexpr const char* InvitePrefix = "wardplanning:join/";

} // namespace DocumentConstants

#pragma once

#include "Identity.h"

#include <QString>
#include <optional>

enum class KeyFileStatus
{
    Loaded,      ///< the key file was read
    Created,     ///< no key file existed, so one was made and saved
    Unreadable,  ///< the file was read and is not a key file this app reads; it is left as it is
    Fault        ///< the file could not be read, or a new one could not be saved; nothing changed
};

struct KeyFileResult
{
    KeyFileStatus status;
    std::optional<Identity> identity;  ///< when Loaded or Created
    QString problem;                   ///< when Unreadable or Fault
};

/// This device's key file: one compact JSON line holding the format and the two secret
/// halves, in app data's keys folder.
class KeyFile
{
public:
    static constexpr int Format = 1;

    /// This device's identity, from the key file at path. A new identity is made and saved
    /// only when no file exists at path; a file that exists is never replaced.
    static KeyFileResult loadOrCreate(const QString& path);

private:
    static KeyFileResult load(const QString& path);
    static KeyFileResult create(const QString& path);
};

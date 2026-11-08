#pragma once

#include <QString>

/// Shows a file to the user in the system's file browser.
namespace ShowInFolder
{

/// Opens the folder holding path, with the file selected where the system's file browser can
/// select it: Explorer on Windows and the Finder on macOS select it; elsewhere the folder opens.
void reveal(const QString& path);

}  // namespace ShowInFolder

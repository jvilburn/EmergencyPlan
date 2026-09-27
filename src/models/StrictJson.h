#pragma once

#include <QJsonObject>
#include <QLatin1String>

#include <initializer_list>

/// Reading the document format strictly: a reader accepts an object only when it
/// understands all of it.
namespace StrictJson
{

/// True when every key in json is one of allowed. A field a newer writer added
/// without a version bump is refused rather than silently dropped.
bool hasOnlyKeys(const QJsonObject& json, std::initializer_list<QLatin1String> allowed);

} // namespace StrictJson

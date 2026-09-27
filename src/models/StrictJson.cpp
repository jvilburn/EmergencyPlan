#include "StrictJson.h"

#include <algorithm>

namespace StrictJson
{

bool hasOnlyKeys(const QJsonObject& json, std::initializer_list<QLatin1String> allowed)
{
    for (auto key = json.constBegin(); key != json.constEnd(); ++key)
    {
        if (std::find(allowed.begin(), allowed.end(), key.key()) == allowed.end())
        {
            return false;
        }
    }
    return true;
}

} // namespace StrictJson

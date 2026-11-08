#include "Fields.h"

#include <sodium.h>

#include <algorithm>
#include <cmath>

namespace
{

/// 2^53, the largest magnitude an integer may have.
constexpr std::int64_t LargestInteger = 9007199254740992;

bool isDigits(const std::string& text, std::size_t start, std::size_t count)
{
    for (std::size_t i = start; i < start + count; ++i)
    {
        if (text[i] < '0' || text[i] > '9')
        {
            return false;
        }
    }
    return true;
}

int numberAt(const std::string& text, std::size_t start, std::size_t count)
{
    return std::stoi(text.substr(start, count));
}

bool isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

/// Whether text is yyyy-MM-dd naming a day of the calendar.
bool isDate(const std::string& text)
{
    if (text.size() != 10
        || !isDigits(text, 0, 4) || text[4] != '-'
        || !isDigits(text, 5, 2) || text[7] != '-'
        || !isDigits(text, 8, 2))
    {
        return false;
    }
    static const int DaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int year = numberAt(text, 0, 4);
    int month = numberAt(text, 5, 2);
    int day = numberAt(text, 8, 2);
    if (month < 1 || month > 12)
    {
        return false;
    }
    int days = DaysInMonth[month - 1] + (month == 2 && isLeapYear(year) ? 1 : 0);
    return day >= 1 && day <= days;
}

/// Whether text is yyyy-MM-ddTHH:mm:ss.zzzZ naming a moment of the calendar.
bool isTime(const std::string& text)
{
    return text.size() == 24
        && isDate(text.substr(0, 10))
        && text[10] == 'T'
        && isDigits(text, 11, 2) && numberAt(text, 11, 2) <= 23 && text[13] == ':'
        && isDigits(text, 14, 2) && numberAt(text, 14, 2) <= 59 && text[16] == ':'
        && isDigits(text, 17, 2) && numberAt(text, 17, 2) <= 59 && text[19] == '.'
        && isDigits(text, 20, 3)
        && text[23] == 'Z';
}

}  // namespace

Fields::Fields(const nlohmann::json& value, const std::string& path)
    : m_object(value)
    , m_path(path)
{
    if (!value.is_object())
    {
        throw Refusal(path + " is not an object");
    }
}

std::string Fields::text(const std::string& key)
{
    return textOf(take(key), pathOf(key));
}

std::string Fields::optionalText(const std::string& key)
{
    const nlohmann::json* value = takeOptional(key);
    if (value == nullptr)
    {
        return std::string();
    }
    std::string text = textOf(*value, pathOf(key));
    if (text.empty())
    {
        throw Refusal(pathOf(key) + " is written out empty");
    }
    return text;
}

std::string Fields::id(const std::string& key)
{
    std::string text = textOf(take(key), pathOf(key));
    if (text.empty())
    {
        throw Refusal(pathOf(key) + " is an empty id");
    }
    return text;
}

std::string Fields::optionalId(const std::string& key)
{
    const nlohmann::json* value = takeOptional(key);
    if (value == nullptr)
    {
        return std::string();
    }
    std::string text = textOf(*value, pathOf(key));
    if (text.empty())
    {
        throw Refusal(pathOf(key) + " is written out empty");
    }
    return text;
}

std::vector<std::string> Fields::ids(const std::string& key)
{
    return textsOf(take(key), pathOf(key), true);
}

std::vector<std::string> Fields::optionalIds(const std::string& key)
{
    const nlohmann::json* value = takeOptional(key);
    if (value == nullptr)
    {
        return std::vector<std::string>();
    }
    std::vector<std::string> ids = textsOf(*value, pathOf(key), true);
    if (ids.empty())
    {
        throw Refusal(pathOf(key) + " is written out empty");
    }
    return ids;
}

std::vector<std::string> Fields::texts(const std::string& key)
{
    return textsOf(take(key), pathOf(key), false);
}

std::vector<std::string> Fields::optionalTexts(const std::string& key)
{
    const nlohmann::json* value = takeOptional(key);
    if (value == nullptr)
    {
        return std::vector<std::string>();
    }
    std::vector<std::string> texts = textsOf(*value, pathOf(key), false);
    if (texts.empty())
    {
        throw Refusal(pathOf(key) + " is written out empty");
    }
    return texts;
}

std::int64_t Fields::integer(const std::string& key)
{
    const nlohmann::json& value = take(key);
    if (!isInteger(value))
    {
        throw Refusal(pathOf(key) + " is not an integer");
    }
    if (value.is_number_float())
    {
        return static_cast<std::int64_t>(value.get<double>());
    }
    return value.get<std::int64_t>();
}

std::optional<std::int64_t> Fields::optionalInteger(const std::string& key)
{
    if (m_object.find(key) == m_object.end())
    {
        m_taken.insert(key);
        return std::nullopt;
    }
    return integer(key);
}

double Fields::number(const std::string& key)
{
    const nlohmann::json& value = take(key);
    if (!value.is_number())
    {
        throw Refusal(pathOf(key) + " is not a number");
    }
    return value.get<double>();
}

std::optional<double> Fields::optionalNumber(const std::string& key)
{
    if (m_object.find(key) == m_object.end())
    {
        m_taken.insert(key);
        return std::nullopt;
    }
    return number(key);
}

bool Fields::boolean(const std::string& key)
{
    const nlohmann::json& value = take(key);
    if (!value.is_boolean())
    {
        throw Refusal(pathOf(key) + " is not a boolean");
    }
    return value.get<bool>();
}

bool Fields::optionalTrue(const std::string& key)
{
    const nlohmann::json* value = takeOptional(key);
    if (value == nullptr)
    {
        return false;
    }
    if (!value->is_boolean() || !value->get<bool>())
    {
        throw Refusal(pathOf(key) + " is not true");
    }
    return true;
}

std::string Fields::word(const std::string& key, const std::vector<std::string>& words)
{
    std::string text = textOf(take(key), pathOf(key));
    if (std::find(words.begin(), words.end(), text) == words.end())
    {
        throw Refusal(pathOf(key) + " is not one of its words");
    }
    return text;
}

std::string Fields::optionalWord(const std::string& key, const std::vector<std::string>& words)
{
    if (m_object.find(key) == m_object.end())
    {
        m_taken.insert(key);
        return std::string();
    }
    return word(key, words);
}

std::string Fields::date(const std::string& key)
{
    std::string text = textOf(take(key), pathOf(key));
    if (!isDate(text))
    {
        throw Refusal(pathOf(key) + " is not a date");
    }
    return text;
}

std::string Fields::optionalDate(const std::string& key)
{
    if (m_object.find(key) == m_object.end())
    {
        m_taken.insert(key);
        return std::string();
    }
    return date(key);
}

std::string Fields::time(const std::string& key)
{
    std::string text = textOf(take(key), pathOf(key));
    if (!isTime(text))
    {
        throw Refusal(pathOf(key) + " is not a time");
    }
    return text;
}

std::vector<unsigned char> Fields::bytes(const std::string& key, std::size_t length)
{
    std::optional<std::vector<unsigned char>> bytes = decode(textOf(take(key), pathOf(key)), length);
    if (!bytes.has_value())
    {
        throw Refusal(pathOf(key) + " is not base64url of " + std::to_string(length) + " bytes");
    }
    return *bytes;
}

std::vector<std::vector<unsigned char>> Fields::optionalBytesList(const std::string& key, std::size_t length)
{
    std::vector<std::vector<unsigned char>> list;
    const nlohmann::json* value = takeOptional(key);
    if (value == nullptr)
    {
        return list;
    }
    std::vector<std::string> texts = textsOf(*value, pathOf(key), false);
    if (texts.empty())
    {
        throw Refusal(pathOf(key) + " is written out empty");
    }
    for (std::size_t i = 0; i < texts.size(); ++i)
    {
        std::optional<std::vector<unsigned char>> bytes = decode(texts[i], length);
        if (!bytes.has_value())
        {
            throw Refusal(pathOf(key) + "[" + std::to_string(i) + "] is not base64url of "
                          + std::to_string(length) + " bytes");
        }
        list.push_back(*bytes);
    }
    return list;
}

Fields Fields::object(const std::string& key)
{
    return Fields(take(key), pathOf(key));
}

std::optional<Fields> Fields::optionalObject(const std::string& key)
{
    const nlohmann::json* value = takeOptional(key);
    if (value == nullptr)
    {
        return std::nullopt;
    }
    if (value->is_object() && value->empty())
    {
        throw Refusal(pathOf(key) + " is written out empty");
    }
    return Fields(*value, pathOf(key));
}

std::vector<Fields> Fields::objects(const std::string& key)
{
    const nlohmann::json& value = take(key);
    if (!value.is_array())
    {
        throw Refusal(pathOf(key) + " is not an array");
    }
    std::vector<Fields> objects;
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        objects.push_back(Fields(value[i], pathOf(key) + "[" + std::to_string(i) + "]"));
    }
    return objects;
}

std::vector<Fields> Fields::optionalObjects(const std::string& key)
{
    if (m_object.find(key) == m_object.end())
    {
        m_taken.insert(key);
        return std::vector<Fields>();
    }
    std::vector<Fields> objects = this->objects(key);
    if (objects.empty())
    {
        throw Refusal(pathOf(key) + " is written out empty");
    }
    return objects;
}

bool Fields::takeNull(const std::string& key)
{
    nlohmann::json::const_iterator it = m_object.find(key);
    if (it == m_object.end() || !it->is_null())
    {
        return false;
    }
    m_taken.insert(key);
    return true;
}

std::vector<std::string> Fields::keys() const
{
    std::vector<std::string> keys;
    for (nlohmann::json::const_iterator it = m_object.begin(); it != m_object.end(); ++it)
    {
        keys.push_back(it.key());
    }
    return keys;
}

std::string Fields::pathOf(const std::string& key) const
{
    return m_path.empty() ? key : m_path + "." + key;
}

void Fields::done() const
{
    for (nlohmann::json::const_iterator it = m_object.begin(); it != m_object.end(); ++it)
    {
        if (m_taken.count(it.key()) == 0)
        {
            throw Refusal(pathOf(it.key()) + " is not in the format");
        }
    }
}

const nlohmann::json& Fields::take(const std::string& key)
{
    nlohmann::json::const_iterator it = m_object.find(key);
    if (it == m_object.end())
    {
        throw Refusal(pathOf(key) + " is missing");
    }
    m_taken.insert(key);
    return *it;
}

const nlohmann::json* Fields::takeOptional(const std::string& key)
{
    m_taken.insert(key);
    nlohmann::json::const_iterator it = m_object.find(key);
    if (it == m_object.end())
    {
        return nullptr;
    }
    return &*it;
}

std::string Fields::textOf(const nlohmann::json& value, const std::string& path) const
{
    if (!value.is_string())
    {
        throw Refusal(path + " is not a string");
    }
    return value.get<std::string>();
}

std::vector<std::string> Fields::textsOf(const nlohmann::json& value, const std::string& path, bool nonEmpty) const
{
    if (!value.is_array())
    {
        throw Refusal(path + " is not an array");
    }
    std::vector<std::string> texts;
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        std::string elementPath = path + "[" + std::to_string(i) + "]";
        std::string text = textOf(value[i], elementPath);
        if (nonEmpty && text.empty())
        {
            throw Refusal(elementPath + " is an empty id");
        }
        texts.push_back(text);
    }
    return texts;
}

bool isInteger(const nlohmann::json& value)
{
    if (value.is_number_unsigned())
    {
        return value.get<std::uint64_t>() <= static_cast<std::uint64_t>(LargestInteger);
    }
    if (value.is_number_integer())
    {
        std::int64_t number = value.get<std::int64_t>();
        return number >= -LargestInteger && number <= LargestInteger;
    }
    if (value.is_number_float())
    {
        double number = value.get<double>();
        return std::floor(number) == number && std::fabs(number) <= static_cast<double>(LargestInteger);
    }
    return false;
}

std::string encode(const std::vector<unsigned char>& bytes)
{
    const int variant = sodium_base64_VARIANT_URLSAFE_NO_PADDING;
    std::string text(sodium_base64_encoded_len(bytes.size(), variant), '\0');
    sodium_bin2base64(&text[0], text.size(), bytes.data(), bytes.size(), variant);
    text.resize(text.size() - 1);
    return text;
}

std::optional<std::vector<unsigned char>> decode(const std::string& text, std::size_t length)
{
    std::vector<unsigned char> bytes(length);
    std::size_t decoded = 0;
    const char* end = nullptr;
    if (sodium_base642bin(bytes.data(), bytes.size(), text.data(), text.size(), nullptr, &decoded, &end,
                          sodium_base64_VARIANT_URLSAFE_NO_PADDING)
            != 0
        || end != text.data() + text.size()
        || decoded != length
        || encode(bytes) != text)
    {
        return std::nullopt;
    }
    return bytes;
}

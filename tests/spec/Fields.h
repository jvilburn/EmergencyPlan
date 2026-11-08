#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

/// What a reader refuses a file for, with the path of the field that fails.
class Refusal : public std::runtime_error
{
public:
    explicit Refusal(const std::string& problem) : std::runtime_error(problem) {}
};

/// One JSON object read as docs/FORMAT.md's conventions state: each field taken by its type,
/// a field omitted when empty refused when written out empty, and done() refusing any field
/// nothing took. Every failure throws a Refusal naming the field's path.
class Fields
{
public:
    /// value, which must be an object, found at path.
    Fields(const nlohmann::json& value, const std::string& path);

    std::string text(const std::string& key);
    std::string optionalText(const std::string& key);

    /// A non-empty string.
    std::string id(const std::string& key);
    std::string optionalId(const std::string& key);

    /// An array of non-empty strings.
    std::vector<std::string> ids(const std::string& key);
    std::vector<std::string> optionalIds(const std::string& key);

    std::vector<std::string> texts(const std::string& key);
    std::vector<std::string> optionalTexts(const std::string& key);

    std::int64_t integer(const std::string& key);
    std::optional<std::int64_t> optionalInteger(const std::string& key);

    double number(const std::string& key);
    std::optional<double> optionalNumber(const std::string& key);

    bool boolean(const std::string& key);

    /// A field written only as true.
    bool optionalTrue(const std::string& key);

    /// One of words.
    std::string word(const std::string& key, const std::vector<std::string>& words);
    std::string optionalWord(const std::string& key, const std::vector<std::string>& words);

    /// yyyy-MM-dd, a date of the calendar.
    std::string date(const std::string& key);
    std::string optionalDate(const std::string& key);

    /// yyyy-MM-ddTHH:mm:ss.zzzZ, a time of the calendar.
    std::string time(const std::string& key);

    /// Base64url without padding of exactly length bytes, returned as the bytes.
    std::vector<unsigned char> bytes(const std::string& key, std::size_t length);
    std::vector<std::vector<unsigned char>> optionalBytesList(const std::string& key, std::size_t length);

    Fields object(const std::string& key);
    std::optional<Fields> optionalObject(const std::string& key);
    std::vector<Fields> objects(const std::string& key);
    std::vector<Fields> optionalObjects(const std::string& key);

    /// True, with key taken, when key holds null; false, with key left for the call that
    /// reads its value, otherwise.
    bool takeNull(const std::string& key);

    /// Every key of an object whose keys are data.
    std::vector<std::string> keys() const;

    /// The path of the field under key, for a refusal a caller words itself.
    std::string pathOf(const std::string& key) const;

    /// Refuses the file for any field nothing took.
    void done() const;

private:
    const nlohmann::json& take(const std::string& key);
    const nlohmann::json* takeOptional(const std::string& key);
    std::string textOf(const nlohmann::json& value, const std::string& path) const;
    std::vector<std::string> textsOf(const nlohmann::json& value, const std::string& path, bool nonEmpty) const;

    nlohmann::json m_object;
    std::string m_path;
    std::set<std::string> m_taken;
};

/// Whether value is a JSON number with no fractional part and a magnitude of at most 2^53.
bool isInteger(const nlohmann::json& value);

/// Base64url without padding.
std::string encode(const std::vector<unsigned char>& bytes);

/// The length bytes text encodes as base64url without padding, or none when it is of another
/// length, padded, outside the alphabet, or has non-zero unused trailing bits.
std::optional<std::vector<unsigned char>> decode(const std::string& text, std::size_t length);

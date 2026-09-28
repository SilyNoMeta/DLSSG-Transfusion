#pragma once
// In-place editing of DLSSG-Transfusion.json (JSON with // comments).
// Values are located exactly as the engine's FindJsonValue does (first
// occurrence of "key", then ':'), and only the value token is replaced, so the
// engine's comments and layout survive every edit.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace config_text
{
inline bool FindValue(const std::string& text, const char* key, size_t& begin, size_t& end)
{
    const std::string quoted = std::string("\"") + key + "\"";
    const size_t keyOffset = text.find(quoted);
    if (keyOffset == std::string::npos)
        return false;
    const size_t colon = text.find(':', keyOffset + quoted.size());
    if (colon == std::string::npos)
        return false;
    begin = text.find_first_not_of(" \t\r\n", colon + 1);
    if (begin == std::string::npos)
        return false;
    if (text[begin] == '"')
    {
        const size_t close = text.find('"', begin + 1);
        if (close == std::string::npos)
            return false;
        end = close + 1;
        return true;
    }
    end = text.find_first_of(",}/ \t\r\n", begin);
    if (end == std::string::npos)
        end = text.size();
    return end > begin;
}

inline bool GetRaw(const std::string& text, const char* key, std::string& raw)
{
    size_t begin = 0, end = 0;
    if (!FindValue(text, key, begin, end))
        return false;
    raw.assign(text, begin, end - begin);
    return true;
}

inline bool GetBool(const std::string& text, const char* key, bool& value)
{
    std::string raw;
    if (!GetRaw(text, key, raw))
        return false;
    if (raw == "true" || raw == "1") { value = true; return true; }
    if (raw == "false" || raw == "0") { value = false; return true; }
    return false;
}

inline bool GetUnsigned(const std::string& text, const char* key, uint32_t& value)
{
    std::string raw;
    if (!GetRaw(text, key, raw) || raw.empty() || raw[0] < '0' || raw[0] > '9')
        return false;
    char* stop = nullptr;
    const unsigned long parsed = std::strtoul(raw.c_str(), &stop, 10);
    if (!stop || *stop != '\0' || parsed > 0xFFFFFFFFul)
        return false;
    value = static_cast<uint32_t>(parsed);
    return true;
}

// Returns the string contents without quotes.
inline bool GetString(const std::string& text, const char* key, std::string& value)
{
    std::string raw;
    if (!GetRaw(text, key, raw) || raw.size() < 2 || raw.front() != '"' || raw.back() != '"')
        return false;
    value = raw.substr(1, raw.size() - 2);
    return true;
}

// Replaces the value of `key` with `raw` (already JSON-encoded). A missing key
// is inserted as the first member so no trailing comma is ever produced.
inline bool SetRaw(std::string& text, const char* key, const std::string& raw)
{
    size_t begin = 0, end = 0;
    if (FindValue(text, key, begin, end))
    {
        text.replace(begin, end - begin, raw);
        return true;
    }
    const size_t brace = text.find('{');
    if (brace == std::string::npos)
        return false;
    const size_t next = text.find_first_not_of(" \t\r\n", brace + 1);
    const bool empty = next == std::string::npos || text[next] == '}';
    text.insert(brace + 1, std::string("\n  \"") + key + "\": " + raw + (empty ? "\n" : ","));
    return true;
}

inline bool SetBool(std::string& text, const char* key, bool value)
{
    return SetRaw(text, key, value ? "true" : "false");
}

inline bool SetUnsigned(std::string& text, const char* key, uint32_t value)
{
    return SetRaw(text, key, std::to_string(value));
}

inline bool SetString(std::string& text, const char* key, const std::string& value)
{
    if (value.find_first_of("\"\\\r\n") != std::string::npos)
        return false;
    return SetRaw(text, key, "\"" + value + "\"");
}
} // namespace config_text

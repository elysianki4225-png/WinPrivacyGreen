#include "ini.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace recovery {
namespace {

std::string trim(const std::string& s) {
    const auto whitespace = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r'; };
    size_t start = 0, end = s.size();
    while (start < end && whitespace(static_cast<unsigned char>(s[start]))) ++start;
    while (end > start && whitespace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    return s;
}

std::string without_bom(const std::string& s) {
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xef &&
        static_cast<unsigned char>(s[1]) == 0xbb && static_cast<unsigned char>(s[2]) == 0xbf)
        return s.substr(3);
    return s;
}

// Returns an empty string for non-section lines. Only ASCII section and key
// names are involved; values and all unrelated content remain opaque bytes.
std::string section_name(const std::string& line) {
    const std::string t = trim(without_bom(line));
    if (t.empty() || t[0] != '[') return {};
    const size_t close = t.find(']');
    if (close == std::string::npos) return {};
    const std::string tail = trim(t.substr(close + 1));
    if (!tail.empty() && tail[0] != ';' && tail[0] != '#') return {};
    return lower(trim(t.substr(1, close - 1)));
}

bool key_value(const std::string& line, std::string& name, std::string& value) {
    const std::string t = trim(without_bom(line));
    if (t.empty() || t[0] == ';' || t[0] == '#' || t[0] == '[') return false;
    const size_t eq = t.find('=');
    if (eq == std::string::npos) return false;
    name = lower(trim(t.substr(0, eq)));
    value = trim(t.substr(eq + 1));
    return !name.empty();
}

bool valid_utf8(const std::string& bytes) {
    size_t i = 0;
    while (i < bytes.size()) {
        const unsigned char lead = static_cast<unsigned char>(bytes[i++]);
        if (lead < 0x80) continue;
        unsigned int codepoint = 0, count = 0, minimum = 0;
        if (lead >= 0xc2 && lead <= 0xdf) { codepoint = lead & 0x1f; count = 1; minimum = 0x80; }
        else if (lead >= 0xe0 && lead <= 0xef) { codepoint = lead & 0x0f; count = 2; minimum = 0x800; }
        else if (lead >= 0xf0 && lead <= 0xf4) { codepoint = lead & 0x07; count = 3; minimum = 0x10000; }
        else return false;
        if (i + count > bytes.size()) return false;
        while (count--) {
            const unsigned char next = static_cast<unsigned char>(bytes[i++]);
            if ((next & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
    }
    return true;
}

std::string scalar(std::string value) {
    value = trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        value = value.substr(1, value.size() - 2);
    return trim(value);
}

}  // namespace

Ini::Ini(const std::string& bytes) {
    // QSettings writes UTF-8/ASCII-compatible INI. Never rewrite binary or
    // UTF-16 data as if it were UTF-8; require the user to resolve it first.
    if (bytes.find('\0') != std::string::npos || !valid_utf8(bytes) ||
        (bytes.size() >= 2 && ((static_cast<unsigned char>(bytes[0]) == 0xff && static_cast<unsigned char>(bytes[1]) == 0xfe) ||
                               (static_cast<unsigned char>(bytes[0]) == 0xfe && static_cast<unsigned char>(bytes[1]) == 0xff)))) {
        valid_ = false;
        return;
    }
    size_t start = 0;
    bool chose_newline = false;
    while (start < bytes.size()) {
        const size_t pos = bytes.find('\n', start);
        if (pos == std::string::npos) {
            lines_.push_back({bytes.substr(start), {}});
            break;
        }
        const bool crlf = pos > start && bytes[pos - 1] == '\r';
        const std::string ending = crlf ? "\r\n" : "\n";
        lines_.push_back({bytes.substr(start, pos - start - (crlf ? 1 : 0)), ending});
        if (!chose_newline) { newline_ = ending; chose_newline = true; }
        start = pos + 1;
    }
    for (const auto& line : lines_) {
        const std::string t = trim(without_bom(line.body));
        if (!t.empty() && t[0] == '[' && section_name(line.body).empty()) {
            valid_ = false;
            break;
        }
    }
}

Key Ini::get(const std::string& section, const std::string& key) const {
    Key result;
    std::string current;
    const std::string target_section = lower(section), target_key = lower(key);
    for (const auto& line : lines_) {
        const std::string group = section_name(line.body);
        if (!group.empty()) { current = group; continue; }
        std::string name, value;
        if (current == target_section && key_value(line.body, name, value) && name == target_key) {
            result.exists = true;
            result.value = value; // Qt INI duplicates resolve to a single value; retain every line for restore.
            result.lines.push_back(line.body);
        }
    }
    return result;
}

void Ini::replace(const std::string& section, const std::string& key,
                  const std::vector<std::string>& replacement) {
    const std::string target_section = lower(section), target_key = lower(key);
    std::string current;
    std::vector<Line> remaining;
    remaining.reserve(lines_.size() + replacement.size() + 2);
    for (const auto& line : lines_) {
        const std::string group = section_name(line.body);
        if (!group.empty()) current = group;
        std::string name, value;
        if (current == target_section && key_value(line.body, name, value) && name == target_key)
            continue;
        remaining.push_back(line);
    }
    lines_.swap(remaining);
    if (replacement.empty()) return;

    size_t section_at = lines_.size(), insert_at = lines_.size();
    for (size_t i = 0; i < lines_.size(); ++i) {
        const std::string group = section_name(lines_[i].body);
        if (section_at == lines_.size()) {
            if (group == target_section) section_at = i;
        } else if (!group.empty()) {
            insert_at = i;
            break;
        }
    }
    if (section_at == lines_.size()) {
        if (!lines_.empty() && lines_.back().ending.empty()) lines_.back().ending = newline_;
        lines_.push_back({"[" + section + "]", newline_});
        insert_at = lines_.size();
    } else if (insert_at == lines_.size() && !lines_.empty() && lines_.back().ending.empty()) {
        lines_.back().ending = newline_;
    }

    std::vector<Line> inserted;
    inserted.reserve(replacement.size());
    for (const auto& body : replacement) inserted.push_back({body, newline_});
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(insert_at), inserted.begin(), inserted.end());
}

std::string Ini::text() const {
    std::string out;
    for (const auto& line : lines_) { out += line.body; out += line.ending; }
    return out;
}

bool is_disabled(const Ini& ini) {
    const Key k = ini.get("UIConfig", "DisabledRecovery");
    if (!k.exists) return false;
    const std::string value = lower(scalar(k.value));
    // QVariant::toBool() is false for an empty string, "0" or "false".
    return !value.empty() && value != "0" && value != "false";
}

bool is_advanced(const Ini& ini) {
    const Key k = ini.get("Options", "ViewMode");
    if (!k.exists) return true; // SandMan defaults to advanced mode (1).
    const std::string value = scalar(k.value);
    if (value.empty()) return false;
    char* end = nullptr;
    const long n = std::strtol(value.c_str(), &end, 10);
    return end != value.c_str() && *end == '\0' && n == 1;
}

Result disable(const std::string& current) {
    Ini ini(current);
    if (!ini.valid()) return {};
    Result result;
    result.ok = true;
    if (!is_disabled(ini)) ini.replace("UIConfig", "DisabledRecovery", {"DisabledRecovery=true"});
    if (!is_advanced(ini)) {
        ini.replace("Options", "ViewMode", {"ViewMode=1"});
        result.view_changed = true;
    }
    result.text = ini.text();
    return result;
}

Result restore(const std::string& current, const std::string& backup) {
    Ini ini(current), original(backup);
    if (!ini.valid() || !original.valid()) return {};
    Result result;
    result.ok = true;
    ini.replace("UIConfig", "DisabledRecovery", original.get("UIConfig", "DisabledRecovery").lines);
    // The tool changes ViewMode only when it was not advanced originally.
    // Do not undo a subsequent manual view change by the user.
    if (!is_advanced(original)) {
        if (is_advanced(ini)) {
            ini.replace("Options", "ViewMode", original.get("Options", "ViewMode").lines);
            result.view_changed = true;
        } else {
            result.view_preserved = true;
        }
    }
    result.text = ini.text();
    return result;
}

}  // namespace recovery

#pragma once

#include <string>
#include <vector>

namespace recovery {

// Sandboxie-Plus uses Qt's UTF-8 INI format. Only these two keys are edited;
// every unrelated byte is retained, including comments and non-ASCII text.
struct Key {
    bool exists = false;
    std::string value;
    std::vector<std::string> lines;
};

class Ini {
public:
    explicit Ini(const std::string& bytes);
    bool valid() const { return valid_; }
    Key get(const std::string& section, const std::string& key) const;
    void replace(const std::string& section, const std::string& key,
                 const std::vector<std::string>& lines);
    std::string text() const;

private:
    struct Line { std::string body, ending; };
    std::vector<Line> lines_;
    std::string newline_ = "\r\n";
    bool valid_ = true;
};

bool is_disabled(const Ini& ini);
bool is_advanced(const Ini& ini);

struct Result {
    bool ok = false;
    bool view_changed = false;
    bool view_preserved = false;
    std::string text;
};

Result disable(const std::string& current);
Result restore(const std::string& current, const std::string& backup);

}  // namespace recovery

#include "ini.h"

#include <cstdlib>
#include <iostream>
#include <string>

using recovery::Ini;
using recovery::disable;
using recovery::restore;

static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { \
    std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " << #x << '\n'; std::exit(1); \
} } while (false)

int main() {
    const std::string original =
        "\xef\xbb\xbf; preserved comment\r\n[Options]\r\nViewMode=0\r\n"
        "UiLanguage=zh_CN\r\n\r\n[UIConfig]\r\nDisabledRecovery=false\r\n"
        "Unrelated=\xe4\xb8\xad\xe6\x96\x87\r\n";
    auto changed = disable(original);
    CHECK(changed.ok && changed.view_changed);
    CHECK(changed.text.substr(0, 3) == "\xef\xbb\xbf");
    Ini edited(changed.text);
    CHECK(recovery::is_disabled(edited));
    CHECK(recovery::is_advanced(edited));
    CHECK(changed.text.find("; preserved comment\r\n") != std::string::npos);
    CHECK(changed.text.find("Unrelated=\xe4\xb8\xad\xe6\x96\x87\r\n") != std::string::npos);
    CHECK(changed.text.find("DisabledRecovery=false") == std::string::npos);
    CHECK(disable(changed.text).text == changed.text); // repeated click is a no-op

    std::string subsequently_edited = changed.text;
    const auto at = subsequently_edited.find("UiLanguage=zh_CN");
    subsequently_edited.replace(at, std::string("UiLanguage=zh_CN").size(), "UiLanguage=en");
    auto undone = restore(subsequently_edited, original);
    CHECK(undone.ok && undone.view_changed && !undone.view_preserved);
    Ini recovered(undone.text);
    CHECK(!recovery::is_disabled(recovered));
    CHECK(!recovery::is_advanced(recovered));
    CHECK(undone.text.find("UiLanguage=en") != std::string::npos);
    CHECK(undone.text.find("Unrelated=\xe4\xb8\xad\xe6\x96\x87") != std::string::npos);

    // Missing key / sections and missing final newline. Preserve the unrelated data.
    const std::string minimal = "[General]\nUser=Example";
    changed = disable(minimal);
    CHECK(changed.ok && !changed.view_changed);
    CHECK(changed.text.find("User=Example\n[UIConfig]\nDisabledRecovery=true\n") != std::string::npos);
    CHECK(recovery::is_disabled(Ini(changed.text)));
    undone = restore(changed.text, minimal);
    CHECK(undone.ok && !Ini(undone.text).get("UIConfig", "DisabledRecovery").exists);
    CHECK(undone.text.find("User=Example") != std::string::npos);

    // Case and duplicate keys: remove all modified instances and restore originals.
    const std::string duplicate = "[uiconfig]\ndisabledrecovery=false\nother=1\n"
                                  "[Options]\nViewMode=2\n[UIConfig]\nDisabledRecovery=0\n";
    changed = disable(duplicate);
    CHECK(changed.ok && recovery::is_disabled(Ini(changed.text)));
    CHECK(Ini(changed.text).get("UIConfig", "DisabledRecovery").lines.size() == 1);
    undone = restore(changed.text, duplicate);
    CHECK(Ini(undone.text).get("UIConfig", "DisabledRecovery").lines.size() == 2);
    CHECK(Ini(undone.text).get("Options", "ViewMode").value == "2");

    // A user-initiated subsequent view change must never be overwritten.
    std::string manually_changed = changed.text;
    auto mode = manually_changed.find("ViewMode=1");
    CHECK(mode != std::string::npos);
    manually_changed.replace(mode, 10, "ViewMode=0");
    undone = restore(manually_changed, duplicate);
    CHECK(undone.ok && undone.view_preserved && !undone.view_changed);
    CHECK(Ini(undone.text).get("Options", "ViewMode").value == "0");

    // The original was already disabled: a view change can still be rolled back.
    const std::string already = "[UIConfig]\nDisabledRecovery=true\n[Options]\nViewMode=2\n";
    changed = disable(already);
    CHECK(changed.ok && changed.view_changed && recovery::is_disabled(Ini(changed.text)));
    undone = restore(changed.text, already);
    CHECK(recovery::is_disabled(Ini(undone.text)));
    CHECK(Ini(undone.text).get("Options", "ViewMode").value == "2");
    CHECK(disable("[UIConfig]\nDisabledRecovery=1\n").text == "[UIConfig]\nDisabledRecovery=1\n");

    // Empty files and quoted Qt scalar values.
    changed = disable("");
    CHECK(changed.ok && recovery::is_disabled(Ini(changed.text)));
    CHECK(!recovery::is_disabled(Ini("[UIConfig]\nDisabledRecovery=\"false\"\n")));
    CHECK(recovery::is_advanced(Ini("[Options]\nViewMode=\"1\"\n")));

    // Fail closed on unsupported encoding and malformed sections.
    CHECK(!disable(std::string("\xff\xfe[\0", 4)).ok);
    CHECK(!disable(std::string("x\0y", 3)).ok);
    CHECK(!disable(std::string("[Options]\nBad=\xc0\xaf\n", 16)).ok); // overlong UTF-8
    CHECK(!disable("[UIConfig\nDisabledRecovery=false\n").ok);
    CHECK(!restore(changed.text, "[Broken\n").ok);

    std::cout << checks << " checks passed\n";
    return 0;
}

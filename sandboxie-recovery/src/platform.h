#pragma once

#include <string>

namespace recovery {

struct ProcessState {
    bool known = false;
    bool running = false;
    std::wstring image;
};

struct Location {
    bool ok = false;
    std::wstring config;
    std::wstring message;
};

struct Outcome {
    int code = 1;
    std::wstring message;
};

ProcessState manager_state();
std::wstring find_sandman();
Location locate_config(const std::wstring& sandman);
std::wstring backup_path(const std::wstring& config);
bool file_exists(const std::wstring& path);
bool read_config(const std::wstring& path, std::string& bytes);
Outcome apply(const std::wstring& sandman, bool restore);

}  // namespace recovery

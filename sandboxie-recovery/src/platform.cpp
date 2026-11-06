#include "platform.h"
#include "ini.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <algorithm>
#include <string>
#include <vector>

namespace recovery {
namespace {

constexpr DWORD kMaxConfigSize = 16 * 1024 * 1024;
const wchar_t* const kIniName = L"Sandboxie-Plus.ini";

struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : h(value) {}
    ~Handle() { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    bool valid() const { return h && h != INVALID_HANDLE_VALUE; }
};

std::wstring dirname(const std::wstring& p) {
    const size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"" : p.substr(0, pos);
}
std::wstring basename(const std::wstring& p) {
    const size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? p : p.substr(pos + 1);
}
std::wstring join(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    return a + ((a.back() == L'\\' || a.back() == L'/') ? L"" : L"\\") + b;
}

// A failed probe must not accidentally be treated as "no shared config" and
// redirect a write to another user's file.
bool probe_file(const std::wstring& path, bool& exists, std::wstring& error) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        const DWORD e = GetLastError();
        if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_NAME) {
            exists = false;
            return true;
        }
        error = L"无法检查配置文件，请检查访问权限：\n" + path;
        return false;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        error = L"配置文件路径实际上是文件夹：\n" + path;
        return false;
    }
    exists = true;
    return true;
}

std::wstring known_folder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &p))) return {};
    std::wstring result = p;
    CoTaskMemFree(p);
    return result;
}

std::wstring env(const wchar_t* name) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (!size) return {};
    std::vector<wchar_t> buf(size);
    const DWORD written = GetEnvironmentVariableW(name, buf.data(), size);
    return written && written < size ? std::wstring(buf.data(), written) : std::wstring();
}

std::wstring registry_string(HKEY root, const wchar_t* key, const wchar_t* name, REGSAM view) {
    HKEY h = nullptr;
    if (RegOpenKeyExW(root, key, 0, KEY_QUERY_VALUE | view, &h) != ERROR_SUCCESS) return {};
    DWORD type = 0, size = 0;
    LONG rc = RegQueryValueExW(h, name, nullptr, &type, nullptr, &size);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || size > 65536) {
        RegCloseKey(h);
        return {};
    }
    std::vector<wchar_t> data(size / sizeof(wchar_t) + 2, L'\0');
    rc = RegQueryValueExW(h, name, nullptr, &type, reinterpret_cast<BYTE*>(data.data()), &size);
    RegCloseKey(h);
    if (rc != ERROR_SUCCESS) return {};
    std::wstring result(data.data());
    if (type == REG_EXPAND_SZ) {
        const DWORD count = ExpandEnvironmentStringsW(result.c_str(), nullptr, 0);
        if (count) {
            std::vector<wchar_t> expanded(count);
            if (ExpandEnvironmentStringsW(result.c_str(), expanded.data(), count)) result = expanded.data();
        }
    }
    return result;
}

std::wstring valid_sandman(const std::wstring& path) {
    return !path.empty() && _wcsicmp(basename(path).c_str(), L"SandMan.exe") == 0 && file_exists(path) ? path : L"";
}

bool read_handle(HANDLE h, std::string& bytes) {
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > kMaxConfigSize) return false;
    bytes.assign(static_cast<size_t>(size.QuadPart), '\0');
    size_t done = 0;
    while (done < bytes.size()) {
        DWORD count = 0;
        const DWORD want = static_cast<DWORD>(std::min<size_t>(bytes.size() - done, 1024 * 1024));
        if (!ReadFile(h, &bytes[done], want, &count, nullptr) || count == 0) return false;
        done += count;
    }
    return true;
}

std::wstring system_error(DWORD code) {
    wchar_t* raw = nullptr;
    const DWORD len = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                      FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr, code, 0, reinterpret_cast<LPWSTR>(&raw), 0, nullptr);
    std::wstring text = len && raw ? std::wstring(raw, len) : L"未知错误";
    if (raw) LocalFree(raw);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ')) text.pop_back();
    return text + L" (" + std::to_wstring(code) + L")";
}

bool write_all(HANDLE h, const std::string& bytes) {
    size_t done = 0;
    while (done < bytes.size()) {
        DWORD count = 0;
        const DWORD want = static_cast<DWORD>(std::min<size_t>(bytes.size() - done, 1024 * 1024));
        if (!WriteFile(h, bytes.data() + done, want, &count, nullptr) || count == 0) return false;
        done += count;
    }
    return FlushFileBuffers(h) != 0;
}

// The temp file is in the same directory so ReplaceFileW is atomic and retains
// the target's security attributes. Never fall back to truncating the INI.
bool atomic_replace(const std::wstring& sandman, const std::wstring& path, const std::string& expected,
                    const std::string& bytes, std::wstring& error) {
    std::wstring temp;
    Handle* output = nullptr;
    for (unsigned int n = 0; n < 64; ++n) {
        temp = path + L".sbrs-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(n) + L".tmp";
        HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) { output = new Handle(h); break; }
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    if (!output) {
        error = L"无法在配置目录创建临时文件：" + system_error(GetLastError());
        return false;
    }
    const bool wrote = write_all(output->h, bytes);
    const DWORD write_error = wrote ? ERROR_SUCCESS : GetLastError();
    delete output; // close before ReplaceFileW
    if (!wrote) {
        DeleteFileW(temp.c_str());
        error = L"写入临时文件失败：" + system_error(write_error);
        return false;
    }
    std::string now;
    if (!read_config(path, now) || now != expected) {
        DeleteFileW(temp.c_str());
        error = L"配置在操作期间发生变化，已取消写入。请退出 Sandboxie Plus 后重试。";
        return false;
    }
    const ProcessState process = manager_state();
    const Location active = locate_config(sandman);
    if (!process.known || process.running || !active.ok || _wcsicmp(active.config.c_str(), path.c_str()) != 0) {
        DeleteFileW(temp.c_str());
        error = L"管理器已启动或实际配置路径发生变化，已取消写入。请退出 Sandboxie Plus 后重试。";
        return false;
    }
    if (!ReplaceFileW(path.c_str(), temp.c_str(), nullptr, 0, nullptr, nullptr)) {
        const DWORD e = GetLastError();
        DeleteFileW(temp.c_str());
        error = L"无法安全替换配置文件：" + system_error(e);
        return false;
    }
    return true;
}

}  // namespace

bool file_exists(const std::wstring& path) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool read_config(const std::wstring& path, std::string& bytes) {
    Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    return file.valid() && read_handle(file.h, bytes);
}

ProcessState manager_state() {
    ProcessState result;
    Handle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap.valid()) return result;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snap.h, &entry)) {
        result.known = GetLastError() == ERROR_NO_MORE_FILES;
        return result;
    }
    result.known = true;
    do {
        if (_wcsicmp(entry.szExeFile, L"SandMan.exe") != 0) continue;
        result.running = true;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID));
        if (process.valid()) {
            std::vector<wchar_t> buf(32768);
            DWORD count = static_cast<DWORD>(buf.size());
            if (QueryFullProcessImageNameW(process.h, 0, buf.data(), &count)) result.image.assign(buf.data(), count);
        }
        break;
    } while (Process32NextW(snap.h, &entry));
    return result;
}

std::wstring find_sandman() {
    const ProcessState state = manager_state();
    if (!valid_sandman(state.image).empty()) return state.image;

    std::vector<wchar_t> module(32768);
    const DWORD length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (length && length < module.size()) {
        std::wstring candidate = join(dirname(std::wstring(module.data(), length)), L"SandMan.exe");
        if (!valid_sandman(candidate).empty()) return candidate;
    }

    const wchar_t* uninstall = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Sandboxie-Plus_is1";
    for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
        for (REGSAM view : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) {
            std::wstring dir = registry_string(root, uninstall, L"InstallLocation", view);
            std::wstring candidate = join(dir, L"SandMan.exe");
            if (!dir.empty() && !valid_sandman(candidate).empty()) return candidate;
        }
    }
    std::wstring driver = registry_string(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\SbieDrv", L"ImagePath", 0);
    if (driver.compare(0, 4, L"\\??\\") == 0) driver.erase(0, 4);
    if (!driver.empty() && driver.front() == L'"') {
        const size_t end = driver.find(L'"', 1);
        if (end != std::wstring::npos) driver = driver.substr(1, end - 1);
    }
    if (!driver.empty()) {
        std::wstring candidate = join(dirname(driver), L"SandMan.exe");
        if (!valid_sandman(candidate).empty()) return candidate;
    }
    for (const wchar_t* variable : {L"ProgramFiles", L"ProgramFiles(x86)"}) {
        const std::wstring base = env(variable);
        if (base.empty()) continue;
        for (const wchar_t* dir : {L"Sandboxie-Plus", L"Sandboxie"}) {
            const std::wstring candidate = join(join(base, dir), L"SandMan.exe");
            if (!valid_sandman(candidate).empty()) return candidate;
        }
    }
    return {};
}

Location locate_config(const std::wstring& sandman) {
    Location result;
    if (valid_sandman(sandman).empty()) {
        result.message = L"未找到 Sandboxie Plus。请点击“选择程序”并选择 SandMan.exe（不支持 Classic）。";
        return result;
    }
    const std::wstring appdir = dirname(sandman);
    const std::wstring plusdata = join(appdir, L"PlusData");
    const DWORD plusdata_attr = GetFileAttributesW(plusdata.c_str());
    if (plusdata_attr == INVALID_FILE_ATTRIBUTES) {
        const DWORD e = GetLastError();
        if (e != ERROR_FILE_NOT_FOUND && e != ERROR_PATH_NOT_FOUND) {
            result.message = L"无法检查 PlusData 路径，请检查访问权限：\n" + plusdata;
            return result;
        }
    }
    // SandMan tests existence (not directory type) before choosing PlusData.
    const std::wstring portable_dir = plusdata_attr != INVALID_FILE_ATTRIBUTES ? plusdata : appdir;
    const std::wstring portable = join(portable_dir, kIniName);
    bool exists = false;
    if (!probe_file(portable, exists, result.message)) return result;
    if (exists) { result.ok = true; result.config = portable; return result; }

    const std::wstring common = known_folder(FOLDERID_ProgramData);
    const std::wstring local = known_folder(FOLDERID_LocalAppData);
    if (common.empty() || local.empty()) {
        result.message = L"无法定位当前用户或系统的 Sandboxie Plus 配置目录。";
        return result;
    }
    const std::wstring shared = join(join(join(common, L"Xanasoft"), L"Sandboxie-Plus"), kIniName);
    const std::wstring user = join(join(join(local, L"Xanasoft"), L"Sandboxie-Plus"), kIniName);
    if (!probe_file(shared, exists, result.message)) return result;
    if (exists) { result.ok = true; result.config = shared; return result; }

    // Recent SandMan migrates a legacy INI to Xanasoft on startup. Refuse to
    // guess before that migration, otherwise we'd write the wrong active INI.
    const std::wstring old_shared = join(join(common, L"Sandboxie-Plus"), kIniName);
    const std::wstring old_user = join(join(local, L"Sandboxie-Plus"), kIniName);
    bool legacy_shared = false, legacy_user = false, user_exists = false;
    if (!probe_file(old_shared, legacy_shared, result.message) ||
        !probe_file(old_user, legacy_user, result.message) ||
        !probe_file(user, user_exists, result.message)) return result;
    if (legacy_shared || (legacy_user && !user_exists)) {
        result.message = L"检测到旧版配置。请先启动当前版本的 Sandboxie Plus 一次以完成配置迁移，"
                         L"然后从托盘彻底退出，再重试。";
        return result;
    }
    if (user_exists) { result.ok = true; result.config = user; return result; }
    result.message = L"尚未找到 Sandboxie-Plus.ini。请先启动 Sandboxie Plus 一次，"
                     L"从托盘彻底退出后再重试。";
    return result;
}

std::wstring backup_path(const std::wstring& config) {
    return config + L".recovery-switch.bak";
}

Outcome apply(const std::wstring& sandman, bool restoring) {
    if (GetModuleHandleW(L"SbieDll.dll")) {
        return {2, L"请在沙盒外运行此工具，避免修改到沙盒内的配置副本。"};
    }
    Handle mutex(CreateMutexW(nullptr, FALSE, L"Local\\SandboxieRecoverySwitch_Config"));
    if (!mutex.valid()) return {2, L"无法创建操作锁，未修改配置。"};
    const DWORD wait = WaitForSingleObject(mutex.h, 0);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) return {2, L"另一个开关工具正在操作，请稍后重试。"};
    struct Unlock { HANDLE h; ~Unlock() { ReleaseMutex(h); } } unlock{mutex.h};

    const ProcessState process = manager_state();
    if (!process.known) return {3, L"无法检查 SandMan.exe 是否正在运行，未修改配置。"};
    if (process.running) return {3, L"Sandboxie Plus 管理器仍在运行。请从系统托盘菜单选择“退出”，"
                                      L"而不只是关闭窗口，然后再点击按钮。\n\n"
                                      L"请先保存工作：如果 Sandboxie 设置为退出时结束沙盒进程，退出管理器可能会结束它们。"
                                      L"本工具不会强制结束进程。"};
    Location loc = locate_config(sandman);
    if (!loc.ok) return {2, loc.message};

    // Deny new writers while taking the snapshot and replacing the file.
    Handle config(CreateFileW(loc.config.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    std::string current;
    if (!config.valid() || !read_handle(config.h, current)) {
        return {4, L"无法读取配置（文件可能被占用、超过 16 MB 或权限不足）：\n" + loc.config};
    }
    const std::wstring backup = backup_path(loc.config);
    bool exists = false;
    std::wstring error;
    if (!probe_file(backup, exists, error)) return {4, error};

    Result result;
    bool created_backup = false;
    if (restoring) {
        if (!exists) return {5, L"没有找到本工具创建的备份。未修改任何配置。\n\n预期备份路径：\n" + backup};
        std::string original;
        if (!read_config(backup, original)) return {4, L"无法读取备份，未修改配置：\n" + backup};
        result = recovery::restore(current, original);
    } else {
        result = recovery::disable(current);
        if (result.ok && result.text == current) {
            return {0, L"文件恢复提示已经处于关闭状态，且当前为高级视图。未修改配置。\n\n重新启动 Sandboxie Plus 后生效。"};
        }
        if (result.ok && !exists) {
            if (!CopyFileW(loc.config.c_str(), backup.c_str(), TRUE)) {
                return {4, L"无法创建备份，未修改配置：\n" + backup + L"\n\n" + system_error(GetLastError())};
            }
            created_backup = true;
            std::string copied;
            if (!read_config(backup, copied) || copied != current) {
                DeleteFileW(backup.c_str());
                return {4, L"备份校验失败，未修改配置。请重试。"};
            }
        } else if (result.ok) {
            std::string existing;
            if (!read_config(backup, existing) || !Ini(existing).valid()) {
                return {4, L"现有备份无法读取或格式无效。为保护原设置，未修改配置：\n" + backup};
            }
        }
    }
    if (!result.ok) return {6, L"配置或备份不是受支持的 UTF-8/ASCII INI 格式，或包含无效配置段；未修改配置。"};

    if (result.text != current && !atomic_replace(sandman, loc.config, current, result.text, error)) {
        if (created_backup && !DeleteFileW(backup.c_str())) {
            error += L"\n\n新备份未能移除：" + backup + L"\n请在重试前确认并手动移除它。";
        }
        return {4, error + L"\n\n如提示权限不足，请右键以管理员身份运行。"};
    }
    if (restoring) {
        if (!DeleteFileW(backup.c_str())) {
            return {4, L"设置已恢复，但旧备份未能移除。请确认后手动删除，否则下次操作会使用旧备份：\n" + backup};
        }
        std::wstring message = L"已恢复修改前的文件恢复设置；其他配置和沙盒文件未回滚。\n\n请重新启动 Sandboxie Plus。";
        if (result.view_preserved) message += L"\n\n检测到视图后来被更改，已保留当前视图。";
        return {0, message};
    }
    std::wstring message = L"已关闭 Sandboxie Plus 的文件恢复提示。请重新启动 Sandboxie Plus 后使用。"
                           L"\n\n清空或自动删除沙盒可能删除其中的文件，请先另存重要文件。";
    if (result.view_changed) message += L"\n\n为使该选项生效，管理器视图已切换到“高级视图”；恢复按钮可还原。";
    message += L"\n\n原配置备份：\n" + backup;
    return {0, message};
}

}  // namespace recovery

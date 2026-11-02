// Thin, dependency free wrappers around the bits of Windows we need.
#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "color.h"

namespace wpg {

enum class Hive { CurrentUser, LocalMachine };

struct RegAddress {
    Hive hive = Hive::CurrentUser;
    std::wstring subkey;
    std::wstring value;
};

// A captured registry value, good enough to restore the exact previous state.
struct ValueSnapshot {
    RegAddress addr;
    bool existed = false;
    bool isString = false;
    DWORD dword = 0;
    std::wstring text;

    bool sameAddress(const ValueSnapshot& other) const {
        return addr.hive == other.addr.hive && _wcsicmp(addr.subkey.c_str(), other.addr.subkey.c_str()) == 0 &&
               _wcsicmp(addr.value.c_str(), other.addr.value.c_str()) == 0;
    }
};

// Files and paths ----------------------------------------------------------
std::wstring appDataDir();
std::wstring backupsDir();
bool ensureDir(const std::wstring& path);
bool readTextFile(const std::wstring& path, std::wstring& out);
bool writeTextFile(const std::wstring& path, const std::wstring& text);
std::wstring exeDir();
std::wstring joinPath(const std::wstring& a, const std::wstring& b);
std::vector<std::wstring> listFiles(const std::wstring& dir, const std::wstring& extension);
bool removeFile(const std::wstring& path);
bool fileExists(const std::wstring& path);

// Registry ----------------------------------------------------------------
bool readRegDword(const RegAddress& addr, DWORD& out);  // false when absent
bool readRegString(const RegAddress& addr, std::wstring& out);
bool writeRegDword(const RegAddress& addr, DWORD value);
bool writeRegString(const RegAddress& addr, const std::wstring& value);
bool deleteRegValue(const RegAddress& addr);
ValueSnapshot snapshotValue(const RegAddress& addr);
bool restoreValue(const ValueSnapshot& snap);

// Environment -------------------------------------------------------------
bool isAdmin();
bool systemUsesDarkMode();
bool systemUsesDarkForApps();
int windowsBuild();
std::wstring windowsVersionName();
std::wstring systemLocaleName();
std::wstring localizedTimestamp(const std::wstring& format);
void openUrl(const std::wstring& url);
void openFolder(const std::wstring& path);

std::wstring wideFromUtf8(const std::string& s);
std::string utf8FromWide(const std::wstring& s);
std::wstring hex(uint32_t v, int digits);

// Wallpaper ---------------------------------------------------------------
// Path of the wallpaper currently painted on the desktop (also works for
// slideshows, where Windows hands out the cached bitmap of the active image).
std::wstring currentWallpaperPath();
// Picks the dominant, reasonably vivid color of an image.
bool sampleImageColor(const std::wstring& path, uint32_t& argbOut, Oklch& seedOut);

}  // namespace wpg

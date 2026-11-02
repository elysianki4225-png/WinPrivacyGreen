// WinPrivacyGreen - entry point
#include <windows.h>
#include <cwchar>
#include <objbase.h>
#include <shellapi.h>

#include "ui.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    // Per monitor v2 DPI awareness when available, otherwise system aware.
    using SetDpiCtx = BOOL(WINAPI*)(HANDLE);
    if (auto fn = (SetDpiCtx)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext")) {
        fn((HANDLE)-4);  // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    } else {
        SetProcessDPIAware();
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    bool forceLight = false;
    int argc = 0;
    if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc)) {
        for (int i = 1; i < argc; ++i) {
            if (_wcsicmp(argv[i], L"--light") == 0 || _wcsicmp(argv[i], L"/light") == 0) forceLight = true;
        }
        LocalFree(argv);
    }

    wpg::App app;
    app.forceLightTheme(forceLight);
    if (!app.create(instance)) {
        MessageBoxW(nullptr, L"WinPrivacyGreen failed to start.", L"WinPrivacyGreen", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }
    int result = app.run();
    CoUninitialize();
    return result;
}

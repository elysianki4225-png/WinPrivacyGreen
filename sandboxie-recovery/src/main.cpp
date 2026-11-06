#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include "ini.h"
#include "platform.h"

#include <string>
#include <vector>

namespace {
constexpr int kChoose = 101;
constexpr int kDisable = 102;
constexpr int kRestore = 103;
constexpr UINT_PTR kRefreshTimer = 1;

HINSTANCE instance;
HWND window, exe_edit, ini_edit, status_text, manager_text, backup_text;
HFONT title_font, body_font, small_font, button_font;
HBRUSH background;
int dpi = 96;
std::wstring selected_sandman;

int px(int n) { return MulDiv(n, dpi, 96); }

HFONT make_font(int size, int weight) {
    return CreateFontW(-px(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

HWND control(const wchar_t* cls, const wchar_t* text, DWORD style,
             int x, int y, int w, int h, int id, HFONT font, DWORD ex = 0) {
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style,
                             px(x), px(y), px(w), px(h), window,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return c;
}

void refresh() {
    if (selected_sandman.empty()) selected_sandman = recovery::find_sandman();
    SetWindowTextW(exe_edit, selected_sandman.empty() ? L"未找到，请手动选择 SandMan.exe" : selected_sandman.c_str());

    const recovery::ProcessState process = recovery::manager_state();
    if (!process.known) {
        SetWindowTextW(manager_text, L"无法检查管理器进程，请检查系统权限。");
    } else if (process.running) {
        SetWindowTextW(manager_text, L"管理器正在运行：请从系统托盘中彻底退出，再点击按钮。");
    } else {
        SetWindowTextW(manager_text, L"管理器已退出，可以安全修改设置。");
    }

    const recovery::Location loc = recovery::locate_config(selected_sandman);
    if (!loc.ok) {
        SetWindowTextW(ini_edit, loc.message.c_str());
        SetWindowTextW(status_text, L"配置状态：未找到可用配置");
        SetWindowTextW(backup_text, L"请选择程序并先启动 Sandboxie Plus 一次。");
        return;
    }
    SetWindowTextW(ini_edit, loc.config.c_str());
    std::string bytes;
    if (!recovery::read_config(loc.config, bytes)) {
        SetWindowTextW(status_text, L"配置状态：无法读取，请检查权限");
    } else {
        recovery::Ini ini(bytes);
        if (!ini.valid()) SetWindowTextW(status_text, L"配置状态：格式不受支持，未修改配置");
        else if (recovery::is_disabled(ini) && recovery::is_advanced(ini))
            SetWindowTextW(status_text, L"配置状态：文件恢复提示已关闭（重启后生效）");
        else if (recovery::is_disabled(ini))
            SetWindowTextW(status_text, L"配置状态：设置已关闭，但当前不是高级视图，尚不能生效");
        else SetWindowTextW(status_text, L"配置状态：文件恢复提示未关闭");
    }
    SetWindowTextW(backup_text, recovery::file_exists(recovery::backup_path(loc.config))
        ? L"已保存原配置备份，可使用“恢复之前设置”。"
        : L"首次修改前自动备份；不会删除或移动沙盒文件。");
}

void choose_sandman() {
    std::vector<wchar_t> path(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = L"Sandboxie Plus 管理器 (SandMan.exe)\0SandMan.exe\0程序文件 (*.exe)\0*.exe\0\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrTitle = L"请选择 Sandboxie Plus 的 SandMan.exe";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        selected_sandman = path.data();
        refresh();
        const auto loc = recovery::locate_config(selected_sandman);
        if (!loc.ok) MessageBoxW(window, loc.message.c_str(), L"Sandboxie 文件恢复开关", MB_OK | MB_ICONINFORMATION);
    }
}

void draw_button(const DRAWITEMSTRUCT* item) {
    HDC dc = item->hDC;
    RECT r = item->rcItem;
    const bool primary = item->CtlID == kDisable;
    COLORREF bg = primary ? RGB(32, 105, 68) : RGB(231, 239, 232);
    COLORREF fg = primary ? RGB(255, 255, 255) : RGB(36, 70, 48);
    if (item->itemState & ODS_DISABLED) { bg = RGB(214, 218, 214); fg = RGB(120, 126, 120); }
    else if (item->itemState & ODS_SELECTED) bg = primary ? RGB(24, 82, 52) : RGB(207, 223, 210);
    HBRUSH brush = CreateSolidBrush(bg);
    HPEN pen = CreatePen(PS_SOLID, 1, bg);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, px(12), px(12));
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
    wchar_t label[128]{};
    GetWindowTextW(item->hwndItem, label, static_cast<int>(sizeof(label) / sizeof(label[0])));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, fg);
    HGDIOBJ old_font = SelectObject(dc, button_font);
    DrawTextW(dc, label, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    SelectObject(dc, old_font);
    if (item->itemState & ODS_FOCUS) {
        InflateRect(&r, -px(5), -px(5));
        DrawFocusRect(dc, &r);
    }
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        window = hwnd;
        title_font = make_font(24, FW_SEMIBOLD);
        body_font = make_font(15, FW_NORMAL);
        small_font = make_font(13, FW_NORMAL);
        button_font = make_font(15, FW_SEMIBOLD);
        background = CreateSolidBrush(RGB(247, 250, 247));
        control(L"STATIC", L"Sandboxie 文件恢复开关", 0, 32, 24, 620, 38, 0, title_font);
        control(L"STATIC", L"一键关闭全局恢复提示 · 适用于 Sandboxie Plus", 0, 34, 69, 650, 24, 0, body_font);
        status_text = control(L"STATIC", L"正在检查配置…", 0, 34, 121, 650, 25, 0, button_font);
        manager_text = control(L"STATIC", L"", 0, 34, 153, 650, 24, 0, small_font);
        control(L"STATIC", L"Sandboxie Plus 程序", 0, 34, 196, 450, 22, 0, small_font);
        exe_edit = control(L"EDIT", L"", WS_BORDER | ES_READONLY | ES_AUTOHSCROLL | WS_TABSTOP,
                           34, 222, 491, 29, 0, small_font, WS_EX_CLIENTEDGE);
        control(L"BUTTON", L"选择程序…", WS_TABSTOP | BS_PUSHBUTTON,
                539, 221, 135, 31, kChoose, small_font);
        control(L"STATIC", L"实际配置文件", 0, 34, 265, 450, 22, 0, small_font);
        ini_edit = control(L"EDIT", L"", WS_BORDER | ES_READONLY | ES_AUTOHSCROLL | WS_TABSTOP,
                           34, 291, 640, 29, 0, small_font, WS_EX_CLIENTEDGE);
        control(L"BUTTON", L"一键关闭文件恢复", WS_TABSTOP | BS_OWNERDRAW,
                34, 345, 310, 50, kDisable, button_font);
        control(L"BUTTON", L"恢复之前设置", WS_TABSTOP | BS_OWNERDRAW,
                364, 345, 310, 50, kRestore, button_font);
        backup_text = control(L"STATIC", L"", 0, 34, 418, 640, 24, 0, small_font);
        control(L"STATIC", L"说明：只关闭自动恢复提示；手动快速恢复仍可使用。\r\n"
                           L"清空或自动删除沙盒可能删除其中的文件，请先另存重要文件。\r\n"
                           L"简洁/经典视图会切换为高级视图；切回后本选项可能失效。\r\n"
                           L"操作后请重新启动 Sandboxie Plus。\r\n"
                           L"退出管理器前请保存工作：Sandboxie 可能配置为退出时结束沙盒进程。",
                0, 34, 455, 640, 106, 0, small_font);
        SetTimer(hwnd, kRefreshTimer, 2000, nullptr);
        refresh();
        return 0;
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED) {
            if (LOWORD(wp) == kChoose) { choose_sandman(); return 0; }
            if (LOWORD(wp) == kDisable || LOWORD(wp) == kRestore) {
                const auto result = recovery::apply(selected_sandman, LOWORD(wp) == kRestore);
                refresh();
                MessageBoxW(hwnd, result.message.c_str(), L"Sandboxie 文件恢复开关",
                            MB_OK | (result.code == 0 ? MB_ICONINFORMATION : MB_ICONWARNING));
                return 0;
            }
        }
        break;
    case WM_DRAWITEM:
        if (wp == kDisable || wp == kRestore) { draw_button(reinterpret_cast<DRAWITEMSTRUCT*>(lp)); return TRUE; }
        break;
    case WM_ERASEBKGND: {
        RECT rect{};
        GetClientRect(hwnd, &rect);
        FillRect(reinterpret_cast<HDC>(wp), &rect, background);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetBkColor(dc, RGB(247, 250, 247));
        SetTextColor(dc, reinterpret_cast<HWND>(lp) == status_text ? RGB(29, 91, 57) : RGB(49, 63, 53));
        return reinterpret_cast<LRESULT>(background);
    }
    case WM_TIMER:
        if (wp == kRefreshTimer) refresh();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kRefreshTimer);
        DeleteObject(title_font);
        DeleteObject(body_font);
        DeleteObject(small_font);
        DeleteObject(button_font);
        DeleteObject(background);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int command_line(int argc, wchar_t** argv) {
    bool disable = false, restore = false, quiet = false, bad = false;
    std::wstring exe;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--disable") disable = true;
        else if (arg == L"--restore") restore = true;
        else if (arg == L"--quiet") quiet = true;
        else if (arg == L"--sandman" && i + 1 < argc) exe = argv[++i];
        else bad = true;
    }
    if (bad || disable == restore) {
        if (!quiet) MessageBoxW(nullptr, L"用法：--disable 或 --restore，可选 --sandman \"SandMan.exe 的完整路径\" 和 --quiet。",
                                L"Sandboxie 文件恢复开关", MB_OK | MB_ICONWARNING);
        return 64;
    }
    if (exe.empty()) exe = recovery::find_sandman();
    const auto result = recovery::apply(exe, restore);
    if (!quiet) MessageBoxW(nullptr, result.message.c_str(), L"Sandboxie 文件恢复开关",
                            MB_OK | (result.code == 0 ? MB_ICONINFORMATION : MB_ICONWARNING));
    return result.code;
}
}  // namespace

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE, PWSTR, int show) {
    instance = hinstance;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;
    if (argc > 1) {
        const int result = command_line(argc, argv);
        LocalFree(argv);
        return result;
    }
    LocalFree(argv);
    if (GetModuleHandleW(L"SbieDll.dll")) {
        MessageBoxW(nullptr, L"请在沙盒外运行此工具。", L"Sandboxie 文件恢复开关", MB_OK | MB_ICONWARNING);
        return 2;
    }
    HDC screen = GetDC(nullptr);
    if (screen) { dpi = GetDeviceCaps(screen, LOGPIXELSX); ReleaseDC(nullptr, screen); }
    if (dpi <= 0) dpi = 96;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = hinstance;
    wc.lpfnWndProc = wnd_proc;
    wc.lpszClassName = L"SandboxieRecoverySwitchWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hinstance, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&wc)) return 1;
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r{0, 0, px(708), px(580)};
    AdjustWindowRectEx(&r, style, FALSE, 0);
    const int width = r.right - r.left, height = r.bottom - r.top;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Sandboxie 文件恢复开关", style,
                                CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, hinstance, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return static_cast<int>(message.wParam);
}

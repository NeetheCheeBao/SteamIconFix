// main.cpp - 程序入口与窗口消息分发
#include "ui/App.h"

#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#include <cstdio>
#include <string>

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hwnd, ((CREATESTRUCTW*)lp)->hInstance);
        return 0;

    case WM_SIZE:
        Layout();
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        RECT r{0, 0, Scale(820), Scale(600)};
        AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, TRUE, 0);
        mmi->ptMinTrackSize.x = r.right - r.left;
        mmi->ptMinTrackSize.y = r.bottom - r.top;
        return 0;
    }

    case WM_DPICHANGED: {
        g.dpi = HIWORD(wp);
        if (g.dpi < 72) g.dpi = 96;
        g.scale = (float)g.dpi / 96.0f;
        HFONT old = g.font;
        g.font = CreateUiFont(g.dpi);
        EnumChildWindows(
            hwnd,
            [](HWND child, LPARAM p) -> BOOL {
                SendMessageW(child, WM_SETFONT, (WPARAM)p, TRUE);
                return TRUE;
            },
            (LPARAM)g.font);
        if (old) DeleteObject(old);
        RECT* sug = (RECT*)lp;
        SetWindowPos(hwnd, nullptr, sug->left, sug->top, sug->right - sug->left,
                     sug->bottom - sug->top, SWP_NOZORDER | SWP_NOACTIVATE);
        Layout();
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        HWND ctl = (HWND)lp;
        if (ctl == g.editLog) {
            SetBkColor(dc, GetSysColor(COLOR_WINDOW));
            SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);
        switch (id) {
        case IDC_BTN_STEAM:
            if (code == BN_CLICKED) BrowseInto(g.editSteam, true);
            return 0;
        case IDC_BTN_ICON:
            if (code == BN_CLICKED) BrowseInto(g.editIcon, false);
            return 0;
        case IDC_BTN_DESKTOP:
            if (code == BN_CLICKED) BrowseInto(g.editDesktop, false);
            return 0;
        case IDC_BTN_START:
        case IDOK:
            if (code == BN_CLICKED || id == IDOK) StartFix();
            return 0;
        case IDC_BTN_STOP:
            if (code == BN_CLICKED && g.running) {
                g.runner.Cancel();
                SetStatus(L"正在停止…");
                AppendLogLine(L"信息", L"已请求停止，等待当前下载结束");
            }
            return 0;
        case IDC_BTN_CLEAR:
            if (code == BN_CLICKED) SetWindowTextW(g.editLog, L"");
            return 0;
        case IDM_REFRESH:
            LoadEnvToUi();
            AppendLogLine(L"信息", L"已刷新环境信息");
            return 0;
        case IDM_OPEN_ICON_DIR: {
            std::wstring dir = GetText(g.editIcon);
            if (!dir.empty() && DirExists(dir)) {
                ShellExecuteW(hwnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            } else {
                MessageBoxW(hwnd, L"图标目录不存在。", L"Steam Icon Fix", MB_ICONWARNING);
            }
            return 0;
        }
        case IDM_CHECK_UPDATE:
            ShellExecuteW(hwnd, L"open", kReleasesUrl, nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        case IDM_ABOUT:
            ShowAbout();
            return 0;
        case IDM_EXIT:
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    }

    case kMsgLog: {
        auto* text = (std::wstring*)lp;
        if (text) {
            AppendLog(*text);
            delete text;
        }
        return 0;
    }

    case kMsgProg: {
        auto* p = (FixProgress*)lp;
        if (p) {
            int pct = p->total > 0 ? (p->current * 100) / p->total : 0;
            SendMessageW(g.progress, PBM_SETPOS, (WPARAM)pct, 0);
            wchar_t buf[32];
            _snwprintf(buf, 32, L"%d%%", pct);
            buf[31] = 0;
            SetWindowTextW(g.lblPercent, buf);
            if (!p->label.empty()) SetStatus(p->label.c_str());
            delete p;
        }
        return 0;
    }

    case kMsgDone: {
        auto* s = (FixSummary*)lp;
        if (s) {
            FinishRun(*s);
            delete s;
        }
        return 0;
    }

    case WM_CLOSE:
        if (g.running) {
            if (MessageBoxW(hwnd, L"修复任务正在运行，确定要退出吗？", L"Steam Icon Fix",
                            MB_ICONQUESTION | MB_YESNO) != IDYES) {
                return 0;
            }
            g.runner.Cancel();
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (g.font) {
            DeleteObject(g.font);
            g.font = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int APIENTRY wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SteamIconFixMain";
    wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 0, 0,
                                 LR_DEFAULTSIZE | LR_SHARED);
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;

    if (!RegisterClassExW(&wc)) return 1;

    int dpi = 96;
    HDC dc = GetDC(nullptr);
    if (dc) {
        dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(nullptr, dc);
    }
    if (dpi < 72) dpi = 96;

    RECT wr{0, 0, MulDiv(940, dpi, 96), MulDiv(700, dpi, 96)};
    AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, TRUE, 0);
    int ww = wr.right - wr.left;
    int wh = wr.bottom - wr.top;

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int x = work.left + ((work.right - work.left) - ww) / 2;
    int y = work.top + ((work.bottom - work.top) - wh) / 2;

    std::wstring title = std::wstring(L"SteamIconFix_") + APP_VERSION_WSTR;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, x, y,
                                ww, wh, nullptr, nullptr, inst, nullptr);
    if (!hwnd) {
        MessageBoxW(nullptr, L"窗口创建失败。", L"Steam Icon Fix", MB_ICONERROR);
        return 1;
    }

    g.accel = LoadAcceleratorsW(inst, MAKEINTRESOURCEW(IDR_ACCEL));

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (g.accel && TranslateAcceleratorW(hwnd, g.accel, &msg)) continue;
        if (IsDialogMessageW(hwnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (SUCCEEDED(hr)) CoUninitialize();
    return (int)msg.wParam;
}

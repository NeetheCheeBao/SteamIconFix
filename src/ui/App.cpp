// App.cpp - 全局状态、字体、日志输出、控件创建与布局
#include "ui/App.h"

#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#include <cstdio>
#include <string>

App g;

int Scale(int v) {
    return (int)(v * g.scale + 0.5f);
}

HFONT CreateUiFont(int dpi) {
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(ncm);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
        LOGFONTW lf = ncm.lfMessageFont;
        if (lf.lfHeight < 0) lf.lfHeight = -MulDiv(-lf.lfHeight, dpi, 96);
        return CreateFontIndirectW(&lf);
    }
    return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

void SetStatus(const wchar_t* text) {
    if (g.status) SendMessageW(g.status, SB_SETTEXTW, 0, (LPARAM)text);
}

void SetCounts(const wchar_t* text) {
    if (g.status) SendMessageW(g.status, SB_SETTEXTW, 1, (LPARAM)text);
}

void AppendLog(const std::wstring& text) {
    if (!g.editLog) return;

    if (GetWindowTextLengthW(g.editLog) > 120000) {
        SetWindowTextW(g.editLog, L"日志过长，已清空。\r\n");
    }

    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t line[4096];
    _snwprintf(line, 4096, L"%02d:%02d:%02d  %s\r\n", st.wHour, st.wMinute, st.wSecond,
               text.c_str());
    line[4095] = 0;

    int len = GetWindowTextLengthW(g.editLog);
    SendMessageW(g.editLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(g.editLog, EM_REPLACESEL, FALSE, (LPARAM)line);
    SendMessageW(g.editLog, EM_SCROLLCARET, 0, 0);
}

void AppendLogLine(const std::wstring& tag, const std::wstring& text) {
    AppendLog(L"[" + tag + L"] " + text);
}

void UpdateInfoLabel() {
    wchar_t buf[256];
    _snwprintf(buf, 256, L"已识别：桌面 %d 个快捷方式，开始菜单 %d 个，库内游戏 %d 个",
               g.env.desktopShortcuts, g.env.startMenuShortcuts, g.env.libraryGames);
    buf[255] = 0;
    SetWindowTextW(g.lblInfo, buf);
}

std::wstring GetText(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring s;
    if (len <= 0) return s;
    s.resize((size_t)len + 1);
    int got = GetWindowTextW(h, &s[0], len + 1);
    s.resize((size_t)(got > 0 ? got : 0));
    return s;
}

bool Checked(int id) {
    return IsDlgButtonChecked(g.hwnd, id) == BST_CHECKED;
}

void EnableConfig(bool enable) {
    HWND ctrls[] = {g.editSteam, g.btnSteam,   g.editIcon,      g.btnIcon,      g.editDesktop,
                    g.btnDesktop, g.radAuto,   g.radFastly,     g.radCf,        g.chkDesktop,
                    g.chkStart,   g.chkLib,    g.chkOverwrite,  g.chkFlush,     g.btnStart};
    for (HWND h : ctrls) {
        if (h) EnableWindow(h, enable ? TRUE : FALSE);
    }
    if (g.btnStop) EnableWindow(g.btnStop, enable ? FALSE : TRUE);
}

void CreateControls(HWND hwnd, HINSTANCE inst) {
        g.hwnd = hwnd;
        g.inst = inst;
        HDC dc = GetDC(hwnd);
        g.dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
        if (dc) ReleaseDC(hwnd, dc);
        if (g.dpi < 72) g.dpi = 96;
        g.scale = (float)g.dpi / 96.0f;
        g.font = CreateUiFont(g.dpi);

        auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style, DWORD ex, int id) {
            HWND h = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10,
                                     hwnd, (HMENU)(INT_PTR)id, g.inst, nullptr);
            if (h && g.font) SendMessageW(h, WM_SETFONT, (WPARAM)g.font, TRUE);
            return h;
        };

        g.grpEnv = mk(L"BUTTON", L"环境", BS_GROUPBOX, 0, IDC_GRP_ENV);
        g.lblSteam = mk(L"STATIC", L"Steam 安装目录:", SS_RIGHT, 0, IDC_LBL_STEAM);
        g.editSteam = mk(L"EDIT", L"", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, IDC_EDIT_STEAM);
        g.btnSteam = mk(L"BUTTON", L"浏览...", BS_PUSHBUTTON, 0, IDC_BTN_STEAM);
        g.lblIcon = mk(L"STATIC", L"图标缓存目录:", SS_RIGHT, 0, IDC_LBL_ICON);
        g.editIcon = mk(L"EDIT", L"", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, IDC_EDIT_ICON);
        g.btnIcon = mk(L"BUTTON", L"浏览...", BS_PUSHBUTTON, 0, IDC_BTN_ICON);
        g.lblDesktop = mk(L"STATIC", L"桌面目录:", SS_RIGHT, 0, IDC_LBL_DESKTOP);
        g.editDesktop = mk(L"EDIT", L"", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, IDC_EDIT_DESKTOP);
        g.btnDesktop = mk(L"BUTTON", L"浏览...", BS_PUSHBUTTON, 0, IDC_BTN_DESKTOP);
        g.lblInfo = mk(L"STATIC", L"", SS_LEFT, 0, IDC_LBL_INFO);

        g.grpSrc = mk(L"BUTTON", L"下载源", BS_GROUPBOX, 0, IDC_GRP_SRC);
        g.radAuto = mk(L"BUTTON", L"自动（推荐）", BS_AUTORADIOBUTTON | WS_GROUP, 0, IDC_RADIO_AUTO);
        g.radFastly = mk(L"BUTTON", L"Fastly", BS_AUTORADIOBUTTON, 0, IDC_RADIO_FASTLY);
        g.radCf = mk(L"BUTTON", L"Cloudflare", BS_AUTORADIOBUTTON, 0, IDC_RADIO_CF);
        g.lblSrcHint = mk(L"STATIC", L"自动：优先 Fastly，失败自动换源", SS_LEFT, 0,
                          IDC_LBL_SRC_HINT);

        g.grpOpt = mk(L"BUTTON", L"修复范围", BS_GROUPBOX, 0, IDC_GRP_OPT);
        g.chkDesktop = mk(L"BUTTON", L"修复桌面快捷方式", BS_AUTOCHECKBOX, 0, IDC_CHK_DESKTOP);
        g.chkStart = mk(L"BUTTON", L"修复开始菜单", BS_AUTOCHECKBOX, 0, IDC_CHK_START);
        g.chkLib = mk(L"BUTTON", L"修复库内游戏", BS_AUTOCHECKBOX, 0, IDC_CHK_LIB);
        g.chkOverwrite = mk(L"BUTTON", L"覆盖已有图标文件", BS_AUTOCHECKBOX, 0, IDC_CHK_OVERWRITE);
        g.chkFlush = mk(L"BUTTON", L"完成后刷新图标缓存", BS_AUTOCHECKBOX, 0, IDC_CHK_FLUSH);

        g.grpLog = mk(L"BUTTON", L"运行日志", BS_GROUPBOX, 0, IDC_GRP_LOG);
        g.editLog = mk(L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL |
                                         ES_NOHIDESEL,
                       WS_EX_CLIENTEDGE, IDC_EDIT_LOG);

        g.progress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                     0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_PROGRESS, g.inst,
                                     nullptr);
        g.lblPercent = mk(L"STATIC", L"0%", SS_LEFT, 0, IDC_LBL_PERCENT);
        g.btnClear = mk(L"BUTTON", L"清空日志", BS_PUSHBUTTON, 0, IDC_BTN_CLEAR);
        g.btnStop = mk(L"BUTTON", L"停止", BS_PUSHBUTTON, 0, IDC_BTN_STOP);
        g.btnStart = mk(L"BUTTON", L"开始修复(&S)", BS_DEFPUSHBUTTON, 0, IDC_BTN_START);

        g.status = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                   0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_STATUS, g.inst, nullptr);
        if (g.status) SendMessageW(g.status, WM_SETFONT, (WPARAM)g.font, TRUE);

        CheckDlgButton(hwnd, IDC_RADIO_AUTO, BST_CHECKED);
        CheckDlgButton(hwnd, IDC_CHK_DESKTOP, BST_CHECKED);
        CheckDlgButton(hwnd, IDC_CHK_START, BST_CHECKED);
        CheckDlgButton(hwnd, IDC_CHK_LIB, BST_CHECKED);
        CheckDlgButton(hwnd, IDC_CHK_FLUSH, BST_CHECKED);
        EnableWindow(g.btnStop, FALSE);

        LoadEnvToUi();
        Layout();

        SetStatus(L"就绪");
        SetCounts(L"成功 0，跳过 0，失败 0");
        AppendLogLine(L"信息", L"Steam Icon Fix " APP_VERSION_WSTR L" 已启动");
        if (g.env.steamFound) {
            AppendLogLine(L"信息", L"已找到 Steam：" + g.env.steamRoot);
        } else {
            AppendLogLine(L"警告", L"未检测到 Steam 安装目录，请手动选择");
        }
}

void Layout() {
    if (!g.hwnd) return;
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    if (W <= 0 || H <= 0) return;

    SendMessageW(g.status, WM_SIZE, 0, 0);
    RECT sr{};
    GetWindowRect(g.status, &sr);
    int statusH = sr.bottom - sr.top;
    int parts[2] = {W - Scale(300), -1};
    SendMessageW(g.status, SB_SETPARTS, 2, (LPARAM)parts);

    int m = Scale(12);
    int gap = Scale(10);
    int envH = Scale(142);
    int midH = Scale(156);
    int btnH = Scale(26);

    int y = m;

    MoveWindow(g.grpEnv, m, y, W - 2 * m, envH, TRUE);
    {
        int gx = m + Scale(12);
        int gw = W - 2 * m - Scale(24);
        int labelW = Scale(92);
        int browseW = Scale(84);
        int editX = gx + labelW + Scale(6);
        int editW = gw - labelW - Scale(6) - browseW - Scale(8);
        if (editW < Scale(80)) editW = Scale(80);

        int rows[3] = {Scale(26), Scale(56), Scale(86)};
        HWND labels[3] = {g.lblSteam, g.lblIcon, g.lblDesktop};
        HWND edits[3] = {g.editSteam, g.editIcon, g.editDesktop};
        HWND btns[3] = {g.btnSteam, g.btnIcon, g.btnDesktop};
        for (int i = 0; i < 3; ++i) {
            int ry = y + rows[i];
            MoveWindow(labels[i], gx, ry + Scale(2), labelW, Scale(20), TRUE);
            MoveWindow(edits[i], editX, ry, editW, Scale(22), TRUE);
            MoveWindow(btns[i], editX + editW + Scale(8), ry, browseW, Scale(22), TRUE);
        }
        MoveWindow(g.lblInfo, gx, y + Scale(112), gw, Scale(18), TRUE);
    }

    y += envH + gap;

    int srcW = Scale(252);
    MoveWindow(g.grpSrc, m, y, srcW, midH, TRUE);
    {
        int sx = m + Scale(12);
        int sw = srcW - Scale(24);
        MoveWindow(g.radAuto, sx, y + Scale(28), sw, Scale(20), TRUE);
        MoveWindow(g.radFastly, sx, y + Scale(54), sw, Scale(20), TRUE);
        MoveWindow(g.radCf, sx, y + Scale(80), sw, Scale(20), TRUE);
        MoveWindow(g.lblSrcHint, sx, y + Scale(108), sw, Scale(36), TRUE);
    }

    int optX = m + srcW + gap;
    int optW = W - m - optX;
    MoveWindow(g.grpOpt, optX, y, optW, midH, TRUE);
    {
        HWND chks[5] = {g.chkDesktop, g.chkStart, g.chkLib, g.chkOverwrite, g.chkFlush};
        for (int i = 0; i < 5; ++i) {
            MoveWindow(chks[i], optX + Scale(12), y + Scale(26) + i * Scale(26), optW - Scale(24),
                       Scale(20), TRUE);
        }
    }

    int btnY = H - statusH - gap - btnH;
    int startW = Scale(104), stopW = Scale(84), clearW = Scale(88);
    int rx = W - m;
    MoveWindow(g.btnStart, rx - startW, btnY, startW, btnH, TRUE);
    MoveWindow(g.btnStop, rx - startW - Scale(8) - stopW, btnY, stopW, btnH, TRUE);
    int clearX = rx - startW - Scale(8) - stopW - Scale(8) - clearW;
    MoveWindow(g.btnClear, clearX, btnY, clearW, btnH, TRUE);

    int pctW = Scale(52);
    int progW = clearX - Scale(12) - pctW - m;
    if (progW < Scale(120)) progW = Scale(120);
    int progH = Scale(18);
    MoveWindow(g.progress, m, btnY + (btnH - progH) / 2, progW, progH, TRUE);
    MoveWindow(g.lblPercent, m + progW + Scale(6), btnY + (btnH - Scale(20)) / 2, pctW, Scale(20),
               TRUE);

    int logY = y + midH + gap;
    int logH = btnY - gap - logY;
    if (logH < Scale(120)) logH = Scale(120);
    MoveWindow(g.grpLog, m, logY, W - 2 * m, logH, TRUE);
    MoveWindow(g.editLog, m + Scale(10), logY + Scale(24), W - 2 * m - Scale(20),
               logH - Scale(34), TRUE);
}


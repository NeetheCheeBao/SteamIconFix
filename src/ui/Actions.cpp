// Actions.cpp - 环境读取、选项收集、开始修复、浏览目录、关于
#include "ui/App.h"

#include <commctrl.h>
#include <objbase.h>
#include <shlobj.h>
#include <shellapi.h>

#include <string>

void LoadEnvToUi() {
    g.env = DetectEnv();
    SetWindowTextW(g.editSteam, g.env.steamRoot.c_str());
    SetWindowTextW(g.editIcon, g.env.iconDir.c_str());
    SetWindowTextW(g.editDesktop, g.env.desktopDir.c_str());
    UpdateInfoLabel();
}

// 只刷新统计数字，不动用户手动填写的路径
void RefreshCounts() {
    EnvInfo env = DetectEnv();
    g.env.desktopShortcuts = env.desktopShortcuts;
    g.env.startMenuShortcuts = env.startMenuShortcuts;
    g.env.libraryGames = env.libraryGames;
    UpdateInfoLabel();
}

FixOptions CollectOptions() {
    FixOptions o;
    o.steamRoot = GetText(g.editSteam);
    o.iconDir = GetText(g.editIcon);
    o.desktopDir = GetText(g.editDesktop);
    o.startMenuDir = g.env.startMenuDir;
    o.cdn = Checked(IDC_RADIO_CF) ? 2 : (Checked(IDC_RADIO_FASTLY) ? 1 : 0);
    o.fixDesktop = Checked(IDC_CHK_DESKTOP);
    o.fixStartMenu = Checked(IDC_CHK_START);
    o.fixLibrary = Checked(IDC_CHK_LIB);
    o.overwrite = Checked(IDC_CHK_OVERWRITE);
    o.flushCache = Checked(IDC_CHK_FLUSH);
    return o;
}

void FinishRun(const FixSummary& s) {
    g.running = false;
    EnableConfig(true);
    SetWindowTextW(g.btnStart, L"开始修复(&S)");
    SendMessageW(g.progress, PBM_SETPOS, (WPARAM)(s.total > 0 ? 100 : 0), 0);
    wchar_t pct[32];
    _snwprintf(pct, 32, L"%d%%", s.total > 0 ? 100 : 0);
    pct[31] = 0;
    SetWindowTextW(g.lblPercent, pct);

    wchar_t counts[128];
    _snwprintf(counts, 128, L"成功 %d，跳过 %d，失败 %d", s.ok, s.skipped, s.failed);
    counts[127] = 0;
    SetCounts(counts);

    if (s.cancelled) {
        SetStatus(L"已取消");
        AppendLogLine(L"取消", L"任务已取消");
    } else if (s.fatal) {
        SetStatus(L"已中止");
        AppendLogLine(L"错误", L"任务中止，请检查网络或更换下载源");
        MessageBoxW(g.hwnd,
                    L"修复过程被中止。\n\n常见原因：网络不可用、下载源被拦截。\n"
                    L"可以尝试切换下载源，或关闭代理/加速器后重试。",
                    L"Steam Icon Fix", MB_ICONWARNING);
    } else {
        SetStatus(L"完成");
        std::wstring line = std::wstring(L"成功 ") + std::to_wstring(s.ok) + L" 个，跳过 " +
                            std::to_wstring(s.skipped) + L" 个，失败 " +
                            std::to_wstring(s.failed) + L" 个";
        if (s.shortcutFixed > 0) {
            line += L"，修正快捷方式指向 " + std::to_wstring(s.shortcutFixed) + L" 个";
        }
        AppendLogLine(L"完成", line);
        std::wstring msg = L"修复完成。\n\n成功 " + std::to_wstring(s.ok) + L" 个\n跳过 " +
                           std::to_wstring(s.skipped) + L" 个\n失败 " +
                           std::to_wstring(s.failed) + L" 个";
        if (s.shortcutFixed > 0) {
            msg += L"\n修正快捷方式指向 " + std::to_wstring(s.shortcutFixed) + L" 个";
        }
        MessageBoxW(g.hwnd, msg.c_str(), L"Steam Icon Fix", MB_ICONINFORMATION);
    }

    RefreshCounts();
}

void StartFix() {
    if (g.running) return;

    FixOptions opt = CollectOptions();
    if (opt.iconDir.empty() || !DirExists(opt.iconDir)) {
        MessageBoxW(g.hwnd,
                    L"图标缓存目录无效。\n\n请选择 Steam 安装目录下的 steam\\games 文件夹，"
                    L"例如 C:\\Program Files (x86)\\Steam\\steam\\games。",
                    L"Steam Icon Fix", MB_ICONWARNING);
        return;
    }
    if (opt.fixDesktop && opt.desktopDir.empty()) {
        MessageBoxW(g.hwnd, L"未找到桌面目录，将跳过桌面快捷方式。", L"Steam Icon Fix",
                    MB_ICONINFORMATION);
    }

    g.running = true;
    EnableConfig(false);
    SetWindowTextW(g.btnStart, L"修复中…");
    SetStatus(L"正在准备…");
    SetCounts(L"成功 0，跳过 0，失败 0");
    SendMessageW(g.progress, PBM_SETRANGE32, 0, 100);
    SendMessageW(g.progress, PBM_SETPOS, 0, 0);
    SetWindowTextW(g.lblPercent, L"0%");
    AppendLogLine(L"开始", L"开始修复");

    HWND hwnd = g.hwnd;
    g.runner.Start(
        opt,
        [hwnd](LogLevel lvl, const std::wstring& text) {
            const wchar_t* tag = L"信息";
            switch (lvl) {
            case LogLevel::Detail: tag = L"详情"; break;
            case LogLevel::Success: tag = L"成功"; break;
            case LogLevel::Warn: tag = L"警告"; break;
            case LogLevel::Error: tag = L"错误"; break;
            default: break;
            }
            auto* payload = new std::wstring(std::wstring(L"[") + tag + L"] " + text);
            if (!PostMessageW(hwnd, kMsgLog, 0, (LPARAM)payload)) delete payload;
        },
        [hwnd](const FixProgress& p) {
            auto* payload = new FixProgress(p);
            if (!PostMessageW(hwnd, kMsgProg, 0, (LPARAM)payload)) delete payload;
        },
        [hwnd](const FixSummary& s) {
            auto* payload = new FixSummary(s);
            if (!PostMessageW(hwnd, kMsgDone, 0, (LPARAM)payload)) delete payload;
        });
}

bool PickFolder(const wchar_t* title, std::wstring& out) {
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg)))) {
        return false;
    }
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    if (title) dlg->SetTitle(title);

    bool ok = false;
    if (SUCCEEDED(dlg->Show(g.hwnd))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                out = path;
                CoTaskMemFree(path);
                ok = true;
            }
            item->Release();
        }
    }
    dlg->Release();
    return ok;
}

void BrowseInto(HWND edit, bool isSteamRoot) {
    std::wstring picked;
    if (!PickFolder(isSteamRoot ? L"选择 Steam 安装目录" : L"选择文件夹", picked)) return;
    SetWindowTextW(edit, picked.c_str());
    if (isSteamRoot) {
        std::wstring games = JoinPath(picked, L"steam\\games");
        if (DirExists(games)) {
            SetWindowTextW(g.editIcon, games.c_str());
        } else {
            std::wstring games2 = JoinPath(picked, L"games");
            if (DirExists(games2)) SetWindowTextW(g.editIcon, games2.c_str());
        }
    }
}

namespace {

// 点击关于对话框里的超链接
HRESULT CALLBACK AboutCallback(HWND, UINT msg, WPARAM, LPARAM lp, LONG_PTR) {
    if (msg == TDN_HYPERLINK_CLICKED) {
        auto url = reinterpret_cast<LPCWSTR>(lp);
        if (url && *url) ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    }
    return S_OK;
}

} // namespace

void ShowAbout() {
    std::wstring content = std::wstring(L"版本 ") + APP_VERSION_WSTR +
                           L"\n\n修复 Steam 桌面与开始菜单快捷方式显示为空白图标的问题。\n"
                           L"图标文件会写入 Steam 安装目录的 steam\\games 下。\n\n"
                           L"图标数据来自 Steam CDN 与 api.steamcmd.net。\n\n";
    content += std::wstring(L"项目地址：<a href=\"") + kProjectUrl + L"\">" + kProjectUrl + L"</a>";

    TASKDIALOGCONFIG cfg{};
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = g.hwnd;
    cfg.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION |
                  TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT;
    cfg.pszWindowTitle = L"关于 Steam Icon Fix";
    cfg.pszMainIcon = TD_INFORMATION_ICON;
    cfg.pszMainInstruction = L"Steam Icon Fix";
    cfg.pszContent = content.c_str();
    cfg.dwCommonButtons = TDCBF_OK_BUTTON;
    cfg.pfCallback = AboutCallback;

    if (FAILED(TaskDialogIndirect(&cfg, nullptr, nullptr, nullptr))) {
        // 极老系统上没有 comctl32 v6 时退回到普通消息框
        MessageBoxW(g.hwnd, content.c_str(), L"关于 Steam Icon Fix", MB_ICONINFORMATION);
    }
}


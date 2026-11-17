// core.h - Steam Icon Fix 核心逻辑
#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif

#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <functional>
#include <atomic>

namespace sif {

enum class LogLevel { Info, Detail, Success, Warn, Error };

// 基础工具
std::wstring Utf8ToWide(const std::string& s);
std::string  WideToUtf8(const std::wstring& s);
std::wstring JoinPath(const std::wstring& dir, const std::wstring& name);
std::wstring FileNameOf(const std::wstring& path);
std::wstring DirNameOf(const std::wstring& path);
bool PathExists(const std::wstring& path);
bool DirExists(const std::wstring& path);
bool FileExists(const std::wstring& path);
long long FileSizeOf(const std::wstring& path);
std::wstring ReadAllText(const std::wstring& path);
std::wstring KnownFolder(REFKNOWNFOLDERID id);
std::wstring SteamInstallPath();
bool RunHidden(const std::wstring& cmdline);
std::wstring NowTimeString();

// 环境探测
struct EnvInfo {
    bool steamFound = false;
    std::wstring steamRoot;        // Steam 安装根目录
    std::wstring iconDir;          // Steam\steam\games
    std::wstring desktopDir;       // 桌面
    std::wstring startMenuRoot;    // 开始菜单 Programs
    std::wstring startMenuDir;     // Programs\Steam
    int desktopShortcuts = 0;      // 桌面上的 Steam 快捷方式数
    int startMenuShortcuts = 0;    // 开始菜单里的 Steam 快捷方式数
    int libraryGames = 0;          // 库内游戏数（appmanifest 数量）
    int libraryCount = 0;          // 库文件夹数量
};
EnvInfo DetectEnv();

// 修复任务
struct FixOptions {
    std::wstring steamRoot;
    std::wstring iconDir;
    std::wstring desktopDir;
    std::wstring startMenuDir;
    int  cdn = 0;              // 0 自动, 1 Fastly, 2 Cloudflare
    bool fixDesktop = true;
    bool fixStartMenu = true;
    bool fixLibrary = true;
    bool overwrite = false;
    bool flushCache = true;
};

struct FixProgress {
    int current = 0;
    int total = 0;
    std::wstring label;
};

struct FixSummary {
    int ok = 0;
    int skipped = 0;
    int failed = 0;
    int total = 0;
    int shortcutFixed = 0;
    bool cancelled = false;
    bool fatal = false;
};

using LogFn = std::function<void(LogLevel, const std::wstring&)>;
using ProgressFn = std::function<void(const FixProgress&)>;
using DoneFn = std::function<void(const FixSummary&)>;

class FixRunner {
public:
    FixRunner() = default;
    ~FixRunner();
    FixRunner(const FixRunner&) = delete;
    FixRunner& operator=(const FixRunner&) = delete;

    void Start(const FixOptions& opt, LogFn log, ProgressFn prog, DoneFn done);
    void Cancel();
    bool Running() const { return m_running.load(); }

private:
    std::atomic<bool> m_cancel{false};
    std::atomic<bool> m_running{false};
    HANDLE m_thread = nullptr;
};

} // namespace sif

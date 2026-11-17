// Fix.cpp - 图标下载流程、缓存刷新、任务线程
#include "core.h"
#include "Env.h"
#include "Net.h"

#include <shlobj.h>
#include <shellapi.h>
#include <process.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace sif {

namespace {

// 图标缓存
int DeleteCachePattern(const std::wstring& dir, const std::wstring& pattern) {
    if (!DirExists(dir)) return 0;
    int n = 0;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(JoinPath(dir, pattern).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring p = JoinPath(dir, fd.cFileName);
        SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL);
        if (DeleteFileW(p.c_str())) ++n;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

const wchar_t* kCdnFastly = L"https://shared.fastly.steamstatic.com/community_assets/images/apps/";
const wchar_t* kCdnCloudflare = L"https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/";

} // namespace

// 修复任务
namespace {

struct JobCtx {
    FixOptions opt;
    LogFn log;
    ProgressFn prog;
    DoneFn done;
    std::atomic<bool>* cancel;
    std::atomic<bool>* running;
};

void DoFlushIconCache(const LogFn& log) {
    log(LogLevel::Info, L"正在刷新系统图标缓存…");

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    log(LogLevel::Detail, L"结束 explorer.exe");
    RunHidden(L"taskkill /f /im explorer.exe");
    Sleep(400);

    std::wstring local = KnownFolder(FOLDERID_LocalAppData);
    std::wstring explorerDir = JoinPath(local, L"Microsoft\\Windows\\Explorer");
    int deleted = 0;
    deleted += DeleteCachePattern(explorerDir, L"iconcache_*.db");
    deleted += DeleteCachePattern(explorerDir, L"thumbcache_*.db");
    deleted += DeleteCachePattern(local, L"iconcache_*.db");
    deleted += DeleteCachePattern(local, L"thumbcache_*.db");

    std::wstring legacy = JoinPath(local, L"IconCache.db");
    if (FileExists(legacy)) {
        SetFileAttributesW(legacy.c_str(), FILE_ATTRIBUTE_NORMAL);
        if (DeleteFileW(legacy.c_str())) ++deleted;
    }

    log(LogLevel::Detail, L"已删除 " + std::to_wstring(deleted) + L" 个缓存文件");

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    wchar_t exe[] = L"explorer.exe";
    if (CreateProcessW(nullptr, exe, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        log(LogLevel::Detail, L"资源管理器已重启");
    } else {
        log(LogLevel::Warn, L"资源管理器启动失败，请手动重启或注销后查看");
    }
}

void RunFix(JobCtx* c) {
    const FixOptions& opt = c->opt;
    const LogFn& log = c->log;
    FixSummary sum;

    auto cancelled = [&]() { return c->cancel->load(); };
    auto report = [&](const std::wstring& label) {
        FixProgress p;
        p.current = sum.ok + sum.skipped + sum.failed;
        p.total = sum.total;
        p.label = label;
        if (c->prog) c->prog(p);
    };

    log(LogLevel::Info, L"开始修复 · " + std::wstring(opt.cdn == 0 ? L"自动选择下载源"
                                                     : opt.cdn == 1 ? L"Fastly"
                                                                    : L"Cloudflare"));

    if (!DirExists(opt.iconDir)) {
        log(LogLevel::Error, L"图标目录不存在：" + opt.iconDir);
        log(LogLevel::Error, L"请点击「浏览」指向 Steam 安装目录\\steam\\games");
        sum.fatal = true;
        if (c->done) c->done(sum);
        return;
    }
    log(LogLevel::Detail, L"图标目录 " + opt.iconDir);

    // 1. 扫描快捷方式
    std::vector<ShortcutInfo> shortcuts;
    if (opt.fixDesktop) {
        for (const auto& f : FindUrlFiles(opt.desktopDir)) {
            ShortcutInfo info;
            if (ParseSteamShortcut(f, info)) shortcuts.push_back(info);
        }
        log(LogLevel::Detail, L"桌面 Steam 快捷方式 " + std::to_wstring(shortcuts.size()) + L" 个");
    }
    if (opt.fixStartMenu && DirExists(opt.startMenuDir)) {
        size_t before = shortcuts.size();
        for (const auto& f : FindUrlFiles(opt.startMenuDir)) {
            ShortcutInfo info;
            if (ParseSteamShortcut(f, info)) shortcuts.push_back(info);
        }
        log(LogLevel::Detail, L"开始菜单 Steam 快捷方式 " +
                                  std::to_wstring(shortcuts.size() - before) + L" 个");
    }

    // appid -> 图标文件名（来自快捷方式的 IconFile）
    std::map<std::string, std::wstring> iconFromShortcut;
    std::map<std::string, std::vector<std::wstring>> shortcutFiles;
    std::vector<std::string> appIds;
    for (const auto& s : shortcuts) {
        std::string id = WideToUtf8(s.appId);
        if (id.empty()) continue;
        appIds.push_back(id);
        shortcutFiles[id].push_back(s.file);
        std::wstring iconName = s.iconFile;
        size_t p = iconName.find_last_of(L"\\/");
        if (p != std::wstring::npos) iconName = iconName.substr(p + 1);
        if (!iconName.empty() && !iconFromShortcut.count(id)) iconFromShortcut[id] = iconName;
    }

    // 2. 库内游戏
    if (opt.fixLibrary) {
        LibraryScan scan = ScanLibraries(opt.steamRoot);
        if (scan.appIds.empty()) {
            log(LogLevel::Warn, L"未在 Steam 库中找到游戏（appmanifest_*.acf）");
        } else {
            log(LogLevel::Detail, L"Steam 库内游戏 " + std::to_wstring(scan.appIds.size()) + L" 个，共 " +
                                      std::to_wstring(scan.folders.size()) + L" 个库目录");
        }
        appIds.insert(appIds.end(), scan.appIds.begin(), scan.appIds.end());
    }

    std::sort(appIds.begin(), appIds.end());
    appIds.erase(std::unique(appIds.begin(), appIds.end()), appIds.end());
    sum.total = (int)appIds.size();

    if (sum.total == 0) {
        log(LogLevel::Warn, L"没有需要处理的游戏。请检查快捷方式或勾选「修复库内游戏」");
        if (c->done) c->done(sum);
        return;
    }
    log(LogLevel::Info, L"待处理 " + std::to_wstring(sum.total) + L" 个游戏，开始下载图标");

    // 不少快捷方式的 IconFile 是空的，或者指向已经不存在的位置。图标下载到 games 目录后
    // 这些快捷方式依然显示白图标，所以要把 IconFile 改写成实际文件路径。
    auto fixShortcutIcon = [&](const std::string& appId, const std::wstring& iconName) {
        if (iconName.empty()) return;
        auto files = shortcutFiles.find(appId);
        if (files == shortcutFiles.end()) return;
        std::wstring target = JoinPath(opt.iconDir, iconName);
        if (FileSizeOf(target) <= 0) return;

        for (const auto& file : files->second) {
            wchar_t cur[MAX_PATH * 2] = {};
            GetPrivateProfileStringW(L"InternetShortcut", L"IconFile", L"", cur, MAX_PATH * 2,
                                     file.c_str());
            if (_wcsicmp(cur, target.c_str()) == 0) continue;

            if (WritePrivateProfileStringW(L"InternetShortcut", L"IconFile", target.c_str(),
                                           file.c_str())) {
                WritePrivateProfileStringW(L"InternetShortcut", L"IconIndex", L"0", file.c_str());
                ++sum.shortcutFixed;
                log(LogLevel::Detail, L"已修正快捷方式图标指向：" + FileNameOf(file));
            } else {
                log(LogLevel::Warn, L"无法写入快捷方式（只读或权限不足）：" + FileNameOf(file));
            }
        }
    };

    // 3. 逐个下载
    int consecutiveFail = 0;
    for (size_t i = 0; i < appIds.size(); ++i) {
        if (cancelled()) {
            sum.cancelled = true;
            break;
        }
        const std::string& appId = appIds[i];
        std::wstring wideId = Utf8ToWide(appId);
        report(L"处理 " + wideId + L"  (" + std::to_wstring(i + 1) + L"/" +
               std::to_wstring(appIds.size()) + L")");

        // 3.1 图标文件名
        std::wstring iconName;
        auto it = iconFromShortcut.find(appId);
        if (it != iconFromShortcut.end()) {
            iconName = it->second;
        } else {
            std::string body;
            HttpResult r;
            HttpGet(L"https://api.steamcmd.net/v1/info/" + wideId, &body, nullptr, r);
            if (!r.ok) {
                log(LogLevel::Error, L"查询 " + wideId + L" 图标信息失败：" + Utf8ToWide(r.err));
                ++sum.failed;
                if (++consecutiveFail >= 3) {
                    log(LogLevel::Error, L"连续 3 次网络失败，已中止。可切换下载源、关闭加速器/代理后重试");
                    sum.fatal = true;
                    break;
                }
                continue;
            }
            std::string hash = JsonStringField(body, "clienticon");
            if (hash.empty()) {
                log(LogLevel::Warn, L"跳过 " + wideId + L"：接口未返回 clienticon（可能是工具/非游戏应用）");
                ++sum.skipped;
                continue;
            }
            iconName = Utf8ToWide(hash);
            // 接口只返回哈希，CDN 上的实际文件名带 .ico 扩展名
            if (iconName.find(L'.') == std::wstring::npos) iconName += L".ico";
        }

        if (iconName.empty()) {
            log(LogLevel::Warn, L"跳过 " + wideId + L"：图标文件名为空");
            ++sum.skipped;
            continue;
        }

        // 3.2 目标路径
        std::wstring dst = JoinPath(opt.iconDir, iconName);
        if (!opt.overwrite && FileSizeOf(dst) > 0) {
            log(LogLevel::Detail, L"跳过 " + wideId + L"：图标已存在 " + iconName);
            ++sum.skipped;
            consecutiveFail = 0;
            fixShortcutIcon(appId, iconName);
            continue;
        }

        // 3.3 下载（自动模式会回退到另一个 CDN）
        std::vector<std::wstring> bases;
        if (opt.cdn == 1) bases.push_back(kCdnFastly);
        else if (opt.cdn == 2) bases.push_back(kCdnCloudflare);
        else { bases.push_back(kCdnFastly); bases.push_back(kCdnCloudflare); }

        bool success = false;
        std::string lastErr;
        int lastStatus = 0;
        for (size_t b = 0; b < bases.size() && !success; ++b) {
            std::wstring url = bases[b] + wideId + L"/" + iconName;
            HANDLE hFile = CreateFileW(dst.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFile == INVALID_HANDLE_VALUE) {
                lastErr = "无法写入图标目录（权限不足？）";
                log(LogLevel::Error, L"写入失败：" + dst + L"，请尝试以管理员身份运行");
                break;
            }
            HttpResult r;
            HttpGet(url, nullptr, hFile, r);
            CloseHandle(hFile);

            if (r.ok) {
                success = true;
            } else {
                lastErr = r.err;
                lastStatus = r.status;
                DeleteFileW(dst.c_str());
                if (b + 1 < bases.size() && (r.status == 0 || r.status >= 500)) {
                    log(LogLevel::Detail, L"源不可用，切换备用下载源重试 " + wideId);
                }
            }
        }

        if (success) {
            ++sum.ok;
            consecutiveFail = 0;
            log(LogLevel::Success, L"已修复 " + wideId + L"  →  " + iconName);
        } else {
            ++sum.failed;
            std::wstring reason = Utf8ToWide(lastErr);
            if (lastStatus == 404)
                log(LogLevel::Warn, L"跳过 " + wideId + L"：服务器上没有这个图标（404）");
            else
                log(LogLevel::Error, L"下载失败 " + wideId + L"：" + reason);
            if (lastStatus == 0 || lastStatus >= 500) {
                if (++consecutiveFail >= 3) {
                    log(LogLevel::Error, L"连续 3 次网络失败，已中止。可切换下载源、关闭加速器/代理后重试");
                    sum.fatal = true;
                    break;
                }
            }
        }

        fixShortcutIcon(appId, iconName);
    }

    // 快捷方式被改写后通知外壳刷新，不必重启资源管理器也能生效
    if (sum.shortcutFixed > 0) {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

    // 4. 刷新缓存
    if (!sum.cancelled && !sum.fatal && opt.flushCache) {
        DoFlushIconCache(log);
    }

    if (c->done) c->done(sum);
}

unsigned __stdcall JobThunk(void* p) {
    JobCtx* c = static_cast<JobCtx*>(p);
    c->cancel->store(false);
    RunFix(c);
    c->cancel->store(false);
    if (c->running) c->running->store(false);
    delete c;
    return 0;
}

} // namespace

void FixRunner::Start(const FixOptions& opt, LogFn log, ProgressFn prog, DoneFn done) {
    if (m_running.load()) return;
    if (m_thread) {
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
    m_cancel.store(false);
    m_running.store(true);

    JobCtx* ctx = new JobCtx();
    ctx->opt = opt;
    ctx->log = std::move(log);
    ctx->prog = std::move(prog);
    ctx->done = std::move(done);
    ctx->cancel = &m_cancel;
    ctx->running = &m_running;

    unsigned tid = 0;
    m_thread = (HANDLE)_beginthreadex(nullptr, 0, JobThunk, ctx, 0, &tid);
    if (!m_thread) {
        m_running.store(false);
        delete ctx;
    }
}

void FixRunner::Cancel() {
    m_cancel.store(true);
}

FixRunner::~FixRunner() {
    m_cancel.store(true);
    if (m_thread) {
        WaitForSingleObject(m_thread, 5000);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
}

} // namespace sif

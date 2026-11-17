// Env.cpp - 快捷方式解析、库目录扫描、环境探测
#include "Env.h"

#include <algorithm>
#include <string>
#include <vector>

namespace sif {

namespace {

// 收集 VDF/ACF 中 "key" "value" 形式的取值
void CollectQuoted(const std::wstring& text, const std::wstring& key, std::vector<std::wstring>& out) {
    std::wstring pat = L"\"" + key + L"\"";
    size_t p = 0;
    while ((p = text.find(pat, p)) != std::wstring::npos) {
        size_t q = p + pat.size();
        size_t colon = text.find(L':', q);
        size_t quote1 = text.find(L'"', q);
        if (quote1 == std::wstring::npos) break;
        // 允许 "key" "value" 这种省略冒号的写法
        if (colon != std::wstring::npos && colon < quote1) quote1 = text.find(L'"', colon);
        if (quote1 == std::wstring::npos) break;
        size_t quote2 = text.find(L'"', quote1 + 1);
        if (quote2 == std::wstring::npos) break;
        out.push_back(text.substr(quote1 + 1, quote2 - quote1 - 1));
        p = quote2 + 1;
    }
}

} // namespace

std::vector<std::wstring> FindUrlFiles(const std::wstring& dir) {
    std::vector<std::wstring> files;
    if (!DirExists(dir)) return files;

    WIN32_FIND_DATAW fd{};
    std::wstring pattern = JoinPath(dir, L"*.url");
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return files;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        files.push_back(JoinPath(dir, fd.cFileName));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return files;
}

bool ParseSteamShortcut(const std::wstring& file, ShortcutInfo& info) {
    wchar_t urlBuf[2048] = {};
    wchar_t iconBuf[MAX_PATH * 2] = {};
    GetPrivateProfileStringW(L"InternetShortcut", L"URL", L"", urlBuf, 2048, file.c_str());
    GetPrivateProfileStringW(L"InternetShortcut", L"IconFile", L"", iconBuf, (DWORD)(MAX_PATH * 2),
                             file.c_str());

    std::wstring url = urlBuf;
    if (url.empty()) return false;
    if (url.size() < 6 || _wcsnicmp(url.c_str(), L"steam:", 6) != 0) return false;

    size_t slash = url.find_last_of(L'/');
    if (slash == std::wstring::npos || slash + 1 >= url.size()) return false;

    info.file = file;
    info.url = url;
    info.iconFile = iconBuf;
    info.appId = url.substr(slash + 1);
    // 去掉可能存在的查询串
    size_t q = info.appId.find_first_of(L"?&#");
    if (q != std::wstring::npos) info.appId = info.appId.substr(0, q);
    return !info.appId.empty();
}

LibraryScan ScanLibraries(const std::wstring& steamRoot) {
    LibraryScan out;
    std::wstring steamapps = JoinPath(steamRoot, L"steamapps");
    std::vector<std::wstring> libs;
    libs.push_back(steamapps);

    std::wstring vdf = ReadAllText(JoinPath(steamapps, L"libraryfolders.vdf"));
    if (!vdf.empty()) {
        std::vector<std::wstring> paths;
        CollectQuoted(vdf, L"path", paths);
        for (const auto& p : paths) {
            std::wstring lib = p;
            if (!lib.empty() && lib.back() != L'\\') lib += L'\\';
            lib += L"steamapps";
            libs.push_back(lib);
        }
    }

    std::sort(libs.begin(), libs.end());
    libs.erase(std::unique(libs.begin(), libs.end()), libs.end());

    for (const auto& lib : libs) {
        if (!DirExists(lib)) continue;
        bool hasAny = false;
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW(JoinPath(lib, L"appmanifest_*.acf").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                hasAny = true;
                std::wstring text = ReadAllText(JoinPath(lib, fd.cFileName));
                if (text.empty()) continue;
                std::vector<std::wstring> ids;
                CollectQuoted(text, L"appid", ids);
                if (!ids.empty()) {
                    std::string id = WideToUtf8(ids[0]);
                    if (!id.empty() && id != "0") out.appIds.push_back(id);
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        if (hasAny) out.folders.push_back(lib);
    }

    std::sort(out.appIds.begin(), out.appIds.end());
    out.appIds.erase(std::unique(out.appIds.begin(), out.appIds.end()), out.appIds.end());
    return out;
}

int CountSteamShortcuts(const std::wstring& dir) {
    int n = 0;
    for (const auto& f : FindUrlFiles(dir)) {
        ShortcutInfo info;
        if (ParseSteamShortcut(f, info)) ++n;
    }
    return n;
}

// 环境探测
EnvInfo DetectEnv() {
    EnvInfo env;
    env.steamRoot = SteamInstallPath();
    env.steamFound = !env.steamRoot.empty();
    env.iconDir = env.steamFound ? JoinPath(env.steamRoot, L"steam\\games") : L"";
    env.desktopDir = KnownFolder(FOLDERID_Desktop);
    env.startMenuRoot = KnownFolder(FOLDERID_Programs);
    if (!env.startMenuRoot.empty()) env.startMenuDir = JoinPath(env.startMenuRoot, L"Steam");

    if (!env.desktopDir.empty()) env.desktopShortcuts = CountSteamShortcuts(env.desktopDir);
    if (!env.startMenuDir.empty()) env.startMenuShortcuts = CountSteamShortcuts(env.startMenuDir);
    if (env.steamFound) {
        LibraryScan scan = ScanLibraries(env.steamRoot);
        env.libraryGames = (int)scan.appIds.size();
        env.libraryCount = (int)scan.folders.size();
    }
    return env;
}

} // namespace sif

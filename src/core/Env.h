// Env.h - 快捷方式与 Steam 库扫描
#pragma once

#include "core.h"

#include <string>
#include <vector>

namespace sif {

struct ShortcutInfo {
    std::wstring file;
    std::wstring url;
    std::wstring iconFile;
    std::wstring appId;
};

// 目录下的 .url 文件列表
std::vector<std::wstring> FindUrlFiles(const std::wstring& dir);

// 解析 Steam 快捷方式，非 Steam 快捷方式返回 false
bool ParseSteamShortcut(const std::wstring& file, ShortcutInfo& info);

struct LibraryScan {
    std::vector<std::wstring> folders;
    std::vector<std::string> appIds;
};

LibraryScan ScanLibraries(const std::wstring& steamRoot);
int CountSteamShortcuts(const std::wstring& dir);

} // namespace sif

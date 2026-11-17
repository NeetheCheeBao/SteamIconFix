// Util.cpp - 字符串、路径、注册表、进程与时间等基础工具
#include "core.h"

#include <shlobj.h>

#include <cstdio>
#include <string>

namespace sif {

// 字符串 / 路径
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out;
    out.resize((size_t)len);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}

std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string out;
    out.resize((size_t)len);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len, nullptr, nullptr);
    return out;
}

std::wstring JoinPath(const std::wstring& dir, const std::wstring& name) {
    if (dir.empty()) return name;
    std::wstring out = dir;
    if (out.back() != L'\\' && out.back() != L'/') out += L'\\';
    out += name;
    return out;
}

std::wstring FileNameOf(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? path : path.substr(p + 1);
}

std::wstring DirNameOf(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? std::wstring() : path.substr(0, p);
}

bool PathExists(const std::wstring& path) {
    if (path.empty()) return false;
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool DirExists(const std::wstring& path) {
    if (path.empty()) return false;
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool FileExists(const std::wstring& path) {
    if (path.empty()) return false;
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

long long FileSizeOf(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return -1;
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return -1;
    return ((long long)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
}

std::wstring ReadAllText(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"";

    DWORD size = GetFileSize(h, nullptr);
    std::string buf;
    if (size != INVALID_FILE_SIZE && size > 0) {
        buf.resize(size);
        DWORD read = 0;
        if (!ReadFile(h, &buf[0], size, &read, nullptr)) read = 0;
        buf.resize(read);
    }
    CloseHandle(h);
    if (buf.empty()) return L"";

    // BOM
    if (buf.size() >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF) {
        return Utf8ToWide(buf.substr(3));
    }
    if (buf.size() >= 2 && (unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE) {
        std::wstring w((const wchar_t*)(buf.data() + 2), (buf.size() - 2) / sizeof(wchar_t));
        return w;
    }
    // UTF-8 严格校验，失败退回本地代码页
    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buf.c_str(), (int)buf.size(), nullptr, 0);
    if (len > 0) return Utf8ToWide(buf);
    len = MultiByteToWideChar(CP_ACP, 0, buf.c_str(), (int)buf.size(), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out;
    out.resize((size_t)len);
    MultiByteToWideChar(CP_ACP, 0, buf.c_str(), (int)buf.size(), &out[0], len);
    return out;
}

std::wstring KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &p))) return L"";
    std::wstring out = p;
    CoTaskMemFree(p);
    return out;
}

std::wstring SteamInstallPath() {
    // 64 位系统下 Steam 通常注册在 WOW6432Node；先试这个，再试原生视图
    const wchar_t* keys[] = {
        L"SOFTWARE\\WOW6432Node\\Valve\\Steam",
        L"SOFTWARE\\Valve\\Steam",
    };
    for (const wchar_t* k : keys) {
        wchar_t buf[MAX_PATH * 2] = {};
        DWORD size = sizeof(buf);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, k, L"InstallPath", RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                         nullptr, buf, &size) == ERROR_SUCCESS) {
            if (buf[0] && DirExists(buf)) return buf;
        }
        // 32 位程序在 64 位系统上会被重定向，用 CURRENT_USER 兜底
        size = sizeof(buf);
        if (RegGetValueW(HKEY_CURRENT_USER, k, L"SteamPath", RRF_RT_REG_SZ, nullptr, buf, &size) ==
            ERROR_SUCCESS) {
            if (buf[0] && DirExists(buf)) {
                std::wstring p = buf;
                for (auto& c : p) if (c == L'/') c = L'\\';
                return p;
            }
        }
    }
    return L"";
}

bool RunHidden(const std::wstring& cmdline) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = cmdline;
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &si, &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, 10000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

std::wstring NowTimeString() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buf[16];
    swprintf(buf, 16, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

} // namespace sif

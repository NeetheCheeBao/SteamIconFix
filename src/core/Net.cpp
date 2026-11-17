// Net.cpp - WinHTTP 实现
#include "Net.h"

#include <winhttp.h>

#include <string>
#include <vector>

namespace sif {

bool HttpGet(const std::wstring& url, std::string* body, HANDLE hFile, HttpResult& res) {
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        res.err = "URL 解析失败";
        return false;
    }

    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength > 0) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

    HINTERNET hSession = WinHttpOpen(L"SteamIconFix-GUI/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        res.err = "网络会话创建失败";
        return false;
    }
    WinHttpSetTimeouts(hSession, 8000, 8000, 10000, 20000);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), uc.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        res.err = "无法连接服务器";
        return false;
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        res.err = "请求创建失败";
        return false;
    }

    bool ok = false;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD code = 0;
        DWORD len = sizeof(code);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &code, &len, WINHTTP_NO_HEADER_INDEX);
        res.status = (int)code;

        if (code >= 200 && code < 300) {
            ok = true;
            std::vector<char> buf(32 * 1024);
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
                    ok = false;
                    res.err = "读取数据超时";
                    break;
                }
                if (avail == 0) break;
                DWORD want = avail > (DWORD)buf.size() ? (DWORD)buf.size() : avail;
                DWORD got = 0;
                if (!WinHttpReadData(hRequest, buf.data(), want, &got)) {
                    ok = false;
                    res.err = "读取数据失败";
                    break;
                }
                if (got == 0) break;
                if (body) {
                    body->append(buf.data(), got);
                } else if (hFile && hFile != INVALID_HANDLE_VALUE) {
                    DWORD written = 0;
                    if (!WriteFile(hFile, buf.data(), got, &written, nullptr) || written != got) {
                        ok = false;
                        res.err = "写入文件失败（可能被占用）";
                        break;
                    }
                }
            }
        } else {
            res.err = "HTTP " + std::to_string((int)code);
        }
    } else {
        res.err = "连接失败 / 网络不可达";
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    res.ok = ok;
    return ok;
}

// 从 JSON 文本里取一个字符串字段（够用即可，替代 jsoncpp）
std::string JsonStringField(const std::string& js, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t p = js.find(pat);
    if (p == std::string::npos) return "";
    p = js.find(':', p + pat.size());
    if (p == std::string::npos) return "";
    p = js.find('"', p);
    if (p == std::string::npos) return "";
    size_t e = js.find('"', p + 1);
    if (e == std::string::npos) return "";
    return js.substr(p + 1, e - p - 1);
}

} // namespace sif

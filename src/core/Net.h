// Net.h - WinHTTP 请求与轻量 JSON 取值
#pragma once

#include "core.h"

#include <string>

namespace sif {

struct HttpResult {
    bool ok = false;
    int status = 0;
    std::string err;
};

// body 与 hFile 二选一：写入内存或写入文件
bool HttpGet(const std::wstring& url, std::string* body, HANDLE hFile, HttpResult& res);

// 从 JSON 文本中取一个字符串字段
std::string JsonStringField(const std::string& js, const std::string& key);

} // namespace sif

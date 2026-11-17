// App.h - 界面状态与共享函数声明
#pragma once

#include "core.h"
#include "resource.h"

#include <windows.h>

#include <string>

using namespace sif;

// 版本号由构建脚本从 version.ini 生成到 version.h，再编译进程序
#if defined(__has_include)
#if __has_include("version.h")
#include "version.h"
#endif
#endif
#ifndef APP_VERSION_WSTR
#define APP_VERSION_WSTR L"1.0.0"
#endif

// 超链接
const wchar_t* const kProjectUrl = L"https://github.com/NeetheCheeBao/SteamIconFix";
const wchar_t* const kReleasesUrl = L"https://github.com/NeetheCheeBao/SteamIconFix/releases";

// 工作线程向界面线程投递的消息
const UINT kMsgLog = WM_APP + 1;
const UINT kMsgProg = WM_APP + 2;
const UINT kMsgDone = WM_APP + 3;

struct App {
    HINSTANCE inst = nullptr;
    HWND hwnd = nullptr;
    HFONT font = nullptr;
    HACCEL accel = nullptr;
    int dpi = 96;
    float scale = 1.0f;

    HWND grpEnv = nullptr;
    HWND lblSteam = nullptr, editSteam = nullptr, btnSteam = nullptr;
    HWND lblIcon = nullptr, editIcon = nullptr, btnIcon = nullptr;
    HWND lblDesktop = nullptr, editDesktop = nullptr, btnDesktop = nullptr;
    HWND lblInfo = nullptr;

    HWND grpSrc = nullptr;
    HWND radAuto = nullptr, radFastly = nullptr, radCf = nullptr, lblSrcHint = nullptr;

    HWND grpOpt = nullptr;
    HWND chkDesktop = nullptr, chkStart = nullptr, chkLib = nullptr, chkOverwrite = nullptr,
         chkFlush = nullptr;

    HWND grpLog = nullptr, editLog = nullptr, btnClear = nullptr;
    HWND progress = nullptr, lblPercent = nullptr;
    HWND btnStart = nullptr, btnStop = nullptr;
    HWND status = nullptr;

    EnvInfo env;
    FixRunner runner;
    bool running = false;
};

extern App g;

int Scale(int v);
HFONT CreateUiFont(int dpi);

void SetStatus(const wchar_t* text);
void SetCounts(const wchar_t* text);
void AppendLog(const std::wstring& text);
void AppendLogLine(const std::wstring& tag, const std::wstring& text);
void UpdateInfoLabel();

std::wstring GetText(HWND h);
bool Checked(int id);

void LoadEnvToUi();
void RefreshCounts();
FixOptions CollectOptions();
void EnableConfig(bool enable);

void CreateControls(HWND hwnd, HINSTANCE inst);
void Layout();

void FinishRun(const FixSummary& s);
void StartFix();
bool PickFolder(const wchar_t* title, std::wstring& out);
void BrowseInto(HWND edit, bool isSteamRoot);
void ShowAbout();

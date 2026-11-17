@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

if not defined CXX set "CXX=g++"
if not defined WR  set "WR=windres"

where "%CXX%" >nul 2>&1
if errorlevel 1 (
  echo ERROR: g++ not found on PATH.
  echo Install MinGW-w64 and add its bin folder to PATH, or set CXX=...
  echo.
  pause
  exit /b 1
)
where "%WR%" >nul 2>&1
if errorlevel 1 (
  echo ERROR: windres not found on PATH.
  echo Install MinGW-w64 and add its bin folder to PATH, or set WR=...
  echo.
  pause
  exit /b 1
)

rem ---- version: single source is version.ini ----
if not exist "version.ini" (
  echo ERROR: version.ini not found.
  echo.
  pause
  exit /b 1
)

set "VER="
for /f "tokens=1,* delims==" %%a in ('findstr /r /c:"^[Vv]ersion=" "version.ini"') do set "VER=%%b"
if not defined VER (
  echo ERROR: version.ini has no Version=x.y.z line.
  echo.
  pause
  exit /b 1
)
set "VER=%VER: =%"

echo %VER%| findstr /r "^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$" >nul
if errorlevel 1 (
  echo ERROR: Version must be x.y.z, got: %VER%
  echo.
  pause
  exit /b 1
)

set "OUT=bin\SteamIconFix.exe"
set "OBJDIR=%TEMP%\SteamIconFix_build"
set "APPRES=%OBJDIR%\app_res.o"
set "VHDR=%OBJDIR%\version.h"

set "SRCS=src\main.cpp src\ui\App.cpp src\ui\Actions.cpp src\core\Util.cpp src\core\Net.cpp src\core\Env.cpp src\core\Fix.cpp"

set /a TOTAL=3
for %%F in (%SRCS%) do set /a TOTAL+=1
set /a STEP=0

if not exist bin mkdir bin
if exist "%OBJDIR%" rmdir /s /q "%OBJDIR%" >nul 2>&1
mkdir "%OBJDIR%" >nul 2>&1
if not exist "%OBJDIR%" (
  echo ERROR: cannot create temp dir %OBJDIR%
  echo.
  pause
  exit /b 1
)

taskkill /F /IM SteamIconFix.exe >nul 2>&1

rem ---- generate version.h: version string only, no VERSIONINFO block ----
> "%VHDR%" echo #pragma once
>>"%VHDR%" echo.
>>"%VHDR%" echo #define APP_VERSION_STR  "%VER%"
>>"%VHDR%" echo #define APP_VERSION_WSTR L"%VER%"

call :progress "read version.ini (%VER%)"
call :progress "resources (app.rc)"
"%WR%" --codepage=65001 -I res -I src -I "%OBJDIR%" res\app.rc -o "%APPRES%"
if errorlevel 1 goto :fail

set "CFLAGS=-std=c++17 -O2 -municode -DUNICODE -D_UNICODE -DNOMINMAX -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -Isrc -Ires -I%OBJDIR%"

set "OBJS="
for %%F in (%SRCS%) do (
  set "name=%%F"
  set "name=!name:\=_!"
  set "name=!name:.cpp=.o!"
  call :progress "compile %%F"
  "%CXX%" %CFLAGS% -c "%%F" -o "%OBJDIR%\!name!"
  if errorlevel 1 goto :fail
  set OBJS=!OBJS! "%OBJDIR%\!name!"
)

call :progress "link %OUT%"
"%CXX%" !OBJS! "%APPRES%" -o "%OUT%" ^
  -municode -mwindows ^
  -static-libgcc -static-libstdc++ -static ^
  -lcomctl32 -lcomdlg32 -lwinhttp -lshlwapi -lshell32 -lole32 -luuid -lgdi32 -luser32
if errorlevel 1 goto :fail

echo.
echo BUILD OK  -^> %OUT%   version %VER%
rmdir /s /q "%OBJDIR%" >nul 2>&1
call :hold 3
exit /b 0

:fail
echo.
echo BUILD FAILED
if exist "%OBJDIR%" rmdir /s /q "%OBJDIR%" >nul 2>&1
echo.
pause
exit /b 1

:hold
echo.
if exist "%SystemRoot%\System32\timeout.exe" (
  "%SystemRoot%\System32\timeout.exe" /t %~1 /nobreak >nul
) else (
  ping -n %~1 127.0.0.1 >nul
)
goto :eof

:progress
set /a STEP+=1
set /a "pct=STEP*100/TOTAL"
set /a "filled=STEP*30/TOTAL"
if !filled! gtr 30 set "filled=30"
set "bar="
for /L %%i in (1,1,30) do (
  if %%i LEQ !filled! (set "bar=!bar!=") else (set "bar=!bar! ")
)
echo [!bar!] !pct!%%  %~1
goto :eof

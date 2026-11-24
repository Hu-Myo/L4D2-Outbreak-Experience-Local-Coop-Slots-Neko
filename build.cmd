@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
  echo Open an x86 Native Tools Command Prompt for Visual Studio 2022 and run build.cmd again.
  exit /b 1
)
if not defined L4N_SDK_DIR set "L4N_SDK_DIR=sdk"
if not exist "%L4N_SDK_DIR%\l4n_plugin.h" (
  echo See sdk\README.md for the Neko SDK header required by this build.
  exit /b 1
)
if not exist build mkdir build
cl /nologo /utf-8 /std:c++17 /EHsc /W4 /WX /permissive- /MT /O2 /LD /I"%L4N_SDK_DIR%" /Fo:build\plugin.obj /Fe:build\NekoLocalCoopSlots.dll plugin.cpp /link /MACHINE:X86 /DEF:plugin.def bcrypt.lib shell32.lib
exit /b %errorlevel%

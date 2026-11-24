@echo off
setlocal
if "%~1"=="" (
  echo Usage: tests\verify.cmd "Left 4 Dead 2 installation folder"
  exit /b 2
)
set "REFERENCE_GAME=%~f1"
cd /d "%~dp0.."
if not defined L4N_SDK_DIR set "L4N_SDK_DIR=sdk"
if not exist build mkdir build
cl /nologo /utf-8 /std:c++17 /EHsc /W4 /WX /permissive- /MT /O2 /I"%L4N_SDK_DIR%" /Fo:build\verify_game_compatibility.obj /Fe:build\verify_game_compatibility.exe tests\verify_game_compatibility.cpp /link /MACHINE:X86 bcrypt.lib shell32.lib
if errorlevel 1 exit /b %errorlevel%
build\verify_game_compatibility.exe "%REFERENCE_GAME%"
exit /b %errorlevel%

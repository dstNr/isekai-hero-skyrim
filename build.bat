@echo off
REM Build the Isekai SKSE plugin. Sets up MSVC env, then CMake configure + build.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

REM VS-bundled CMake + Ninja onto PATH, plus the VS Installer (for vswhere).
set "VS_CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "PATH=%VS_CMAKE%\CMake\bin;%VS_CMAKE%\Ninja;%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\Installer"

set "VCPKG_ROOT=C:\Users\dstNr\vcpkg"

REM Static triplet: bake spdlog/fmt/CommonLibSSE into a single self-contained
REM plugin DLL (dynamic CRT via -md), so no extra .dll files need shipping.
REM RelWithDebInfo, not Release: same optimisation, but it emits a .pdb. Without
REM one, Crash Logger can only print raw addresses for our frames in a stack trace.
cmake -S "%~dp0." -B "%~dp0build" -G Ninja ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
if errorlevel 1 exit /b 1

cmake --build "%~dp0build"
if errorlevel 1 exit /b 1

REM Auto-deploy plugin + symbols into the game so testing is one step.
set "SKSE_PLUGINS=E:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins"
if not exist "%SKSE_PLUGINS%" mkdir "%SKSE_PLUGINS%"
copy /Y "%~dp0build\IsekaiHeroSKSE.dll" "%SKSE_PLUGINS%\" >nul
copy /Y "%~dp0build\IsekaiHeroSKSE.pdb" "%SKSE_PLUGINS%\" >nul
echo Deployed IsekaiHeroSKSE.dll + .pdb -^> %SKSE_PLUGINS%

echo BUILD_OK

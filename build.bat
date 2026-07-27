@echo off
REM Build the Isekai SKSE plugin, then deploy it into the game.
REM
REM Keep this file pure ASCII. cmd reads .bat in the OEM codepage, and a stray
REM non-ASCII byte (an em dash in a comment, say) corrupts the parse of later
REM lines - "if errorlevel" then gets run as a command named "errorlevel".

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

REM VS-bundled CMake + Ninja onto PATH, plus the VS Installer (for vswhere).
set "VS_CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "PATH=%VS_CMAKE%\CMake\bin;%VS_CMAKE%\Ninja;%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\Installer"

REM Respect an existing VCPKG_ROOT (set it in your environment); the fallback below
REM is a generic default so this committed file carries no personal path.
if not defined VCPKG_ROOT set "VCPKG_ROOT=%USERPROFILE%\vcpkg"

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

set "SKSE_PLUGINS=E:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins"
if not exist "%SKSE_PLUGINS%" mkdir "%SKSE_PLUGINS%"

REM A running Skyrim holds the DLL open and the copy fails. Silently, if we let it:
REM the build claims success, the game keeps loading the previous build, and every
REM test after that measures stale code. That happened, and it cost hours of chasing
REM "contradictory" results that were really the same binary three times over.
REM So: fail loudly, and never report a deploy that did not happen.
tasklist /FI "IMAGENAME eq SkyrimSE.exe" 2>nul | find /I "SkyrimSE.exe" >nul
if not errorlevel 1 goto :game_running

copy /Y "%~dp0build\IsekaiHeroSKSE.dll" "%SKSE_PLUGINS%\" >nul
if errorlevel 1 goto :copy_failed
copy /Y "%~dp0build\IsekaiHeroSKSE.pdb" "%SKSE_PLUGINS%\" >nul
if errorlevel 1 goto :copy_failed

REM Panel icons ride along; the plugin loads them from Data\SKSE\Plugins\IsekaiHero.
if not exist "%SKSE_PLUGINS%\IsekaiHero\icons" mkdir "%SKSE_PLUGINS%\IsekaiHero\icons"
copy /Y "%~dp0icons\*.png" "%SKSE_PLUGINS%\IsekaiHero\icons\" >nul
if errorlevel 1 goto :copy_failed

echo Deployed IsekaiHeroSKSE.dll + .pdb -^> %SKSE_PLUGINS%
echo BUILD_OK
exit /b 0

:game_running
echo DEPLOY_FAILED: Skyrim is running and holding IsekaiHeroSKSE.dll open.
echo Close the game, then build again.
exit /b 1

:copy_failed
echo DEPLOY_FAILED: could not copy the plugin into the game folder.
exit /b 1

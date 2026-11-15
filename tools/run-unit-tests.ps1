# Build and run the unit tests in tests/ (no Skyrim, no vcpkg).
# Exit code 0 = all passed. The last line is ctest's summary
# ("100% tests passed, 0 tests failed out of N"), which test runners can parse.
#
#   pwsh -NoProfile -File tools/run-unit-tests.ps1
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\build-tests')
)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')

# The same MSVC Build Tools as build.bat. vcvars64 only sets environment variables,
# so run it in cmd and take the resulting environment over into this process.
$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) {
    Write-Error "MSVC Build Tools not found: $vcvars"
}
# vcvars asks vswhere, which lives with the VS Installer (build.bat adds it the same way).
$env:PATH = "$env:PATH;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2]) }
}
$vsCmake = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake'
$env:PATH = "$vsCmake\CMake\bin;$vsCmake\Ninja;$env:PATH"

cmake -S (Join-Path $root 'tests') -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Debug | Out-Null
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir $BuildDir --output-on-failure
exit $LASTEXITCODE

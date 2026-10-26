# Builds the Nhill Windows applications: Nhill Business, Nhill Console and Nhill POS,
# then (optionally) one installer per application with Inno Setup 6.
# NOT EXECUTED during development (no Windows machine was available): see
# docs/installation.md, section Windows, for the verification checklist.
#
# Prerequisites:
#   - Visual Studio 2022 (Desktop C++), CMake >= 3.21, Ninja
#   - Qt 6.5+ for MSVC 2019/2022 64-bit (online installer): Qt Quick, Qt Svg, Qt Linguist tools
#   - QtKeychain built for the same Qt (vcpkg: `vcpkg install qtkeychain-qt6:x64-windows`)
#     Without QtKeychain the client works but does not remember the session.
# The server is Linux-only (deployed on a Linux host); only the client is built here.
param(
  [string]$QtDir = "C:\Qt\6.5.3\msvc2019_64",
  [string]$BuildDir = "build-windows",
  [string]$VcpkgToolchain = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$args = @("-S", $root, "-B", $BuildDir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
          "-DCMAKE_PREFIX_PATH=$QtDir", "-DNHILL_BUILD_SERVER=OFF", "-DNHILL_BUILD_TESTS=OFF",
          "-DNHILL_SERVER_URL=https://api.example.com")
if ($VcpkgToolchain) { $args += "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain" }
cmake @args
cmake --build $BuildDir
# Collect the Qt runtime next to each executable (one folder per application).
$apps = @("nhill-business", "nhill-console", "nhill-pos")
foreach ($app in $apps) {
  $dist = "$BuildDir\dist\$app"
  New-Item -ItemType Directory -Force -Path $dist | Out-Null
  Copy-Item "$BuildDir\client\$app.exe" "$dist\"
  & "$QtDir\bin\windeployqt.exe" --release --qmldir "$root\client\qml" "$dist\$app.exe"
}
# Installers (Inno Setup 6, https://jrsoftware.org/isinfo.php): ISCC must be on PATH.
$iscc = Get-Command ISCC.exe -ErrorAction SilentlyContinue
if ($iscc) {
  foreach ($app in $apps) {
    & $iscc.Source "/DAppId=$app" "/DSourceDir=$((Resolve-Path "$BuildDir\dist\$app").Path)" `
      "/DOutputDir=$((Resolve-Path "$BuildDir\dist").Path)" "$root\packaging\windows\nhill-app.iss"
  }
  Write-Host "Installers written to $BuildDir\dist (sign them before distribution)."
} else {
  Write-Host "Inno Setup not found: applications ready in $BuildDir\dist\<app> (no installer built)."
}

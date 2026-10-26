# Construit Nhill Business, Nhill Console et Nhill POS pour Windows 10/11 x64, puis un
# installateur .exe par application (NSIS). À exécuter sous Windows dans
# « x64 Native Tools Command Prompt for VS 2022 » puis PowerShell :
#   powershell -ExecutionPolicy Bypass -File build-windows.ps1 -Source C:\src\nhill -QtDir C:\Qt\6.5.3\msvc2019_64 `
#       [-VcpkgRoot C:\vcpkg] [-ServerUrl https://api.exemple.com] [-BuildDir C:\build\nhill-win]
# Prérequis (vérifiés ci-dessous) : Visual Studio 2022 (C++), CMake >= 3.21, Ninja, Qt 6.5+ MSVC 64 bits
# (Qt Quick, Qt Svg, Qt Linguist), NSIS 3 (makensis), facultatif : QtKeychain via vcpkg
# (vcpkg install qtkeychain-qt6:x64-windows) pour mémoriser la session.
param(
  [Parameter(Mandatory = $true)][string]$Source,
  [Parameter(Mandatory = $true)][string]$QtDir,
  [string]$VcpkgRoot = "",
  [string]$ServerUrl = "",
  [string]$BuildDir = "$PSScriptRoot\build-windows"
)
$ErrorActionPreference = "Stop"
function Fail($msg) { Write-Host "ERREUR : $msg" -ForegroundColor Red; exit 1 }
function Need($cmd, $hint) { if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) { Fail "$cmd introuvable. $hint" } }

if ([Environment]::OSVersion.Version.Major -lt 10) { Fail "Windows 10 ou 11 requis pour la construction." }
if (-not [Environment]::Is64BitOperatingSystem) { Fail "Windows 64 bits requis." }
if (-not (Test-Path "$Source\CMakeLists.txt")) { Fail "code source introuvable : $Source" }
if (-not (Test-Path "$QtDir\bin\windeployqt.exe")) { Fail "Qt introuvable dans $QtDir (windeployqt.exe absent). Installez Qt 6.5+ MSVC 64 bits." }
Need cl.exe "Ouvrez « x64 Native Tools Command Prompt for VS 2022 » (Visual Studio 2022, charge de travail C++)."
Need cmake.exe "Installez CMake 3.21+ (https://cmake.org) ou winget install Kitware.CMake."
Need ninja.exe "Installez Ninja (winget install Ninja-build.Ninja)."
$makensis = (Get-Command makensis.exe -ErrorAction SilentlyContinue).Source
if (-not $makensis -and (Test-Path "${env:ProgramFiles(x86)}\NSIS\makensis.exe")) { $makensis = "${env:ProgramFiles(x86)}\NSIS\makensis.exe" }
if (-not $makensis) { Fail "NSIS 3 introuvable (winget install NSIS.NSIS)." }

$cm = Get-Content "$Source\CMakeLists.txt" -Raw
if ($cm -match 'project\(nhill\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') { $Version = $Matches[1] } else { Fail "version introuvable dans CMakeLists.txt" }
Write-Host "Nhill $Version"
$cmakeArgs = @("-S", $Source, "-B", $BuildDir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
          "-DCMAKE_PREFIX_PATH=$QtDir", "-DNHILL_BUILD_SERVER=OFF", "-DNHILL_BUILD_TESTS=OFF")
if ($ServerUrl) { $cmakeArgs += "-DNHILL_SERVER_URL=$ServerUrl" }
if ($VcpkgRoot) {
  $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
  $cmakeArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows"
} else { Write-Host "Attention : sans QtKeychain (vcpkg), la session ne sera pas mémorisée." -ForegroundColor Yellow }
& cmake @cmakeArgs; if ($LASTEXITCODE) { Fail "configuration CMake échouée" }
& cmake --build $BuildDir --parallel; if ($LASTEXITCODE) { Fail "compilation échouée" }

$dist = Join-Path $BuildDir "dist"
New-Item -ItemType Directory -Force -Path $dist | Out-Null
foreach ($app in @("nhill-business", "nhill-console", "nhill-pos")) {
  $exe = Join-Path $BuildDir "client\$app.exe"
  if (-not (Test-Path $exe)) { Fail "$app.exe non produit par la compilation" }
  $dir = Join-Path $dist $app
  Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $dir | Out-Null
  Copy-Item $exe $dir
  Copy-Item "$Source\assets\icons\$app.ico" $dir
  & "$QtDir\bin\windeployqt.exe" --release --qmldir "$Source\client\qml" --no-translations "$dir\$app.exe"
  if ($LASTEXITCODE) { Fail "windeployqt a échoué pour $app" }
  if ($VcpkgRoot) { Copy-Item "$VcpkgRoot\installed\x64-windows\bin\qt6keychain.dll" $dir -ErrorAction SilentlyContinue }
  & $makensis "/DAPP_ID=$app" "/DAPP_VERSION=$Version" "/DSOURCE_DIR=$dir" "/DOUTPUT_DIR=$dist" "$PSScriptRoot\nhill-installer.nsi"
  if ($LASTEXITCODE) { Fail "makensis a échoué pour $app" }
}
Get-ChildItem "$dist\*-setup.exe" | Get-FileHash -Algorithm SHA256 | ForEach-Object { "{0}  {1}" -f $_.Hash.ToLower(), (Split-Path $_.Path -Leaf) } | Set-Content "$dist\SHA256SUMS"
Write-Host "Installateurs : $dist\*-setup.exe (signez-les avec signtool avant distribution)." -ForegroundColor Green

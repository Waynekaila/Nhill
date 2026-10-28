# Construit Nhill Business, Nhill Console et Nhill POS pour Windows 10/11 x64, puis un
# installateur .exe par application (NSIS). À exécuter sous Windows dans
# « x64 Native Tools Command Prompt for VS 2022 » puis PowerShell :
#   powershell -ExecutionPolicy Bypass -File build-windows.ps1 -Source C:\src\nhill -QtDir C:\Qt\6.5.3\msvc2019_64 `
#       [-KeychainDir C:\build\qtkeychain] [-BuildDir C:\build\nhill-win]
# Prérequis (vérifiés ci-dessous) : Visual Studio 2022 (C++), CMake >= 3.21, Ninja, Git, Qt 6.5+ MSVC 64 bits
# (Qt Quick, Qt Svg, Qt Linguist, Qt Test), NSIS 3 (makensis).
# QtKeychain (session mémorisée dans le Gestionnaire d'identification Windows) est obligatoire :
# sans -KeychainDir, il est compilé depuis sa source vérifiée par build-qtkeychain.ps1, en
# bibliothèque statique (aucune DLL à livrer ; voir ce script). Chaque .exe est ensuite contrôlé :
# il doit importer CredReadW, CredWriteW et CredDeleteW d'ADVAPI32 (dumpbin).
# -AllowNoKeychain construit quand même sans lui (la session n'est alors jamais mémorisée).
param(
  [Parameter(Mandatory = $true)][string]$Source,
  [Parameter(Mandatory = $true)][string]$QtDir,
  [string]$KeychainDir = "",
  [string]$VcpkgRoot = "",
  [string]$ServerUrl = "",
  [string]$BuildDir = "$PSScriptRoot\build-windows",
  [switch]$AllowNoKeychain
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
# ---- QtKeychain statique (Gestionnaire d'identification Windows)
# Le paquet vcpkg de QtKeychain est une DLL (qt6keychain.dll) que windeployqt ne sait pas
# déployer : même avec -VcpkgRoot, QtKeychain est construit depuis sa source vérifiée.
if (-not $KeychainDir -and -not $AllowNoKeychain) {
  $KeychainDir = Join-Path $BuildDir "deps\qtkeychain"
  & powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\build-qtkeychain.ps1" -QtDir $QtDir -Prefix $KeychainDir -WorkDir (Join-Path $BuildDir "deps\qtkeychain-work")
  if ($LASTEXITCODE) { Fail "QtKeychain n'a pas pu être construit (voir ci-dessus) ; -AllowNoKeychain pour construire sans mémorisation de session" }
}
$prefixPath = $QtDir
$keychainArgs = @()
if ($KeychainDir) {
  if (-not (Test-Path "$KeychainDir\lib\cmake\Qt6Keychain\Qt6KeychainConfig.cmake")) { Fail "QtKeychain introuvable dans $KeychainDir" }
  if (-not (Test-Path "$KeychainDir\lib\qt6keychain.lib") -or (Test-Path "$KeychainDir\bin\qt6keychain.dll")) {
    Fail "$KeychainDir n'est pas une construction statique de QtKeychain : relancez build-qtkeychain.ps1 dans un dossier vide"
  }
  $prefixPath = "$QtDir;$KeychainDir"
  $keychainArgs = @("-DQt6Keychain_DIR:PATH=$KeychainDir\lib\cmake\Qt6Keychain")
}
$cmakeArgs = @("-S", $Source, "-B", $BuildDir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
          "-DCMAKE_PREFIX_PATH=$prefixPath", "-DNHILL_BUILD_SERVER=OFF", "-DNHILL_BUILD_TESTS=OFF",
          "-DNHILL_BUILD_KEYCHAIN_TEST=ON") + $keychainArgs
if ($ServerUrl) { Write-Warning "ServerUrl ignored: removed in 3.0.0, the server address is entered by the user at first launch" }
if ($VcpkgRoot) {
  $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
  $cmakeArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows"
}
& cmake @cmakeArgs; if ($LASTEXITCODE) { Fail "configuration CMake échouée" }
$cache = Get-Content (Join-Path $BuildDir "CMakeCache.txt") -Raw
$haveKeychain = $cache -match 'Qt6Keychain_DIR:[A-Z]+=(?!.*NOTFOUND).+'
if (-not $haveKeychain) {
  if (-not $AllowNoKeychain) { Fail "QtKeychain non trouvé par CMake : la session ne pourrait pas être mémorisée" }
  Write-Host "Attention : construction SANS QtKeychain, la session ne sera jamais mémorisée." -ForegroundColor Yellow
} else {
  Need dumpbin.exe "dumpbin (outils Visual Studio C++) sert à vérifier la liaison de QtKeychain."
  Write-Host "QtKeychain : session mémorisée dans le Gestionnaire d'identification Windows"
}
& cmake --build $BuildDir --parallel; if ($LASTEXITCODE) { Fail "compilation échouée" }

# Contrôle de la liaison : le stockage sécurisé est dans l'exécutable, aucune DLL QtKeychain.
function Test-CredentialStoreLinked([string]$Exe) {
  $imports = (& dumpbin.exe /nologo /imports $Exe) -join "`n"
  if ($LASTEXITCODE) { Fail "dumpbin a échoué pour $Exe" }
  foreach ($fn in @("CredReadW", "CredWriteW", "CredDeleteW")) {
    if ($imports -notmatch "\b$fn\b") { Fail "$(Split-Path -Leaf $Exe) n'importe pas $fn : QtKeychain (Gestionnaire d'identification) non lié" }
  }
  if ($imports -match "(?i)qt6keychain") { Fail "$(Split-Path -Leaf $Exe) dépend d'une DLL QtKeychain (liaison statique attendue)" }
}

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
  # Notices des composants tiers (QtKeychain est liée statiquement : sa licence BSD impose
  # de reproduire sa notice avec le binaire ; Qt LGPL ; polices).
  Copy-Item "$Source\packaging\notices\THIRD-PARTY-NOTICES.txt" $dir
  if ($haveKeychain) { Test-CredentialStoreLinked (Join-Path $dir "$app.exe") }
  & "$QtDir\bin\windeployqt.exe" --release --qmldir "$Source\client\qml" --no-translations "$dir\$app.exe"
  if ($LASTEXITCODE) { Fail "windeployqt a échoué pour $app" }
  & $makensis "/DAPP_ID=$app" "/DAPP_VERSION=$Version" "/DSOURCE_DIR=$dir" "/DOUTPUT_DIR=$dist" "$PSScriptRoot\nhill-installer.nsi"
  if ($LASTEXITCODE) { Fail "makensis a échoué pour $app" }
}
# SHA-256 par .NET (Get-FileHash peut manquer quand Windows PowerShell est lancé depuis pwsh)
function Get-Sha256Hex([string]$Path) {
  $sha = [System.Security.Cryptography.SHA256]::Create()
  $fs = [System.IO.File]::OpenRead($Path)
  try { return (($sha.ComputeHash($fs) | ForEach-Object { $_.ToString("x2") }) -join "") }
  finally { $fs.Dispose(); $sha.Dispose() }
}
Get-ChildItem "$dist\*-setup.exe" | ForEach-Object { "{0}  {1}" -f (Get-Sha256Hex $_.FullName), $_.Name } |
  Set-Content -Encoding ASCII "$dist\SHA256SUMS"
Write-Host "Installateurs : $dist\*-setup.exe (signez-les avec signtool avant distribution)." -ForegroundColor Green

# Construit QtKeychain (mémorisation sécurisée de la session : Gestionnaire d'identification
# Windows, protégé par DPAPI pour l'utilisateur) contre le Qt MSVC utilisé pour Nhill.
# Source : dépôt officiel frankosterfeld/qtkeychain, étiquette 0.17.0, dont le commit est
# vérifié avant toute compilation (aucun binaire téléchargé).
#   powershell -ExecutionPolicy Bypass -File build-qtkeychain.ps1 -QtDir C:\Qt\6.5.3\msvc2019_64 -Prefix C:\build\qtkeychain
# À lancer dans « x64 Native Tools Command Prompt for VS 2022 » (cl.exe, cmake, ninja, git).
# Résultat : <Prefix>\bin\qt6keychain.dll et <Prefix>\lib\cmake\Qt6Keychain (pour CMAKE_PREFIX_PATH).
param(
  [Parameter(Mandatory = $true)][string]$QtDir,
  [Parameter(Mandatory = $true)][string]$Prefix,
  [string]$WorkDir = "$env:TEMP\nhill-qtkeychain"
)
$ErrorActionPreference = "Stop"
$Tag = "0.17.0"
$Commit = "875f77d9f61bd97fd84cca47ce3bc71186dfbd09"
function Fail($msg) { Write-Host "ERREUR : $msg" -ForegroundColor Red; exit 1 }
foreach ($c in @("git.exe", "cmake.exe", "ninja.exe", "cl.exe")) {
  if (-not (Get-Command $c -ErrorAction SilentlyContinue)) { Fail "$c introuvable (Visual Studio 2022 C++, CMake, Ninja, Git requis)." }
}
if (-not (Test-Path "$QtDir\lib\cmake\Qt6\Qt6Config.cmake")) { Fail "Qt 6 introuvable dans $QtDir" }
if (Test-Path "$Prefix\bin\qt6keychain.dll") { Write-Host "QtKeychain déjà présent : $Prefix"; exit 0 }

$src = Join-Path $WorkDir "src"
Remove-Item -Recurse -Force $WorkDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
& git clone --quiet --depth 1 --branch $Tag https://github.com/frankosterfeld/qtkeychain.git $src
if ($LASTEXITCODE) { Fail "clonage de QtKeychain $Tag impossible" }
$head = (& git -C $src rev-parse HEAD).Trim()
if ($head -ne $Commit) { Fail "QtKeychain $Tag : commit $head inattendu (attendu $Commit) — source refusée" }
Write-Host "QtKeychain $Tag ($Commit) vérifié"

$build = Join-Path $WorkDir "build"
& cmake -S $src -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtDir" "-DCMAKE_INSTALL_PREFIX=$Prefix" `
  -DBUILD_WITH_QT5=OFF -DBUILD_SHARED_LIBS=ON -DBUILD_TRANSLATIONS=OFF -DBUILD_TEST_APPLICATION=OFF -DBUILD_TESTING=OFF `
  -DUSE_CREDENTIAL_STORE=ON
if ($LASTEXITCODE) { Fail "configuration de QtKeychain échouée" }
& cmake --build $build --parallel; if ($LASTEXITCODE) { Fail "compilation de QtKeychain échouée" }
& cmake --install $build; if ($LASTEXITCODE) { Fail "installation de QtKeychain échouée" }
if (-not (Test-Path "$Prefix\bin\qt6keychain.dll")) { Fail "qt6keychain.dll absent après installation" }
if (-not (Test-Path "$Prefix\lib\cmake\Qt6Keychain\Qt6KeychainConfig.cmake")) { Fail "configuration CMake Qt6Keychain absente" }
Write-Host "QtKeychain installé dans $Prefix" -ForegroundColor Green

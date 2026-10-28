# Point d'entrée Windows : délègue à packaging\windows\build-windows.ps1 (implémentation unique).
# Windows entry point: forwards to packaging\windows\build-windows.ps1 (single implementation).
#   powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1 -Source C:\src\nhill `
#     -QtDir C:\Qt\6.5.3\msvc2019_64 [-VcpkgRoot C:\vcpkg]
param(
  [Parameter(Mandatory = $true)][string]$Source,
  [Parameter(Mandatory = $true)][string]$QtDir,
  [string]$VcpkgRoot = "",
  [string]$ServerUrl = "",
  [string]$BuildDir = ""
)
$ErrorActionPreference = "Stop"
$impl = Join-Path $PSScriptRoot "..\packaging\windows\build-windows.ps1"
$p = @{ Source = $Source; QtDir = $QtDir; VcpkgRoot = $VcpkgRoot; ServerUrl = $ServerUrl }
if ($BuildDir) { $p.BuildDir = $BuildDir }
& $impl @p
exit $LASTEXITCODE

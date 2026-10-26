#!/usr/bin/env bash
# =============================================================================
# build-windows.sh — installateurs Windows 10/11 x64 de Nhill Business, Nhill POS
# et Nhill Console (un .exe NSIS par application, runtime Qt inclus).
# Windows 10/11 x64 installers (one NSIS .exe per application, Qt runtime included).
#
# Trois façons de construire / three ways to build:
#   1. Sous Windows (recommandé, vérifié par la CI) :
#        powershell -ExecutionPolicy Bypass -File packaging\windows\build-windows.ps1 `
#          -Source C:\src\nhill -QtDir C:\Qt\6.5.3\msvc2019_64 [-ServerUrl https://api.exemple.com]
#   2. Intégration continue : .github/workflows/windows.yml (runner windows-2022, Qt 6.5.3
#      MSVC, NSIS) — artefact « nhill-windows-installers ».
#   3. Depuis Linux (compilation croisée MinGW, ce script) : nécessite Qt pour Windows
#      compilé par packaging/windows/build-qt-mingw.sh (long : plusieurs heures).
#
# Usage (Linux) :
#   scripts/build-windows.sh [--qt-prefix /opt/nhill-win/qt6] [--server-url URL]
#                            [--output DIR] [--force] [--dry-run]
# Sortie : 0 succès · 1 échec · 2 usage · 3 environnement incompatible (Windows, Qt
# pour Windows ou MinGW absents : le message indique la marche à suivre).
# =============================================================================
PROG=build-windows.sh
source "$(dirname "$0")/lib/common.sh"

QT_PREFIX=/opt/nhill-win/qt6 SERVER_URL="" OUT="" FORCE=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --qt-prefix) QT_PREFIX="${2:?}"; shift ;;
    --server-url) SERVER_URL="${2:?}"; shift ;;
    --output|-o) OUT="${2:?}"; shift ;;
    --force) FORCE=1 ;;
    --dry-run) DRY_RUN=1 ;;
    -h|--help) sed -n '2,22p' "$0"; exit 0 ;;
    *) die "option inconnue : $1" 2 ;;
  esac
  shift
done

start_log
step "Contrôle de l'environnement"
detect_os
case "$NH_KERNEL" in
  MINGW*|MSYS*|CYGWIN*) incompatible "sous Windows, utilisez packaging\\windows\\build-windows.ps1 (MSVC + Qt + NSIS)" ;;
  Linux) ;;
  *) incompatible "système $NH_KERNEL non pris en charge pour la construction Windows" ;;
esac
MISSING=()
for t in x86_64-w64-mingw32-g++-posix x86_64-w64-mingw32-objdump makensis cmake ninja; do
  command -v "$t" >/dev/null 2>&1 || MISSING+=("$t")
done
if (( ${#MISSING[@]} > 0 )); then
  incompatible "outils absents : ${MISSING[*]}.
  -> scripts/install-deps-linux.sh --with-windows-cross   (MinGW-w64, NSIS)
  -> ou construisez sous Windows (packaging/windows/build-windows.ps1) ou par la CI (.github/workflows/windows.yml)"
fi
if [[ ! -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  incompatible "Qt pour Windows introuvable dans $QT_PREFIX.
  -> packaging/windows/build-qt-mingw.sh $QT_PREFIX   (compile Qt 6.4.2 pour Windows depuis les sources officielles github.com/qt, plusieurs heures)
  -> ou construisez sous Windows / par la CI (recommandé)"
fi
VERSION="$(project_version)"
OUT="${OUT:-$ROOT/dist/windows}"
prepare_output_dir "$OUT" "$FORCE"
check_disk "$ROOT" 5

step "Compilation croisée et installateurs NSIS"
BUILD="$ROOT/build/windows-cross" OUT="$OUT" run "$ROOT/packaging/windows/build-windows-cross.sh" "$ROOT" "$QT_PREFIX" ${SERVER_URL:+"$SERVER_URL"}

[[ "$DRY_RUN" == 1 ]] && exit 0
step "Vérification des installateurs"
for app in nhill-business nhill-console nhill-pos; do
  f="$OUT/$app-$VERSION-setup.exe"
  expect_file "$f" 5000000
  # Un vrai exécutable Windows : en-tête « MZ » puis signature « PE\0\0 ».
  [[ "$(head -c2 "$f")" == MZ ]] || die "$f n'est pas un exécutable Windows (en-tête MZ absent)"
  file "$f" 2>/dev/null | grep -q 'PE32' || die "$f : format PE non reconnu"
done
write_checksums "$OUT"
warn "Installateurs non signés : signez-les (signtool, certificat de signature de code) avant diffusion publique."

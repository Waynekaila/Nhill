#!/usr/bin/env bash
# =============================================================================
# install-deps-linux.sh — installe les outils et bibliothèques nécessaires pour
# construire et tester Nhill sur une machine Linux NEUVE (aucun compilateur, aucun SDK).
# Installs everything needed to build and test Nhill on a FRESH Linux machine.
#
# Distributions validées : Debian 13 (trixie), Ubuntu 24.04 (noble), amd64.
# Les paquets viennent UNIQUEMENT des dépôts officiels de la distribution (apt).
#
# Usage :
#   scripts/install-deps-linux.sh [--with-packaging] [--with-docs] [--with-gui-tests]
#                                 [--with-windows-cross] [--with-android-jdk] [--all]
#                                 [--dry-run] [--force-distro] [--yes]
#   --with-packaging      dpkg-dev, fakeroot, lintian (paquets .deb)
#   --with-docs           pandoc + WeasyPrint (PDF de la documentation)
#   --with-gui-tests      Xvfb + polices (captures d'écran des applications sans écran)
#   --with-windows-cross  MinGW-w64 + NSIS (compilation croisée Windows, voir docs)
#   --with-android-jdk    JDK pour Android (le SDK/NDK Android ne vient pas d'apt : voir docs)
#   --all                 tout ce qui précède
#   --dry-run             affiche ce qui serait installé, n'installe rien
#   --force-distro        tente une autre distribution dérivée de Debian (non validé)
#   --yes                 pas de question (machines de construction, CI)
#
# Sortie : 0 succès · 1 échec · 2 usage · 3 environnement incompatible.
# Un inventaire des versions installées est écrit dans build/logs/deps-manifest-*.txt
# (traçabilité d'une construction reproductible).
# =============================================================================
PROG=install-deps-linux.sh
source "$(dirname "$0")/lib/common.sh"

WITH_PKG=0 WITH_DOCS=0 WITH_GUI=0 WITH_WIN=0 WITH_ANDROID=0 FORCE_DISTRO=0 ASSUME_YES=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --with-packaging) WITH_PKG=1 ;;
    --with-docs) WITH_DOCS=1 ;;
    --with-gui-tests) WITH_GUI=1 ;;
    --with-windows-cross) WITH_WIN=1 ;;
    --with-android-jdk) WITH_ANDROID=1 ;;
    --all) WITH_PKG=1 WITH_DOCS=1 WITH_GUI=1 WITH_WIN=1 WITH_ANDROID=1 ;;
    --dry-run) DRY_RUN=1 ;;
    --force-distro) FORCE_DISTRO=1 ;;
    --yes|-y) ASSUME_YES=1 ;;
    -h|--help) sed -n '2,27p' "$0"; exit 0 ;;
    *) die "option inconnue : $1 (voir --help)" 2 ;;
  esac
  shift
done

start_log
step "Contrôle du système"
detect_os
[[ "$NH_KERNEL" == Linux ]] || incompatible "Linux requis (système détecté : $NH_KERNEL). Windows : packaging/windows/build-windows.ps1 ; Android : docs/fr/guide-compilation.md"
is_debian_like || incompatible "distribution non dérivée de Debian ($NH_OS_ID) : ce script utilise apt"
if ! is_supported_distro; then
  [[ "$FORCE_DISTRO" == 1 ]] || incompatible "distribution $NH_OS_ID $NH_OS_VERSION non validée (Debian 13 ou Ubuntu 24.04). --force-distro pour essayer quand même"
  warn "distribution $NH_OS_ID $NH_OS_VERSION non validée : poursuite demandée (--force-distro)"
fi
[[ "$NH_ARCH" == x86_64 || "$NH_ARCH" == aarch64 ]] || warn "architecture $NH_ARCH non testée (testé : x86_64)"
need apt-get
need dpkg-query
ok "$NH_OS_ID $NH_OS_VERSION ($NH_OS_CODENAME), $NH_ARCH"
check_disk "$ROOT" 6

# ------------------------------------------------------------------ packages
BASE=(build-essential cmake ninja-build pkg-config git ca-certificates file)
SERVER=(libpqxx-dev libsodium-dev libpng-dev libcpp-httplib-dev nlohmann-json3-dev libhpdf-dev libssl-dev zlib1g-dev libcurl4-openssl-dev)
TESTS=(libgtest-dev postgresql poppler-utils python3)
CLIENT=(qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-tools-dev qt6-l10n-tools qt6-tools-dev-tools
        qtkeychain-qt6-dev libgl-dev
        qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts
        qml6-module-qtquick-dialogs qml6-module-qtquick-templates qml6-module-qtquick-window
        qml6-module-qtqml-workerscript qml6-module-qtqml-models qml6-module-qtqml
        qml6-module-qt-labs-folderlistmodel qt6-qpa-plugins)
PKG=() ; DOCS=() ; GUI=() ; WIN=() ; ANDROID=()
[[ "$WITH_PKG" == 1 ]] && PKG=(dpkg-dev fakeroot lintian debhelper)
[[ "$WITH_DOCS" == 1 ]] && DOCS=(pandoc weasyprint fonts-dejavu-core)   # paquet « weasyprint » (Debian 13, Ubuntu 24.04 universe)
[[ "$WITH_GUI" == 1 ]] && GUI=(xvfb xauth imagemagick fonts-dejavu-core)
[[ "$WITH_WIN" == 1 ]] && WIN=(g++-mingw-w64-x86-64-posix mingw-w64-tools nsis)
if [[ "$WITH_ANDROID" == 1 ]]; then
  if [[ "$NH_OS_ID" == debian ]]; then ANDROID=(openjdk-21-jdk-headless unzip); warn "Debian 13 ne fournit pas le JDK 17 : JDK 21 installé (Qt 6.8+ pour Android recommandé)"
  else ANDROID=(openjdk-17-jdk-headless unzip); fi
fi
WANTED=("${BASE[@]}" "${SERVER[@]}" "${TESTS[@]}" "${CLIENT[@]}" "${PKG[@]}" "${DOCS[@]}" "${GUI[@]}" "${WIN[@]}" "${ANDROID[@]}")

step "Recherche des paquets manquants"
MISSING=()
for p in "${WANTED[@]}"; do
  if dpkg-query -W -f='${Status}' "$p" 2>/dev/null | grep -q 'install ok installed'; then :; else MISSING+=("$p"); fi
done
ok "${#WANTED[@]} paquets demandés, $(( ${#WANTED[@]} - ${#MISSING[@]} )) déjà installés, ${#MISSING[@]} à installer"

if (( ${#MISSING[@]} > 0 )); then
  printf '  %s\n' "${MISSING[@]}" >&2
  step "Mise à jour des index APT"
  as_root apt-get update -q || die "apt-get update a échoué : vérifiez le réseau et /etc/apt/sources.list (universe requis sur Ubuntu)"
  step "Vérification de la disponibilité"
  UNAVAILABLE=()
  if [[ "$DRY_RUN" != 1 ]]; then
    for p in "${MISSING[@]}"; do
      cand="$(apt-cache policy "$p" 2>/dev/null | awk '/Candidate:/ {print $2}')"
      [[ -n "$cand" && "$cand" != "(none)" ]] || UNAVAILABLE+=("$p")
    done
  fi
  if (( ${#UNAVAILABLE[@]} > 0 )); then
    die "paquets introuvables dans les dépôts configurés : ${UNAVAILABLE[*]} (Ubuntu : activez « universe » ; Debian : « main »)"
  fi
  step "Installation"
  APT_OPTS=(-q --no-install-recommends)
  [[ "$ASSUME_YES" == 1 || ! -t 0 ]] && APT_OPTS+=(-y)
  as_root env DEBIAN_FRONTEND=noninteractive apt-get install "${APT_OPTS[@]}" "${MISSING[@]}"
fi

if [[ "$DRY_RUN" != 1 ]]; then
  step "Contrôle après installation"
  for t in g++ cmake ninja pkg-config; do need "$t"; done
  QTV="$( (qtpaths6 --qt-query QT_VERSION || /usr/lib/qt6/bin/qtpaths --qt-query QT_VERSION) 2>/dev/null || echo '?')"
  PGBIN="$(ls -d /usr/lib/postgresql/*/bin 2>/dev/null | sort -V | tail -n1)"
  [[ -x "$PGBIN/initdb" ]] || die "PostgreSQL (initdb) introuvable : les tests du serveur ne pourront pas s'exécuter"
  ok "g++ $(g++ -dumpversion), CMake $(cmake --version | awk 'NR==1{print $3}'), Qt $QTV, PostgreSQL $(basename "$(dirname "$PGBIN")")"
  MANIFEST="$ROOT/build/logs/deps-manifest-$(date +%Y%m%d-%H%M%S).txt"
  {
    echo "# Nhill — inventaire des dépendances ($NH_OS_ID $NH_OS_VERSION $NH_ARCH, $(date -Iseconds))"
    dpkg-query -W -f='${Package}\t${Version}\n' "${WANTED[@]}" 2>/dev/null | sort
  } > "$MANIFEST"
  ok "inventaire des versions : $MANIFEST"
fi
if [[ "$WITH_ANDROID" == 1 ]]; then
  warn "Le SDK et le NDK Android ainsi que Qt pour Android ne sont pas distribués par apt."
  warn "Voir docs/fr/guide-compilation.md § Android, ou le workflow .github/workflows/android.yml."
fi
ok "Terminé."

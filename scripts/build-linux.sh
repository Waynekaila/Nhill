#!/usr/bin/env bash
# =============================================================================
# build-linux.sh — compile Nhill (serveur + 3 applications Qt) et exécute les tests.
# Builds Nhill (server + 3 Qt apps) and runs the test suites.
#
# Usage :
#   scripts/build-linux.sh [BUILD_DIR] [--no-tests] [--server-only|--client-only]
#                          [--server-url URL] [--jobs N] [--debug] [--deb] [--dry-run]
#
# Prérequis : scripts/install-deps-linux.sh (Debian 13 / Ubuntu 24.04).
# Le code source n'est pas modifié : compilation hors arborescence dans BUILD_DIR
# (défaut : build/). Journal : build/logs/build-linux-*.log.
# --deb : enchaîne scripts/build-deb.sh (paquets Debian) après la compilation.
# Sortie : 0 succès · 1 échec (compilation ou tests) · 2 usage · 3 environnement.
# =============================================================================
PROG=build-linux.sh
source "$(dirname "$0")/lib/common.sh"

BUILD="" TESTS=ON SERVER=ON CLIENT=ON DEB=0 JOBS="$(nproc 2>/dev/null || echo 2)" TYPE=Release SERVER_URL=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-tests) TESTS=OFF ;;
    --server-only) CLIENT=OFF ;;
    --client-only) SERVER=OFF ;;
    --server-url) SERVER_URL="${2:?--server-url URL}"; shift ;;
    --jobs|-j) JOBS="${2:?--jobs N}"; shift ;;
    --debug) TYPE=Debug ;;
    --deb) DEB=1 ;;
    --dry-run) DRY_RUN=1 ;;
    -h|--help) sed -n '2,16p' "$0"; exit 0 ;;
    -*) die "option inconnue : $1" 2 ;;
    *) BUILD="$1" ;;
  esac
  shift
done
BUILD="${BUILD:-$ROOT/build}"
[[ "$SERVER" == ON || "$CLIENT" == ON ]] || die "--server-only et --client-only sont exclusifs" 2

start_log
step "Contrôle de l'environnement"
detect_os
[[ "$NH_KERNEL" == Linux ]] || incompatible "Linux requis (Windows : packaging/windows/build-windows.ps1)"
for t in cmake ninja g++ pkg-config; do need "$t" "lancez scripts/install-deps-linux.sh"; done
if [[ "$CLIENT" == ON ]]; then
  pkg-config --exists Qt6Quick 2>/dev/null || [[ -d /usr/lib/x86_64-linux-gnu/cmake/Qt6Quick || -d /usr/lib/aarch64-linux-gnu/cmake/Qt6Quick || -n "${CMAKE_PREFIX_PATH:-}" ]] \
    || die "Qt 6 (qt6-declarative-dev) introuvable : lancez scripts/install-deps-linux.sh" 3
fi
if [[ "$SERVER" == ON ]]; then
  pkg-config --exists libpqxx libsodium libcurl || die "libpqxx-dev / libsodium-dev / libcurl4-openssl-dev introuvables : lancez scripts/install-deps-linux.sh" 3
fi
check_disk "$(dirname "$BUILD")" 3
VERSION="$(project_version)"
ok "Nhill $VERSION — $NH_OS_ID $NH_OS_VERSION, $(g++ -dumpversion | sed 's/^/g++ /'), $JOBS tâche(s)"

step "Configuration (CMake, $TYPE)"
ARGS=(-S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DCMAKE_INSTALL_PREFIX=/usr
      -DNHILL_BUILD_SERVER="$SERVER" -DNHILL_BUILD_CLIENT="$CLIENT" -DNHILL_BUILD_TESTS="$TESTS")
[[ -n "$SERVER_URL" ]] && ARGS+=(-DNHILL_SERVER_URL="$SERVER_URL")
# Drapeaux de durcissement Debian quand dpkg-buildflags est disponible.
if command -v dpkg-buildflags >/dev/null; then
  ARGS+=(-DCMAKE_CXX_FLAGS="$(dpkg-buildflags --get CXXFLAGS) $(dpkg-buildflags --get CPPFLAGS)"
         -DCMAKE_EXE_LINKER_FLAGS="$(dpkg-buildflags --get LDFLAGS)")
fi
run cmake "${ARGS[@]}"

step "Compilation"
run cmake --build "$BUILD" --parallel "$JOBS"

if [[ "$DRY_RUN" != 1 ]]; then
  step "Vérification des binaires produits"
  [[ "$SERVER" == ON ]] && expect_file "$BUILD/server/nhill-server" 100000
  if [[ "$CLIENT" == ON ]]; then
    for app in nhill-business nhill-pos nhill-console; do expect_file "$BUILD/client/$app" 100000; done
  fi
fi

if [[ "$TESTS" == ON ]]; then
  step "Tests (ctest) — le serveur démarre des grappes PostgreSQL jetables"
  run ctest --test-dir "$BUILD" --output-on-failure --timeout 1200 \
    || die "des tests ont échoué : voir le journal ci-dessus ($LOG_FILE)"
  ok "tous les tests sont passés"
fi

if [[ "$DEB" == 1 ]]; then
  step "Paquets Debian"
  run "$ROOT/scripts/build-deb.sh" --build-dir "$BUILD"
fi
ok "Construction terminée : $BUILD"
[[ "$DEB" == 1 ]] || log "Installation locale : sudo cmake --install $BUILD   ou paquets : scripts/build-deb.sh"

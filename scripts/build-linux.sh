#!/usr/bin/env bash
# Builds Nhill on Linux (Ubuntu 24.04 / Debian 13 tested: Ubuntu 24.04).
#   scripts/build-linux.sh [build-dir] [--no-tests] [--server-only|--client-only] [--deb]
# --deb also builds the Debian packages nhill-server, nhill-business, nhill-console, nhill-pos.
# Dependencies (Ubuntu 24.04):
#   sudo apt install build-essential cmake ninja-build pkg-config libpqxx-dev libsodium-dev libpng-dev \
#        libcpp-httplib-dev nlohmann-json3-dev libhpdf-dev libssl-dev libgtest-dev postgresql-16 \
#        qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-tools-dev qt6-l10n-tools qtkeychain-qt6-dev \
#        qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
#        qml6-module-qtquick-dialogs qml6-module-qtquick-templates qml6-module-qtqml-workerscript
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build}"; shift || true
TESTS=ON SERVER=ON CLIENT=ON DEB=OFF
for a in "$@"; do
  case "$a" in
    --no-tests) TESTS=OFF ;;
    --server-only) CLIENT=OFF ;;
    --client-only) SERVER=OFF ;;
    --deb) DEB=ON ;;
  esac
done
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
      -DNHILL_BUILD_SERVER=$SERVER -DNHILL_BUILD_CLIENT=$CLIENT -DNHILL_BUILD_TESTS=$TESTS
cmake --build "$BUILD"
if [ "$TESTS" = ON ]; then
  # The server tests start a throwaway PostgreSQL 16 cluster (initdb/pg_ctl).
  ctest --test-dir "$BUILD" --output-on-failure
fi
if [ "$DEB" = ON ]; then
  (cd "$BUILD" && cpack -G DEB)
  echo "Packages: $(ls "$BUILD"/*.deb | tr '\n' ' ')"
  echo "Install with: sudo apt install ./nhill-server_*.deb (server) or ./nhill-business_*.deb, ./nhill-pos_*.deb, ./nhill-console_*.deb"
else
  echo "Build done in $BUILD. Install with: sudo cmake --install $BUILD"
fi

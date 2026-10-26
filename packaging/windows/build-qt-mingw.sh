#!/usr/bin/env bash
# Construit Qt 6.4.2 (qtbase, qtshadertools, qtdeclarative, qtsvg) et QtKeychain pour
# Windows x86_64 par compilation croisée depuis Ubuntu 24.04 / Debian, avec MinGW-w64.
# Les outils hôtes (moc, rcc, qmlcachegen, qsb, lrelease…) sont ceux du Qt 6.4.2 du système,
# même version que la cible (QT_HOST_PATH).
#   ./build-qt-mingw.sh [préfixe=/opt/nhill-win/qt6]   (≈ 1 à 2 h sur 2 cœurs)
set -Eeuo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${1:-/opt/nhill-win/qt6}"
WORK="${NHILL_QT_WORK:-/opt/nhill-win/work}"
QT_VERSION=6.4.2
JOBS="${JOBS:-$(nproc)}"
die() { echo "erreur: $*" >&2; exit 1; }
trap 'echo "erreur: échec ligne $LINENO : $BASH_COMMAND" >&2' ERR

need=(cmake ninja git perl python3 x86_64-w64-mingw32-g++-posix)
for t in "${need[@]}"; do command -v "$t" >/dev/null || die "outil manquant : $t
  sudo apt install cmake ninja-build git perl python3 g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix \\
       qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-declarative-dev-tools qt6-shadertools-dev qt6-svg-dev qt6-tools-dev qt6-l10n-tools"; done
HOST_QT="$(qtpaths6 --qt-query QT_VERSION 2>/dev/null || /usr/lib/qt6/bin/qtpaths --qt-query QT_VERSION 2>/dev/null || true)"
[[ "$HOST_QT" == "$QT_VERSION" ]] || die "Qt hôte $QT_VERSION requis pour les outils de génération (trouvé : ${HOST_QT:-aucun}) — Ubuntu 24.04 fournit Qt 6.4.2"
HOST_CMAKE_DIR="$(dirname "$(dpkg -L qt6-base-dev | grep -m1 '/cmake/Qt6/Qt6Config.cmake$')")/.."
[[ -d "$HOST_CMAKE_DIR/Qt6CoreTools" ]] || die "Qt6CoreTools hôte introuvable sous $HOST_CMAKE_DIR"

mkdir -p "$WORK/src" "$PREFIX"
for m in qtbase qtshadertools qtdeclarative qtsvg; do
  [[ -d "$WORK/src/$m" ]] || git clone -q --depth 1 -b "v$QT_VERSION" "https://github.com/qt/$m.git" "$WORK/src/$m"
done
[[ -d "$WORK/src/qtkeychain" ]] || git clone -q --depth 1 -b 0.14.3 https://github.com/frankosterfeld/qtkeychain.git "$WORK/src/qtkeychain"

export NHILL_WIN_PREFIX="$PREFIX"
COMMON=(-G Ninja -DCMAKE_TOOLCHAIN_FILE="$HERE/toolchain-mingw64.cmake" -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="$PREFIX" -DQT_HOST_PATH=/usr -DQT_HOST_PATH_CMAKE_DIR="$HOST_CMAKE_DIR"
        -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF -DCMAKE_CXX_FLAGS="-include cstdint")
build() {  # build <module> <cmake args…>
  local m="$1"; shift
  if [[ -f "$WORK/build-$m/.installed" ]]; then echo "== $m déjà installé"; return; fi
  echo "== $m : configuration"
  cmake -S "$WORK/src/$m" -B "$WORK/build-$m" "${COMMON[@]}" "$@" >"$WORK/$m-configure.log" 2>&1 \
    || { tail -n 40 "$WORK/$m-configure.log" >&2; die "configuration de $m échouée ($WORK/$m-configure.log)"; }
  echo "== $m : compilation"
  cmake --build "$WORK/build-$m" --parallel "$JOBS" >"$WORK/$m-build.log" 2>&1 \
    || { grep -E 'error|FAILED' "$WORK/$m-build.log" | head -n 30 >&2; die "compilation de $m échouée ($WORK/$m-build.log)"; }
  cmake --install "$WORK/build-$m" >"$WORK/$m-install.log" 2>&1 || die "installation de $m échouée"
  touch "$WORK/build-$m/.installed"
}
# qtbase : sans Widgets, SQL, D-Bus ni impression (inutiles à Nhill) ; TLS par Schannel (Windows)
build qtbase -DFEATURE_widgets=OFF -DFEATURE_sql=OFF -DFEATURE_dbus=OFF -DFEATURE_printsupport=OFF \
  -DFEATURE_openssl=OFF -DFEATURE_schannel=ON -DFEATURE_system_zlib=OFF -DFEATURE_system_pcre2=OFF \
  -DFEATURE_system_harfbuzz=OFF -DFEATURE_system_freetype=OFF -DFEATURE_system_libpng=OFF \
  -DFEATURE_system_libjpeg=OFF -DFEATURE_system_doubleconversion=OFF -DFEATURE_opengl=ON -DFEATURE_opengl_desktop=ON
export CMAKE_PREFIX_PATH="$PREFIX"
build qtshadertools -DQt6_DIR="$PREFIX/lib/cmake/Qt6"
# qtdeclarative : seuls les styles Basic (utilisé par Nhill) et Windows des Qt Quick Controls
build qtdeclarative -DQt6_DIR="$PREFIX/lib/cmake/Qt6" -DFEATURE_qml_debug=OFF \
  -DFEATURE_quickcontrols2_material=OFF -DFEATURE_quickcontrols2_universal=OFF \
  -DFEATURE_quickcontrols2_imagine=OFF -DFEATURE_quickcontrols2_fusion=OFF \
  -DFEATURE_quick_particles=OFF -DFEATURE_qml_jit=ON
build qtsvg -DQt6_DIR="$PREFIX/lib/cmake/Qt6"
# QtKeychain : jetons de session dans le Gestionnaire d'identification Windows
build qtkeychain -DBUILD_WITH_QT6=ON -DBUILD_TRANSLATIONS=OFF -DBUILD_TEST_APPLICATION=OFF -DQt6_DIR="$PREFIX/lib/cmake/Qt6"
echo "Qt $QT_VERSION pour Windows installé dans $PREFIX"

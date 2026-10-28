#!/usr/bin/env bash
# Compilation croisée des applications Windows depuis Linux (après build-qt-mingw.sh) :
# nhill-business.exe, nhill-console.exe, nhill-pos.exe + un installateur NSIS chacun.
#   ./build-windows-cross.sh /chemin/vers/nhill [/opt/nhill-win/qt6]
# Prérequis : g++-mingw-w64-x86-64-posix, cmake, ninja, nsis (makensis), Qt hôte 6.4.2
# (paquets Ubuntu 24.04) et Qt 6.4.2 pour Windows construit par build-qt-mingw.sh.
set -Eeuo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="${1:?usage : $0 /chemin/vers/nhill [préfixe-qt-windows] [adresse-serveur]}"
PREFIX="${2:-/opt/nhill-win/qt6}"
SERVER_URL="${3:-}"
BUILD="${BUILD:-$PWD/build-win}"
OUT="${OUT:-$PWD/dist/windows}"
MINGW_SYSROOT=/usr/x86_64-w64-mingw32
die() { echo "erreur : $*" >&2; exit 1; }
trap 'echo "erreur : échec ligne $LINENO : $BASH_COMMAND" >&2' ERR
for t in cmake ninja makensis x86_64-w64-mingw32-g++-posix x86_64-w64-mingw32-objdump; do
  command -v "$t" >/dev/null || die "outil manquant : $t (sudo apt install cmake ninja-build nsis g++-mingw-w64-x86-64-posix)"
done
[[ -f "$PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]] || die "Qt pour Windows introuvable dans $PREFIX (lancer build-qt-mingw.sh)"
export NHILL_WIN_PREFIX="$PREFIX"
VERSION="$(sed -nE 's/^project\([^)]*VERSION[[:space:]]+([0-9.]+).*/\1/p' "$SRC/CMakeLists.txt" | head -n1)"
[[ -n "$VERSION" ]] || die "version introuvable dans $SRC/CMakeLists.txt"
HOST_CMAKE_DIR="$(dirname "$(dpkg -L qt6-base-dev | grep -m1 '/cmake/Qt6/Qt6Config.cmake$')")/.."
ARGS=(-S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$HERE/toolchain-mingw64.cmake" -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_PREFIX_PATH="$PREFIX" -DQt6_DIR="$PREFIX/lib/cmake/Qt6" -DQT_HOST_PATH=/usr
      -DQT_HOST_PATH_CMAKE_DIR="$HOST_CMAKE_DIR" -DNHILL_BUILD_SERVER=OFF -DNHILL_BUILD_TESTS=OFF)
[[ -n "$SERVER_URL" ]] && { echo "adresse de serveur ignorée : option retirée en 3.0.0 : aucune adresse de serveur n'est compilée dans les applications (saisie au premier lancement) / removed in 3.0.0: no server address is compiled into the applications" >&2; }
cmake "${ARGS[@]}" || die "configuration CMake échouée"
cmake --build "$BUILD" --parallel || die "compilation échouée"

# Déploiement sans windeployqt (outil Windows) : DLL résolues récursivement par objdump.
deploy_dlls() {  # deploy_dlls <dossier>
  local dir="$1" changed=1 f dll src
  while (( changed )); do
    changed=0
    for f in "$dir"/*.exe "$dir"/*.dll "$dir"/*/*.dll "$dir"/qml/*/*.dll "$dir"/qml/*/*/*.dll "$dir"/qml/*/*/*/*.dll; do
      [[ -f "$f" ]] || continue
      while read -r dll; do
        [[ -f "$dir/$dll" ]] && continue
        for src in "$PREFIX/bin/$dll" "$MINGW_SYSROOT/lib/$dll" "$MINGW_SYSROOT/bin/$dll" \
                   /usr/lib/gcc/x86_64-w64-mingw32/*-posix/"$dll"; do
          if [[ -f "$src" ]]; then cp "$src" "$dir/"; changed=1; break; fi
        done
      done < <(x86_64-w64-mingw32-objdump -p "$f" | sed -n 's/^[[:space:]]*DLL Name: //p')
    done
  done
}
mkdir -p "$OUT"
for app in nhill-business nhill-console nhill-pos; do
  d="$OUT/$app"; rm -rf "$d"; mkdir -p "$d/platforms" "$d/imageformats" "$d/tls" "$d/qml"
  [[ -f "$BUILD/client/$app.exe" ]] || die "$app.exe non produit"
  cp "$BUILD/client/$app.exe" "$SRC/assets/icons/$app.ico" "$d/"
  cp "$PREFIX/plugins/platforms/qwindows.dll" "$d/platforms/"
  cp "$PREFIX/plugins/imageformats/qsvg.dll" "$d/imageformats/"
  cp "$PREFIX"/plugins/tls/qschannelbackend.dll "$d/tls/" || die "greffon TLS Schannel absent (HTTPS impossible)"
  for m in QtQml QtQuick; do cp -r "$PREFIX/qml/$m" "$d/qml/"; done
  printf '[Paths]\nPrefix = .\nPlugins = .\nQml2Imports = qml\n' > "$d/qt.conf"
  deploy_dlls "$d"
  makensis -V2 -DAPP_ID="$app" -DAPP_VERSION="$VERSION" -DSOURCE_DIR="$d" -DOUTPUT_DIR="$OUT" "$HERE/nhill-installer.nsi" \
    || die "makensis a échoué pour $app"
done
( cd "$OUT" && sha256sum ./*-setup.exe > SHA256SUMS )
echo "Installateurs : $OUT/*-setup.exe"

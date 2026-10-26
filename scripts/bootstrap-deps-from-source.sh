#!/usr/bin/env bash
# =============================================================================
# Nhill — construction des dépendances depuis leurs sources officielles (GitHub)
# Nhill — build the build-time dependencies from their official sources
#
# QUAND L'UTILISER / WHEN TO USE
#   Uniquement si les dépôts apt de la distribution ne sont PAS joignables
#   (réseau filtré, machine hors ligne avec miroir Git, etc.). Sur une machine
#   normale, utilisez scripts/install-deps-linux.sh (paquets officiels).
#   Only when the distribution apt mirrors are NOT reachable. On a normal
#   machine use scripts/install-deps-linux.sh (official packages).
#
# CE QUE FAIT LE SCRIPT / WHAT IT DOES
#   Clone des étiquettes (tags) précises, depuis les organisations officielles :
#     qt/qtbase, qt/qtshadertools, qt/qtsvg, qt/qtdeclarative, qt/qttools  v6.4.2
#       (même version que Qt d'Ubuntu 24.04, pour la compatibilité binaire)
#     frankosterfeld/qtkeychain 0.14.3, jedisct1/libsodium 1.0.18-RELEASE,
#     postgres/postgres REL_16_4 (libpq seulement), jtv/libpqxx 7.8.1,
#     yhirose/cpp-httplib v0.14.3, nlohmann/json v3.11.3,
#     libharu/libharu v2.4.4, google/googletest v1.14.0
#   puis installe tout dans un préfixe privé (défaut : /opt/nhill-sdk), sans
#   toucher aux bibliothèques du système.
#
# USAGE
#   scripts/bootstrap-deps-from-source.sh [--prefix DIR] [--jobs N] [--only LIST] [--dry-run]
#   LIST : sodium,libpq,pqxx,httplib,json,haru,gtest,qt,keychain (défaut : tout)
#   Ensuite / then:
#     export CMAKE_PREFIX_PATH=/opt/nhill-sdk PKG_CONFIG_PATH=/opt/nhill-sdk/lib/pkgconfig
#     scripts/build-linux.sh
#
# Prérequis système (déjà présents sur Ubuntu 24.04 « build-essential ») :
#   gcc/g++ ≥ 11, cmake ≥ 3.21, ninja, perl, python3, git, pkg-config,
#   en-têtes zlib/libpng/OpenSSL/fontconfig/OpenGL.
# =============================================================================
set -Eeuo pipefail

PREFIX=/opt/nhill-sdk
JOBS="$(nproc 2>/dev/null || echo 2)"
ONLY="sodium,libpq,pqxx,httplib,json,haru,gtest,qt,keychain"
DRY=0
SRC_CACHE="${NHILL_SRC_CACHE:-$HOME/.cache/nhill-src}"
QT_VER=6.4.2

log()  { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }
die()  { printf '\033[1;31mERREUR / ERROR: %s\033[0m\n' "$*" >&2; exit 1; }
run()  { if [ "$DRY" = 1 ]; then echo "[simulation] $*"; else "$@"; fi; }
want() { case ",$ONLY," in *",$1,"*) return 0;; *) return 1;; esac; }
trap 'die "échec ligne $LINENO (commande : $BASH_COMMAND)"' ERR

while [ $# -gt 0 ]; do
  case "$1" in
    --prefix) PREFIX="$2"; shift 2 ;;
    --jobs)   JOBS="$2"; shift 2 ;;
    --only)   ONLY="$2"; shift 2 ;;
    --dry-run) DRY=1; shift ;;
    -h|--help) sed -n '2,32p' "$0"; exit 0 ;;
    *) die "option inconnue : $1" ;;
  esac
done

[ "$(uname -s)" = Linux ] || die "Linux requis"
for t in git cmake ninja g++ perl python3 pkg-config; do
  command -v "$t" >/dev/null || die "outil manquant : $t"
done
avail_kb=$(df -Pk "$(dirname "$SRC_CACHE")" 2>/dev/null | awk 'NR==2{print $4}' || echo 0)
[ "${avail_kb:-0}" -gt 15000000 ] || echo "Attention : moins de 15 Go libres pour les sources et la compilation de Qt."

run mkdir -p "$SRC_CACHE" "$PREFIX"
[ -w "$PREFIX" ] || [ "$DRY" = 1 ] || die "pas de droit d'écriture sur $PREFIX (utilisez sudo ou --prefix)"
export PATH="$PREFIX/bin:$PATH"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig:${PKG_CONFIG_PATH:-}"
export CMAKE_PREFIX_PATH="$PREFIX"

# clone REPO TAG DIR — clone superficiel d'une étiquette ; réutilise un clone existant
clone() {
  local repo="$1" tag="$2" dir="$SRC_CACHE/$3"
  if [ -d "$dir/.git" ]; then
    echo "déjà présent : $dir ($(git -C "$dir" describe --tags 2>/dev/null || echo '?'))"
  else
    run git clone --quiet --depth 1 --branch "$tag" "https://github.com/$repo.git" "$dir"
  fi
}
stamp() { [ -f "$PREFIX/.stamp-$1" ]; }
done_() { run touch "$PREFIX/.stamp-$1"; }

cmake_build() { # SRC BUILD [cmake args…]
  local s="$1" b="$2"; shift 2
  run cmake -S "$s" -B "$b" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
      -DCMAKE_PREFIX_PATH="$PREFIX" "$@"
  run cmake --build "$b" -j "$JOBS"
  run cmake --install "$b"
}

if want sodium && ! stamp sodium; then
  log "libsodium 1.0.18 (même ABI que libsodium23 de Debian/Ubuntu)"
  clone jedisct1/libsodium 1.0.18-RELEASE libsodium
  ( cd "$SRC_CACHE/libsodium" && run ./configure --quiet --prefix="$PREFIX" --disable-static \
      && run make -s -j "$JOBS" && run make -s install )
  done_ sodium
fi

if want libpq && ! stamp libpq; then
  log "libpq (PostgreSQL 16.4, en-têtes et bibliothèque cliente uniquement)"
  clone postgres/postgres REL_16_4 postgres
  ( cd "$SRC_CACHE/postgres" && run ./configure --quiet --prefix="$PREFIX" --without-readline --without-icu \
      --with-openssl && run make -s -C src/include install && run make -s -C src/interfaces/libpq -j "$JOBS" install \
      && run make -s -C src/bin/pg_config install && run make -s -C src/common install && run make -s -C src/port install )
  done_ libpq
fi

if want pqxx && ! stamp pqxx; then
  log "libpqxx 7.8.1 (statique, lié dans nhill-server)"
  clone jtv/libpqxx 7.8.1 libpqxx
  cmake_build "$SRC_CACHE/libpqxx" "$SRC_CACHE/libpqxx/_b" -DBUILD_SHARED_LIBS=OFF -DSKIP_BUILD_TEST=ON \
      -DBUILD_DOC=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DPostgreSQL_ROOT="$PREFIX"
  done_ pqxx
fi

if want httplib && ! stamp httplib; then
  log "cpp-httplib 0.14.3 (en-tête seul)"
  clone yhirose/cpp-httplib v0.14.3 cpp-httplib
  cmake_build "$SRC_CACHE/cpp-httplib" "$SRC_CACHE/cpp-httplib/_b" -DHTTPLIB_REQUIRE_OPENSSL=ON \
      -DHTTPLIB_USE_BROTLI_IF_AVAILABLE=OFF -DHTTPLIB_USE_ZLIB_IF_AVAILABLE=ON
  # Le projet cherche cpp-httplib par pkg-config (comme le paquet Debian) : fichier .pc minimal.
  run mkdir -p "$PREFIX/lib/pkgconfig"
  if [ "$DRY" = 0 ]; then
    cat > "$PREFIX/lib/pkgconfig/cpp-httplib.pc" <<EOF
prefix=$PREFIX
includedir=\${prefix}/include
Name: cpp-httplib
Description: C++ header-only HTTP/HTTPS server and client library
Version: 0.14.3
Requires: openssl
Cflags: -I\${includedir} -DCPPHTTPLIB_OPENSSL_SUPPORT -DCPPHTTPLIB_ZLIB_SUPPORT
Libs: -lz -lpthread
EOF
  fi
  done_ httplib
fi

if want json && ! stamp json; then
  log "nlohmann/json 3.11.3"
  clone nlohmann/json v3.11.3 json
  cmake_build "$SRC_CACHE/json" "$SRC_CACHE/json/_b" -DJSON_BuildTests=OFF -DJSON_Install=ON
  done_ json
fi

if want haru && ! stamp haru; then
  log "libharu 2.4.4 (statique, lié dans nhill-server)"
  clone libharu/libharu v2.4.4 libharu
  cmake_build "$SRC_CACHE/libharu" "$SRC_CACHE/libharu/_b" -DBUILD_SHARED_LIBS=OFF -DLIBHPDF_SHARED=OFF \
      -DLIBHPDF_STATIC=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  done_ haru
fi

if want gtest && ! stamp gtest; then
  log "GoogleTest 1.14.0"
  clone google/googletest v1.14.0 googletest
  cmake_build "$SRC_CACHE/googletest" "$SRC_CACHE/googletest/_b" -DINSTALL_GTEST=ON -DBUILD_GMOCK=OFF
  done_ gtest
fi

if want qt; then
  # Qt 6.4.2 : même version et mêmes options d'ABI que les paquets Ubuntu 24.04,
  # pour que les binaires s'exécutent avec le Qt du système (ou le runtime embarqué).
  QT_COMMON=(-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX"
             -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF -G Ninja)
  if ! stamp qtbase; then
    log "Qt $QT_VER — qtbase (long : 45 à 90 min sur 2 cœurs)"
    clone qt/qtbase "v$QT_VER" qtbase
    run cmake -S "$SRC_CACHE/qtbase" -B "$SRC_CACHE/qtbase/_b" "${QT_COMMON[@]}" \
        -DFEATURE_xcb=OFF -DFEATURE_xlib=OFF -DFEATURE_opengl=ON -DFEATURE_sql=OFF \
        -DFEATURE_system_freetype=OFF -DFEATURE_system_harfbuzz=OFF -DFEATURE_system_jpeg=OFF \
        -DFEATURE_system_pcre2=OFF -DFEATURE_system_doubleconversion=OFF -DFEATURE_dbus=ON \
        -DFEATURE_dbus_linked=OFF -DFEATURE_icu=OFF -DINPUT_openssl=runtime
    run cmake --build "$SRC_CACHE/qtbase/_b" -j "$JOBS"
    run cmake --install "$SRC_CACHE/qtbase/_b"
    done_ qtbase
  fi
  for m in qtshadertools qtsvg qtdeclarative; do
    if ! stamp "$m"; then
      log "Qt $QT_VER — $m"
      clone "qt/$m" "v$QT_VER" "$m"
      run "$PREFIX/bin/qt-configure-module" "$SRC_CACHE/$m" -- -G Ninja -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
          -B "$SRC_CACHE/$m/_b" 2>/dev/null || \
        run cmake -S "$SRC_CACHE/$m" -B "$SRC_CACHE/$m/_b" "${QT_COMMON[@]}"
      run cmake --build "$SRC_CACHE/$m/_b" -j "$JOBS"
      run cmake --install "$SRC_CACHE/$m/_b"
      done_ "$m"
    fi
  done
  if ! stamp qttools; then
    log "Qt $QT_VER — qttools (Linguist : lupdate/lrelease uniquement)"
    clone qt/qttools "v$QT_VER" qttools
    run cmake -S "$SRC_CACHE/qttools" -B "$SRC_CACHE/qttools/_b" "${QT_COMMON[@]}" \
        -DFEATURE_assistant=OFF -DFEATURE_designer=OFF -DFEATURE_distancefieldgenerator=OFF \
        -DFEATURE_pixeltool=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF -DFEATURE_qdoc=OFF \
        -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qev=OFF -DFEATURE_linguist=ON \
        -DFEATURE_clang=OFF -DFEATURE_kmap2qmap=OFF -DFEATURE_qtdbus=OFF
    run cmake --build "$SRC_CACHE/qttools/_b" -j "$JOBS"
    run cmake --install "$SRC_CACHE/qttools/_b"
    done_ qttools
  fi
fi

if want keychain && ! stamp keychain; then
  log "QtKeychain 0.14.3 (Qt 6, stockage via libsecret/D-Bus Secret Service)"
  clone frankosterfeld/qtkeychain 0.14.3 qtkeychain
  cmake_build "$SRC_CACHE/qtkeychain" "$SRC_CACHE/qtkeychain/_b" -DBUILD_WITH_QT6=ON -DBUILD_TRANSLATIONS=OFF \
      -DBUILD_TEST_APPLICATION=OFF -DLIBSECRET_SUPPORT=OFF
  done_ keychain
fi

log "Terminé. Préfixe : $PREFIX"
echo "export CMAKE_PREFIX_PATH=$PREFIX PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig"

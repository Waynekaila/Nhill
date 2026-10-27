#!/usr/bin/env bash
# build-deb-debian13.sh — paquets Nhill installables sur Debian 13 (Trixie), construits
# depuis une machine Ubuntu 24.04 (glibc 2.39 ≤ glibc 2.41 de Debian 13).
#
# Debian 13 ne fournit pas les versions de bibliothèques avec lesquelles Nhill est compilé
# sur Ubuntu 24.04 (Qt 6.4 / 6.8, libpqxx 7.8 / 7.10, cpp-httplib 0.14 / 0.18, ICU 74 / 76).
# Ces bibliothèques sont donc embarquées dans des dossiers privés (FHS : /usr/lib/<paquet>) ;
# toutes les autres dépendances sont des paquets Debian 13 standard, résolus par apt :
#   nhill-qt6-runtime  Qt 6.4.2, QtKeychain, ICU 74, greffons et modules QML utilisés
#   nhill-business, nhill-pos, nhill-console   applications (dépendent du runtime)
#   nhill-server       serveur (embarque libpqxx 7.8 et cpp-httplib 0.14)
#
#   ./build-deb-debian13.sh -s SOURCE -b BUILD_DIR [-o dist/debian13] [-r 1]
# BUILD_DIR : compilation CMake déjà faite (par ex. build-deb.sh --build-dir …), sur Ubuntu 24.04.
set -Eeuo pipefail
umask 022
export LC_ALL=C.UTF-8
die()  { echo "erreur : $*" >&2; exit 1; }
log()  { echo "==> $*" >&2; }
trap 'echo "erreur : échec ligne $LINENO : $BASH_COMMAND" >&2' ERR

SRC="" BUILD="" OUT="$PWD/dist/debian13" REV=1
while (( $# )); do
  case "$1" in
    -s) SRC="$2"; shift 2 ;; -b) BUILD="$2"; shift 2 ;; -o) OUT="$2"; shift 2 ;; -r) REV="$2"; shift 2 ;;
    -h|--help) sed -n '2,17p' "$0"; exit 0 ;; *) die "option inconnue : $1" ;;
  esac
done
[[ -f "$SRC/CMakeLists.txt" ]] || die "code source introuvable (-s)"
[[ -f "$BUILD/CMakeCache.txt" ]] || die "dossier de compilation introuvable (-b)"
for t in readelf patchelf dpkg-deb dpkg-shlibdeps file gzip; do command -v "$t" >/dev/null || die "outil manquant : $t (sudo apt install patchelf dpkg-dev file binutils)"; done
. /etc/os-release
GLIBC="$(getconf GNU_LIBC_VERSION | awk '{print $2}')"
awk -v g="$GLIBC" 'BEGIN { exit !(g <= 2.41) }' || die "glibc $GLIBC de la machine > 2.41 (Debian 13) : construisez sur Ubuntu 24.04"
SRC="$(cd "$SRC" && pwd)"; BUILD="$(cd "$BUILD" && pwd)"; mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
ARCH="$(dpkg --print-architecture)"; MA="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
VERSION="$(sed -nE 's/^project\([^)]*VERSION[[:space:]]+([0-9.]+).*/\1/p' "$SRC/CMakeLists.txt" | head -n1)"
DEBV="${VERSION}-${REV}+deb13"
MAINT="${DEBFULLNAME:-Nhill} <${DEBEMAIL:-support@nhill.invalid}>"
QTLIB="/usr/lib/$MA"; QTPLUG="$QTLIB/qt6/plugins"; QTQML="$QTLIB/qt6/qml"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/nhill-deb13.XXXXXXXX")"
trap 'rm -rf --one-file-system -- "$TMP"' EXIT

# Bibliothèques à embarquer (absentes de Debian 13 dans ces versions) : motif de soname
BUNDLE_RE='^(libjpeg\.so\.8|libQt6[A-Za-z0-9]*\.so\.6|libqt6keychain\.so\.1|libicu(uc|i18n|data)\.so\.74|libpqxx-7\.8\.so|libcpp-httplib\.so\.0\.14)$'
# Paquets attendus sous ces noms dans Debian 13 (vérifiés sur l'archive trixie, voir README)
TRIXIE_OK_RE='^(libc6|libstdc\+\+6|libgcc-s1|zlib1g|libzstd1|libb2-1|libdouble-conversion3|libpcre2-16-0|libpcre2-8-0|libmd4c0|libglib2\.0-0t64|libfreetype6|libfontconfig1|libharfbuzz0b|libpng16-16t64|libdbus-1-3|libsecret-1-0|libx11-6|libx11-xcb1|libxcb[a-z0-9-]*|libxkbcommon0|libxkbcommon-x11-0|libsm6|libice6|libegl1|libgl1|libglx0|libopengl0|libssl3t64|libcurl4t64|libpq5|libsodium23|libhpdf-2\.3\.0|libgssapi-krb5-2|libproxy1v5|libbrotli1|libmtdev1t64|libinput10|libudev1|libts0t64|libxrender1|libxi6|libxext6|libdrm2|libgbm1|libwayland-[a-z0-9-]*|adduser|systemd)$'

needed() { readelf -d "$1" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'; }
resolve() {  # soname -> chemin système
  local so="$1" p
  for p in "$QTLIB/$so" "/lib/$MA/$so" "/usr/lib/$so" "/lib/$so"; do if [[ -e "$p" ]]; then readlink -f "$p"; return; fi; done
  ldconfig -p | awk -v s="$so" '$1 == s { print $NF; exit }'
}
is_elf() { file -b "$1" | grep -q '^ELF'; }
# Copie récursive des bibliothèques embarquées nécessaires aux fichiers ELF de $1 vers $2
bundle_closure() {
  local root="$1" libdir="$2" changed=1 f so real
  mkdir -p "$libdir"
  while (( changed )); do
    changed=0
    while IFS= read -r -d '' f; do
      is_elf "$f" || continue
      while read -r so; do
        [[ "$so" =~ $BUNDLE_RE ]] || continue
        [[ -e "$libdir/$so" ]] && continue
        real="$(resolve "$so")"; [[ -n "$real" ]] || die "bibliothèque $so introuvable"
        cp -L "$real" "$libdir/$so"; chmod 0644 "$libdir/$so"; changed=1
      done < <(needed "$f")
    done < <(find "$root" -type f -print0)
  done
}
# RUNPATH relatif vers le dossier des bibliothèques embarquées
set_runpath() {  # set_runpath root libdir-absolu-dans-le-paquet
  local root="$1" lib="$2" f d rel
  while IFS= read -r -d '' f; do
    is_elf "$f" || continue
    d="/${f#"$root"/}"; d="${d%/*}"
    rel="$(realpath -m --relative-to="$d" "$lib")"
    patchelf --set-rpath "\$ORIGIN/$rel" "$f" || die "patchelf a échoué : $f"
  done < <(find "$root" -type f \( -name '*.so*' -o -perm -u+x \) -print0)
}
strip_all() {
  local f
  while IFS= read -r -d '' f; do
    if is_elf "$f"; then strip --strip-unneeded --remove-section=.comment "$f" || die "strip : $f"; fi
  done < <(find "$1" -type f -print0)
}
shlib_deps() {  # dépendances système (dpkg-shlibdeps), bibliothèques embarquées exclues
  local root="$1" pkg="$2" libdir="$3" files=() f w
  w="$TMP/shl-$pkg"
  while IFS= read -r -d '' f; do if is_elf "$f"; then files+=("$f"); fi; done < <(find "$root" -type f -print0)
  (( ${#files[@]} )) || return 0
  mkdir -p "$w/debian"; printf 'Source: nhill\n\nPackage: %s\nArchitecture: any\n' "$pkg" >"$w/debian/control"
  (cd "$w" && dpkg-shlibdeps -O -x"$pkg" --ignore-missing-info ${libdir:+-l"$libdir"} "${files[@]}" 2>"$w/err") \
    | sed -n 's/^shlibs:Depends=//p'
}
# Paquets système dont les bibliothèques sont embarquées : retirés des dépendances calculées
# (dpkg-shlibdeps attribue une bibliothèque embarquée au paquet Ubuntu de même soname).
bundled_pkgs() {
  # paquets propriétaires des bibliothèques embarquées + noms qu'ils déclarent dans leurs
  # fichiers shlibs/symbols (ex. libjpeg.so.8 : paquet libjpeg-turbo8, dépendance « libjpeg8 »)
  local libdir="$1" so real owner f
  for so in "$libdir"/*; do
    real="$(resolve "$(basename "$so")")"
    [[ -n "$real" ]] || continue
    owner="$({ dpkg -S "$real" 2>/dev/null || dpkg -S "/usr$real" 2>/dev/null; } | head -n1 | cut -d: -f1)"
    [[ -n "$owner" ]] || continue
    echo "$owner"
    for f in /var/lib/dpkg/info/"$owner".shlibs /var/lib/dpkg/info/"$owner":"$ARCH".shlibs; do
      [[ -f "$f" ]] && awk '$1 !~ /^#/ && NF >= 3 { print $3 }' "$f"
    done
    for f in /var/lib/dpkg/info/"$owner".symbols /var/lib/dpkg/info/"$owner":"$ARCH".symbols; do
      [[ -f "$f" ]] && awk '/^[^ *|#]/ { print $2 }' "$f"
    done
  done | sort -u
  printf '%s\n' qt6-base-abi qtdeclarative-abi
}
drop_bundled() {  # drop_bundled "deps" libdir…
  local deps="$1"; shift
  local pk; pk="$(for d in "$@"; do bundled_pkgs "$d"; done | sort -u)"
  tr ',' '\n' <<<"$deps" | sed 's/^ *//; s/ *$//' | grep -v '^$' \
    | awk -v list="$pk" 'BEGIN { n = split(list, a, "\n"); for (i = 1; i <= n; i++) skip[a[i]] = 1 }
        { name = $1; sub(/\(.*/, "", name); base = name; sub(/-[0-9].*$/, "", base)
          if (!(name in skip) && !(base in skip) && name !~ /^(qt6-base-abi|qtdeclarative-abi)/) print }' \
    | paste -sd, - | sed 's/,/, /g'
}
check_trixie() {  # refuse toute dépendance non vérifiée pour Debian 13
  local deps="$1" p bad=()
  for p in $(tr ',|' '\n\n' <<<"$deps" | sed -E 's/\(.*\)//; s/^ *//; s/ *$//' | grep -v '^$'); do
    [[ "$p" =~ $TRIXIE_OK_RE ]] || bad+=("$p")
  done
  (( ${#bad[@]} == 0 )) || die "dépendances non vérifiées pour Debian 13 : ${bad[*]} (embarquer la bibliothèque ou valider le paquet)"
}
docs() {  # docs root pkg
  local d="$1/usr/share/doc/$2"; mkdir -p "$d"
  cat >"$d/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: nhill

Files: *
Copyright: $(date +%Y) les créateurs de nhill
License: proprietary
 Licence du projet : propriétaire, à définir par ses créateurs (README.md).

Files: usr/lib/nhill/lib/libQt6* usr/lib/nhill/qt6/*
Copyright: The Qt Company Ltd. and other contributors
License: LGPL-3
 Qt 6.4.2 (Ubuntu 24.04), lié dynamiquement. Texte : /usr/share/common-licenses/LGPL-3.

Files: usr/lib/nhill/lib/libqt6keychain*
Copyright: Frank Osterfeld and contributors
License: BSD-3-Clause

Files: usr/lib/nhill/lib/libicu*
Copyright: Unicode, Inc. and others
License: Unicode-3.0

Files: usr/lib/nhill-server/*
Copyright: Jeroen T. Vermeulen (libpqxx) ; Yuji Hirose (cpp-httplib)
License: BSD-3-Clause and MIT
EOF
  printf '%s (%s) trixie; urgency=medium\n\n  * Paquet pour Debian 13 construit par build-deb-debian13.sh.\n\n -- %s  %s\n' \
    "$2" "$DEBV" "$MAINT" "$(date -R)" | gzip -9n >"$d/changelog.Debian.gz"
  chmod 0644 "$d"/*
}
control() {  # control root pkg section depends recommends suggests "description"
  local root="$1" size; size="$(du -sk --exclude=DEBIAN "$root" | cut -f1)"
  mkdir -p "$root/DEBIAN"
  (cd "$root" && find . -type f ! -path './DEBIAN/*' -printf '%P\0' | LC_ALL=C sort -z | xargs -0 md5sum) >"$root/DEBIAN/md5sums"
  {
    echo "Package: $2"; echo "Version: $DEBV"; echo "Architecture: $ARCH"; echo "Maintainer: $MAINT"
    echo "Installed-Size: $size"; [[ -n "$4" ]] && echo "Depends: $4"; [[ -n "$5" ]] && echo "Recommends: $5"
    [[ -n "$6" ]] && echo "Suggests: $6"; echo "Section: $3"; echo "Priority: optional"
    printf '%s\n' "$7" | sed '1s/^/Description: /; 2,$s/^/ /'
  } >"$root/DEBIAN/control"
  find "$root" -type d -exec chmod 0755 {} +
  chmod 0644 "$root/DEBIAN/control" "$root/DEBIAN/md5sums"
}
build_deb() { dpkg-deb --root-owner-group -Zxz --build "$1" "$OUT/$2_${DEBV}_${ARCH}.deb" >/dev/null || die "dpkg-deb $2"; echo "$OUT/$2_${DEBV}_${ARCH}.deb"; }

# ------------------------------------------------------------------ runtime Qt
log "nhill-qt6-runtime"
RT="$TMP/nhill-qt6-runtime"; mkdir -p "$RT/usr/lib/nhill/bin" "$RT/usr/lib/nhill/qt6/plugins" "$RT/usr/lib/nhill/qt6/qml"
# Modules QML : imports du code + dépendances déclarées dans les qmldir
mapfile -t imports < <(grep -rhoE '^[[:space:]]*import[[:space:]]+Qt[A-Za-z0-9.]*' "$SRC/client/qml" | awk '{print $2}' | sort -u)
queue=("${imports[@]}") seen=" "
while (( ${#queue[@]} )); do
  m="${queue[0]}"; queue=("${queue[@]:1}")
  [[ "$seen" == *" $m "* ]] && continue; seen+="$m "
  src="$QTQML/${m//.//}"
  if [[ ! -f "$src/qmldir" ]]; then
    # module requis indirectement : paquet qml6-module-<module> installé depuis APT
    pkg="qml6-module-$(tr 'A-Z.' 'a-z-' <<<"$m")"
    log "module QML $m requis : installation de $pkg"
    if (( EUID == 0 )); then DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends "$pkg" >/dev/null 2>&1 || true
    else sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends "$pkg" >/dev/null 2>&1 || true; fi
    [[ -f "$src/qmldir" ]] || die "module QML $m absent ($src) et paquet $pkg non installable"
  fi
  dst="$RT/usr/lib/nhill/qt6/qml/${m//.//}"; mkdir -p "$dst"
  if [[ "$m" == "QtQuick" || "$m" == "QtQml" ]]; then
    # racines : seulement leurs propres fichiers (leurs sous-dossiers sont d'autres modules)
    find "$src" -maxdepth 1 -type f -exec cp -L {} "$dst/" \;
    depth=1
  else
    # module complet, avec ses sous-modules internes (styles, implémentations)
    cp -rL "$src/." "$dst/"
    depth=10
  fi
  while read -r dep; do if [[ -n "$dep" ]]; then queue+=("$dep"); fi; done < <(
    { sed -nE 's/^(default[[:space:]]+)?(import|depends)[[:space:]]+([A-Za-z0-9_.]+).*/\3/p' "$src/qmldir"
      find "$src" -maxdepth "$depth" -name qmldir ! -path "$src/qmldir" -exec sed -nE 's/^(default[[:space:]]+)?(import|depends)[[:space:]]+([A-Za-z0-9_.]+).*/\3/p' {} +
      find "$src" -maxdepth "$depth" -name '*.qml' -exec grep -hoE '^[[:space:]]*import[[:space:]]+Qt[A-Za-z0-9_.]*' {} + | awk '{print $2}'
    } | sort -u)
done
# Greffons Qt : X11 (+ intégrations GL), SVG/JPEG, TLS OpenSSL, saisie (touches mortes, IBus)
REQUIRED_PLUGINS=(platforms/libqxcb.so platforms/libqoffscreen.so imageformats/libqsvg.so
                  imageformats/libqjpeg.so iconengines/libqsvgicon.so tls/libqopensslbackend.so
                  platforminputcontexts/libcomposeplatforminputcontextplugin.so)
OPTIONAL_PLUGINS=(xcbglintegrations/libqxcb-glx-integration.so xcbglintegrations/libqxcb-egl-integration.so
                  platforminputcontexts/libibusplatforminputcontextplugin.so platformthemes/libqxdgdesktopportal.so
                  platformthemes/libqgtk3.so)
for p in "${REQUIRED_PLUGINS[@]}" "${OPTIONAL_PLUGINS[@]}"; do
  if [[ ! -f "$QTPLUG/$p" ]]; then
    if printf '%s\n' "${REQUIRED_PLUGINS[@]}" | grep -qx "$p"; then die "greffon Qt requis absent : $QTPLUG/$p"; fi
    continue
  fi
  mkdir -p "$RT/usr/lib/nhill/qt6/plugins/${p%/*}"; cp -L "$QTPLUG/$p" "$RT/usr/lib/nhill/qt6/plugins/$p"
done
printf '[Paths]\nPrefix = /usr/lib/nhill/qt6\nLibraries = /usr/lib/nhill/lib\nPlugins = plugins\nQmlImports = qml\nQml2Imports = qml\n' \
  >"$RT/usr/lib/nhill/bin/qt.conf"
# Les applications (compilées) servent à déterminer les bibliothèques nécessaires
APPS_TMP="$TMP/apps"; mkdir -p "$APPS_TMP"
for app in nhill-business nhill-pos nhill-console; do cp "$BUILD/client/$app" "$APPS_TMP/"; done
cp -r "$RT/usr/lib/nhill/qt6" "$APPS_TMP/qt6"
bundle_closure "$APPS_TMP" "$RT/usr/lib/nhill/lib"
bundle_closure "$RT" "$RT/usr/lib/nhill/lib"
strip_all "$RT"
set_runpath "$RT" "/usr/lib/nhill/lib"
docs "$RT" nhill-qt6-runtime
D_RT="$(drop_bundled "$(shlib_deps "$RT" nhill-qt6-runtime "$RT/usr/lib/nhill/lib")" "$RT/usr/lib/nhill/lib")"
check_trixie "$D_RT"
control "$RT" nhill-qt6-runtime libs "$D_RT" "" "gnome-keyring | kwalletmanager" \
"Qt 6.4 runtime for the Nhill applications
Private copy of Qt 6.4.2 (Core, Gui, Network, Qml, Quick, Quick Controls, Svg),
QtKeychain and ICU 74, with the Qt plugins and QML modules used by Nhill Business,
Nhill POS and Nhill Console. Installed under /usr/lib/nhill; it does not replace
the Qt of the system."
DEBS=("$(build_deb "$RT" nhill-qt6-runtime)")

# ------------------------------------------------------------------ applications
declare -A DESC=(
  [nhill-business]="Nhill Business — sales, invoices, stock and payments
Desktop application for traders, independent entrepreneurs and SMEs. Connects to
a Nhill server; no database credential is stored on the computer."
  [nhill-pos]="Nhill POS — cash register for a Nhill Business establishment
Cash sessions with opening float and closing count, sales with several payment
methods, returns and voids with supervisor approval, receipts and X/Z reports."
  [nhill-console]="Nhill Console — private administration of the Nhill platform
For the Nhill operators: establishments, subscriptions, plans, promo codes, payment
providers, support tickets, audit and server installation."
)
for app in nhill-business nhill-pos nhill-console; do
  log "$app"
  R="$TMP/$app"; DESTDIR="$R" cmake --install "$BUILD" --component "${app#nhill-}" --prefix /usr >/dev/null || die "installation $app"
  mkdir -p "$R/usr/lib/nhill/bin"; mv "$R/usr/bin/$app" "$R/usr/lib/nhill/bin/$app"
  ln -s "../lib/nhill/bin/$app" "$R/usr/bin/$app"
  strip_all "$R"; set_runpath "$R" "/usr/lib/nhill/lib"
  # dépendances résiduelles : uniquement celles qui ne sont pas embarquées
  D="$(drop_bundled "$(shlib_deps "$R" "$app" "$RT/usr/lib/nhill/lib")" "$RT/usr/lib/nhill/lib")"
  check_trixie "$D"
  D="nhill-qt6-runtime (= $DEBV)${D:+, $D}"
  docs "$R" "$app"
  control "$R" "$app" misc "$D" "" "" "${DESC[$app]}"
  DEBS+=("$(build_deb "$R" "$app")")
done

# ------------------------------------------------------------------ serveur
log "nhill-server"
S="$TMP/nhill-server"; DESTDIR="$S" cmake --install "$BUILD" --component server --prefix /usr >/dev/null || die "installation serveur"
bundle_closure "$S/usr/bin" "$S/usr/lib/nhill-server"
strip_all "$S"; set_runpath "$S" "/usr/lib/nhill-server"
D="$(drop_bundled "$(shlib_deps "$S" nhill-server "$S/usr/lib/nhill-server")" "$S/usr/lib/nhill-server")"
D="${D:+$D, }adduser, systemd"
check_trixie "$D"
mkdir -p "$S/DEBIAN"
for f in postinst prerm postrm; do install -m 0755 "$SRC/packaging/debian/server/$f" "$S/DEBIAN/$f"; done
docs "$S" nhill-server
control "$S" nhill-server net "$D" "postgresql (>= 15) | postgresql-client, caddy" "age" \
"Nhill API server (multi-establishment backend)
Serves Nhill Business, Nhill POS and Nhill Console over HTTPS (behind a reverse
proxy). On first start without a database it runs in setup mode: the operator
installs the PostgreSQL / Supabase database from Nhill Console with a one-time
token printed in the journal."
DEBS+=("$(build_deb "$S" nhill-server)")

( cd "$OUT" && sha256sum -- "${DEBS[@]##*/}" > SHA256SUMS )
log "paquets Debian 13 :"; printf '   %s\n' "${DEBS[@]##*/}" >&2

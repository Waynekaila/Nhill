#!/usr/bin/env bash
# build-deb.sh — compile le projet Nhill (C++17 / CMake / Qt 6) et construit ses paquets
# Debian binaires pour Debian 13 (Trixie) : un paquet par composant d'installation CMake
# (nhill-server, nhill-business, nhill-console, nhill-pos).
#
#   sudo ./build-deb.sh [options]          (ou en utilisateur avec sudo disponible)
#
# Options :
#   -s, --source DIR       racine du code source (défaut : dossier contenant CMakeLists.txt,
#                          recherché depuis le dossier du script puis le dossier courant)
#   -o, --output DIR       dossier de sortie des .deb (défaut : ./dist)
#   -c, --components LIST  composants à empaqueter, séparés par des virgules (défaut : tous)
#   -j, --jobs N           compilation parallèle (défaut : nombre de processeurs)
#   -r, --revision REV     révision Debian (défaut : 1)
#   -a, --arch ARCH        architecture cible (défaut : architecture de la machine ;
#                          seule la compilation native est prise en charge)
#       --no-tests         ne pas exécuter les tests (ctest)
#       --no-install-deps  ne pas installer les dépendances (vérification seulement)
#       --force-distro     accepter une autre distribution que Debian 13 (non recommandé)
#       --build-dir DIR    dossier de compilation persistant (recompilation incrémentale)
#       --keep-temp        conserver le dossier temporaire (diagnostic)
#   -h, --help             aide
#
# Le code source n'est jamais modifié : compilation hors de l'arborescence dans un dossier
# temporaire, empreinte du source vérifiée avant/après. Les dépendances sont déduites des
# fichiers CMake du projet (find_package, pkg_check_modules, find_library), de ses imports
# QML et de ses scripts de maintenance, avec les noms de paquets documentés dans
# scripts/build-linux.sh et docs/installation.md ; les dépendances d'exécution des
# binaires sont calculées par dpkg-shlibdeps et dpkg -S.
#
# État de vérification : script exécuté de bout en bout sur Ubuntu 24.04 (option
# --force-distro), dérivé Debian aux noms de paquets identiques. Il n'a pas pu être
# exécuté sur Debian 13 dans l'environnement de développement (miroirs Debian
# inaccessibles) : les versions Trixie (Qt 6.8, libpqxx 7.10, cpp-httplib 0.18,
# PostgreSQL 17) diffèrent de celles avec lesquelles le projet a été testé.

set -Eeuo pipefail
shopt -s nullglob
umask 022
export LC_ALL=C.UTF-8 LANG=C.UTF-8

readonly PROG="${0##*/}"
STEP="initialisation"
TMP_ROOT=""
KEEP_TEMP=0

# ------------------------------------------------------------------ messages
if [[ -t 2 ]]; then C_R=$'\e[31m'; C_G=$'\e[32m'; C_Y=$'\e[33m'; C_B=$'\e[1m'; C_0=$'\e[0m'; else C_R=; C_G=; C_Y=; C_B=; C_0=; fi
log()  { printf '%s==>%s %s\n' "$C_B" "$C_0" "$*" >&2; }
ok()   { printf '%s  ok%s %s\n' "$C_G" "$C_0" "$*" >&2; }
warn() { printf '%sattention:%s %s\n' "$C_Y" "$C_0" "$*" >&2; }
die()  { printf '%serreur [%s]:%s %s\n' "$C_R" "$STEP" "$C_0" "$*" >&2; exit 1; }
step() { STEP="$1"; log "$1"; }

stop_test_clusters() {
    # Grappes PostgreSQL jetables des tests du projet (/tmp/nhtest-*, /tmp/nhsetup-*).
    local pids
    pids="$(pgrep -u postgres -f -- '-D /tmp/nh(test|setup)-' 2>/dev/null || true)"
    if [[ -n "$pids" ]]; then kill $pids 2>/dev/null || true; fi
}

on_error() {
    local rc=$? line=${1:-?}
    printf '%serreur [%s]:%s commande échouée (code %s) ligne %s : %s\n' \
        "$C_R" "$STEP" "$C_0" "$rc" "$line" "${BASH_COMMAND}" >&2
    exit "$rc"
}
trap 'on_error $LINENO' ERR

cleanup() {
    local rc=$?
    stop_test_clusters || true
    if [[ -n "$TMP_ROOT" && -d "$TMP_ROOT" ]]; then
        if (( KEEP_TEMP )); then
            warn "dossier temporaire conservé : $TMP_ROOT"
        else
            # Seul le dossier créé par mktemp est supprimé, après vérification de son préfixe.
            case "$TMP_ROOT" in
                "${TMPDIR:-/tmp}"/nhill-deb.*) rm -rf --one-file-system -- "$TMP_ROOT" ;;
                *) warn "dossier temporaire inattendu non supprimé : $TMP_ROOT" ;;
            esac
        fi
    fi
    exit "$rc"
}
trap cleanup EXIT
trap 'die "interrompu"' INT TERM

usage() { sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'; }

# ------------------------------------------------------------------ options
SRC="" OUT="$PWD/dist" COMPONENTS="" JOBS="" REVISION="1" ARCH="" SERVER_URL=""
RUN_TESTS=1 INSTALL_DEPS=1 FORCE_DISTRO=0 BUILD_DIR=""
while (( $# )); do
    case "$1" in
        -s|--source)     SRC="${2:?--source demande un dossier}"; shift 2 ;;
        -o|--output)     OUT="${2:?--output demande un dossier}"; shift 2 ;;
        -c|--components) COMPONENTS="${2:?--components demande une liste}"; shift 2 ;;
        -j|--jobs)       JOBS="${2:?--jobs demande un nombre}"; shift 2 ;;
        -r|--revision)   REVISION="${2:?--revision demande une valeur}"; shift 2 ;;
        -a|--arch)       ARCH="${2:?--arch demande une architecture}"; shift 2 ;;
        --server-url)    echo "--server-url : option retirée en 3.0.0 : aucune adresse de serveur n'est compilée dans les applications (saisie au premier lancement) / removed in 3.0.0: no server address is compiled into the applications" >&2; exit 2 ;;
        --no-tests)      RUN_TESTS=0; shift ;;
        --no-install-deps) INSTALL_DEPS=0; shift ;;
        --force-distro)  FORCE_DISTRO=1; shift ;;
        --keep-temp)     KEEP_TEMP=1; shift ;;
        --build-dir)     BUILD_DIR="${2:?--build-dir demande un dossier}"; shift 2 ;;
        -h|--help)       usage; exit 0 ;;
        *) die "option inconnue : $1 (voir $PROG --help)" ;;
    esac
done
[[ -z "$JOBS" ]] && JOBS="$(nproc 2>/dev/null || echo 2)"
[[ "$JOBS" =~ ^[1-9][0-9]*$ ]] || die "--jobs doit être un entier positif (reçu : $JOBS)"
[[ "$REVISION" =~ ^[A-Za-z0-9.+~]+$ ]] || die "révision Debian invalide : $REVISION"

# ------------------------------------------------------------------ privilèges
SUDO=()
if (( EUID != 0 )); then
    if command -v sudo >/dev/null 2>&1; then SUDO=(sudo); else SUDO=(); fi
fi
as_root() {
    if (( EUID == 0 )); then "$@"
    elif (( ${#SUDO[@]} )); then "${SUDO[@]}" "$@"
    else die "droits administrateur requis pour : $* (lancez le script avec sudo)"; fi
}

# ------------------------------------------------------------------ distribution
step "Vérification du système"
[[ -r /etc/os-release ]] || die "/etc/os-release introuvable : système non pris en charge"
# shellcheck disable=SC1091
. /etc/os-release
DISTRO="${ID:-?} ${VERSION_ID:-?} (${VERSION_CODENAME:-?})"
if [[ "${ID:-}" == "debian" && ( "${VERSION_ID:-}" == "13" || "${VERSION_CODENAME:-}" == "trixie" ) ]]; then
    ok "Debian 13 (Trixie) détecté"
elif (( FORCE_DISTRO )); then
    warn "distribution $DISTRO : ce script vise Debian 13 ; poursuite demandée par --force-distro"
else
    die "distribution $DISTRO : ce script construit des paquets pour Debian 13 (Trixie). Utilisez --force-distro pour passer outre."
fi
for t in apt-get apt-cache dpkg dpkg-deb dpkg-query; do
    command -v "$t" >/dev/null 2>&1 || die "outil Debian manquant : $t"
done
HOST_ARCH="$(dpkg --print-architecture)"
[[ -z "$ARCH" ]] && ARCH="$HOST_ARCH"
if [[ "$ARCH" != "$HOST_ARCH" ]]; then
    die "architecture demandée « $ARCH » différente de la machine « $HOST_ARCH » : la compilation croisée de Qt n'est pas prise en charge ; lancez ce script sur une machine $ARCH (architectures étrangères configurées : $(dpkg --print-foreign-architectures | tr '\n' ' '))"
fi
ok "architecture : $ARCH"

# ------------------------------------------------------------------ source
step "Détection du code source"
find_source() {
    local d
    for d in "$(cd "$(dirname "$0")" && pwd)" "$PWD"; do
        while [[ "$d" != "/" ]]; do
            if [[ -f "$d/CMakeLists.txt" ]] && grep -qiE '^[[:space:]]*project[[:space:]]*\(' "$d/CMakeLists.txt"; then
                echo "$d"; return 0
            fi
            d="$(dirname "$d")"
        done
    done
    return 1
}
if [[ -z "$SRC" ]]; then SRC="$(find_source)" || die "aucun CMakeLists.txt racine trouvé : indiquez le code source avec --source"; fi
[[ -d "$SRC" ]] || die "dossier source introuvable : $SRC"
SRC="$(cd "$SRC" && pwd)"
OUT="$(mkdir -p "$OUT" && cd "$OUT" && pwd)" || die "impossible de créer le dossier de sortie $OUT"
case "$OUT/" in "$SRC"/*) warn "le dossier de sortie est dans l'arborescence source ($OUT) : seul ce dossier y sera écrit" ;; esac

# Langage et système de construction
BUILD_SYSTEM=""
if [[ -f "$SRC/CMakeLists.txt" ]]; then BUILD_SYSTEM="cmake"
elif [[ -f "$SRC/meson.build" ]]; then BUILD_SYSTEM="meson"
elif [[ -f "$SRC/configure.ac" || -f "$SRC/configure" ]]; then BUILD_SYSTEM="autotools"
elif [[ -f "$SRC/Cargo.toml" ]]; then BUILD_SYSTEM="cargo"
elif [[ -f "$SRC/go.mod" ]]; then BUILD_SYSTEM="go"
elif [[ -f "$SRC/pyproject.toml" || -f "$SRC/setup.py" ]]; then BUILD_SYSTEM="python"
elif [[ -f "$SRC/package.json" ]]; then BUILD_SYSTEM="npm"
elif [[ -f "$SRC/Makefile" ]]; then BUILD_SYSTEM="make"
fi
[[ "$BUILD_SYSTEM" == "cmake" ]] || die "système de construction détecté : « ${BUILD_SYSTEM:-inconnu} ». Ce script prend en charge le projet CMake/C++ (Nhill) uniquement."

PROJECT_LINE="$(grep -iE '^[[:space:]]*project[[:space:]]*\(' "$SRC/CMakeLists.txt" | head -n1)"
PROJECT_NAME="$(sed -E 's/^[[:space:]]*[Pp][Rr][Oo][Jj][Ee][Cc][Tt][[:space:]]*\([[:space:]]*([A-Za-z0-9_.+-]+).*/\1/' <<<"$PROJECT_LINE" | tr 'A-Z' 'a-z')"
PROJECT_VERSION="$(sed -nE 's/.*VERSION[[:space:]]+([0-9]+(\.[0-9]+)*).*/\1/p' <<<"$PROJECT_LINE")"
[[ -n "$PROJECT_NAME" ]] || die "nom du projet introuvable dans project()"
[[ -n "$PROJECT_VERSION" ]] || die "version introuvable dans project(... VERSION x.y.z)"
LANGS="$(sed -nE 's/.*LANGUAGES[[:space:]]+([A-Za-z ]+).*/\1/p' <<<"$PROJECT_LINE")"
CXX_STD="$(grep -hoE 'CMAKE_CXX_STANDARD[[:space:]]+[0-9]+' "$SRC/CMakeLists.txt" | grep -oE '[0-9]+$' | head -n1 || true)"
USES_QT=0; grep -rqsE 'find_package\(Qt6' --include=CMakeLists.txt --include='*.cmake' "$SRC" && USES_QT=1
ok "projet ${PROJECT_NAME} ${PROJECT_VERSION} — CMake, langage(s) ${LANGS:-CXX}, C++${CXX_STD:-?}$( ((USES_QT)) && echo ', Qt 6 / QML')"
DEB_VERSION="${PROJECT_VERSION}-${REVISION}"

# Empreinte du source (doit rester identique). Les dossiers générés et ignorés par git
# (build/ : journaux, dist/ et release/ : livrables) ne font pas partie du source.
source_digest() {
    (cd "$SRC" && find . \( -path ./.git -o -path ./build -o -path ./dist -o -path ./release \) -prune \
        -o -type f ! -path "./${OUT#"$SRC"/}/*" -print0 \
        | LC_ALL=C sort -z | xargs -0 sha256sum) | sha256sum | cut -d' ' -f1
}
SRC_DIGEST="$(source_digest)"

# ------------------------------------------------------------------ dépendances
step "Déduction des dépendances depuis les fichiers CMake"
CMAKE_FILES=()
while IFS= read -r -d '' f; do CMAKE_FILES+=("$f"); done < <(find "$SRC" \( -path "$SRC/.git" -o -name CMakeFiles -o -name '_deps' -o -path "$OUT" \
                 -o -exec test -e '{}/CMakeCache.txt' ';' \) -prune -o \( -name CMakeLists.txt -o -name '*.cmake' \) -type f -print0)
cmake_grep() { grep -hoE "$1" "${CMAKE_FILES[@]}" 2>/dev/null || true; }

# Noms de paquets Debian des modules utilisés par le projet, tels que listés dans
# scripts/build-linux.sh et docs/installation.md (vérifiés ensuite dans apt).
declare -A PKGCONFIG_MAP=(
    [libpqxx]=libpqxx-dev [libsodium]=libsodium-dev [libpng]=libpng-dev [cpp-httplib]=libcpp-httplib-dev
    [libcurl]=libcurl4-openssl-dev
)
declare -A FINDPKG_MAP=(
    [OpenSSL]=libssl-dev [nlohmann_json]=nlohmann-json3-dev [Threads]="" [PkgConfig]=pkgconf
    [GTest]=libgtest-dev [Qt6Keychain]=qtkeychain-qt6-dev
)
declare -A FINDLIB_MAP=( [hpdf]=libhpdf-dev )
declare -A QT_COMPONENT_MAP=(
    [Core]=qt6-base-dev [Gui]=qt6-base-dev [Network]=qt6-base-dev [Test]=qt6-base-dev
    [Qml]="qt6-declarative-dev qt6-declarative-dev-tools" [Quick]="qt6-declarative-dev qt6-declarative-dev-tools"
    [QuickControls2]=qt6-declarative-dev [Svg]=qt6-svg-dev [LinguistTools]="qt6-tools-dev qt6-l10n-tools"
)
BUILD_DEPS=(build-essential cmake ninja-build pkgconf dpkg-dev file)
UNMAPPED=()
add_dep() { local p; for p in "$@"; do [[ -n "$p" ]] && BUILD_DEPS+=("$p"); done; }

# pkg_check_modules
while read -r mod; do
    [[ -z "$mod" ]] && continue
    if [[ -v PKGCONFIG_MAP[$mod] ]]; then add_dep "${PKGCONFIG_MAP[$mod]}"; else UNMAPPED+=("pkg-config:$mod"); fi
done < <(cmake_grep 'pkg_check_modules\([A-Z_]+[[:space:]]+(REQUIRED[[:space:]]+|QUIET[[:space:]]+)?(IMPORTED_TARGET[[:space:]]+)?[A-Za-z0-9_.+-]+' | awk '{print $NF}' | sort -u)
# find_package
while read -r name; do
    [[ -z "$name" || "$name" == "Qt6" ]] && continue
    if [[ -v FINDPKG_MAP[$name] ]]; then add_dep ${FINDPKG_MAP[$name]}; else UNMAPPED+=("find_package:$name"); fi
done < <(cmake_grep 'find_package\([A-Za-z0-9_]+' | sed 's/find_package(//' | sort -u)
# Composants Qt
while read -r comp; do
    [[ -z "$comp" ]] && continue
    if [[ -v QT_COMPONENT_MAP[$comp] ]]; then add_dep ${QT_COMPONENT_MAP[$comp]}; else UNMAPPED+=("Qt6:$comp"); fi
done < <(cmake_grep 'find_package\(Qt6[^)]*COMPONENTS[^)]*\)' | sed -E 's/.*COMPONENTS//; s/\)//' | tr ' ' '\n' | grep -E '^[A-Z]' | sort -u)
# find_library
while read -r lib; do
    [[ -z "$lib" ]] && continue
    if [[ -v FINDLIB_MAP[$lib] ]]; then add_dep "${FINDLIB_MAP[$lib]}"; else UNMAPPED+=("find_library:$lib"); fi
done < <(cmake_grep 'find_library\([A-Z_]+[[:space:]]+NAMES[[:space:]]+[A-Za-z0-9_+-]+' | awk '{print $NF}' | sort -u)
(( ${#UNMAPPED[@]} == 0 )) || die "dépendances CMake sans correspondance de paquet Debian connue : ${UNMAPPED[*]} — complétez les tables *_MAP de ce script"

# Modules QML importés par les applications (exécution et tests hors écran)
QML_IMPORTS=()
if (( USES_QT )); then
    while read -r imp; do QML_IMPORTS+=("$imp"); done < <(
        find "$SRC" -path "$SRC/.git" -prune -o -name '*.qml' -type f -print0 \
        | xargs -0 grep -hoE '^[[:space:]]*import[[:space:]]+(Qt[A-Za-z0-9.]*)' 2>/dev/null \
        | awk '{print $2}' | sort -u)
    for imp in "${QML_IMPORTS[@]}"; do
        # Convention Debian : QtQuick.Controls -> qml6-module-qtquick-controls
        add_dep "qml6-module-$(tr 'A-Z.' 'a-z-' <<<"$imp")"
    done
    add_dep qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtqml-workerscript
    # Greffon de plateforme X11 de Qt (paquet séparé sur Debian 13, intégré à libqt6gui6 ailleurs)
    cand="$(apt-cache policy qt6-qpa-plugins 2>/dev/null | awk '/Candidate:/ {print $2}')"
    [[ -n "$cand" && "$cand" != "(none)" ]] && add_dep qt6-qpa-plugins
fi
# Tests : programmes externes appelés par les sources des tests (std::system / popen),
# par ex. initdb/pg_ctl/psql (grappes PostgreSQL jetables) et pdftotext/pdfimages (PDF).
declare -A TEST_TOOL_MAP=(
    [initdb]=postgresql [pg_ctl]=postgresql [psql]=postgresql-client
    [pdftotext]=poppler-utils [pdfimages]=poppler-utils [pdfinfo]=poppler-utils [pdftoppm]=poppler-utils
    [runuser]="" [chown]="" [sh]=""
)
if (( RUN_TESTS )) && [[ -d "$SRC/tests" ]]; then
    while read -r tool; do
        [[ -z "$tool" ]] && continue
        if [[ -v TEST_TOOL_MAP[$tool] ]]; then add_dep ${TEST_TOOL_MAP[$tool]}
        else UNMAPPED+=("test:$tool"); fi
    done < <(grep -rhoE '(std::system|popen|sh)\(.*|"(initdb|pg_ctl|psql|pdf[a-z]+|runuser|chown)[ /"]|/(initdb|pg_ctl)[ "]' \
                 --include='*.cpp' --include='*.hpp' "$SRC/tests" 2>/dev/null \
             | grep -oE '\b(initdb|pg_ctl|psql|pdf[a-z]+|runuser|chown)\b' | sort -u)
    # procps (pgrep) sert à arrêter les grappes PostgreSQL laissées par les tests
    add_dep procps
    (( ${#UNMAPPED[@]} == 0 )) || die "outils de test sans paquet Debian connu : ${UNMAPPED[*]}"
fi
mapfile -t BUILD_DEPS < <(printf '%s\n' "${BUILD_DEPS[@]}" | sort -u)
ok "${#BUILD_DEPS[@]} paquets requis : ${BUILD_DEPS[*]}"

step "Vérification de la disponibilité des paquets ($ARCH)"
missing_pkgs() {
    local p st
    for p in "$@"; do
        st="$(dpkg-query -W -f='${db:Status-Status}' "$p" 2>/dev/null || true)"
        [[ "$st" == "installed" ]] || echo "$p"
    done
}
mapfile -t MISSING < <(missing_pkgs "${BUILD_DEPS[@]}")
if (( ${#MISSING[@]} )); then
    if (( INSTALL_DEPS )); then
        log "installation de ${#MISSING[@]} paquet(s) manquant(s) : ${MISSING[*]}"
        as_root apt-get update -q || die "apt-get update a échoué (réseau ou sources APT)"
        UNAVAILABLE=()
        for p in "${MISSING[@]}"; do
            cand="$(apt-cache policy "$p" 2>/dev/null | awk '/Candidate:/ {print $2}')"
            [[ -n "$cand" && "$cand" != "(none)" ]] || UNAVAILABLE+=("$p")
        done
        (( ${#UNAVAILABLE[@]} == 0 )) || die "paquets indisponibles pour $ARCH dans les sources APT : ${UNAVAILABLE[*]} (activez la section main de Debian 13 ou vérifiez l'architecture)"
        as_root env DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends "${MISSING[@]}" \
            || die "l'installation des dépendances a échoué"
        mapfile -t STILL < <(missing_pkgs "${BUILD_DEPS[@]}")
        (( ${#STILL[@]} == 0 )) || die "dépendances toujours absentes après installation : ${STILL[*]}"
    else
        die "dépendances manquantes (relancer sans --no-install-deps ou installer) : ${MISSING[*]}"
    fi
fi
ok "toutes les dépendances de compilation sont installées"
command -v lintian >/dev/null 2>&1 || warn "lintian absent : contrôle qualité des paquets ignoré (apt install lintian)"

# ------------------------------------------------------------------ compilation
step "Préparation du dossier temporaire"
TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/nhill-deb.XXXXXXXX")"
chmod 0755 "$TMP_ROOT"
if [[ -n "$BUILD_DIR" ]]; then
    mkdir -p "$BUILD_DIR" || die "impossible de créer $BUILD_DIR"
    BUILD="$(cd "$BUILD_DIR" && pwd)"
    case "$BUILD/" in "$SRC"/*) die "--build-dir ne doit pas être dans le code source ($BUILD)" ;; esac
else
    BUILD="$TMP_ROOT/build"
fi
STAGE="$TMP_ROOT/stage"
mkdir -p "$BUILD" "$STAGE"
ok "$TMP_ROOT"

step "Configuration CMake (drapeaux de durcissement Debian)"
# dpkg-buildflags : -O2, -fstack-protector-strong, _FORTIFY_SOURCE, relro/now, etc.
eval "$(dpkg-buildflags --export=sh)"
# CMake ignore CPPFLAGS : comme debhelper, on les ajoute aux drapeaux du compilateur.
export CFLAGS="${CFLAGS:-} ${CPPFLAGS:-}" CXXFLAGS="${CXXFLAGS:-} ${CPPFLAGS:-}"
CMAKE_ARGS=(-S "$SRC" -B "$BUILD" -G Ninja
    -DCMAKE_BUILD_TYPE=None -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc
    -DCMAKE_INSTALL_LOCALSTATEDIR=/var -DCMAKE_VERBOSE_MAKEFILE=OFF
    -DNHILL_BUILD_SERVER=ON -DNHILL_BUILD_CLIENT=ON -DNHILL_BUILD_TESTS="$( ((RUN_TESTS)) && echo ON || echo OFF)")
cmake "${CMAKE_ARGS[@]}" >"$TMP_ROOT/cmake-configure.log" 2>&1 \
    || { tail -n 40 "$TMP_ROOT/cmake-configure.log" >&2; die "configuration CMake échouée (journal ci-dessus)"; }
grep -q 'QtKeychain found' "$TMP_ROOT/cmake-configure.log" \
    || warn "QtKeychain non trouvé par CMake : les applications ne mémoriseront pas la session"
ok "configuration terminée"

step "Compilation ($JOBS tâches)"
cmake --build "$BUILD" --parallel "$JOBS" >"$TMP_ROOT/build.log" 2>&1 \
    || { grep -E 'error|Error|FAILED' "$TMP_ROOT/build.log" | head -n 40 >&2; die "compilation échouée (extrait ci-dessus ; journal complet avec --keep-temp)"; }
WARNINGS="$(grep -c 'warning:' "$TMP_ROOT/build.log" || true)"
ok "compilation terminée (${WARNINGS} avertissement(s))"

# ------------------------------------------------------------------ tests
if (( RUN_TESTS )); then
    step "Tests (ctest)"
    PG_BIN=""
    for d in /usr/lib/postgresql/*/bin; do [[ -x "$d/initdb" ]] && PG_BIN="$d"; done
    [[ -n "$PG_BIN" ]] || die "initdb introuvable sous /usr/lib/postgresql/*/bin (paquet postgresql)"
    if (( EUID == 0 )); then id postgres >/dev/null 2>&1 || die "compte postgres absent (paquet postgresql)"; fi
    NH_PG_BIN="$PG_BIN" QT_QPA_PLATFORM=offscreen \
        ctest --test-dir "$BUILD" --output-on-failure --timeout 900 -j1 >"$TMP_ROOT/ctest.log" 2>&1 \
        || { tail -n 60 "$TMP_ROOT/ctest.log" >&2; stop_test_clusters; die "des tests ont échoué (extrait ci-dessus)"; }
    stop_test_clusters
    ok "$(grep -E 'tests passed' "$TMP_ROOT/ctest.log" | tail -n1)"
else
    warn "tests non exécutés (--no-tests)"
fi

# ------------------------------------------------------------------ composants
step "Détection des composants d'installation"
mapfile -t ALL_COMPONENTS < <(grep -rhoE 'CMAKE_INSTALL_COMPONENT STREQUAL "[A-Za-z0-9_-]+"' --include=cmake_install.cmake "$BUILD" \
    | sed -E 's/.*"(.*)"/\1/' | grep -vx 'Unspecified' | sort -u)
(( ${#ALL_COMPONENTS[@]} )) || die "aucun composant install(... COMPONENT ...) déclaré dans le projet"
if grep -rqsE 'CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified"' --include=cmake_install.cmake "$BUILD"; then
    warn "des fichiers sans COMPONENT existent dans le projet : ils ne sont dans aucun paquet"
fi
if [[ -n "$COMPONENTS" ]]; then
    IFS=',' read -r -a WANTED <<<"$COMPONENTS"
    for c in "${WANTED[@]}"; do printf '%s\n' "${ALL_COMPONENTS[@]}" | grep -qx "$c" || die "composant inconnu : $c (disponibles : ${ALL_COMPONENTS[*]})"; done
else
    WANTED=("${ALL_COMPONENTS[@]}")
fi
ok "composants : ${WANTED[*]}"

# Métadonnées déclarées par le projet (cmake/…Packaging.cmake via CPackConfig.cmake)
cpack_var() {
    local cfg="$BUILD/CPackConfig.cmake"
    [[ -f "$cfg" ]] || return 0
    cat >"$TMP_ROOT/getvar.cmake" <<'EOF'
include("${CFG}")
if(DEFINED ${VAR})
  string(REPLACE ";" ", " _v "${${VAR}}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${_v}")
endif()
EOF
    cmake -DCFG="$cfg" -DVAR="$1" -P "$TMP_ROOT/getvar.cmake" 2>/dev/null || true
}
MAINTAINER="${DEBFULLNAME:+$DEBFULLNAME <${DEBEMAIL:-}>}"
[[ -z "$MAINTAINER" ]] && MAINTAINER="$(cpack_var CPACK_PACKAGE_CONTACT)"
[[ -n "$MAINTAINER" ]] || die "mainteneur inconnu : définissez DEBFULLNAME et DEBEMAIL"
MULTIARCH="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"

# ------------------------------------------------------------------ outils de paquet
deps_from_elves() {
    # dpkg-shlibdeps pour les ELF d'un dossier ; renvoie la liste Depends.
    local root="$1" pkg="$2" elves=() f
    while IFS= read -r -d '' f; do
        file -b "$f" | grep -q '^ELF' && elves+=("$f")
    done < <(find "$root" -type f \( -perm -u+x -o -name '*.so*' \) ! -path "$root/DEBIAN/*" -print0)
    (( ${#elves[@]} )) || return 0
    local work="$TMP_ROOT/shlibdeps-$pkg"
    mkdir -p "$work/debian"
    printf 'Source: %s\n\nPackage: %s\nArchitecture: any\n' "$PROJECT_NAME" "$pkg" >"$work/debian/control"
    (cd "$work" && dpkg-shlibdeps -O -x"$pkg" "${elves[@]}" 2>"$work/err") \
        | sed -n 's/^shlibs:Depends=//p' || { cat "$work/err" >&2; die "dpkg-shlibdeps a échoué pour $pkg"; }
}
pkg_of_path() { dpkg -S "$1" 2>/dev/null | head -n1 | cut -d: -f1 || true; }   # vide si aucun paquet
ensure_qml_module() {
    # Module QML requis indirectement mais absent : paquet Debian déduit de la convention
    # qml6-module-<module>, vérifié dans APT puis installé.
    local m="$1" dir="$2" pkg cand
    pkg="qml6-module-$(tr 'A-Z.' 'a-z-' <<<"$m")"
    cand="$(apt-cache policy "$pkg" 2>/dev/null | awk '/Candidate:/ {print $2}')"
    [[ -n "$cand" && "$cand" != "(none)" ]] || die "module QML $m requis mais introuvable ($dir) et paquet $pkg indisponible"
    (( INSTALL_DEPS )) || die "module QML $m requis : installez $pkg (ou relancez sans --no-install-deps)"
    log "module QML $m requis indirectement : installation de $pkg"
    as_root env DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends "$pkg" >&2 \
        || die "installation de $pkg échouée"
    [[ -f "$dir/qmldir" ]] || die "$pkg installé mais $dir/qmldir toujours absent"
}
qml_runtime_deps() {
    # Paquet fournissant chaque module QML importé par le code, avec ses dépendances
    # transitives déclarées dans les qmldir (import, default import, depends ; les
    # « optional import » sont ignorés), résolu par dpkg -S.
    local qmlroot="/usr/lib/$MULTIARCH/qt6/qml" queue=("${QML_IMPORTS[@]}") seen=" " m dir p out=() dep cand
    while (( ${#queue[@]} )); do
        m="${queue[0]}"; queue=("${queue[@]:1}")
        [[ "$seen" == *" $m "* ]] && continue
        seen+="$m "
        dir="$qmlroot/${m//.//}"
        [[ -f "$dir/qmldir" ]] || ensure_qml_module "$m" "$dir"
        p="$(pkg_of_path "$dir/qmldir")"
        [[ -n "$p" ]] || die "paquet du module QML $m introuvable (dpkg -S $dir/qmldir)"
        out+=("$p")
        while read -r dep; do [[ -n "$dep" ]] && queue+=("$dep"); done < <(
            sed -nE 's/^(default[[:space:]]+)?(import|depends)[[:space:]]+([A-Za-z0-9_.]+).*/\3/p' "$dir/qmldir")
    done
    # Greffon de plateforme (X11) et format d'image SVG utilisés au lancement
    p="$(pkg_of_path "/usr/lib/$MULTIARCH/qt6/plugins/platforms/libqxcb.so")"; [[ -n "$p" ]] && out+=("$p")
    # Le greffon SVG est dans un paquet séparé sur Debian 13 (qt6-svg-plugins) : installé
    # si absent, car les icônes de l'interface sont en SVG.
    local svg="/usr/lib/$MULTIARCH/qt6/plugins/imageformats/libqsvg.so"
    if [[ ! -f "$svg" ]] && (( INSTALL_DEPS )); then
        cand="$(apt-cache policy qt6-svg-plugins 2>/dev/null | awk '/Candidate:/ {print $2}')"
        if [[ -n "$cand" && "$cand" != "(none)" ]]; then
            log "greffon SVG absent : installation de qt6-svg-plugins"
            as_root env DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends qt6-svg-plugins >&2 \
                || die "installation de qt6-svg-plugins échouée"
        fi
    fi
    p="$(pkg_of_path "$svg")"
    if [[ -n "$p" ]]; then out+=("$p"); else warn "greffon SVG introuvable ($svg) : les icônes pourraient ne pas s'afficher"; fi
    printf '%s\n' "${out[@]}" | sed 's/:.*//' | sort -u | paste -sd, - | sed 's/,/, /g'
}
deps_from_scripts() {
    # Commandes non essentielles appelées par les scripts de maintenance du projet.
    local d=() f
    for f in "$@"; do
        grep -qE '(^|[^a-z])adduser([^a-z]|$)' "$f" && d+=(adduser)
        grep -qE 'deb-systemd-(helper|invoke)' "$f" && d+=("init-system-helpers")
    done
    printf '%s\n' "${d[@]}" | sort -u | paste -sd, - | sed 's/,/, /g'
}
join_deps() {
    # Fusionne des listes de dépendances ; un même paquet n'apparaît qu'une fois
    # (la première forme, souvent versionnée par dpkg-shlibdeps, est conservée).
    printf '%s\n' "$@" | tr ',' '\n' | sed 's/^ *//; s/ *$//' | grep -v '^$' \
        | awk '{ key = ($0 ~ /\|/) ? $0 : $1; if (!seen[key]++) print }' | paste -sd, - | sed 's/,/, /g'
}
wrap_description() {
    # Première ligne = résumé ; suite = description longue au format Debian.
    local text="$1" first rest
    first="$(head -n1 <<<"$text")"; rest="$(tail -n +2 <<<"$text")"
    printf 'Description: %s\n' "$first"
    if [[ -n "$rest" ]]; then
        while IFS= read -r l; do
            l="${l#"${l%%[![:space:]]*}"}"
            if [[ -z "$l" ]]; then echo " ."; else fold -s -w 78 <<<"$l" | sed 's/ *$//; s/^/ /'; fi
        done <<<"$rest"
    else
        echo " $first"
    fi
}

# Sections de l'archive Debian (Debian Policy § 2.4)
DEBIAN_SECTIONS="$(printf '%s\n' admin cli-mono comm database debug devel doc editors education electronics \
    embedded fonts games gnome gnu-r gnustep golang graphics hamradio haskell httpd interpreters introspection \
    java javascript kde kernel libdevel libs lisp localization mail math metapackages misc net news ocaml \
    oldlibs otherosfs perl php python ruby rust science shells sound tasks tex text utils vcs video web x11 xfce zope)"

# ------------------------------------------------------------------ paquets
step "Construction des paquets"
BUILT=()
for comp in "${WANTED[@]}"; do
    UP="$(tr 'a-z-' 'A-Z_' <<<"$comp")"
    PKG="$(cpack_var "CPACK_DEBIAN_${UP}_PACKAGE_NAME")"; [[ -z "$PKG" ]] && PKG="${PROJECT_NAME}-${comp}"
    PKG="$(tr 'A-Z_' 'a-z-' <<<"$PKG")"
    [[ "$PKG" =~ ^[a-z0-9][a-z0-9+.-]+$ ]] || die "nom de paquet invalide : $PKG"
    ROOT="$STAGE/$PKG"
    log "  $PKG ($comp)"
    DESTDIR="$ROOT" cmake --install "$BUILD" --component "$comp" --prefix /usr >"$TMP_ROOT/install-$comp.log" 2>&1 \
        || { tail -n 20 "$TMP_ROOT/install-$comp.log" >&2; die "installation du composant $comp échouée"; }
    [[ -n "$(find "$ROOT" -type f -print -quit)" ]] || die "le composant $comp n'installe aucun fichier"

    # Binaires : suppression des symboles (bonne pratique Debian sans paquet -dbgsym)
    while IFS= read -r -d '' f; do
        if file -b "$f" | grep -qE '^ELF.*(executable|shared object)'; then
            strip --strip-unneeded --remove-section=.comment --remove-section=.note "$f" || die "strip a échoué : $f"
        fi
    done < <(find "$ROOT" -type f -print0)
    # Permissions normalisées (dossiers 0755, fichiers 0644, exécutables 0755)
    find "$ROOT" -type d -exec chmod 0755 {} +
    find "$ROOT" -type f -exec chmod u+rw,go+r,go-w {} +

    # Documentation obligatoire : copyright (DEP-5) et changelog.Debian.gz
    DOC="$ROOT/usr/share/doc/$PKG"
    mkdir -p "$DOC"
    {
        echo "Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/"
        echo "Upstream-Name: ${PROJECT_NAME}"
        echo
        echo "Files: *"
        echo "Copyright: $(date +%Y) les créateurs de ${PROJECT_NAME}"
        echo "License: proprietary"
        echo " Licence du projet : propriétaire, à définir par ses créateurs (README.md)."
        echo " Dépendances et licences : docs/licences-dependances.md du code source."
        if [[ -d "$ROOT/usr/share/${PROJECT_NAME}/fonts" ]]; then
            echo
            echo "Files: usr/share/${PROJECT_NAME}/fonts/*"
            echo "Copyright: 2017-2018 IBM Corporation (IBM Plex)"
            echo "License: OFL-1.1"
            echo " Texte complet : /usr/share/${PROJECT_NAME}/fonts/OFL.txt"
        fi
    } >"$DOC/copyright"
    {
        printf '%s (%s) unstable; urgency=medium\n\n' "$PKG" "$DEB_VERSION"
        printf '  * Paquet binaire construit par build-deb.sh depuis le code source %s.\n\n' "$PROJECT_VERSION"
        printf ' -- %s  %s\n' "$MAINTAINER" "$(date -R)"
    } | gzip -9n >"$DOC/changelog.Debian.gz"
    chmod 0644 "$DOC"/*

    # Scripts de maintenance fournis par le projet (copiés, jamais modifiés)
    mkdir -p "$ROOT/DEBIAN"
    SCRIPTS=()
    EXTRA="$(cpack_var "CPACK_DEBIAN_${UP}_PACKAGE_CONTROL_EXTRA")"
    for s in ${EXTRA//, / }; do
        [[ -f "$s" ]] || die "script de maintenance déclaré mais absent : $s"
        name="$(basename "$s")"
        case "$name" in preinst|postinst|prerm|postrm) ;; *) die "script de maintenance inattendu : $name" ;; esac
        sh -n "$s" || die "erreur de syntaxe dans $s"
        install -m 0755 "$s" "$ROOT/DEBIAN/$name"
        SCRIPTS+=("$ROOT/DEBIAN/$name")
    done

    # Dépendances : bibliothèques (dpkg-shlibdeps), modules QML, scripts, projet
    D_SHLIBS="$(deps_from_elves "$ROOT" "$PKG")"
    D_QML=""
    USES_QUICK=0
    while IFS= read -r -d '' f; do
        readelf -d "$f" 2>/dev/null | grep -q 'NEEDED.*libQt6Quick\.so' && USES_QUICK=1
    done < <(find "$ROOT" -type f -perm -u+x -print0)
    (( USES_QUICK )) && D_QML="$(qml_runtime_deps)"
    D_SCRIPTS=""; (( ${#SCRIPTS[@]} )) && D_SCRIPTS="$(deps_from_scripts "${SCRIPTS[@]}")"
    D_PROJECT="$(cpack_var "CPACK_DEBIAN_${UP}_PACKAGE_DEPENDS")"
    DEPENDS="$(join_deps "$D_SHLIBS" "$D_QML" "$D_SCRIPTS" "$D_PROJECT")"
    RECOMMENDS="$(cpack_var "CPACK_DEBIAN_${UP}_PACKAGE_RECOMMENDS")"
    SUGGESTS="$(cpack_var "CPACK_DEBIAN_${UP}_PACKAGE_SUGGESTS")"
    # Le trousseau (QtKeychain/Secret Service) est documenté dans docs/installation.md
    if [[ -n "$D_QML" ]] && grep -q 'libqt6keychain' <<<"$DEPENDS"; then
        SUGGESTS="$(join_deps "$SUGGESTS" "gnome-keyring | kwalletmanager")"
    fi
    if [[ -n "$D_QML" ]]; then
        cand="$(apt-cache policy qt6-wayland 2>/dev/null | awk '/Candidate:/ {print $2}')"
        [[ -n "$cand" && "$cand" != "(none)" ]] && RECOMMENDS="$(join_deps "$RECOMMENDS" qt6-wayland)"
    fi
    SECTION="$(cpack_var "CPACK_DEBIAN_${UP}_PACKAGE_SECTION")"; [[ -z "$SECTION" ]] && SECTION="$(cpack_var CPACK_DEBIAN_PACKAGE_SECTION)"
    if [[ -n "$SECTION" ]] && ! grep -qwx -- "$SECTION" <<<"$DEBIAN_SECTIONS"; then
        warn "section « $SECTION » inconnue de Debian pour $PKG : « misc » utilisée"
        SECTION="misc"
    fi
    DESC="$(cpack_var "CPACK_COMPONENT_${UP}_DESCRIPTION")"
    [[ -z "$DESC" ]] && DESC="${PROJECT_NAME} — composant ${comp}"

    # md5sums puis control
    (cd "$ROOT" && find . -type f ! -path './DEBIAN/*' -printf '%P\0' | LC_ALL=C sort -z | xargs -0 md5sum) >"$ROOT/DEBIAN/md5sums"
    chmod 0644 "$ROOT/DEBIAN/md5sums"
    SIZE="$(du -sk --exclude=DEBIAN "$ROOT" | cut -f1)"
    {
        echo "Package: $PKG"
        echo "Version: $DEB_VERSION"
        echo "Architecture: $ARCH"
        echo "Maintainer: $MAINTAINER"
        echo "Installed-Size: $SIZE"
        [[ -n "$DEPENDS" ]] && echo "Depends: $DEPENDS"
        [[ -n "$RECOMMENDS" ]] && echo "Recommends: $RECOMMENDS"
        [[ -n "$SUGGESTS" ]] && echo "Suggests: $SUGGESTS"
        echo "Section: ${SECTION:-misc}"
        echo "Priority: optional"
        wrap_description "$DESC"
    } >"$ROOT/DEBIAN/control"
    chmod 0644 "$ROOT/DEBIAN/control"

    DEB="$OUT/${PKG}_${DEB_VERSION}_${ARCH}.deb"
    dpkg-deb --root-owner-group -Zxz --build "$ROOT" "$DEB" >/dev/null || die "dpkg-deb a échoué pour $PKG"
    dpkg-deb --info "$DEB" >/dev/null || die "paquet illisible : $DEB"
    BUILT+=("$DEB")
    ok "$(basename "$DEB") — Depends: ${DEPENDS:-aucune}"
done

# ------------------------------------------------------------------ contrôles finaux
step "Contrôles"
for deb in "${BUILT[@]}"; do
    n="$(dpkg-deb -c "$deb" | wc -l)"
    (( n > 0 )) || die "paquet vide : $deb"
    if command -v lintian >/dev/null 2>&1; then
        if ! lintian --tag-display-limit 0 --suppress-tags no-manual-page,initial-upload-closes-no-bugs "$deb" >"$TMP_ROOT/lintian-$(basename "$deb").log" 2>&1; then :; fi
        e="$(grep -c '^E:' "$TMP_ROOT/lintian-$(basename "$deb").log" || true)"
        w="$(grep -c '^W:' "$TMP_ROOT/lintian-$(basename "$deb").log" || true)"
        if (( e > 0 )); then warn "lintian $(basename "$deb") : $e erreur(s), $w avertissement(s)"; grep '^E:' "$TMP_ROOT/lintian-$(basename "$deb").log" | head -n 10 >&2
        else ok "lintian $(basename "$deb") : 0 erreur, $w avertissement(s)"; fi
    fi
done
(cd "$OUT" && sha256sum -- "${BUILT[@]##*/}" >SHA256SUMS) || die "écriture de SHA256SUMS impossible"
[[ "$(source_digest)" == "$SRC_DIGEST" ]] || die "le code source a été modifié pendant la construction (anomalie)"
ok "code source inchangé"

STEP="terminé"
log "Paquets générés dans $OUT :"
for deb in "${BUILT[@]}"; do printf '    %s\n' "$(basename "$deb")" >&2; done
log "Installation : sudo apt install ./$(realpath --relative-to="$PWD" "$OUT" 2>/dev/null || echo "$OUT")/<paquet>.deb"

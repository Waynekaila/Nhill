# shellcheck shell=bash
# Fonctions communes des scripts Nhill (à « sourcer », pas à exécuter).
# Common helpers for the Nhill scripts (source it, do not execute it).
#
#   ROOT          racine du dépôt / repository root
#   DRY_RUN=1     affiche les commandes au lieu de les exécuter (run, as_root)
#   LOG_FILE      journal de l'étape en cours (tee) ; défaut : build/logs/<script>-<date>.log
#
# Conventions de sortie / exit codes:
#   0 succès · 1 échec · 2 usage invalide · 3 environnement incompatible (OS, outils)

set -Eeuo pipefail
umask 022
export LC_ALL="${LC_ALL:-C.UTF-8}"

ROOT="${ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
PROG="${PROG:-$(basename "$0")}"
DRY_RUN="${DRY_RUN:-0}"
NH_STEP="initialisation"

if [[ -t 2 ]]; then
  _R=$'\e[31m' _G=$'\e[32m' _Y=$'\e[33m' _B=$'\e[1m' _0=$'\e[0m'
else
  _R="" _G="" _Y="" _B="" _0=""
fi
ts()   { date '+%H:%M:%S'; }
log()  { printf '%s[%s] ==>%s %s\n' "$_B" "$(ts)" "$_0" "$*" >&2; }
ok()   { printf '%s[%s]   ok%s %s\n' "$_G" "$(ts)" "$_0" "$*" >&2; }
warn() { printf '%s[%s] attention:%s %s\n' "$_Y" "$(ts)" "$_0" "$*" >&2; }
die()  { local code="${2:-1}"; printf '%s[%s] erreur [%s]:%s %s\n' "$_R" "$(ts)" "$NH_STEP" "$_0" "$1" >&2; exit "$code"; }
step() { NH_STEP="$1"; log "$1"; }
incompatible() { die "$1" 3; }

_on_err() {
  local rc=$? line=$1
  printf '%s[%s] erreur [%s]:%s commande échouée (code %s, ligne %s) : %s\n' "$_R" "$(ts)" "$NH_STEP" "$_0" "$rc" "$line" "$BASH_COMMAND" >&2
  [[ -n "${LOG_FILE:-}" ]] && printf 'Journal complet : %s\n' "$LOG_FILE" >&2
  exit "$rc"
}
trap '_on_err $LINENO' ERR

# Journal : tout ce qui sort sur stdout/stderr est aussi écrit dans LOG_FILE.
start_log() {
  local dir="${NH_LOG_DIR:-$ROOT/build/logs}"
  mkdir -p "$dir"
  LOG_FILE="${LOG_FILE:-$dir/${PROG%.sh}-$(date +%Y%m%d-%H%M%S).log}"
  exec > >(tee -a "$LOG_FILE") 2>&1
  log "Journal : $LOG_FILE"
}

# run CMD… : exécute, ou affiche en mode simulation.
run() {
  if [[ "$DRY_RUN" == 1 ]]; then { printf '[simulation]'; printf ' %q' "$@"; printf '\n'; } >&2; else "$@"; fi
}
# as_root CMD… : sudo si nécessaire (jamais de désactivation des protections du système).
as_root() {
  if [[ "$(id -u)" == 0 ]]; then run "$@"
  elif command -v sudo >/dev/null 2>&1; then run sudo "$@"
  else die "droits administrateur requis pour : $* (installez sudo ou relancez en root)" 3; fi
}

need() { # need OUTIL [conseil]
  command -v "$1" >/dev/null 2>&1 || die "outil manquant : $1${2:+ — $2}" 3
}

# Système d'exploitation : NH_OS_ID (debian, ubuntu…), NH_OS_VERSION (13, 24.04…), NH_OS_CODENAME.
detect_os() {
  NH_KERNEL="$(uname -s)"
  NH_ARCH="$(uname -m)"
  NH_OS_ID="" NH_OS_VERSION="" NH_OS_CODENAME="" NH_OS_LIKE=""
  if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    NH_OS_ID="${ID:-}" NH_OS_VERSION="${VERSION_ID:-}" NH_OS_CODENAME="${VERSION_CODENAME:-}" NH_OS_LIKE="${ID_LIKE:-}"
  fi
  export NH_KERNEL NH_ARCH NH_OS_ID NH_OS_VERSION NH_OS_CODENAME NH_OS_LIKE
}
is_debian_like() { [[ "$NH_OS_ID" == debian || "$NH_OS_ID" == ubuntu || "$NH_OS_LIKE" == *debian* ]]; }
# Cibles validées par le projet : Debian 13 (trixie) et Ubuntu 24.04 (noble).
is_supported_distro() {
  [[ ( "$NH_OS_ID" == debian && "$NH_OS_VERSION" == 13 ) || ( "$NH_OS_ID" == ubuntu && "$NH_OS_VERSION" == 24.04 ) ]]
}

check_disk() { # check_disk DOSSIER GO_MIN
  local dir="$1" need_gb="$2" avail_kb
  mkdir -p "$dir"
  avail_kb="$(df -Pk "$dir" | awk 'NR==2 {print $4}')"
  if (( avail_kb < need_gb * 1024 * 1024 )); then
    die "espace disque insuffisant dans $dir : $((avail_kb / 1024 / 1024)) Go libres, $need_gb Go nécessaires" 3
  fi
  ok "espace disque : $((avail_kb / 1024 / 1024)) Go libres dans $dir"
}

project_version() {
  sed -nE 's/^project\([^)]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "$ROOT/CMakeLists.txt" | head -n1
}

# expect_file FICHIER [taille_min_octets] : vérifie qu'un livrable existe réellement.
expect_file() {
  local f="$1" min="${2:-1}" size
  [[ -f "$f" ]] || die "livrable attendu absent : $f"
  size="$(stat -c %s "$f")"
  (( size >= min )) || die "livrable trop petit ($size octets < $min) : $f"
  ok "$(realpath --relative-to="$ROOT" "$f" 2>/dev/null || echo "$f") ($size octets)"
}

# Ne jamais écraser silencieusement : refuse un dossier de sortie non vide sans --force.
prepare_output_dir() { # prepare_output_dir DOSSIER FORCE(0/1)
  local d="$1" force="${2:-0}"
  if [[ -d "$d" && -n "$(ls -A "$d" 2>/dev/null)" && "$force" != 1 ]]; then
    die "le dossier de sortie $d n'est pas vide : relancez avec --force pour le remplacer, ou choisissez --output" 2
  fi
  run mkdir -p "$d"
}

write_checksums() { # write_checksums DOSSIER
  ( cd "$1" && find . -maxdepth 1 -type f ! -name SHA256SUMS -printf '%f\n' | sort | xargs -r sha256sum > SHA256SUMS )
  ok "sommes de contrôle : $1/SHA256SUMS"
}

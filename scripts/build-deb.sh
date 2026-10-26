#!/usr/bin/env bash
# =============================================================================
# build-deb.sh — construit les paquets Debian de Nhill pour la distribution courante.
# Builds the Debian packages of Nhill for the running distribution.
#
#   Debian 13 (trixie)  -> dist/linux/debian-13/      paquets natifs (Qt 6.8 de Debian,
#                                                     mises à jour de sécurité par Debian)
#   Ubuntu 24.04 (noble)-> dist/linux/ubuntu-24.04/   paquets natifs (Qt 6.4 d'Ubuntu)
#   Ubuntu 24.04 + --debian13-embedded -> dist/linux/debian-13-embedded/ : variante
#                       Debian 13 avec Qt 6.4.2 embarqué (méthode de la 0.1.0 ; à éviter :
#                       ces bibliothèques embarquées ne reçoivent pas les correctifs Debian)
#
# Usage :
#   scripts/build-deb.sh [--output DIR] [--build-dir DIR] [--components server,business,pos,console]
#                        [--server-url URL] [--revision N] [--no-tests] [--debian13-embedded]
#                        [--force] [--dry-run]
# Implémentation : packaging/linux/build-deb.sh (compilation, tests, un paquet par
# composant CMake, dépendances calculées par dpkg-shlibdeps, contrôle lintian).
# Sortie : 0 succès · 1 échec · 2 usage · 3 environnement incompatible.
# =============================================================================
PROG=build-deb.sh
source "$(dirname "$0")/lib/common.sh"

OUT="" BUILD_DIR="" COMPONENTS="" SERVER_URL="" REVISION=1 NO_TESTS=0 EMBEDDED=0 FORCE=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --output|-o) OUT="${2:?}"; shift ;;
    --build-dir) BUILD_DIR="${2:?}"; shift ;;
    --components|-c) COMPONENTS="${2:?}"; shift ;;
    --server-url) SERVER_URL="${2:?}"; shift ;;
    --revision|-r) REVISION="${2:?}"; shift ;;
    --no-tests) NO_TESTS=1 ;;
    --debian13-embedded) EMBEDDED=1 ;;
    --force) FORCE=1 ;;
    --dry-run) DRY_RUN=1 ;;
    -h|--help) sed -n '2,21p' "$0"; exit 0 ;;
    *) die "option inconnue : $1" 2 ;;
  esac
  shift
done

start_log
step "Contrôle de l'environnement"
detect_os
[[ "$NH_KERNEL" == Linux ]] || incompatible "Linux requis"
is_debian_like || incompatible "les paquets .deb se construisent sur Debian ou Ubuntu (détecté : $NH_OS_ID)"
for t in dpkg-deb dpkg-shlibdeps apt-cache; do need "$t" "scripts/install-deps-linux.sh --with-packaging"; done
VERSION="$(project_version)"
FORCE_DISTRO=()
case "$NH_OS_ID-$NH_OS_VERSION" in
  debian-13) TARGET=debian-13 ;;
  ubuntu-24.04) TARGET=ubuntu-24.04; FORCE_DISTRO=(--force-distro) ;;
  *) incompatible "distribution $NH_OS_ID $NH_OS_VERSION non prise en charge (Debian 13 ou Ubuntu 24.04)" ;;
esac
[[ "$EMBEDDED" == 0 || "$TARGET" == ubuntu-24.04 ]] || die "--debian13-embedded ne s'utilise que sur Ubuntu 24.04 (sur Debian 13 les paquets natifs sont préférables)" 2
OUT="${OUT:-$ROOT/dist/linux/$TARGET}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build/deb-$TARGET}"
prepare_output_dir "$OUT" "$FORCE"
check_disk "$(dirname "$BUILD_DIR")" 4
ok "Nhill $VERSION -> paquets $TARGET dans $OUT"

step "Compilation, tests et paquets ($TARGET)"
ARGS=(-s "$ROOT" -o "$OUT" -r "$REVISION" --build-dir "$BUILD_DIR" "${FORCE_DISTRO[@]}")
[[ -n "$COMPONENTS" ]] && ARGS+=(-c "$COMPONENTS")
[[ -n "$SERVER_URL" ]] && ARGS+=(--server-url "$SERVER_URL")
[[ "$NO_TESTS" == 1 ]] && ARGS+=(--no-tests)
if [[ "$(id -u)" == 0 ]]; then run "$ROOT/packaging/linux/build-deb.sh" "${ARGS[@]}"
else need sudo "build-deb installe les dépendances manquantes avec apt"; run sudo -E "$ROOT/packaging/linux/build-deb.sh" "${ARGS[@]}"; fi

if [[ "$EMBEDDED" == 1 ]]; then
  step "Variante Debian 13 avec runtime Qt embarqué (depuis Ubuntu 24.04)"
  EMB_OUT="$ROOT/dist/linux/debian-13-embedded"
  prepare_output_dir "$EMB_OUT" "$FORCE"
  run sudo -E "$ROOT/packaging/linux/build-deb-debian13.sh" -s "$ROOT" -b "$BUILD_DIR" -o "$EMB_OUT"
fi

[[ "$DRY_RUN" == 1 ]] && exit 0
step "Vérification des paquets"
shopt -s nullglob
DEBS=("$OUT"/*.deb)
(( ${#DEBS[@]} > 0 )) || die "aucun paquet .deb produit dans $OUT"
for d in "${DEBS[@]}"; do
  expect_file "$d" 1000
  v="$(dpkg-deb -f "$d" Version)"
  [[ "$v" == "$VERSION"-* ]] || die "$d : version $v, attendue $VERSION-*"
  n="$(dpkg-deb -c "$d" | wc -l)"
  (( n > 3 )) || die "$d : contenu vide"
  dpkg-deb -f "$d" Package Version Architecture Depends | sed 's/^/    /' >&2
done
write_checksums "$OUT"
log "Paquets : $OUT"
log "Installation : sudo apt install $OUT/nhill-<composant>_${VERSION}-${REVISION}*_$(dpkg --print-architecture).deb"

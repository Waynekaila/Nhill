#!/usr/bin/env bash
# =============================================================================
# verify-build.sh — vérifie les livrables réellement présents (rien n'est supposé).
# Verifies the deliverables that actually exist (nothing is assumed).
#
#   scripts/verify-build.sh [DIST_DIR] [--require linux,windows,android,website,docs]
#
# Pour chaque plateforme trouvée dans DIST_DIR (défaut dist/) : formats (paquet Debian,
# exécutable PE Windows, APK), version 2.x attendue, sommes SHA-256 (SHA256SUMS),
# PDF présents et à jour (docs/pdf/manifest.json). --require rend obligatoires les
# plateformes citées (échec si absentes). Rapport : build/logs/verify-build-*.log.
# Sortie : 0 tout est conforme · 1 au moins un défaut.
# =============================================================================
PROG=verify-build.sh
source "$(dirname "$0")/lib/common.sh"
DIST="$ROOT/dist" REQUIRE=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --require) REQUIRE="${2:?}"; shift ;;
    -h|--help) sed -n '2,13p' "$0"; exit 0 ;;
    *) DIST="$1" ;;
  esac
  shift
done
start_log
VERSION="$(project_version)"
FAIL=0
bad() { printf '%s  ÉCHEC%s %s\n' "$_R" "$_0" "$*" >&2; FAIL=1; }
required() { [[ ",$REQUIRE," == *",$1,"* ]]; }
check_sums() { # check_sums DIR
  if [[ -f "$1/SHA256SUMS" ]]; then
    ( cd "$1" && sha256sum --quiet -c SHA256SUMS ) && ok "SHA256SUMS conformes : $1" || bad "sommes de contrôle incorrectes : $1"
  else bad "SHA256SUMS absent : $1"; fi
}
shopt -s nullglob

step "Paquets Debian"
DEBS=("$DIST"/linux/*/*.deb)
if (( ${#DEBS[@]} )); then
  for d in "${DEBS[@]}"; do
    v="$(dpkg-deb -f "$d" Version 2>/dev/null || true)"
    [[ "$v" == "$VERSION"-* ]] && ok "$(basename "$d") ($v)" || bad "$d : version « $v », attendue $VERSION-*"
  done
  for dir in "$DIST"/linux/*/; do check_sums "${dir%/}"; done
else required linux && bad "aucun paquet .deb dans $DIST/linux" || warn "aucun paquet .deb (non construit)"; fi

step "Installateurs Windows"
EXES=("$DIST"/windows/*-setup.exe)
if (( ${#EXES[@]} )); then
  for e in "${EXES[@]}"; do
    if [[ "$(head -c2 "$e")" == MZ ]] && file "$e" | grep -q PE32; then ok "$(basename "$e") : exécutable PE ($(stat -c %s "$e") octets)"
    else bad "$e n'est pas un exécutable Windows"; fi
    [[ "$(basename "$e")" == *"-$VERSION-setup.exe" ]] || bad "$e : version absente du nom"
  done
  check_sums "$DIST/windows"
else required windows && bad "aucun installateur Windows" || warn "aucun installateur Windows (non construit)"; fi

step "APK Android"
APKS=("$DIST"/android/*.apk)
if (( ${#APKS[@]} )); then
  for a in "${APKS[@]}"; do
    if unzip -l "$a" 2>/dev/null | grep -c AndroidManifest.xml >/dev/null; then ok "$(basename "$a")"; else bad "$a : APK invalide"; fi
  done
  check_sums "$DIST/android"
else required android && bad "aucun APK" || warn "aucun APK (non construit)"; fi

step "Site vitrine"
SITES=("$DIST"/website/*.tar.gz)
if (( ${#SITES[@]} )); then
  for s in "${SITES[@]}"; do
    if tar -tzf "$s" | grep -cE '(^|/)\.env$' >/dev/null; then bad "$s contient un fichier .env"; else ok "$(basename "$s") (sans .env)"; fi
  done
  check_sums "$DIST/website"
else required website && bad "archive du site absente" || warn "archive du site absente (non construite)"; fi

step "PDF de documentation"
if python3 "$ROOT/scripts/docs/md2pdf.py" --check; then ok "PDF présents et à jour"; else
  required docs && bad "PDF absents ou périmés" || warn "PDF absents ou périmés (scripts/build-docs-pdf.sh)"; fi

(( FAIL == 0 )) && ok "Vérification terminée : conforme." || die "Vérification terminée : au moins un défaut (voir ci-dessus)."

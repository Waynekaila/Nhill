#!/usr/bin/env bash
# =============================================================================
# build-all.sh — construit tout ce qui est constructible sur CETTE machine, puis
# vérifie et assemble la livraison. Ce qui exige une autre plateforme est signalé.
# Builds everything this machine can build, then verifies and packages the release.
#
#   scripts/build-all.sh [--install-deps] [--skip linux,windows,android,docs,website] [--force]
#
# Ordre : dépendances (option) -> Linux (compilation + tests + .deb) -> Windows (si
# MinGW + Qt pour Windows) -> Android (si SDK/NDK/Qt Android) -> site -> PDF ->
# verify-build -> package-release. Résumé final : réussi / ignoré / échoué.
# Sortie : 0 si aucune étape lancée n'a échoué · 1 sinon.
# =============================================================================
PROG=build-all.sh
source "$(dirname "$0")/lib/common.sh"
INSTALL=0 SKIP="" FORCE=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --install-deps) INSTALL=1 ;;
    --skip) SKIP="${2:?}"; shift ;;
    --force) FORCE=(--force) ;;
    -h|--help) sed -n '2,13p' "$0"; exit 0 ;;
    *) die "option inconnue : $1" 2 ;;
  esac
  shift
done
start_log
declare -A RESULT
skipped() { [[ ",$SKIP," == *",$1,"* ]]; }
attempt() { # attempt NAME CMD…  (3 = environnement incompatible -> « ignoré »)
  local name="$1"; shift
  if skipped "$name"; then RESULT[$name]="ignoré (--skip)"; return 0; fi
  set +e; "$@"; local rc=$?; set -e
  case $rc in 0) RESULT[$name]="réussi" ;; 3) RESULT[$name]="ignoré (environnement incompatible, voir journal)" ;; *) RESULT[$name]="ÉCHEC (code $rc)" ;; esac
}
trap - ERR
[[ "$INSTALL" == 1 ]] && attempt deps "$ROOT/scripts/install-deps-linux.sh" --with-packaging --with-docs --yes
attempt linux "$ROOT/scripts/build-deb.sh" "${FORCE[@]}"
attempt windows "$ROOT/scripts/build-windows.sh" "${FORCE[@]}"
attempt android "$ROOT/scripts/build-android.sh" "${FORCE[@]}"
attempt website "$ROOT/scripts/build-website.sh" "${FORCE[@]}"
attempt docs "$ROOT/scripts/build-docs-pdf.sh"
attempt verify "$ROOT/scripts/verify-build.sh"
attempt release "$ROOT/scripts/package-release.sh" "${FORCE[@]}"
step "Résumé"
FAILED=0
for k in deps linux windows android website docs verify release; do
  [[ -n "${RESULT[$k]:-}" ]] || continue
  printf '  %-8s %s\n' "$k" "${RESULT[$k]}" >&2
  [[ "${RESULT[$k]}" == ÉCHEC* ]] && FAILED=1
done
exit $FAILED

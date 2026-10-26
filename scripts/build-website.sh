#!/usr/bin/env bash
# =============================================================================
# build-website.sh — contrôle et empaquette le site vitrine (website/) :
# HTML/CSS/JS statiques + backend de formulaires (website/backend, Python standard).
# Checks and packages the marketing website (static files + form backend).
#
#   scripts/build-website.sh [--output DIR] [--force] [--skip-tests]
#
# Contrôles : liens internes, attributs alt, titres et descriptions, absence de secrets
# dans les fichiers publics, tests du backend (base SQLite jetable).
# Résultat : dist/website/nhill-website-<version>.tar.gz (+ SHA256SUMS).
# Sortie : 0 succès · 1 échec · 2 usage · 3 outils absents.
# =============================================================================
PROG=build-website.sh
source "$(dirname "$0")/lib/common.sh"
OUT="" FORCE=0 TESTS=1
while [[ $# -gt 0 ]]; do
  case "$1" in
    --output|-o) OUT="${2:?}"; shift ;;
    --force) FORCE=1 ;;
    --skip-tests) TESTS=0 ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    *) die "option inconnue : $1" 2 ;;
  esac
  shift
done
need python3
need tar
VERSION="$(project_version)"
OUT="${OUT:-$ROOT/dist/website}"
start_log
step "Contrôles statiques du site"
python3 "$ROOT/website/tools/check_site.py" "$ROOT/website"
if [[ "$TESTS" == 1 ]]; then
  step "Tests du backend des formulaires"
  ( cd "$ROOT/website/backend" && python3 -m unittest discover -s tests -v )
fi
step "Archive"
prepare_output_dir "$OUT" "$FORCE"
ARCHIVE="$OUT/nhill-website-$VERSION.tar.gz"
tar --exclude='*.pyc' --exclude='__pycache__' --exclude='.env' --exclude='*.sqlite3' --exclude='data' \
    -C "$ROOT" -czf "$ARCHIVE" website
expect_file "$ARCHIVE" 10000
if tar -tzf "$ARCHIVE" | grep -E '(^|/)\.env$' >/dev/null; then die "un fichier .env a été archivé"; fi
write_checksums "$OUT"

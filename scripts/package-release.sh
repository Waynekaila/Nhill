#!/usr/bin/env bash
# =============================================================================
# package-release.sh — assemble le dossier de livraison release/V<version>/ à partir
# des livrables RÉELLEMENT construits (dist/), de la documentation, du juridique et
# du marketing, puis calcule les sommes de contrôle.
# Assembles release/V<version>/ from what was actually built.
#
#   scripts/package-release.sh [--force] [--dry-run]
#
# Structure : linux/ windows/ android/ website/ documentation/{fr,en} legal/{fr,en}
#             marketing/{fr,en} source/ checksums/ RELEASE-NOTES.md
# Une plateforme non construite reste absente et est signalée dans
# checksums/MISSING.txt (jamais de fichier factice). Les sources restent séparées des
# installateurs (source/nhill-<version>-src.tar.gz, produit par git archive).
# Sortie : 0 succès · 1 échec · 2 usage.
# =============================================================================
PROG=package-release.sh
source "$(dirname "$0")/lib/common.sh"
FORCE=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --force) FORCE=1 ;;
    --dry-run) DRY_RUN=1 ;;
    -h|--help) sed -n '2,16p' "$0"; exit 0 ;;
    *) die "option inconnue : $1" 2 ;;
  esac
  shift
done
start_log
VERSION="$(project_version)"
REL="$ROOT/release/V$VERSION"
DIST="$ROOT/dist"
prepare_output_dir "$REL" "$FORCE"
[[ "$FORCE" == 1 && "$DRY_RUN" != 1 ]] && rm -rf "${REL:?}"/*
shopt -s nullglob
MISSING=()
copy_dir() { # copy_dir SRC DEST LABEL
  if [[ -d "$1" && -n "$(ls -A "$1" 2>/dev/null)" ]]; then run mkdir -p "$2"; run cp -a "$1/." "$2/"; ok "$3"
  else MISSING+=("$3 : non construit ($1 absent)"); warn "$3 absent"; fi
}
step "Livrables installables"
copy_dir "$DIST/linux" "$REL/linux" "Linux (.deb)"
copy_dir "$DIST/windows" "$REL/windows" "Windows (.exe)"
copy_dir "$DIST/android" "$REL/android" "Android (.apk)"
copy_dir "$DIST/website" "$REL/website" "Site vitrine"
step "Documentation, juridique, marketing (Markdown + PDF)"
for lang in fr en; do
  run mkdir -p "$REL/documentation/$lang" "$REL/legal/$lang" "$REL/marketing/$lang"
  for f in "$ROOT"/docs/"$lang"/*.md "$ROOT"/docs/pdf/"$lang"/*.pdf; do run cp "$f" "$REL/documentation/$lang/"; done
  for f in "$ROOT"/legal/"$lang"/*.md "$ROOT"/legal/pdf/"$lang"/*.pdf; do run cp "$f" "$REL/legal/$lang/"; done
  for f in "$ROOT"/marketing/"$lang"/*.md "$ROOT"/marketing/pdf/"$lang"/*.pdf; do run cp "$f" "$REL/marketing/$lang/"; done
done
for f in "$ROOT"/README.md "$ROOT"/README.en.md "$ROOT"/README.pdf "$ROOT"/README.en.pdf; do [[ -f "$f" ]] && run cp "$f" "$REL/documentation/"; done
[[ -f "$ROOT/docs/pdf/RELEASE-NOTES.pdf" ]] && run cp "$ROOT/docs/pdf/RELEASE-NOTES.pdf" "$REL/RELEASE-NOTES.pdf"
[[ -d "$ROOT/marketing/visuals" ]] && run cp -a "$ROOT/marketing/visuals" "$REL/marketing/"
[[ -d "$ROOT/marketing/trackers" ]] && run cp -a "$ROOT/marketing/trackers" "$REL/marketing/"
[[ -f "$ROOT/marketing/README.md" ]] && run cp "$ROOT/marketing/README.md" "$REL/marketing/"
[[ -d "$ROOT/docs/assets/v3" ]] && { run mkdir -p "$REL/documentation/captures-3.0"; run cp -a "$ROOT/docs/assets/v3/." "$REL/documentation/captures-3.0/"; }
# Preuves de construction (fichiers RUN-INFO des jobs de CI qui ont produit les livrables)
[[ -d "$DIST/ci-evidence" ]] && { run mkdir -p "$REL/checksums/ci-evidence"; run cp -a "$DIST/ci-evidence/." "$REL/checksums/ci-evidence/"; }
step "Code source"
run mkdir -p "$REL/source"
if git -C "$ROOT" rev-parse --git-dir >/dev/null 2>&1; then
  run git -C "$ROOT" archive --format=tar.gz --prefix="nhill-$VERSION/" -o "$REL/source/nhill-$VERSION-src.tar.gz" HEAD
else
  run tar --exclude=./build --exclude=./dist --exclude=./release --exclude=./.git -C "$ROOT" -czf "$REL/source/nhill-$VERSION-src.tar.gz" .
fi
[[ -f "$ROOT/RELEASE-NOTES.md" ]] && run cp "$ROOT/RELEASE-NOTES.md" "$REL/RELEASE-NOTES.md"
[[ "$DRY_RUN" == 1 ]] && exit 0
step "Sommes de contrôle"
mkdir -p "$REL/checksums"
( cd "$REL" && find . -type f ! -path './checksums/*' -printf '%P\n' | sort | xargs -d '\n' sha256sum ) > "$REL/checksums/SHA256SUMS"
ok "$(wc -l < "$REL/checksums/SHA256SUMS") fichiers dans checksums/SHA256SUMS"
{ echo "# Livrables absents de cette livraison / deliverables missing from this release"
  if (( ${#MISSING[@]} )); then printf '%s\n' "${MISSING[@]}"; else echo "aucun / none"; fi; } > "$REL/checksums/MISSING.txt"
log "Livraison : $REL  (vérification : cd $REL && sha256sum -c checksums/SHA256SUMS)"

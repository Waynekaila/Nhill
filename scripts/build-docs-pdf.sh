#!/usr/bin/env bash
# =============================================================================
# build-docs-pdf.sh — génère tous les PDF (documentation FR/EN, juridique, marketing)
# depuis leurs sources Markdown, selon docs/pdf-manifest.txt.
# Generates every PDF from its Markdown source, as listed in docs/pdf-manifest.txt.
#
#   scripts/build-docs-pdf.sh [--check] [--engine auto|weasyprint|playwright|chromium|wkhtmltopdf] [--only GLOB]
#   --check   vérifie seulement que chaque PDF existe et correspond à sa source actuelle
#
# Prérequis : pandoc + un moteur PDF (scripts/install-deps-linux.sh --with-docs installe
# pandoc et WeasyPrint). Les Markdown sont la seule source éditable : ne modifiez jamais
# un PDF à la main, régénérez-le.
# Sortie : 0 succès · 1 échec ou PDF périmé · 3 outils absents.
# =============================================================================
PROG=build-docs-pdf.sh
source "$(dirname "$0")/lib/common.sh"
ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --check) ARGS+=(--check) ;;
    --engine|--only) ARGS+=("$1" "${2:?}"); shift ;;
    -h|--help) sed -n '2,13p' "$0"; exit 0 ;;
    *) die "option inconnue : $1" 2 ;;
  esac
  shift
done
need python3
step "PDF depuis Markdown (docs/pdf-manifest.txt)"
python3 "$ROOT/scripts/docs/md2pdf.py" --manifest "$ROOT/docs/pdf-manifest.txt" "${ARGS[@]}"

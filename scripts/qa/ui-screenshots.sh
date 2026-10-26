#!/usr/bin/env bash
# =============================================================================
# ui-screenshots.sh — captures d'écran RÉELLES des applications compilées, pour la
# revue visuelle de l'interface (CI : artefact « nhill-ci-logs-and-screenshots »).
# Real screenshots of the compiled applications, for visual UI review.
#
#   scripts/qa/ui-screenshots.sh BUILD_DIR OUTPUT_DIR
#
# 1. démarre la pile de développement (PostgreSQL jetable + serveur, scripts/dev/dev-stack.sh)
# 2. crée un établissement de DÉMONSTRATION (données fictives, scripts/dev/seed-demo.py)
# 3. lance la visite automatique de Nhill Business (NHILL_TOUR_*) sous Xvfb à plusieurs
#    tailles d'écran (1366x768, 1280x800, 1920x1080, 1024x600) et à 390x844 (téléphone),
#    puis la visite de Nhill POS ; la visite enregistre ses propres captures.
# Prérequis : Xvfb (scripts/install-deps-linux.sh --with-gui-tests), compilation faite.
# =============================================================================
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$(realpath "${1:?usage: $0 BUILD_DIR OUTPUT_DIR}")"
OUT="$(realpath -m "${2:?usage: $0 BUILD_DIR OUTPUT_DIR}")"
command -v Xvfb >/dev/null || { echo "Xvfb absent (install-deps-linux.sh --with-gui-tests)" >&2; exit 3; }
[[ -x "$BUILD/client/nhill-business" && -x "$BUILD/server/nhill-server" ]] || { echo "compilez d'abord (scripts/build-linux.sh $BUILD)" >&2; exit 3; }
mkdir -p "$OUT"
export NH_DEV_STATE="${NH_DEV_STATE:-$(mktemp -d /tmp/nhill-shots-XXXX)}"
cleanup() { "$ROOT/scripts/dev/dev-stack.sh" stop "$BUILD" >/dev/null 2>&1 || true; [[ -n "${XP:-}" ]] && kill "$XP" 2>/dev/null || true; }
trap cleanup EXIT
"$ROOT/scripts/dev/dev-stack.sh" start "$BUILD"
SEED="$(python3 "$ROOT/scripts/dev/seed-demo.py" --server http://127.0.0.1:8080)"
EMAIL="$(python3 -c 'import json,sys; print(json.loads(sys.stdin.read().strip().splitlines()[-1])["email"])' <<<"$SEED")"
PASSWORD="$(python3 -c 'import json,sys; print(json.loads(sys.stdin.read().strip().splitlines()[-1])["password"])' <<<"$SEED")"

shoot() { # shoot APP SCREEN TAG [window WxH]
  local app="$1" screen="$2" tag="$3" size="${4:-}" d=":$((40 + RANDOM % 40))"
  Xvfb "$d" -screen 0 "${screen}x24" -nolisten tcp >/dev/null 2>&1 & XP=$!
  sleep 1
  DISPLAY="$d" NHILL_SERVER=http://127.0.0.1:8080 NHILL_TOUR_DIR="$OUT" NHILL_TOUR_TAG="$tag-" \
  NHILL_TOUR_EMAIL="$EMAIL" NHILL_TOUR_PASSWORD="$PASSWORD" NHILL_TOUR_SIZE="$size" \
    timeout 240 "$BUILD/client/$app" > "$OUT/$tag.log" 2>&1 || echo "$app ($tag) : code $?" >&2
  # Capture de l'écran entier : montre aussi la taille et la position de la fenêtre.
  DISPLAY="$d" import -window root "$OUT/$tag-ecran-final.png" 2>/dev/null || true
  kill "$XP" 2>/dev/null || true; XP=""
}
for s in 1366x768 1280x800 1920x1080 1024x600; do shoot nhill-business "$s" "business-$s"; done
shoot nhill-business 1280x900 business-telephone 390x844
shoot nhill-pos 1366x768 pos-1366x768
shoot nhill-pos 1024x600 pos-1024x600
ls -1 "$OUT" | sed 's/^/  /'
echo "Captures : $OUT"

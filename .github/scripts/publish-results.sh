#!/usr/bin/env bash
# publish-results.sh BRANCHE CHEMIN... — publie les livrables et journaux d'un job CI dans
# une branche orpheline « ci-results/… » du dépôt (écrasée à chaque exécution), pour les
# récupérer par un simple `git fetch` (sans jeton d'API). Aucun secret n'est publié :
# seuls les chemins donnés sont copiés. Fichiers > 95 Mo ignorés (limite GitHub 100 Mo).
# Publishes a CI job's outputs and logs to an orphan branch, retrievable with git fetch.
set -euo pipefail
branch="${1:?branche}"; shift
tmp="$(mktemp -d)"
for p in "$@"; do
  if [ -e "$p" ]; then cp -r "$p" "$tmp/"; else echo "absent : $p" >>"$tmp/ABSENT.txt"; fi
done
find "$tmp" -type f -size +95M -print -delete | sed 's/^/trop gros, non publié : /' >>"$tmp/ABSENT.txt" || true
find "$tmp" -type f \( -name '*.env' -o -name '*.keystore' -o -name '*.jks' -o -name '*.p12' \) -delete
{
  echo "workflow : ${GITHUB_WORKFLOW:-?}"
  echo "job      : ${GITHUB_JOB:-?} ${MATRIX_NAME:-}"
  echo "état     : ${JOB_STATUS:-?}"
  echo "commit   : ${GITHUB_SHA:-?} (${GITHUB_REF:-?})"
  echo "exécution: ${GITHUB_SERVER_URL:-}/${GITHUB_REPOSITORY:-}/actions/runs/${GITHUB_RUN_ID:-}"
  echo "date UTC : $(date -u +%Y-%m-%dT%H:%M:%SZ)"
} >"$tmp/RUN-INFO.txt"
( cd "$tmp" && find . -type f ! -name SHA256SUMS.ci -print0 | sort -z | xargs -0 sha256sum >SHA256SUMS.ci )
cd "$tmp"
git init -q
git checkout -q -b "$branch"
git -c user.name="nhill-ci" -c user.email="ci@nhill.invalid" add -A
git -c user.name="nhill-ci" -c user.email="ci@nhill.invalid" commit -q -m "Résultats CI ${GITHUB_WORKFLOW:-} / ${GITHUB_JOB:-} ${MATRIX_NAME:-} — ${GITHUB_SHA:-}"
remote="${PUBLISH_REMOTE:-https://x-access-token:${GITHUB_TOKEN:?jeton}@github.com/${GITHUB_REPOSITORY:?}.git}"
git push -q -f "$remote" "HEAD:refs/heads/$branch"
echo "publié dans la branche $branch"

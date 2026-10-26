#!/usr/bin/env bash
# Encrypted PostgreSQL backup of Nhill (custom format, then age encryption).
# The plain dump never touches the disk: pg_dump | age > file.
# Runs as the read-only role nhill_backup (db/bootstrap/01-backup-role.sql),
# the only role allowed to read across establishments (row-level security bypass).
set -euo pipefail
: "${NH_BACKUP_RECIPIENTS:?}" "${NH_BACKUP_DIR:?}"
KEEP="${NH_BACKUP_KEEP_DAYS:-30}"
command -v age >/dev/null || { echo "age is required (https://age-encryption.org)" >&2; exit 1; }
mkdir -p "$NH_BACKUP_DIR"
stamp=$(date -u +%Y%m%dT%H%M%SZ)
out="$NH_BACKUP_DIR/nhill-$stamp.dump.age"
recips=()
for r in $NH_BACKUP_RECIPIENTS; do recips+=(-r "$r"); done
pg_dump --format=custom --compress=6 | age "${recips[@]}" -o "$out.partial"
mv "$out.partial" "$out"
sha256sum "$out" > "$out.sha256"
echo "backup written: $out ($(du -h "$out" | cut -f1))"
if [ -n "${NH_BACKUP_OFFSITE_CMD:-}" ]; then
  eval "${NH_BACKUP_OFFSITE_CMD//\{\}/$out}"
  eval "${NH_BACKUP_OFFSITE_CMD//\{\}/$out.sha256}"
fi
find "$NH_BACKUP_DIR" -name 'nhill-*.dump.age*' -mtime +"$KEEP" -delete

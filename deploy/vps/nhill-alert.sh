#!/usr/bin/env bash
# Nhill — alert after a failed health check (started by nhill-alert.service).
# Always writes the failure to the system journal (priority err); also runs NH_ALERT_CMD
# from /etc/nhill/alert.env when it is set (e-mail, chat or push notification of your choice),
# with the failed checks in the environment variable NH_ALERT_TEXT.
set -uo pipefail
text="Nhill $(hostname -f 2>/dev/null || hostname): health check failed at $(date -u +%FT%TZ)
$(cat /var/lib/nhill/health-failed.txt 2>/dev/null || echo 'see: journalctl -u nhill-healthcheck')"
logger -t nhill-alert -p daemon.err -- "$text"
NH_ALERT_CMD=""
[[ -f /etc/nhill/alert.env ]] && . /etc/nhill/alert.env
if [[ -n "${NH_ALERT_CMD:-}" ]]; then
  export NH_ALERT_TEXT="$text"
  bash -c "$NH_ALERT_CMD" || logger -t nhill-alert -p daemon.err -- "alert command failed"
fi
exit 0

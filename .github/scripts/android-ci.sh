#!/usr/bin/env bash
# android-ci.sh — prépare l'environnement Android sur un runner GitHub ubuntu-24.04 et
# construit l'APK, en journalisant tout (publié dans ci-results/android).
# Composants : SDK préinstallé du runner (sdkmanager), NDK r25c, Qt 6.5.3 hôte + Android
# installés par aqtinstall depuis les miroirs officiels de Qt (download.qt.io).
set -Eeuo pipefail
QT_VER=6.5.3
NDK_VER=25.2.9519653
QT_DIR="${RUNNER_TEMP:-/tmp}/qt"
echo "== Java"; java -version 2>&1
SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/usr/local/lib/android/sdk}}"
SDKMANAGER="$(ls "$SDK"/cmdline-tools/*/bin/sdkmanager 2>/dev/null | sort | tail -n1)"
[[ -x "$SDKMANAGER" ]] || { echo "sdkmanager introuvable dans $SDK"; ls -la "$SDK" || true; exit 3; }
echo "== SDK : $SDK ($SDKMANAGER)"
yes | "$SDKMANAGER" --licenses >/dev/null || true
"$SDKMANAGER" --install "platforms;android-34" "build-tools;34.0.0" "platform-tools" "ndk;$NDK_VER"
# Qt 6.5 choisit la plateforme Android la plus récente installée ; son greffon Gradle
# (AGP 7.x) ne sait pas lire les plateformes 35+ préinstallées sur le runner : on les
# retire pour que la compilation se fasse contre android-34 (cible déclarée de l'APK).
for d in "$SDK"/platforms/android-*; do
  n="$(basename "$d" | sed -E 's/^android-([0-9]+).*/\1/')"
  if [[ "$n" =~ ^[0-9]+$ ]] && (( n > 34 )); then
    echo "retrait de la plateforme $(basename "$d") (plus récente que android-34)"
    "$SDKMANAGER" --uninstall "platforms;$(basename "$d")" || rm -rf -- "$d"
  fi
done
ls "$SDK/platforms"
echo "== aqtinstall"
python3 -m pip install --user --break-system-packages -q "aqtinstall>=3.1,<4" 2>/dev/null \
  || python3 -m pip install --user -q "aqtinstall>=3.1,<4"
export PATH="$HOME/.local/bin:$PATH"
aqt version
echo "== Qt $QT_VER hôte (gcc_64) et Android (arm64-v8a)"
aqt install-qt linux desktop "$QT_VER" linux_gcc_64 -m qtshadertools -O "$QT_DIR" \
  || aqt install-qt linux desktop "$QT_VER" gcc_64 -m qtshadertools -O "$QT_DIR"
aqt install-qt linux android "$QT_VER" android_arm64_v8a -m qtshadertools -O "$QT_DIR"
ls "$QT_DIR/$QT_VER"
sudo apt-get install -y -q ninja-build >/dev/null
export ANDROID_SDK_ROOT="$SDK" ANDROID_NDK_ROOT="$SDK/ndk/$NDK_VER"
export QT_ANDROID="$QT_DIR/$QT_VER/android_arm64_v8a" QT_HOST="$QT_DIR/$QT_VER/gcc_64"
[[ -d "$QT_HOST" ]] || QT_HOST="$(ls -d "$QT_DIR/$QT_VER"/*gcc_64 | head -n1)"
echo "== Construction (QT_HOST=$QT_HOST, QT_ANDROID=$QT_ANDROID)"
scripts/build-android.sh --force "$@"

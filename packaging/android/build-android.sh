#!/usr/bin/env bash
# Construit l'APK Android de Nhill Business (arm64-v8a) depuis Linux ou macOS.
# Nhill Console et Nhill POS sont des applications de bureau : elles ne sont pas
# construites pour Android (client/CMakeLists.txt).
#
# Prérequis (vérifiés ci-dessous) :
#   - JDK 17                         (sudo apt install openjdk-17-jdk)
#   - Android SDK : platforms;android-34, build-tools;34.0.0, platform-tools
#                                    (sdkmanager "platforms;android-34" "build-tools;34.0.0" "platform-tools")
#   - Android NDK r25c (25.2.9519653) (sdkmanager "ndk;25.2.9519653")
#   - Qt 6.5+ pour Android arm64-v8a ET Qt hôte de même version (Qt Online Installer
#     ou : pip install aqtinstall && aqt install-qt linux android 6.5.3 android_arm64_v8a
#          && aqt install-qt linux desktop 6.5.3 gcc_64)
#   - CMake >= 3.21, Ninja
#   - facultatif : QtKeychain compilé pour Android (mémorisation de la session dans le
#     Keystore) ; sans lui l'application fonctionne mais redemande la connexion.
#
# Utilisation :
#   QT_ANDROID=~/Qt/6.5.3/android_arm64_v8a QT_HOST=~/Qt/6.5.3/gcc_64 \
#   ANDROID_SDK_ROOT=~/Android/Sdk ANDROID_NDK_ROOT=~/Android/Sdk/ndk/25.2.9519653 \
#   ./build-android.sh /chemin/vers/nhill [https://api.exemple.com]
# Signature de publication (facultative) : KEYSTORE=… KEYSTORE_ALIAS=… KEYSTORE_PASS=…
# Sans keystore, l'APK est signé avec la clé de débogage : installable (adb install),
# mais pas publiable sur le Play Store.
set -Eeuo pipefail
die() { echo "erreur : $*" >&2; exit 1; }
trap 'echo "erreur : échec ligne $LINENO : $BASH_COMMAND" >&2' ERR

SRC="${1:?usage : $0 /chemin/vers/nhill [adresse-du-serveur]}"
SERVER_URL="${2:-}"
[[ -f "$SRC/CMakeLists.txt" ]] || die "code source introuvable : $SRC"
: "${QT_ANDROID:?définir QT_ANDROID (Qt pour Android arm64-v8a)}" "${QT_HOST:?définir QT_HOST (Qt hôte même version)}"
: "${ANDROID_SDK_ROOT:?définir ANDROID_SDK_ROOT}" "${ANDROID_NDK_ROOT:?définir ANDROID_NDK_ROOT}"
[[ -x "$QT_ANDROID/bin/qt-cmake" ]] || die "qt-cmake absent de $QT_ANDROID/bin"
[[ -x "$QT_HOST/bin/qmake" || -x "$QT_HOST/bin/qtpaths" ]] || die "Qt hôte introuvable dans $QT_HOST"
[[ -d "$ANDROID_SDK_ROOT/platforms/android-34" ]] || die "plateforme Android 34 absente (sdkmanager \"platforms;android-34\")"
[[ -d "$ANDROID_SDK_ROOT/build-tools" ]] || die "build-tools absents (sdkmanager \"build-tools;34.0.0\")"
[[ -f "$ANDROID_NDK_ROOT/source.properties" ]] || die "NDK introuvable : $ANDROID_NDK_ROOT"
command -v java >/dev/null || die "JDK 17 requis (java introuvable)"
JV="$(java -version 2>&1 | sed -nE 's/.*version "([0-9]+).*/\1/p' | head -n1)"
[[ "${JV:-0}" -ge 17 ]] || die "JDK 17 ou plus récent requis (trouvé : ${JV:-?})"
command -v ninja >/dev/null || die "ninja requis"
QTV="$("$QT_HOST/bin/qtpaths" --qt-query QT_VERSION 2>/dev/null || true)"
case "$QTV" in 6.[5-9]*|6.1[0-9]*) ;; *) warn_qt=1 ;; esac
[[ -z "${warn_qt:-}" ]] || echo "attention : Qt hôte $QTV ; Qt 6.5+ recommandé pour Android" >&2

BUILD="${BUILD:-$PWD/build-android}"
OUT="${OUT:-$PWD/dist}"
mkdir -p "$OUT"
ARGS=(-S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DQT_HOST_PATH="$QT_HOST"
      -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" -DQT_ANDROID_ABIS=arm64-v8a)
[[ -n "$SERVER_URL" ]] && ARGS+=(-DNHILL_SERVER_URL="$SERVER_URL")
if [[ -n "${KEYSTORE:-}" ]]; then
  [[ -f "$KEYSTORE" ]] || die "keystore introuvable : $KEYSTORE"
  : "${KEYSTORE_ALIAS:?KEYSTORE_ALIAS requis}" "${KEYSTORE_PASS:?KEYSTORE_PASS requis}"
  ARGS+=(-DQT_ANDROID_SIGN_APK=ON)
  export QT_ANDROID_KEYSTORE_PATH="$KEYSTORE" QT_ANDROID_KEYSTORE_ALIAS="$KEYSTORE_ALIAS" \
         QT_ANDROID_KEYSTORE_STORE_PASS="$KEYSTORE_PASS" QT_ANDROID_KEYSTORE_KEY_PASS="${KEYSTORE_KEY_PASS:-$KEYSTORE_PASS}"
fi
"$QT_ANDROID/bin/qt-cmake" "${ARGS[@]}" || die "configuration CMake échouée"
cmake --build "$BUILD" --parallel --target apk || die "construction de l'APK échouée"
APK="$(find "$BUILD" -name '*.apk' -path '*outputs/apk*' -newer "$BUILD/CMakeCache.txt" | head -n1)"
[[ -n "$APK" ]] || die "aucun APK produit"
BT="$(ls -d "$ANDROID_SDK_ROOT"/build-tools/* | sort -V | tail -n1)"
VERSION="$(sed -nE 's/^project\([^)]*VERSION[[:space:]]+([0-9.]+).*/\1/p' "$SRC/CMakeLists.txt" | head -n1)"
FINAL="$OUT/nhill-business-${VERSION:?version introuvable}-arm64-v8a.apk"
if [[ -n "${KEYSTORE:-}" ]]; then
  cp "$APK" "$FINAL"            # déjà signé par androiddeployqt (clé de publication)
else
  # Sans clé de publication : alignement puis signature avec la clé de débogage Android
  # standard (créée si absente), pour obtenir un APK installable par adb.
  DEBUG_KS="${HOME}/.android/debug.keystore"
  if [[ ! -f "$DEBUG_KS" ]]; then
    mkdir -p "$(dirname "$DEBUG_KS")"
    keytool -genkeypair -v -keystore "$DEBUG_KS" -storepass android -alias androiddebugkey -keypass android \
      -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US" >/dev/null \
      || die "création de la clé de débogage impossible (keytool)"
  fi
  "$BT/zipalign" -f -p 4 "$APK" "$FINAL.aligned" || die "zipalign a échoué"
  "$BT/apksigner" sign --ks "$DEBUG_KS" --ks-pass pass:android --key-pass pass:android \
    --out "$FINAL" "$FINAL.aligned" || die "apksigner a échoué"
  rm -f "$FINAL.aligned" "$FINAL.idsig"
  echo "attention : APK signé avec la clé de DÉBOGAGE (installable, non publiable)" >&2
fi
"$BT/apksigner" verify --print-certs "$FINAL" >/dev/null || die "signature de l'APK invalide"
echo "APK : $FINAL  (installation : adb install -r \"$FINAL\")"

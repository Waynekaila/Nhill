#!/usr/bin/env bash
# android-keychain-emulator.sh — vérifie « Se souvenir de moi » sur un vrai Android :
# construit tests/keychain (Qt 6.5.3 android_x86_64 + QtKeychain 0.17.0 statique), le lance
# sur un émulateur Android 11 (x86_64, KVM) et enchaîne les phases en ARRÊTANT l'application
# entre chaque (am force-stop = fermeture puis réouverture réelles) :
#   write  : connexion avec « Se souvenir de moi »  -> jeton chiffré par le Keystore
#   read   : nouveau processus, session retrouvée     -> jeton renouvelé stocké
#   logout : nouveau processus, déconnexion           -> plus rien de stocké
#   forget : sans « Se souvenir de moi »              -> rien n'est stocké
# Tout est journalisé (publié dans ci-results/android-keychain).
set -Eeuo pipefail
QT_VER=6.5.3
NDK_VER=25.2.9519653
API=30
IMG="system-images;android-$API;default;x86_64"
QT_DIR="${RUNNER_TEMP:-/tmp}/qt"
LOGS="${LOGS:-$PWD/android-keychain-logs}"
mkdir -p "$LOGS"
SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/usr/local/lib/android/sdk}}"
SDKMANAGER="$(ls "$SDK"/cmdline-tools/*/bin/sdkmanager 2>/dev/null | sort | tail -n1)"
AVDMANAGER="$(dirname "$SDKMANAGER")/avdmanager"
[[ -x "$SDKMANAGER" ]] || { echo "sdkmanager introuvable dans $SDK"; exit 3; }
yes | "$SDKMANAGER" --licenses >/dev/null || true
"$SDKMANAGER" --install "platforms;android-34" "build-tools;34.0.0" "platform-tools" "ndk;$NDK_VER" "emulator" "$IMG" | grep -v '^\[' || true
ls "$SDK/system-images/android-$API/default/x86_64" >/dev/null || { echo "image système $IMG non installée"; exit 1; }
for d in "$SDK"/platforms/android-*; do
  n="$(basename "$d" | sed -E 's/^android-([0-9]+).*/\1/')"
  if [[ "$n" =~ ^[0-9]+$ ]] && (( n > 34 )); then "$SDKMANAGER" --uninstall "platforms;$(basename "$d")" || rm -rf -- "$d"; fi
done

echo "== Qt $QT_VER hôte et Android x86_64"
python3 -m pip install --user --break-system-packages -q "aqtinstall>=3.1,<4" 2>/dev/null \
  || python3 -m pip install --user -q "aqtinstall>=3.1,<4"
export PATH="$HOME/.local/bin:$PATH"
aqt install-qt linux desktop "$QT_VER" linux_gcc_64 -m qtshadertools -O "$QT_DIR" \
  || aqt install-qt linux desktop "$QT_VER" gcc_64 -m qtshadertools -O "$QT_DIR"
aqt install-qt linux android "$QT_VER" android_x86_64 -m qtshadertools -O "$QT_DIR"
sudo apt-get install -y -q ninja-build >/dev/null
export ANDROID_SDK_ROOT="$SDK" ANDROID_NDK_ROOT="$SDK/ndk/$NDK_VER"
export QT_ANDROID="$QT_DIR/$QT_VER/android_x86_64" QT_HOST="$QT_DIR/$QT_VER/gcc_64"
[[ -d "$QT_HOST" ]] || QT_HOST="$(ls -d "$QT_DIR/$QT_VER"/*gcc_64 | head -n1)"

echo "== QtKeychain 0.17.0 (x86_64)"
export KEYCHAIN_PREFIX="${RUNNER_TEMP:-/tmp}/qtkeychain-android-x86_64"
packaging/android/build-qtkeychain-android.sh "$KEYCHAIN_PREFIX"

echo "== APK de test (tests/keychain)"
BUILD="$PWD/build/android-keychain-test"
ANDROID_ABI=x86_64 BUILD="$BUILD" OUT="$PWD/dist/android-keychain-test" APK_TARGET=test_keychain_make_apk SKIP_FINAL_APK=1 \
  EXTRA_CMAKE_ARGS="-DNHILL_BUILD_KEYCHAIN_TEST=ON" packaging/android/build-android.sh "$PWD"
RAW="$(find "$BUILD/tests/keychain" -name '*.apk' -path '*outputs/apk*' | head -n1)"
[[ -n "$RAW" ]] || { echo "APK de test introuvable"; find "$BUILD/tests/keychain" -name '*.apk'; exit 1; }
BT="$(ls -d "$SDK"/build-tools/* | sort -V | tail -n1)"
KS="$HOME/.android/debug.keystore"
if [[ ! -f "$KS" ]]; then
  mkdir -p "$(dirname "$KS")"
  keytool -genkeypair -keystore "$KS" -storepass android -alias androiddebugkey -keypass android -keyalg RSA \
    -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US" >/dev/null
fi
APK="$LOGS/test_keychain-x86_64.apk"
"$BT/zipalign" -f -p 4 "$RAW" "$APK.aligned"
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --key-pass pass:android --out "$APK" "$APK.aligned"
rm -f "$APK.aligned" "$APK.idsig"
PKG="$("$BT/aapt" dump badging "$APK" | sed -n "s/^package: name='\([^']*\)'.*/\1/p")"
ACT="$("$BT/aapt" dump badging "$APK" | sed -n "s/^launchable-activity: name='\([^']*\)'.*/\1/p")"
echo "paquet $PKG, activité $ACT"

echo "== Émulateur Android $API x86_64 (KVM)"
echo 'KERNEL=="kvm", GROUP="kvm", MODE="0666", OPTIONS+="static_node=kvm"' | sudo tee /etc/udev/rules.d/99-kvm4all.rules >/dev/null
sudo udevadm control --reload-rules && sudo udevadm trigger --name-match=kvm || true
ls -l /dev/kvm || true
export ANDROID_AVD_HOME="${ANDROID_AVD_HOME:-$HOME/.android/avd}"
mkdir -p "$ANDROID_AVD_HOME"
env | grep -E '^ANDROID_' | sed 's/=.*TOKEN.*/=***/' || true
ls -d "$SDK/system-images/android-$API/default/x86_64" || { echo "image système absente"; exit 1; }
echo no | "$AVDMANAGER" create avd -n nhill -k "$IMG" -p "$ANDROID_AVD_HOME/nhill.avd" --force
"$AVDMANAGER" list avd
ls -la "$ANDROID_AVD_HOME"
ADB="$SDK/platform-tools/adb"
"$SDK/emulator/emulator" -avd nhill -no-window -no-audio -no-boot-anim -no-snapshot -gpu swiftshader_indirect \
  -camera-back none -camera-front none >"$LOGS/emulator.log" 2>&1 &
EMU=$!
trap '"$ADB" emu kill >/dev/null 2>&1 || kill $EMU 2>/dev/null || true' EXIT
timeout 300 "$ADB" wait-for-device
for i in $(seq 1 120); do
  [[ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == 1 ]] && break
  sleep 5
done
[[ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == 1 ]] || { echo "émulateur non démarré"; exit 1; }
"$ADB" shell getprop ro.build.version.release
"$ADB" install -r "$APK"
"$ADB" logcat -c

status=0
for phase in write read logout forget; do
  "$ADB" shell am force-stop "$PKG"          # l'application est réellement fermée
  sleep 2
  "$ADB" shell "am start -W -n $PKG/$ACT -e applicationArguments '--phase $phase'" >/dev/null
  verdict=""
  for i in $(seq 1 60); do
    verdict="$("$ADB" logcat -d 2>/dev/null | grep -o "NHILL_KEYCHAIN_PHASE $phase .*" | tail -n1 | tr -d '\r' || true)"
    [[ -n "$verdict" ]] && break
    sleep 2
  done
  echo "phase $phase : ${verdict:-aucun verdict}"
  if [[ "$verdict" != "NHILL_KEYCHAIN_PHASE $phase OK" ]]; then status=1; break; fi
done
"$ADB" logcat -d >"$LOGS/logcat.txt" 2>&1 || true
"$ADB" shell am force-stop "$PKG" || true
if [[ $status == 0 ]]; then echo "Keystore Android : session mémorisée entre deux lancements — OK"; fi
exit $status

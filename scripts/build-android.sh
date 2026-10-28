#!/usr/bin/env bash
# =============================================================================
# build-android.sh — APK Android de Nhill Business (arm64-v8a).
# Android APK of Nhill Business (arm64-v8a). Nhill POS and Nhill Console are
# desktop applications and are not built for Android (project decision).
#
# Prérequis (non fournis par apt) : JDK 17, SDK Android (platforms;android-34,
# build-tools;34.0.0, platform-tools), NDK r25c (25.2.9519653), Qt 6.5+ pour Android
# arm64-v8a ET Qt hôte (Linux) de la même version.
# Les emplacements se donnent par variables d'environnement :
#   QT_ANDROID=~/Qt/6.5.3/android_arm64_v8a QT_HOST=~/Qt/6.5.3/gcc_64 \
#   ANDROID_SDK_ROOT=~/Android/Sdk ANDROID_NDK_ROOT=~/Android/Sdk/ndk/25.2.9519653 \
#   scripts/build-android.sh [--output DIR] [--force] [--dry-run]
# QtKeychain (« Se souvenir de moi », Keystore Android) : KEYCHAIN_PREFIX=… construit par
# packaging/android/build-qtkeychain-android.sh (obligatoire, ALLOW_NO_KEYCHAIN=1 pour s'en passer).
# Signature de publication : KEYSTORE=… KEYSTORE_ALIAS=… KEYSTORE_PASS=… (sinon clé de
# débogage : APK installable par adb, non publiable).
# CI : .github/workflows/android.yml (installe SDK, NDK et Qt pour Android).
# Sortie : 0 succès · 1 échec · 2 usage · 3 environnement incompatible.
# =============================================================================
PROG=build-android.sh
source "$(dirname "$0")/lib/common.sh"

SERVER_URL="" OUT="" FORCE=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --server-url) echo "--server-url : option retirée en 3.0.0 : aucune adresse de serveur n'est compilée dans les applications (saisie au premier lancement) / removed in 3.0.0: no server address is compiled into the applications" >&2; exit 2 ;;
    --output|-o) OUT="${2:?}"; shift ;;
    --force) FORCE=1 ;;
    --dry-run) DRY_RUN=1 ;;
    -h|--help) sed -n '2,21p' "$0"; exit 0 ;;
    *) die "option inconnue : $1" 2 ;;
  esac
  shift
done

start_log
step "Contrôle de l'environnement"
detect_os
[[ "$NH_KERNEL" == Linux || "$NH_KERNEL" == Darwin ]] || incompatible "Linux ou macOS requis pour ce script"
MISSING=()
for v in QT_ANDROID QT_HOST ANDROID_SDK_ROOT ANDROID_NDK_ROOT; do [[ -n "${!v:-}" ]] || MISSING+=("$v"); done
if (( ${#MISSING[@]} > 0 )); then
  incompatible "variables non définies : ${MISSING[*]}.
  Installez le SDK/NDK Android (sdkmanager) et Qt pour Android (Qt Online Installer ou aqtinstall),
  puis relancez avec ces variables — détails : docs/fr/guide-compilation.md § Android.
  Alternative sans installation locale : .github/workflows/android.yml"
fi
need java "JDK 17 (scripts/install-deps-linux.sh --with-android-jdk)"
need ninja
need cmake
VERSION="$(project_version)"
OUT="${OUT:-$ROOT/dist/android}"
prepare_output_dir "$OUT" "$FORCE"
check_disk "$ROOT" 6

step "Construction de l'APK"
BUILD="$ROOT/build/android" OUT="$OUT" run "$ROOT/packaging/android/build-android.sh" "$ROOT" ${SERVER_URL:+"$SERVER_URL"}

[[ "$DRY_RUN" == 1 ]] && exit 0
step "Vérification"
APK="$OUT/nhill-business-$VERSION-arm64-v8a.apk"
expect_file "$APK" 3000000
unzip -l "$APK" | grep -q 'AndroidManifest.xml' || die "$APK : AndroidManifest.xml absent"
unzip -l "$APK" | grep -q 'lib/arm64-v8a/' || die "$APK : bibliothèques arm64-v8a absentes"
# Ouverture des PDF/CSV téléchargés dans une autre application (Android 7+ : content://) :
# le fournisseur doit être déclaré ET sa classe présente, sinon l'application ne démarre pas.
unzip -p "$APK" AndroidManifest.xml | tr -d '\000' | grep -q 'androidx.core.content.FileProvider' \
  || die "$APK : fournisseur de fichiers (FileProvider) absent du manifeste"
unzip -p "$APK" 'classes*.dex' | grep -aq 'Landroidx/core/content/FileProvider;' \
  || die "$APK : classe androidx FileProvider absente (dépendance androidx.core manquante)"
unzip -l "$APK" | grep -q 'res/xml/qtprovider_paths.xml' || die "$APK : res/xml/qtprovider_paths.xml absent"
write_checksums "$OUT"

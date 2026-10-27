#!/usr/bin/env bash
# Construit QtKeychain pour Android (bibliothèque statique liée dans l'application) :
# la session est chiffrée par une clé du Keystore Android, qui ne quitte jamais le
# Keystore (AES-GCM, clé enveloppée RSA-2048 générée par AndroidKeyStore).
# Source : dépôt officiel frankosterfeld/qtkeychain, étiquette 0.17.0, commit vérifié.
#
#   QT_ANDROID=~/Qt/6.5.3/android_arm64_v8a QT_HOST=~/Qt/6.5.3/gcc_64 \
#   ANDROID_SDK_ROOT=… ANDROID_NDK_ROOT=… \
#   packaging/android/build-qtkeychain-android.sh PREFIX
# Ensuite : KEYCHAIN_PREFIX=PREFIX scripts/build-android.sh
set -Eeuo pipefail
die() { echo "erreur : $*" >&2; exit 1; }
PREFIX="${1:?usage : $0 PREFIX}"
TAG=0.17.0
COMMIT=875f77d9f61bd97fd84cca47ce3bc71186dfbd09
: "${QT_ANDROID:?définir QT_ANDROID}" "${QT_HOST:?définir QT_HOST}" "${ANDROID_SDK_ROOT:?}" "${ANDROID_NDK_ROOT:?}"
[[ -x "$QT_ANDROID/bin/qt-cmake" ]] || die "qt-cmake absent de $QT_ANDROID/bin"
if [[ -f "$PREFIX/lib/cmake/Qt6Keychain/Qt6KeychainConfig.cmake" ]]; then echo "QtKeychain déjà présent : $PREFIX"; exit 0; fi
WORK="${WORK:-$(mktemp -d "${TMPDIR:-/tmp}/qtkeychain-android.XXXXXX")}"
git clone -q --depth 1 --branch "$TAG" https://github.com/frankosterfeld/qtkeychain.git "$WORK/src" \
  || die "clonage de QtKeychain $TAG impossible"
HEAD="$(git -C "$WORK/src" rev-parse HEAD)"
[[ "$HEAD" == "$COMMIT" ]] || die "QtKeychain $TAG : commit $HEAD inattendu (attendu $COMMIT) — source refusée"
echo "QtKeychain $TAG ($COMMIT) vérifié"
"$QT_ANDROID/bin/qt-cmake" -S "$WORK/src" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQT_HOST_PATH="$QT_HOST" -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" -DBUILD_WITH_QT5=OFF -DBUILD_SHARED_LIBS=OFF -DBUILD_TRANSLATIONS=OFF \
  -DBUILD_TEST_APPLICATION=OFF -DBUILD_TESTING=OFF || die "configuration de QtKeychain échouée"
cmake --build "$WORK/build" --parallel || die "compilation de QtKeychain échouée"
cmake --install "$WORK/build" >/dev/null || die "installation de QtKeychain échouée"
[[ -f "$PREFIX/lib/cmake/Qt6Keychain/Qt6KeychainConfig.cmake" ]] || die "configuration CMake Qt6Keychain absente"
ls "$PREFIX"/lib/libqt6keychain*.a >/dev/null 2>&1 || die "libqt6keychain.a absente"
echo "QtKeychain (Android, statique) installé dans $PREFIX"

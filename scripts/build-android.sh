#!/usr/bin/env bash
# Builds the Nhill Android application (APK/AAB). NOT EXECUTED during
# development (no Android SDK/NDK in the build environment) — see docs/installation.md.
# Only Nhill Business is built for Android (Nhill Console and Nhill POS are desktop-only).
# Requirements: Qt 6.5+ for Android (arm64-v8a) + host Qt of the same version,
# Android SDK (platform 34, build-tools), NDK r25+, JDK 17.
#   QT_ANDROID=~/Qt/6.5.3/android_arm64_v8a QT_HOST=~/Qt/6.5.3/gcc_64 \
#   ANDROID_SDK_ROOT=~/Android/Sdk ANDROID_NDK_ROOT=~/Android/Sdk/ndk/25.2.9519653 scripts/build-android.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${QT_ANDROID:?}" "${QT_HOST:?}" "${ANDROID_SDK_ROOT:?}" "${ANDROID_NDK_ROOT:?}"
BUILD="${BUILD:-$ROOT/build-android}"
"$QT_ANDROID/bin/qt-cmake" -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQT_HOST_PATH="$QT_HOST" -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
  -DNHILL_SERVER_URL=https://api.example.com
cmake --build "$BUILD" --target apk
echo "APK: $(find "$BUILD" -name '*.apk' | head -1) — sign it with your release key (apksigner) before distribution."

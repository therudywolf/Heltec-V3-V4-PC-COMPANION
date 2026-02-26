#!/bin/sh
# One-click APK build for BMW Assistant (Android 15 / API 35)
cd "$(dirname "$0")"

if [ ! -f "./gradlew" ]; then
    echo "Gradle wrapper not found. Generating..."
    gradle wrapper --gradle-version 8.4 || { echo "Install Gradle or run from Android Studio."; exit 1; }
fi

chmod +x ./gradlew 2>/dev/null
echo "Building release APK (targetSdk 35)..."
./gradlew assembleRelease || exit 1

OUTPUT="app/build/outputs/apk/release/app-release.apk"
if [ -f "$OUTPUT" ]; then
    echo ""
    echo "APK built: $OUTPUT"
    echo "Install: adb install -r $OUTPUT"
else
    echo "APK not found at $OUTPUT"
    exit 1
fi

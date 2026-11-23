#!/bin/sh
# Compila el APK de diagnóstico y bring-up de Leonida Mobile (launcher/app).
# Requiere Android SDK + NDK y Gradle instalado (o gradlew).
set -e
cd "$(dirname "$0")/../launcher"

if [ -x "./gradlew" ]; then
  ./gradlew assembleDebug
elif command -v gradle >/dev/null 2>&1; then
  gradle assembleDebug
else
  echo "ERROR: No se encontró ./gradlew ni gradle en PATH."
  echo "Abre la carpeta 'leonida-mobile/launcher' en Android Studio o instala Gradle 8.x."
  exit 1
fi

APK="app/build/outputs/apk/debug/app-debug.apk"
if [ -f "$APK" ]; then
  echo "OK: APK generado en launcher/$APK"
  echo "Instala en tu móvil con: adb install -r launcher/$APK"
fi

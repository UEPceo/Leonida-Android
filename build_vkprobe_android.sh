#!/bin/sh
# Compilar vkprobe para Android ARM64 (requiere ANDROID_NDK).
# Uso: ./build_vkprobe_android.sh
set -e
NDK="${ANDROID_NDK:-$HOME/Android/Sdk/ndk/26.1.10909125}"
if [ ! -d "$NDK" ]; then
  echo "ERROR: ANDROID_NDK no encontrado. Exporta ANDROID_NDK o instala el NDK."
  exit 1
fi
cmake -S . -B build/android \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-29 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/android -j$(nproc)
echo "Listo: build/android/vkprobe"
echo "Sube con: adb push build/android/vkprobe scripts/f1_capture.sh /data/local/tmp/"

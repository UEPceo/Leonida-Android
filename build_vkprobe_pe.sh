#!/bin/sh
# Compilar vkprobe.c como PE x64 (mingw-w64) para ejecutar F1 / V0 bajo Wine.
set -e
CC=x86_64-w64-mingw32-gcc
if ! command -v "$CC" >/dev/null 2>&1; then
  echo "ERROR: instala mingw-w64 (paquete que provea $CC)"
  exit 1
fi
mkdir -p build/pe
"$CC" -O2 -Wall -Wextra -o build/pe/vkprobe.exe probe/vkprobe.c -lvulkan-1
echo "OK: build/pe/vkprobe.exe"
echo "Siguiente: ./scripts/record_v0_wine.sh"

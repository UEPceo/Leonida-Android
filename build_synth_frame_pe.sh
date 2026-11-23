#!/bin/sh
# Compilar synth_frame.c como PE x64 (mingw-w64).
set -e
CC=x86_64-w64-mingw32-gcc
if ! command -v "$CC" >/dev/null 2>&1; then
  echo "ERROR: instala mingw-w64 (paquete que provea $CC)"
  exit 1
fi
"$CC" -O2 -Wall -Wextra -o synth_frame.exe probe/synth_frame.c -lvulkan-1 -luser32 -lgdi32
echo "OK: synth_frame.exe"
echo "Prueba: wine synth_frame.exe"
echo "Registro: ./scripts/record_synth_frame.sh"

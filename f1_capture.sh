#!/bin/sh
# F1 / V0 - captura mínima en Android (adb shell o Termux), sin Python.
# Uso: f1_capture.sh [ruta/a/vkprobe] [directorio_salida]
# Guarda env.txt (modelo, SoC, Android, tamaño de página, kernel) y vkprobe.json.
# No recoge serie, IMEI ni identificadores del equipo.

PROBE="${1:-./vkprobe}"
OUT="${2:-./f1-$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "$OUT" || exit 2

gp() { getprop "$1" 2>/dev/null; }

{
  echo "model=$(gp ro.product.model)"
  echo "soc=$(gp ro.soc.model)"
  echo "board=$(gp ro.board.platform)"
  echo "android=$(gp ro.build.version.release)"
  echo "sdk=$(gp ro.build.version.sdk)"
  echo "page_size=$(getconf PAGESIZE 2>/dev/null || echo n/a)"
  echo "kernel=$(uname -r)"
  echo "arch=$(uname -m)"
} > "$OUT/env.txt"

"$PROBE" > "$OUT/vkprobe.json" 2> "$OUT/vkprobe.err"
RC=$?
echo "vkprobe_exit=$RC" >> "$OUT/env.txt"
echo "listo: $OUT (vkprobe salió con $RC)"
exit $RC

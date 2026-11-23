#!/bin/sh
# F2 / V1-V2: validación del loader bajo contenedor móvil (3 arranques consistentes).
# Requiere gta6rt.exe y sustituto autorizado compilados en F0.
# Uso: TRANSLATOR="Box64" ./scripts/f2_runtime_check.sh /ruta/a/gta6rt.exe /ruta/a/stand-in.exe
set -e
GTA6RT="${1:?Uso: $0 <ruta/gta6rt.exe> <ruta/stand-in.exe>}"
STANDIN="${2:?Uso: $0 <ruta/gta6rt.exe> <ruta/stand-in.exe>}"

for i in 1 2 3; do
  echo "=== F2 / V1 intento $i de 3 ==="
  python3 tools/record_run.py \
    --label "v1-init-run${i}" \
    --commit e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1 \
    --container "${CONTAINER:-Winlator/Wine}" \
    --wine "${WINE_VERSION:-wine}" \
    --translator "${TRANSLATOR:-}" \
    --expect-exit 0 \
    --timeout 30 \
    -- wine "$GTA6RT" --init "$STANDIN"
done
echo "OK: 3 ejecuciones V1 completadas."

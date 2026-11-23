#!/bin/sh
# F1 / V0 bajo Wine: captura y registra capacidades Vulkan expuestas por winevulkan.
# Cambia una sola variable al comparar traductores (ej. TRANSLATOR="Box64 0.3.2" vs "FEX-2603").
set -e
PROBE_EXE="${1:-build/pe/vkprobe.exe}"
OUT_DIR="${2:-out-v0-wine}"
mkdir -p "$OUT_DIR"

wine "$PROBE_EXE" > "$OUT_DIR/vkprobe_wine.json" 2> "$OUT_DIR/vkprobe_wine.err" || true

python3 tools/record_run.py \
  --label v0-wine \
  --commit e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1 \
  --container "${CONTAINER:-Winlator/Wine}" \
  --wine "${WINE_VERSION:-wine}" \
  --translator "${TRANSLATOR:-}" \
  --probe-json "$OUT_DIR/vkprobe_wine.json" \
  --expect-regex '"any_device_submitted": true' \
  --timeout 30 \
  -- wine "$PROBE_EXE"

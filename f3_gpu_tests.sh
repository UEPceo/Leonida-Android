#!/bin/sh
# F3 / V3–V5: validación sintética de GPU, mesh shaders por ruta y creación de pipelines.
# Registra por separado pipelines creados, fallidos y omitidos (el 99% global es auxiliar).
#
# Uso:
#   TRANSLATOR="Box64" ./scripts/f3_gpu_tests.sh /ruta/a/build_tests /ruta/a/spirv_dir
set -e

TEST_BIN_DIR="${1:?Uso: $0 <dir_binarios_test_pc> <dir_spirv_traducidos>}"
SPIRV_DIR="${2:?Uso: $0 <dir_binarios_test_pc> <dir_spirv_traducidos>}"
OUT_DIR="out-f3-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$OUT_DIR"

echo "=== V3: Pruebas sintéticas de GPU (vertex fetch, texturas, wave64, LDS, barreras) ==="
if [ -x "$TEST_BIN_DIR/gpu_synth_tests.exe" ] || [ -f "$TEST_BIN_DIR/gpu_synth_tests.exe" ]; then
  python3 tools/record_run.py \
    --label v3-gpu-synth \
    --commit e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1 \
    --container "${CONTAINER:-Winlator/Wine}" \
    --wine "${WINE_VERSION:-wine}" \
    --translator "${TRANSLATOR:-}" \
    --expect-exit 0 \
    --timeout 120 \
    -- wine "$TEST_BIN_DIR/gpu_synth_tests.exe" || true
else
  echo "AVISO: no se encontró gpu_synth_tests.exe en $TEST_BIN_DIR"
fi

echo "=== V4: Mesh shaders por ruta (nativo vs --mesh-compute) ==="
for MODE in native compute; do
  if [ -f "$TEST_BIN_DIR/ms_tests_${MODE}.exe" ]; then
    python3 tools/record_run.py \
      --label "v4-mesh-${MODE}" \
      --commit e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1 \
      --container "${CONTAINER:-Winlator/Wine}" \
      --wine "${WINE_VERSION:-wine}" \
      --translator "${TRANSLATOR:-}" \
      --expect-exit 0 \
      --timeout 120 \
      -- wine "$TEST_BIN_DIR/ms_tests_${MODE}.exe" || true
  fi
done

echo "=== V5: Creación de pipelines sobre SPIR-V traducidos ==="
if [ -f "$TEST_BIN_DIR/vk_pipeline_check.exe" ]; then
  python3 tools/record_run.py \
    --label v5-pipeline-check \
    --commit e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1 \
    --container "${CONTAINER:-Winlator/Wine}" \
    --wine "${WINE_VERSION:-wine}" \
    --translator "${TRANSLATOR:-}" \
    --expect-exit 0 \
    --timeout 600 \
    -- wine "$TEST_BIN_DIR/vk_pipeline_check.exe" "$SPIRV_DIR" --report "$OUT_DIR/v5_pipelines.json" || true
  echo "Revisa $OUT_DIR/v5_pipelines.json: verifica creados, fallidos y omitidos por separado."
fi

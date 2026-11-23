#!/bin/sh
# Ejemplo de registro de una ejecución (ajusta rutas).
python3 tools/record_run.py \
  --label v0-nativo \
  --commit e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1 \
  --container "Termux" \
  --probe-json out/vkprobe.json \
  --expect-regex '"any_device_submitted": true' \
  --timeout 30 \
  -- ./vkprobe

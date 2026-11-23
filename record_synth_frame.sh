#!/bin/sh
# Registrar una ejecución del fotograma sintético.
# Uso (dentro de Wine/Termux o con wine en PATH):
#   ./record_synth_frame.sh
python3 tools/record_run.py \
  --label synth-frame \
  --commit e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1 \
  --container "${CONTAINER:-Wine}" \
  --translator "${TRANSLATOR:-}" \
  --expect-regex "first_present OK" \
  --timeout 15 \
  -- wine synth_frame.exe

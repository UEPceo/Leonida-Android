# Siguientes pasos (Leonida Mobile)

Referencia fija: `ProjectLeonida/Leonida` @ `e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1`

## 1. Primer entregable móvil (listo para ejecutar)

```sh
./scripts/build_synth_frame_pe.sh
# En el dispositivo / contenedor Wine:
./scripts/record_synth_frame.sh
```

Esperado en el log: `first_present OK` + ventana azul. Guarda captura y `record.json`.

## 2. F1 / V0 completo (nativo + Wine + comparativa automática)

```sh
# Nativo Android ARM64:
./scripts/build_vkprobe_android.sh
# Bajo Wine x64 (comparativa winevulkan vs nativo):
./scripts/build_vkprobe_pe.sh
TRANSLATOR="Box64" ./scripts/record_v0_wine.sh

# Comparar nativo vs Wine y generar perfil JSON de flags para make_game.py:
python3 tools/compare_v0.py \
  --native out/vkprobe.json \
  --wine out-v0-wine/vkprobe_wine.json \
  --soc-id sm8550-adreno-740 \
  --out-profile profiles/measured_soc.json
```

## 3. F2 / V1–V2 (loader bajo traductor móvil, tras F0 en PC)

```sh
TRANSLATOR="Box64" ./scripts/f2_runtime_check.sh /ruta/gta6rt.exe /ruta/stand-in.exe
```

## 4. F3 / V3–V5 (pruebas GPU sintéticas, mesh por ruta y pipelines)

```sh
TRANSLATOR="Box64" ./scripts/f3_gpu_tests.sh /ruta/tests_gpu /ruta/spirv_traducidos
```

## Estado de APK / Launcher Android

Todavía **no existe ningún `.apk`** en el repositorio. Según el plan de ejecución, la construcción de `mobile/launcher/` (APK Android con UI y controles táctiles) se pospone hasta validar F1–F4 en contenedor Wine/Winlator existente para no invertir en empaquetado antes de confirmar la compatibilidad del runtime.

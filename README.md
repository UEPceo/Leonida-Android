# Leonida Mobile — arranque (F1 / V0)

Andamiaje independiente para la versión móvil. **No modifica upstream, no
contiene el juego ni material suyo** y no explica cómo obtenerlo.

## Estado (2026-11-23)

| Pieza | Estado |
|---|---|
| `tools/record_run.py` (registro mínimo por ejecución) | Hecho y probado: éxito, código distinto, timeout, comando inexistente, aviso de material del juego |
| `probe/vkprobe.c` (sonda Vulkan) | Escrito, **sin compilar ni ejecutar** (el entorno donde se escribió no tenía cabeceras Vulkan ni red). El escritor JSON sí se validó aparte |
| `scripts/f1_capture.sh` | Escrito, sin ejecutar en Android |
| F0: baseline en PC del runtime y del ejecutable sustituto | **No empezado** (requiere clonar el repo) |
| Contenedor Wine/traductor en Android | **No empezado** |
| Presentación de un fotograma sintético (primer entregable móvil) | **No empezado**; la sonda lo marca `not_tested` |

## Qué mide la sonda (V0)

Instancia y extensiones; subgrupos (tamaño, mín./máx., si wave64 nativo es
posible); features (geometry, BC/ASTC/ETC2, bindless, 64-bit, mesh, baricéntricas,
float controls); límites y heaps de memoria. Además **prueba** crear el
dispositivo (mínimo y con todo lo soportado) y enviar un command buffer vacío,
registrando cada intento por separado. Esto sirve para decidir con datos los
flags del build (`--wave64-emulate`, `--mesh-compute`, `--bary-fallback`,
`--no-float-controls`, `--texel-offset-fallback`) según el SoC.

## Uso

Compilar (cabeceras Vulkan ≥ 1.3.250):

```sh
# Android ARM64 (NDK)
cmake -S . -B build/android -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29
cmake --build build/android

# Linux / PC
cmake -S . -B build/host && cmake --build build/host
```

Capturar en Android nativo:

```sh
adb push build/android/vkprobe scripts/f1_capture.sh /data/local/tmp/
adb shell "cd /data/local/tmp && chmod +x vkprobe f1_capture.sh && ./f1_capture.sh ./vkprobe out"
adb pull /data/local/tmp/out
```

Registrar una ejecución (Termux o dentro del contenedor):

```sh
python3 tools/record_run.py --label v0-nativo --repo ~/Leonida \
  --container "Termux" --probe-json out/vkprobe.json \
  --expect-regex '"any_device_submitted": true' -- ./vkprobe
```

Para V0 bajo Wine, compila el mismo `vkprobe.c` como PE x64 (mingw-w64 o
clang-cl con las cabeceras y `vulkan-1`) y ejecútalo con `--wine`/`--translator`
rellenados. Cambia **una sola variable** entre ejecuciones comparables.

## Registro mínimo (lo escribe `record_run.py`)

Commit, modelo, SoC, Android, tamaño de página, GPU/driver (de `vkprobe.json`),
contenedor, Wine, traductor, comando, código de salida, esperado y observado,
log (con SHA-256) y captura. Si el comando o el log mencionan `.rpf`, `.ttpf` o
`MicrosoftGame.Config`, el registro sale con `publishable: false`.

## Siguiente paso

1. Compilar y ejecutar `vkprobe` en el móvil (F1/V0 nativo).
2. F0 en PC: clonar el repo y fijar commit; ejecutar el sustituto con `gta6rt --run`.
3. Primer entregable: un ejecutable sintético Windows x64 presentando un fotograma
   en Android ARM64 (necesita superficie: ahí falta la prueba de presentación).

## Estructura añadida

- `profiles/template.json`: plantilla de perfil por SoC (flags de traducción).
- `probe/synth_frame.c`: esqueleto del primer entregable móvil (fotograma sintético desde PE x64).

## Primer entregable móvil (recordatorio)

Presentar un fotograma sintético correcto desde un ejecutable Windows x64 en Android ARM64 y conservar el log. Esto valida parte de la ruta móvil, no el juego. No requiere esperar a F0 ni invertir en un launcher.

El esqueleto está en `probe/synth_frame.c`. El siguiente paso concreto es completar el bucle de presentación (superficie Wine + swapchain + clear) y registrar la ejecución con `tools/record_run.py`.

## Progreso en paralelo (2026-11-23)

- Scripts de build: `scripts/build_vkprobe_android.sh`, `scripts/build_synth_frame_pe.sh`
- Ejemplo de registro: `scripts/example_record.sh`
- Perfiles SoC: `profiles/adreno_740.json`, `profiles/adreno_750.json`
- Esqueleto Win32 + Vulkan: `probe/synth_frame.c` (ventana creada; falta surface/swapchain)
- Notas F0: `bringup/f0_baseline.md`

Los 4 frentes avanzados en paralelo. El fotograma sintético sigue siendo el primer entregable móvil viable sin F0.

## Bucle de presentación (2026-11-23)

`probe/synth_frame.c` ahora incluye el bucle completo:
- Ventana Win32
- Superficie Vulkan (VK_KHR_win32_surface)
- Swapchain
- Clear de color azul (0.1, 0.3, 0.8) vía `vkCmdClearColorImage`
- Present continuo

Compilar con `scripts/build_synth_frame_pe.sh` y ejecutar bajo Wine.
Este es el primer entregable móvil viable sin F0.

## Repositorio upstream

Trabajamos con **https://github.com/ProjectLeonida/Leonida** (organización canónica).
El commit de referencia actual de main es `e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1 (referencia fija 23-11-2026)

`ItsNotRexed/Leonida` redirige o es el origen transferido a la organización.

## Referencia fija

**ProjectLeonida/Leonida @ e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1**

Ver `docs/REFERENCE.md`.

## Mejoras del fotograma sintético
- Código pulido: designated initializers, logging consistente, extent dinámico.

- Logging explícito a stdout (`instance OK`, `first_present OK`, etc.) para `record_run.py`.
- Script `scripts/record_synth_frame.sh` listo para registrar la ejecución bajo Wine.


## Compilación automática en la nube (GitHub Actions -> `.apk`)

El repositorio incluye `.github/workflows/build-apk.yml`. Al subir esta carpeta a GitHub:
1. Ve a la pestaña **Actions** de tu repositorio.
2. Selecciona el workflow **Build Leonida Mobile APK & PE Probes**.
3. Descarga directamente el artefacto **`LeonidaMobile-debug-apk`** (`app-debug.apk`) e instálalo en tu móvil Android, junto con **`LeonidaMobile-pe-probes`** (`synth_frame.exe` y `vkprobe.exe`) para probar dentro de Winlator/Wine.

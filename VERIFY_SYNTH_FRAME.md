# Verificación del fotograma sintético

1. Compilar: `./scripts/build_synth_frame_pe.sh`
2. Ejecutar bajo Wine: `wine synth_frame.exe`
3. Comprobar en el log:
   - `instance OK`
   - `surface OK`
   - `swapchain OK`
   - `first_present OK`
4. Visual: ventana con fondo azul continuo.
5. Registrar: `./scripts/record_synth_frame.sh`

Si falla, el mensaje `FAIL ...` indica el paso exacto.

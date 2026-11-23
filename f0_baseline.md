# F0 — Baseline en PC (Leonida)

**Repositorio canónico:** https://github.com/ProjectLeonida/Leonida  
**Commit de referencia fija:** `e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1` (main, 23-11-2026)

## Pasos mínimos (sin el juego)

```sh
git clone --recursive https://github.com/ProjectLeonida/Leonida.git
cd Leonida
git checkout e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1
# Construir runtime
cmake -S runtime -B runtime/build
cmake --build runtime/build --config RelWithDebInfo
# Ejecutar sustituto
runtime/build/gta6rt --run stand-in.exe --trace-d3d12
```

Conservar logs. El loader ejecuta un sustituto de extremo a extremo.

## Estado conocido

- PE loader + shims: IMPLEMENTADO parcial (sustituto OK).
- D3D12→Vulkan: IMPLEMENTADO parcial.
- Juego real: no arranca todavía.

Seguir upstream en paralelo.

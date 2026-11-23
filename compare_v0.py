#!/usr/bin/env python3
"""Comparador V0 (Nativo vs Wine) y generador de perfil SoC (Leonida Mobile).

Analiza vkprobe.json (Android nativo) y opcionalmente vkprobe_wine.json (bajo
winevulkan), verifica los requisitos obligatorios del runtime de Leonida,
detecta extensiones o features recortadas por la capa de traducción de Wine y
emite el bloque JSON de flags recomendados para make_game.py.

Uso:
  python3 tools/compare_v0.py --native out/vkprobe.json [--wine out-v0-wine/vkprobe_wine.json] [--out-profile profiles/mi_soc.json]
"""
import argparse
import json
import sys
from pathlib import Path

MANDATORY_FEATURES = [
    "dynamicRendering",
    "synchronization2",
    "bufferDeviceAddress",
    "runtimeDescriptorArray",
    "descriptorBindingPartiallyBound",
]

MANDATORY_EXTS_OR_CORE13 = [
    "VK_EXT_conditional_rendering",
    "VK_EXT_custom_border_color",
]


def load_device(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    devs = data.get("devices", [])
    if not devs:
        raise RuntimeError(f"Sin dispositivos Vulkan en {path}")
    # Preferir GPU integrada/discreta frente a CPU (llvmpipe)
    for d in devs:
        if d.get("type") in ("integrated", "discrete"):
            return d
    return devs[0]


def check_mandatory(dev: dict) -> dict:
    feats = dev.get("features", {})
    exts = set(dev.get("extensions", []))
    api_ver = dev.get("api_version", "1.0.0")
    major_minor = tuple(int(x) for x in api_ver.split(".")[:2])
    is_13 = major_minor >= (1, 3)

    missing = []
    for f in MANDATORY_FEATURES:
        if not feats.get(f, False):
            missing.append(f"feature:{f}")

    if not (feats.get("subgroupSizeControl", False) or "VK_EXT_subgroup_size_control" in exts):
        missing.append("feature_or_ext:subgroupSizeControl")

    for e in MANDATORY_EXTS_OR_CORE13:
        if e not in exts:
            missing.append(f"ext:{e}")

    return {
        "device_name": dev.get("name"),
        "api_version": api_ver,
        "vulkan_1_3_or_higher": is_13,
        "passes_minimum": len(missing) == 0,
        "missing_mandatory": missing,
    }


def recommend_flags(dev: dict) -> dict:
    feats = dev.get("features", {})
    sg = dev.get("subgroup", {})
    exts = set(dev.get("extensions", []))

    wave64_native = bool(sg.get("wave64_native_possible", False))
    mesh_hw = bool(feats.get("meshShader", False) and "VK_EXT_mesh_shader" in exts)
    bary_hw = bool(feats.get("fragmentShaderBarycentric", False) and "VK_KHR_fragment_shader_barycentric" in exts)
    float_ctrl = "VK_KHR_shader_float_controls" in exts

    return {
        "wave64_emulate": not wave64_native,
        "mesh_compute": not mesh_hw,
        "no_float_controls": not float_ctrl,
        "bary_fallback": not bary_hw,
        "texel_offset_fallback": True,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--native", required=True, help="JSON de vkprobe nativo Android")
    ap.add_argument("--wine", help="JSON de vkprobe bajo Wine (opcional)")
    ap.add_argument("--soc-id", default="detected-soc", help="Identificador para el perfil JSON")
    ap.add_argument("--out-profile", help="Guardar perfil JSON recomendado en esta ruta")
    args = ap.parse_args()

    nat_dev = load_device(Path(args.native))
    nat_check = check_mandatory(nat_dev)
    eval_dev = nat_dev

    report = {
        "schema": "leonida-mobile/v0-comparison/1",
        "native": nat_check,
    }

    if args.wine:
        wine_dev = load_device(Path(args.wine))
        wine_check = check_mandatory(wine_dev)
        nat_exts = set(nat_dev.get("extensions", []))
        wine_exts = set(wine_dev.get("extensions", []))
        dropped_exts = sorted(nat_exts - wine_exts)

        nat_feats = nat_dev.get("features", {})
        wine_feats = wine_dev.get("features", {})
        dropped_feats = sorted(k for k, v in nat_feats.items() if v and not wine_feats.get(k, False))

        report["wine"] = wine_check
        report["winevulkan_diff"] = {
            "dropped_extensions_count": len(dropped_exts),
            "dropped_extensions": dropped_exts,
            "dropped_features": dropped_feats,
        }
        eval_dev = wine_dev

    flags = recommend_flags(eval_dev)
    report["recommended_translation_flags"] = flags

    print(json.dumps(report, indent=2, ensure_ascii=False))

    if args.out_profile:
        profile = {
            "schema": "leonida-mobile/soc-profile/1",
            "soc": args.soc_id,
            "description": f"Perfil generado automáticamente desde V0 ({eval_dev.get('name')}).",
            "translation_flags": flags,
            "runtime_notes": {
                "api_version": eval_dev.get("api_version"),
                "driver_name": eval_dev.get("driver_name", "unknown"),
                "driver_info": eval_dev.get("driver_info", "unknown"),
                "passes_mandatory_check": check_mandatory(eval_dev)["passes_minimum"],
            },
            "source": "medido con tools/compare_v0.py",
        }
        Path(args.out_profile).write_text(json.dumps(profile, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"Perfil guardado en: {args.out_profile}", file=sys.stderr)

    final_ok = report.get("wine", nat_check)["passes_minimum"]
    return 0 if final_ok else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Registro mínimo por ejecución (Leonida Mobile).

Ejecuta un comando, guarda su log y escribe record.json con los campos del plan:
commit, modelo, SoC, Android, tamaño de página, GPU/driver, contenedor, Wine,
traductor, comando, código de salida, resultado esperado y observado, log y captura.

Uso:
  record_run.py --label NOMBRE [--commit SHA | --repo RUTA] [--container TXT]
                [--wine RUTA] [--translator TXT] [--probe-json F.json]
                [--expect-exit N] [--expect-regex RE] [--timeout S]
                [--capture IMG] [--out DIR] -- COMANDO [ARGS...]

Código de salida: 0 = resultado esperado, 1 = distinto del esperado, 2 = uso.
No recoge serie, IMEI, variables de entorno ni nombre del equipo.
"""
import argparse
import datetime
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path

SCHEMA = "leonida-mobile/run-record/1"
# Pistas de material del juego: si aparecen, el registro NO es publicable.
GAME_HINT = re.compile(r"\.rpf\b|\.ttpf\b|MicrosoftGame\.Config", re.I)


def run_cmd(argv, timeout=10):
    try:
        r = subprocess.run(argv, capture_output=True, text=True,
                           timeout=timeout, errors="replace")
    except (OSError, subprocess.SubprocessError):
        return None
    return (r.stdout or r.stderr or "").strip() or None


def prop(name):
    return run_cmd(["getprop", name]) if shutil.which("getprop") else None


def dmi_model():
    try:
        return Path("/sys/devices/virtual/dmi/id/product_name").read_text().strip() or None
    except OSError:
        return None


def device_info():
    try:
        page = os.sysconf("SC_PAGE_SIZE")
    except (ValueError, OSError, AttributeError):
        page = None
    return {
        "model": prop("ro.product.model") or dmi_model(),
        "soc": prop("ro.soc.model") or prop("ro.board.platform") or prop("ro.hardware"),
        "android": prop("ro.build.version.release"),
        "android_sdk": prop("ro.build.version.sdk"),
        "page_size": page,
        "kernel": run_cmd(["uname", "-r"]),
        "arch": run_cmd(["uname", "-m"]),
    }


def gpu_from_probe(path):
    if not path:
        return None
    try:
        data = json.loads(Path(path).read_text())
    except (OSError, ValueError):
        return {"error": f"no se pudo leer {path}"}
    keys = ("name", "type", "vendor_id", "api_version",
            "driver_version_raw", "driver_name", "driver_info")
    return [{k: d.get(k) for k in keys} for d in data.get("devices", [])]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--label", required=True)
    ap.add_argument("--commit")
    ap.add_argument("--repo", help="repo git del que leer el commit")
    ap.add_argument("--container", help="p. ej. 'Termux+proot', 'Winlator 10.x'")
    ap.add_argument("--wine", default="wine", help="ejecutable de Wine")
    ap.add_argument("--translator", help="traductor bajo prueba (cambia solo uno por comparación)")
    ap.add_argument("--probe-json", help="salida de vkprobe para anotar GPU/driver")
    ap.add_argument("--expect-exit", type=int, default=0)
    ap.add_argument("--expect-regex", help="patrón que debe aparecer en el log")
    ap.add_argument("--timeout", type=float, default=600)
    ap.add_argument("--capture", help="ruta de la captura, si procede")
    ap.add_argument("--out", default="runs")
    ap.add_argument("command", nargs=argparse.REMAINDER)
    args = ap.parse_args()

    cmd = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not cmd:
        ap.error("falta el comando tras --")

    commit = args.commit or (run_cmd(["git", "-C", args.repo, "rev-parse", "HEAD"])
                             if args.repo else None) or "n/a"
    wine_ver = run_cmd([args.wine, "--version"]) if shutil.which(args.wine) else None

    now = datetime.datetime.now(datetime.timezone.utc)
    slug = re.sub(r"[^A-Za-z0-9._-]+", "-", args.label)
    out_dir = Path(args.out) / f"{now.strftime('%Y%m%dT%H%M%SZ')}-{slug}"
    out_dir.mkdir(parents=True, exist_ok=True)

    timed_out = False
    t0 = time.monotonic()
    try:
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, errors="replace")
        try:
            log, _ = p.communicate(timeout=args.timeout)
            code = p.returncode
        except subprocess.TimeoutExpired:
            p.kill()
            log, _ = p.communicate()
            code, timed_out = None, True
    except OSError as e:
        log, code = f"no se pudo ejecutar: {e}\n", 127
    dur = round(time.monotonic() - t0, 3)

    log = log or ""
    (out_dir / "log.txt").write_text(log, encoding="utf-8")

    regex_ok = None if not args.expect_regex else bool(re.search(args.expect_regex, log))
    ok = (not timed_out and code == args.expect_exit and regex_ok in (None, True))
    hint = bool(GAME_HINT.search(shlex.join(cmd)) or GAME_HINT.search(log))

    record = {
        "schema": SCHEMA,
        "label": args.label,
        "utc": now.isoformat(timespec="seconds"),
        "commit": commit,
        "device": device_info(),
        "gpu_driver": gpu_from_probe(args.probe_json),
        "container": args.container,
        "wine": wine_ver,
        "translator": args.translator,
        "command": shlex.join(cmd),
        "exit_code": code,
        "timed_out": timed_out,
        "duration_s": dur,
        "expected": {"exit_code": args.expect_exit, "regex": args.expect_regex},
        "observed": {"exit_code": code, "regex_found": regex_ok, "timed_out": timed_out},
        "result": "pass" if ok else "fail",
        "log": "log.txt",
        "log_sha256": hashlib.sha256(log.encode("utf-8")).hexdigest(),
        "capture": args.capture,
        "game_material_hint": hint,
        "publishable": not hint,
    }
    (out_dir / "record.json").write_text(json.dumps(record, indent=2, ensure_ascii=False) + "\n",
                                         encoding="utf-8")

    print(f"[{record['result']}] {args.label}: exit={code} -> {out_dir}")
    if hint:
        print("AVISO: el comando o el log parecen contener material del juego; no publicar.",
              file=sys.stderr)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

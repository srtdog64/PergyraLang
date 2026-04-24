#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "missing python for module taxonomy smoke" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
manifest_path = root / "docs" / "language_module_manifest.json"
cases_path = root / "docs" / "language_module_cases.json"
taxonomy_path = root / "docs" / "99_language_module_taxonomy.md"
readme_path = root / "README.md"

manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
cases_doc = json.loads(cases_path.read_text(encoding="utf-8"))
taxonomy = taxonomy_path.read_text(encoding="utf-8")
readme = readme_path.read_text(encoding="utf-8")

required = {
    "pgy.foundation",
    "pgy.core",
    "pgy.execution",
    "pgy.runtime.scheduler",
    "pgy.compat.oop",
    "pgy.compat.fp",
    "pgy.std.money",
    "pgy.std.datetime",
    "pgy.std.timer",
    "pgy.std.versioning",
    "pgy.kit.ledger",
    "pgy.kit.obligation",
    "pgy.kit.device_adapter",
}

modules = manifest.get("modules", [])
names = {item.get("name") for item in modules}
missing = sorted(required - names)
if missing:
    raise SystemExit(f"module manifest missing required modules: {missing}")

for item in modules:
    name = item.get("name")
    if not name or not isinstance(name, str):
        raise SystemExit("module manifest entry missing name")
    for field in ("layer", "status", "beta_blocker", "surfaces"):
        if field not in item:
            raise SystemExit(f"{name} missing field {field}")
    if not isinstance(item["surfaces"], list) or not item["surfaces"]:
        raise SystemExit(f"{name} surfaces must be a non-empty list")
    if name in required and name not in taxonomy:
        raise SystemExit(f"{name} missing from taxonomy doc")
    path = item.get("path")
    if path and not (root / path).is_file():
        raise SystemExit(f"{name} path does not exist: {path}")

for token in ("pgy.core", "pgy.foundation", "pgy.execution"):
    if token not in readme:
        raise SystemExit(f"{token} missing from README")

cases = cases_doc.get("cases", [])
if not cases:
    raise SystemExit("language_module_cases.json must contain cases")

seen_core = False
seen_execution = False
seen_compat = False
seen_kit = False
for case in cases:
    path = case.get("path")
    modules_for_case = case.get("modules", [])
    if not path or not (root / path).is_file():
        raise SystemExit(f"case path missing: {path}")
    if not isinstance(modules_for_case, list) or not modules_for_case:
        raise SystemExit(f"{path} must declare module tags")
    unknown = sorted(set(modules_for_case) - names)
    if unknown:
        raise SystemExit(f"{path} uses unknown module tags: {unknown}")
    seen_core = seen_core or "pgy.core" in modules_for_case
    seen_execution = seen_execution or "pgy.execution" in modules_for_case
    seen_compat = seen_compat or any(m.startswith("pgy.compat.") for m in modules_for_case)
    seen_kit = seen_kit or any(m.startswith("pgy.kit.") for m in modules_for_case)

if not (seen_core and seen_execution and seen_compat and seen_kit):
    raise SystemExit("case manifest must cover core, execution, compatibility, and kit layers")

print("[module-taxonomy] manifest and docs ok")
PY

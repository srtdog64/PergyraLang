#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT_DIR/docs/post_selfhost_validation_bug_classes.json"
DOCUMENT="$ROOT_DIR/docs/post_selfhost_validation_milestone.md"
DIAG_CODES="$ROOT_DIR/src/semantic/diag_codes.h"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN=python3
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN=python
    else
        echo "[post-selfhost-validation] python is required" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" "$MANIFEST" "$DIAG_CODES" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
data = json.loads(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
diag_text = pathlib.Path(sys.argv[3]).read_text(encoding="utf-8")
if data.get("schema") != "pgy.post-selfhost-validation-bug-classes.v1":
    raise SystemExit("unexpected manifest schema")
rows = data.get("bug_classes", [])
if [row.get("id") for row in rows] != list(range(1, 11)):
    raise SystemExit("bug class ids must remain the frozen sequence 1..10")
computed = {
    "total": len(rows),
    "static": sum(bool(row.get("static_claim")) for row in rows),
    "runtime": sum(bool(row.get("runtime_claim")) for row in rows),
    "static_only": sum(bool(row.get("static_claim")) and not row.get("runtime_claim") for row in rows),
    "runtime_only": sum(bool(row.get("runtime_claim")) and not row.get("static_claim") for row in rows),
    "static_and_runtime": sum(bool(row.get("static_claim")) and bool(row.get("runtime_claim")) for row in rows),
}
if data.get("claim_totals") != computed:
    raise SystemExit(f"claim totals drift: declared={data.get('claim_totals')} computed={computed}")
if data.get("pass_policy") != {"static_required": 6, "runtime_required": 4}:
    raise SystemExit(f"unexpected frozen pass policy: {data.get('pass_policy')}")
for row in rows:
    if not row.get("static_claim") and not row.get("runtime_claim"):
        raise SystemExit(f"bug class {row['id']} has no claim")
    diagnostic = row.get("expected_diagnostic")
    if row.get("static_claim"):
        if not diagnostic or diagnostic not in diag_text:
            raise SystemExit(f"bug class {row['id']} lacks a registered diagnostic")
    elif diagnostic is not None:
        raise SystemExit(f"runtime-only bug class {row['id']} declares a static diagnostic")
    if row.get("runtime_claim") and not row.get("expected_runtime_observable"):
        raise SystemExit(f"bug class {row['id']} lacks a runtime observable")
    paths = row.get("fixture_paths", [])
    if not paths:
        raise SystemExit(f"bug class {row['id']} lacks fixture ownership")
    status = row.get("fixture_status")
    if status not in {"planned", "landed"}:
        raise SystemExit(f"bug class {row['id']} has invalid fixture status {status!r}")
    if status == "landed":
        for path in paths:
            if not (root / path).is_file():
                raise SystemExit(f"landed fixture is missing: {path}")
PY

"$PYTHON_BIN" "$ROOT_DIR/scripts/render_post_selfhost_validation_manifest.py" \
    "$MANIFEST" "$DOCUMENT" --check

if grep -Eq '7 S-classes|both R-classes|S=static 7|R=runtime fail-close 3' \
        "$DOCUMENT"; then
    echo "[post-selfhost-validation] stale prose-maintained class counts remain" >&2
    exit 1
fi

echo "[post-selfhost-validation] frozen 10-class manifest and generated 8S/4R policy agree"

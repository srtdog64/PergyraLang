#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SOURCE="$ROOT_DIR/tests/air_erasure/fixtures/03_secure_slot.pgy"
OUT="$ROOT_DIR/.tmp/destructure-type-facts.json"
FIELD_SOURCE="$ROOT_DIR/tests/cases/backend_compare/secure_field_slot/main.pgy"
FIELD_OUT="$ROOT_DIR/.tmp/destructure-type-field-facts.json"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "[destructure-type-fact] Python is required" >&2
        exit 1
    fi
fi

mkdir -p "$(dirname "$OUT")"
# Preserve explicit policy requests as rejection witnesses, not downgraded
# noarg safety claims. Analyze only; malformed programs are never emitted/run.
"$PYTHON_BIN" - "$ROOT_DIR" "$PGY" <<'PY'
import hashlib, pathlib, subprocess, sys
root, native = map(pathlib.Path, sys.argv[1:])
fixtures = [root / 'tests/concept_semantics/secure_slot' / name for name in (
    'claim_hardware_local_invalid.pgy', 'claim_hardware_method_invalid.pgy',
    'claim_encrypted_field_invalid.pgy')]
paths = [native, root / 'tests/destructure_type_fact_smoke.sh', *fixtures]
before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
for source in fixtures:
    result = subprocess.run([str(native), '--native-pipeline', '--mir-json',
        '--error-format=json', str(source)], capture_output=True, timeout=30)
    output = (result.stdout + result.stderr).decode(errors='replace')
    if (result.returncode != 1 or '"pgy.mir.v1"' in output
        or 'PGY_SEM_BUILTIN_ARGS_INVALID' not in output
        or 'semantic:builtin:signature_mismatch' not in output):
        raise SystemExit(f'unsupported Claim policy was not exactly rejected: {source}, rc={result.returncode}, {output}')
if any(hashlib.sha256(p.read_bytes()).hexdigest() != digest for p, digest in before.items()):
    raise SystemExit('Claim policy input/binary guard drift')
print('[destructure-type-fact] original hardware/encrypted requests rejected; no physical policy support claim')
PY
"$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$OUT"
"$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$FIELD_SOURCE")" >"$FIELD_OUT"

"$PYTHON_BIN" - "$ROOT_DIR" "$OUT" "$FIELD_OUT" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
with open(sys.argv[2], encoding="utf-8") as stream:
    document = json.load(stream)

main = next(
    (routine for routine in document.get("routines", [])
     if routine.get("name") == "Main"),
    None,
)
expected = [
    (0, 2, "SecureSlot<Int>"),
    (1, 2, "Token<Int>"),
]
actual = [] if main is None else [
    (row.get("binding_index"), row.get("binding_count"),
     row.get("binding_type"))
    for row in main.get("destructure_type_facts", [])
]
if main is None or main.get("destructure_type_fact_count") != 2 \
        or actual != expected:
    raise SystemExit(
        f"secure destructure type facts drifted: count="
        f"{None if main is None else main.get('destructure_type_fact_count')} "
        f"rows={actual!r}"
    )

with open(sys.argv[3], encoding="utf-8") as stream:
    field_document = json.load(stream)
field_rows = [
    row
    for routine in field_document.get("routines", [])
    for row in routine.get("destructure_type_facts", [])
]
if field_rows:
    raise SystemExit(
        "class-field destructure leaked into routine type facts: "
        f"{field_rows!r}"
    )

source = (root / "src/compiler/mir_source_local_types.c").read_text(
    encoding="utf-8"
)
case = source.split("case AST_LET_DESTRUCTURE:", 1)[1].split(
    "case AST_BLOCK:", 1
)[0]
for forbidden in (
    "mir_source_local_expr_type_name",
    "mir_source_local_unwrap_array_or_slice_type",
    "mir_source_local_tuple_element_type",
):
    if forbidden in case:
        raise SystemExit(
            f"destructure source-local typing reopened fallback: {forbidden}"
        )

test = (root / "src/tests/mir/test_mir_lowering_part_i.cases.h").read_text(
    encoding="utf-8"
)
for required in (
    "MIR destructure local typing fails when semantic fact is missing",
    "missing or invalid source-local type fact",
):
    if required not in test:
        raise SystemExit(f"missing destructure negative gate term: {required}")

self_host_gate = (
    root / "tests/self_hosted/parity/driver_rung2_destructure_parity_owner.sh"
).read_text(encoding="utf-8")
for required in (
    "missing semantic destructure type must fail self-host MIR production",
    "missing semantic destructure type was accepted",
    "destructure initializer element type is unknown",
):
    if required not in self_host_gate:
        raise SystemExit(
            f"missing self-host destructure negative gate term: {required}"
        )
PY

# Closure claim: missing semantic destructure fact must fail MIR lowering.
echo "[destructure-type-fact] semantic owner, MIR consumer, and missing-fact gate ok"

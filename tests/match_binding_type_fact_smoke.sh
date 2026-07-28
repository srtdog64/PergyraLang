#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/option_match.pgy"
MIR_LOWER_SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"
BUILD_DIR="$ROOT_DIR/.tmp/match_binding_type_fact"
MIR_JSON="$BUILD_DIR/option_match.mirjson"
MIR_MISSING="$BUILD_DIR/option_match.missing-type.mirjson"
MIR_UNKNOWN="$BUILD_DIR/option_match.unknown-type.mirjson"
MIR_STRAY_RUNTIME="$BUILD_DIR/option_match.stray-domain-runtime.mirjson"
REAST="$BUILD_DIR/option_match.reast"
MIR_LOWER="$BUILD_DIR/mir_lower.exe"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "[match-binding-type-fact] Python is required" >&2
        exit 1
    fi
fi

mkdir -p "$BUILD_DIR"
(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$MIR_JSON")

"$PYTHON_BIN" - "$MIR_JSON" "$MIR_MISSING" "$MIR_UNKNOWN" \
    "$MIR_STRAY_RUNTIME" <<'PY'
import copy
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    document = json.load(stream)

match_rows = [
    instruction
    for routine in document.get("routines", [])
    for block in routine.get("blocks", [])
    for instruction in block.get("instructions", [])
    if instruction.get("source_type") == "AST_MATCH_CASE"
]
some = next((row for row in match_rows
             if row.get("match_variant") == "Some"), None)
none = next((row for row in match_rows
             if row.get("match_variant") == "None"), None)
if some is None or some.get("match_bindings") != ["v"] \
        or some.get("match_binding_types") != ["Int"]:
    raise SystemExit(f"Some binding type carrier drifted: {some!r}")
if none is None or none.get("match_bindings") != [] \
        or none.get("match_binding_types") != []:
    raise SystemExit(f"None binding type carrier drifted: {none!r}")

missing = copy.deepcopy(document)
unknown = copy.deepcopy(document)
for candidate, replacement in ((missing, []), (unknown, ["Unknown"])):
    for routine in candidate["routines"]:
        for block in routine["blocks"]:
            for instruction in block["instructions"]:
                if instruction.get("match_variant") == "Some":
                    instruction["match_binding_types"] = replacement

for path, candidate in ((sys.argv[2], missing), (sys.argv[3], unknown)):
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(candidate, stream, ensure_ascii=False, separators=(",", ":"))
        stream.write("\n")

runtime = document.get("domain_runtime_assignments")
if runtime != {"participant_roles": [], "projection_members": []}:
    raise SystemExit(f"canonical empty domain runtime projection drifted: {runtime!r}")
stray_runtime = copy.deepcopy(document)
stray_runtime["domain_runtime_assignments"]["participant_roles"].append({})
with open(sys.argv[4], "w", encoding="utf-8", newline="\n") as stream:
    json.dump(stray_runtime, stream, ensure_ascii=False, separators=(",", ":"))
    stream.write("\n")
PY

(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER_SOURCE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER")" \
    >/dev/null)
(cd "$ROOT_DIR" && "$MIR_LOWER" "${MIR_JSON#$ROOT_DIR/}" >"$REAST")
grep -Fq 'Let: v : Int = UnwrapOption(val)' "$REAST" || {
    echo "[match-binding-type-fact] typed match binding was not reconstructed" >&2
    exit 1
}

# The native writer always projects the canonical two-array domain-runtime
# object.  For a program without domain topology, only the exact empty pair is
# semantically absent.  A non-empty stray carrier must still fail before AST
# reconstruction instead of being hidden by the empty-projection rule.
if (cd "$ROOT_DIR" && "$MIR_LOWER" "${MIR_STRAY_RUNTIME#$ROOT_DIR/}" \
        >"$MIR_STRAY_RUNTIME.out" 2>"$MIR_STRAY_RUNTIME.err"); then
    echo "[match-binding-type-fact] stray domain runtime fact was accepted" >&2
    exit 1
fi
grep -Fq 'MIR machine-layer facts are missing or invalid' \
    "$MIR_STRAY_RUNTIME.out" "$MIR_STRAY_RUNTIME.err" || {
    echo "[match-binding-type-fact] stray domain runtime diagnostic drifted" >&2
    cat "$MIR_STRAY_RUNTIME.out" "$MIR_STRAY_RUNTIME.err" >&2
    exit 1
}
if grep -Eq '^Program:|^  Function:' \
    "$MIR_STRAY_RUNTIME.out" "$MIR_STRAY_RUNTIME.err"; then
    echo "[match-binding-type-fact] stray domain runtime fact emitted partial AST" >&2
    exit 1
fi

# untyped_match_binding_render is forbidden: the positive assertion above
# requires the semantic type annotation, and both absent/Unknown rows below
# must fail before a binding statement can be reconstructed.

for mutation in missing unknown; do
    input="$BUILD_DIR/option_match.$mutation-type.mirjson"
    output="$BUILD_DIR/option_match.$mutation-type.out"
    error="$BUILD_DIR/option_match.$mutation-type.err"
    if (cd "$ROOT_DIR" && "$MIR_LOWER" "${input#$ROOT_DIR/}" \
            >"$output" 2>"$error"); then
        echo "[match-binding-type-fact] $mutation binding type was accepted" >&2
        exit 1
    fi
    grep -Fq 'match_binding_type' "$output" "$error" || {
        echo "[match-binding-type-fact] $mutation diagnostic drifted" >&2
        cat "$output" "$error" >&2
        exit 1
    }
done

grep -Fq 'mir_instruction_match_binding_type_at' \
    "$ROOT_DIR/src/compiler/mir_json_dump.c" || {
    echo "[match-binding-type-fact] JSON projection bypassed MIR owner" >&2
    exit 1
}

# variant_name_type_guess and source_match_binding_type_reparse are forbidden:
# JSON projection must read the carried MIR fact through the owner query above;
# it must not derive a binding type from a constructor spelling or source AST.

echo "[match-binding-type-fact] semantic -> HIR -> MIR -> Pergyra carrier ok"

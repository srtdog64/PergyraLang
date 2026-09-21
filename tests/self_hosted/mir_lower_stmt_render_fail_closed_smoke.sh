#!/usr/bin/env bash
# MIR statement render fail-closed gate.
#
# Owner: src/self_hosted/mir_lower/stmt_render.pgy,
# src/self_hosted/mir_lower/match_binding_render_owner.pgy and the string-array
# fact reader in src/self_hosted/mir_lower/json_fact_read.pgy.
#
# Positive rows pin the reconstructed AST lines for destructure temporaries and
# direct-call defer bodies. Negative rows mutate native MIR JSON so that each
# missing/malformed statement fact must stop reconstruction with its own
# MIR-LOWER diagnostic instead of truncating, reclassifying, or passing the
# expression text through as a statement line.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
MIR_LOWER_SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"
FIXTURE_DIR="$ROOT_DIR/src/self_hosted/mir_lower/fixture"
BUILD_DIR="$ROOT_DIR/.tmp/mir_lower_stmt_render_fail_closed"
MIR_LOWER="$BUILD_DIR/mir_lower.exe"
PYTHON_BIN="${PYTHON_BIN:-}"
LABEL="[mir-lower-stmt-render]"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "$LABEL Python is required" >&2
        exit 1
    fi
fi

mkdir -p "$BUILD_DIR"

emit_mir_json() {
    local fixture="$1"
    local out="$2"
    (cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
        "$(pgy_path_for_compiler "$PGY" "$FIXTURE_DIR/$fixture.pgy")" \
        >"$out")
}

emit_mir_json array_destructure "$BUILD_DIR/destructure.mirjson"
emit_mir_json option_match "$BUILD_DIR/option_match.mirjson"
emit_mir_json let_log "$BUILD_DIR/let_log.mirjson"
emit_mir_json defer_direct_call "$BUILD_DIR/defer_direct_call.mirjson"

"$PYTHON_BIN" - "$BUILD_DIR" <<'PY'
import copy
import json
import pathlib
import sys

build = pathlib.Path(sys.argv[1])


def load(name):
    with open(build / f"{name}.mirjson", encoding="utf-8") as stream:
        return json.load(stream)


def instructions(document):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                yield instruction


def mutate(name, output, select, apply):
    document = copy.deepcopy(load(name))
    hits = 0
    for instruction in instructions(document):
        if select(instruction):
            apply(instruction)
            hits += 1
            break
    if hits != 1:
        raise SystemExit(f"mutation {output} selected {hits} rows")
    with open(build / f"{output}.mirjson", "w", encoding="utf-8",
              newline="\n") as stream:
        json.dump(document, stream, ensure_ascii=False, separators=(",", ":"))
        stream.write("\n")


def is_destructure(row):
    return row.get("kind") == "destructure"


def is_some_case(row):
    return row.get("match_variant") == "Some"


def is_let_decl(row):
    return row.get("kind") == "def" and \
        row.get("source_type") == "AST_LET_DECL"


def is_log_stmt(row):
    return row.get("kind") == "stmt" and row.get("arg0") == "Log"


def set_field(field, value):
    def apply(row):
        row[field] = value
    return apply


def drop_fields(*fields):
    def apply(row):
        for field in fields:
            if field not in row:
                raise SystemExit(f"field {field} is absent before mutation")
            del row[field]
    return apply


def hollow_middle_binding(row):
    bindings = row.get("destructure_bindings") or []
    if len(bindings) < 3:
        raise SystemExit(f"destructure fixture drifted: {bindings!r}")
    row["destructure_bindings"] = [bindings[0], "", bindings[2]]


def non_string_binding(row):
    bindings = row.get("destructure_bindings") or []
    row["destructure_bindings"] = [bindings[0], 7] + bindings[2:]


mutate("destructure", "destructure.hollow-binding", is_destructure,
       hollow_middle_binding)
mutate("destructure", "destructure.non-string-binding", is_destructure,
       non_string_binding)
mutate("destructure", "destructure.unknown-element", is_destructure,
       set_field("destructure_element_type", "Unknown"))
mutate("option_match", "option_match.absent-bindings", is_some_case,
       drop_fields("match_bindings", "match_binding_types"))
mutate("option_match", "option_match.null-binding-type", is_some_case,
       set_field("match_binding_types", [None]))
mutate("let_log", "let_log.nameless-let", is_let_decl,
       set_field("arg0", ""))
mutate("let_log", "let_log.cleanup-kind", is_log_stmt,
       set_field("kind", "cleanup"))
mutate("let_log", "let_log.non-string-use", is_log_stmt,
       set_field("uses", [7]))
PY

(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER_SOURCE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER")" \
    >/dev/null)

run_mir_lower() {
    local input="$1"
    local output="$2"
    local error="$3"
    (cd "$ROOT_DIR" && "$MIR_LOWER" "${input#$ROOT_DIR/}" \
        >"$output" 2>"$error")
}

expect_line() {
    local file="$1"
    local line="$2"
    local what="$3"
    grep -Fq -- "$line" "$file" || {
        echo "$LABEL $what was not reconstructed: $line" >&2
        cat "$file" >&2
        exit 1
    }
}

run_mir_lower "$BUILD_DIR/destructure.mirjson" \
    "$BUILD_DIR/destructure.reast" "$BUILD_DIR/destructure.err"
expect_line "$BUILD_DIR/destructure.reast" \
    'Let: _pgy_destructure_id_str : Array<String> = Split(csv, ",")' \
    "destructure temporary"
expect_line "$BUILD_DIR/destructure.reast" \
    'Let: active_str : String = _pgy_destructure_id_str[2]' \
    "destructure binding"

run_mir_lower "$BUILD_DIR/defer_direct_call.mirjson" \
    "$BUILD_DIR/defer_direct_call.reast" "$BUILD_DIR/defer_direct_call.err"
expect_line "$BUILD_DIR/defer_direct_call.reast" \
    'Call: Cleanup("done")' "direct-call defer body"

# Each row: mutation name, required diagnostic fragment.
negative_rows=(
    "destructure.hollow-binding|destructure binding-name fact is empty"
    "destructure.non-string-binding|destructure instruction is missing binding-name facts"
    "destructure.unknown-element|destructure initializer element type is unknown"
    # Field absence reads as an empty array and the routine fact index then
    # rejects it before the binding renderer runs. A present array with a
    # null element is refused earlier, by the string-array fact reader.
    "option_match.absent-bindings|routine MIR fact index is incomplete"
    "option_match.null-binding-type|MIR string array fact is malformed: match_binding_types"
    "let_log.nameless-let|local declaration is missing its binding-name fact"
    "let_log.cleanup-kind|instruction kind has no statement rendering"
    # A present string array with a non-string element used to read as
    # its (here empty) prefix.
    "let_log.non-string-use|MIR string array fact is malformed: uses"
)

for row in "${negative_rows[@]}"; do
    mutation="${row%%|*}"
    diagnostic="${row#*|}"
    input="$BUILD_DIR/$mutation.mirjson"
    output="$BUILD_DIR/$mutation.out"
    error="$BUILD_DIR/$mutation.err"
    if run_mir_lower "$input" "$output" "$error"; then
        echo "$LABEL $mutation was accepted" >&2
        cat "$output" >&2
        exit 1
    fi
    grep -Fq -- "$diagnostic" "$output" "$error" || {
        echo "$LABEL $mutation diagnostic drifted (want: $diagnostic)" >&2
        cat "$output" "$error" >&2
        exit 1
    }
    if grep -Eq '^Program:' "$output"; then
        echo "$LABEL $mutation emitted a partial AST" >&2
        exit 1
    fi
done

echo "$LABEL destructure/defer/match/let render facts and string-array facts fail closed"

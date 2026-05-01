#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_ROOT="$ROOT_DIR/.tmp"
mkdir -p "$WORK_ROOT"
WORK_DIR="$(mktemp -d "$WORK_ROOT/pgy_air_backend_nonimpact.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_PGY="${TMPDIR:-/tmp}/pgy-$(basename "$ROOT_DIR")-bin/pgy"
if [[ -x "${DEFAULT_PGY}.exe" ]]; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if [[ -x "${TMP_PGY}.exe" ]]; then
    TMP_PGY="${TMP_PGY}.exe"
fi
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY_BIN="$PGY_BIN"
elif [[ -x "$TMP_PGY" ]]; then
    PGY_BIN="$TMP_PGY"
else
    PGY_BIN="$DEFAULT_PGY"
fi

if [[ ! -x "$PGY_BIN" ]]; then
    echo "air-backend-nonimpact: missing compiler binary: $PGY_BIN" >&2
    exit 1
fi

require_normal_backend_air_mir_gate() {
    local source_rel="tests/cases/backend_compare/intent_zone_binding/main.pgy"
    local out="$WORK_DIR/air_mir_gate.c"
    local log="$WORK_DIR/air_mir_gate.log"

    if ! (cd "$ROOT_DIR" && PGY_DEBUG_PIPELINE_STAGE=1 "$PGY_BIN" "$source_rel" --emit-c -o "$out") \
        >"$log" 2>&1; then
        echo "air-backend-nonimpact: normal backend AIR/MIR gate probe failed" >&2
        cat "$log" >&2
        return 1
    fi

    if ! grep -Fq "[driver stage] air_mir_evidence" "$log"; then
        echo "air-backend-nonimpact: normal backend path did not report AIR MIR evidence stage" >&2
        cat "$log" >&2
        return 1
    fi
    if ! awk '
        /\[driver stage\] air_mir_evidence/ { saw_air = NR }
        /\[driver stage\] backend_c/ { saw_backend = NR }
        END { exit !(saw_air > 0 && saw_backend > saw_air) }
    ' "$log"; then
        echo "air-backend-nonimpact: backend stage did not run after AIR MIR evidence gate" >&2
        cat "$log" >&2
        return 1
    fi
}

require_normal_backend_air_mir_gate

DEFAULT_SOURCES=(
    "tests/cases/backend_compare/intent_zone_binding/main.pgy"
    "tests/cases/backend_compare/intent_cross_world_transfer/main.pgy"
    "tests/cases/backend_compare/handoff_projection_frontier/main.pgy"
    "tests/cases/backend_compare/handoff_world_state_frontier/main.pgy"
    "tests/cases/backend_compare/world_zone_projection_visibility/main.pgy"
    "tests/cases/backend_compare/world_embedded_action_frontier/main.pgy"
    "tests/cases/backend_compare/relation_effect_propagation/main.pgy"
    "tests/cases/backend_compare/authority_failure_surface/main.pgy"
)

if [[ "${PGY_AIR_NONIMPACT_SOURCE:-}" == "all" ]]; then
    mapfile -t SOURCES < <(
        cd "$ROOT_DIR"
        find tests/cases/backend_compare -mindepth 2 -maxdepth 2 -name main.pgy | LC_ALL=C sort
    )
elif [[ -n "${PGY_AIR_NONIMPACT_SOURCE:-}" ]]; then
    read -r -a SOURCES <<<"$PGY_AIR_NONIMPACT_SOURCE"
else
    SOURCES=("${DEFAULT_SOURCES[@]}")
fi

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi

generated_files_equal() {
    local left="$1"
    local right="$2"
    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" <<'PY'
import pathlib, re, sys

def normalize(path):
    text = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    text = re.sub(r"ast=0x[0-9A-Fa-f]+", "ast=<ptr>", text)
    return text

left = normalize(sys.argv[1])
right = normalize(sys.argv[2])
raise SystemExit(0 if left == right else 1)
PY
        return $?
    fi
    sed -E 's/ast=0x[0-9A-Fa-f]+/ast=<ptr>/g' "$left" >"$left.norm"
    sed -E 's/ast=0x[0-9A-Fa-f]+/ast=<ptr>/g' "$right" >"$right.norm"
    cmp -s "$left.norm" "$right.norm"
}

show_generated_diff() {
    local left="$1"
    local right="$2"
    local left_norm="$WORK_DIR/$(basename "$left").norm"
    local right_norm="$WORK_DIR/$(basename "$right").norm"
    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" "$left_norm" "$right_norm" <<'PY'
import pathlib, re, sys

def normalize(path):
    text = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    return re.sub(r"ast=0x[0-9A-Fa-f]+", "ast=<ptr>", text)

pathlib.Path(sys.argv[3]).write_text(normalize(sys.argv[1]), encoding="utf-8")
pathlib.Path(sys.argv[4]).write_text(normalize(sys.argv[2]), encoding="utf-8")
PY
    else
        sed -E 's/ast=0x[0-9A-Fa-f]+/ast=<ptr>/g' "$left" >"$left_norm"
        sed -E 's/ast=0x[0-9A-Fa-f]+/ast=<ptr>/g' "$right" >"$right_norm"
    fi
    if command -v git >/dev/null 2>&1; then
        git --no-pager diff --no-index -- "$left_norm" "$right_norm" || true
    elif command -v diff >/dev/null 2>&1; then
        diff -u "$left_norm" "$right_norm" || true
    else
        echo "--- $left_norm"
        cat "$left_norm"
        echo "--- $right_norm"
        cat "$right_norm"
    fi
}

sanitize_case_name() {
    local source_rel="$1"
    printf '%s\n' "$source_rel" | sed -E 's#[^A-Za-z0-9_]+#_#g; s#_main_pgy$##'
}

run_emit_pair() {
    local name="$1"
    local flag="$2"
    local ext="$3"
    local source_rel="$4"
    local case_name
    local relaxed_out
    local strict_out
    local relaxed_log
    local strict_log

    case_name="$(sanitize_case_name "$source_rel")"
    relaxed_out="$WORK_DIR/${case_name}_${name}_relaxed.${ext}"
    strict_out="$WORK_DIR/${case_name}_${name}_strict.${ext}"
    relaxed_log="$WORK_DIR/${case_name}_${name}_relaxed.log"
    strict_log="$WORK_DIR/${case_name}_${name}_strict.log"

    if ! (cd "$ROOT_DIR" && PGY_AIR_STRICT_EVIDENCE=0 "$PGY_BIN" "$source_rel" "$flag" -o "$relaxed_out") \
        >"$relaxed_log" 2>&1; then
        echo "air-backend-nonimpact: relaxed AIR emit failed for $name" >&2
        cat "$relaxed_log" >&2
        return 1
    fi

    if ! (cd "$ROOT_DIR" && unset PGY_AIR_STRICT_EVIDENCE && "$PGY_BIN" "$source_rel" "$flag" -o "$strict_out") \
        >"$strict_log" 2>&1; then
        echo "air-backend-nonimpact: default strict AIR emit failed for $name" >&2
        cat "$strict_log" >&2
        return 1
    fi

    if ! generated_files_equal "$relaxed_out" "$strict_out"; then
        echo "air-backend-nonimpact: generated $name output changed under default strict AIR" >&2
        show_generated_diff "$relaxed_out" "$strict_out" >&2
        return 1
    fi

    echo "air-backend-nonimpact: PASS $name ($source_rel)"
}

for source_rel in "${SOURCES[@]}"; do
    source="$ROOT_DIR/$source_rel"
    if [[ ! -f "$source" ]]; then
        echo "air-backend-nonimpact: missing source fixture: $source_rel" >&2
        exit 1
    fi

    run_emit_pair "c" "--emit-c" "c" "$source_rel"

    if [[ "${PGY_AIR_NONIMPACT_BACKENDS:-c llvm}" == *"llvm"* ]]; then
        if "$PGY_BIN" --help 2>&1 | grep -q -- "--emit-llvm"; then
            run_emit_pair "llvm" "--emit-llvm" "ll" "$source_rel"
        else
            echo "air-backend-nonimpact: SKIP llvm (compiler built without LLVM support)"
        fi
    fi
done

echo "air-backend-nonimpact: ok"

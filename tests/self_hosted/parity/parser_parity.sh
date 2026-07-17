#!/usr/bin/env bash
# Rung 1 parity for the minimal Pergyra-origin parser (2026-05-28).
# Each source pair is committed under fixture/<source>.pgy +
# fixture/<source>_ast.txt. The Pergyra binary reads Args()[0] to pick
# which source to parse.
# See tests/self_hosted/parity/README.md.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/parser_tool_build_leg.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:parser] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:parser] missing compiler binary: $PGY" >&2
    exit 1
fi
PGY_EXEC="$(pgy_path_for_bash_tool "$PGY")"

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/parser}"
ARTIFACT_COMPARE_BUILD_DIR="$PERGYRA_TOOL_BUILD_DIR/artifact_owner"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/parser_harness_paths.txt"
PARSER_FIXTURE_MANIFEST_FILE="$PERGYRA_TOOL_BUILD_DIR/parser_fixture_manifest.txt"
C_PARSER_COMPILED=0

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:parser" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "parser-parity-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 4 ]]; then
    echo "[self-host-parity:parser] TestHarness manifest expected 4 parser paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"
COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[1]}"
FIXTURE_DIR="$ROOT_DIR/${harness_paths[2]}"
EXPECTED_FILE="$ROOT_DIR/${harness_paths[3]}"

for path in "$PERGYRA_TOOL_SOURCE" "$COMPARATOR_SOURCE" "$EXPECTED_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:parser] missing TestHarness input: $path" >&2
        exit 1
    fi
done
if [[ ! -d "$FIXTURE_DIR" ]]; then
    echo "[self-host-parity:parser] missing TestHarness fixture dir: $FIXTURE_DIR" >&2
    exit 1
fi

pgy_selfhost_compile_backend_output_comparator \
    "self-host-parity:parser" "$ARTIFACT_COMPARE_BUILD_DIR" "$COMPARATOR_SOURCE"
AST_COMPARATOR_BIN="$(pgy_selfhost_backend_output_comparator_bin "$ARTIFACT_COMPARE_BUILD_DIR")"

compare_parser_ast_with_owner() {
    local label="$1"
    local expected_file="$2"
    local actual_file="$3"
    local expected_norm="$ARTIFACT_COMPARE_BUILD_DIR/${label//[^A-Za-z0-9_]/_}_expected.txt"
    local actual_norm="$ARTIFACT_COMPARE_BUILD_DIR/${label//[^A-Za-z0-9_]/_}_actual.txt"
    local cmp_out="$ARTIFACT_COMPARE_BUILD_DIR/${label//[^A-Za-z0-9_]/_}.compare.out"
    local cmp_err="$ARTIFACT_COMPARE_BUILD_DIR/${label//[^A-Za-z0-9_]/_}.compare.err"
    local expected_text
    local actual_text
    local expected_rel
    local actual_rel

    expected_text="$(tr -d '\r' < "$expected_file")"
    actual_text="$(tr -d '\r' < "$actual_file")"
    printf '%s' "$expected_text" > "$expected_norm"
    printf '%s' "$actual_text" > "$actual_norm"
    expected_rel="$(pgy_selfhost_path_relative_to_root "$expected_norm")"
    actual_rel="$(pgy_selfhost_path_relative_to_root "$actual_norm")"

    if ! (cd "$ROOT_DIR" && "$AST_COMPARATOR_BIN" "$expected_rel" "$actual_rel" 0 2 ast_text \
        >"$cmp_out" 2>"$cmp_err"); then
        echo "[self-host-parity:parser] $label: AST artifact parity FAIL" >&2
        cat "$cmp_out" "$cmp_err" >&2
        exit 1
    fi
}

# Sources: each pair is "<source.pgy path relative to repo root>:<fixture base>"
# where fixture base resolves to fixture/<base>_ast.txt. The compiled parser
# owner emits this inventory through --fixture-manifest.
SOURCE_PAIRS=()

read_parser_fixture_manifest() {
    local manifest_bin="$PERGYRA_TOOL_BUILD_DIR/main_c.exe"
    local manifest_log="$PERGYRA_TOOL_BUILD_DIR/main_manifest.compile.log"
    local line

    if ! pgy_selfhost_compile_parser_tool \
        "self-host-parity:parser" "$PERGYRA_TOOL_SOURCE" c \
        "$manifest_bin" "$manifest_log"; then
        echo "[self-host-parity:parser] parser fixture manifest owner failed to build" >&2
        cat "$manifest_log" >&2
        exit 1
    fi
    C_PARSER_COMPILED=1

    SOURCE_PAIRS=()
    if ! (cd "$ROOT_DIR" && "$manifest_bin" --fixture-manifest \
        >"$PARSER_FIXTURE_MANIFEST_FILE" \
        2>"$PERGYRA_TOOL_BUILD_DIR/parser_fixture_manifest.err"); then
        echo "[self-host-parity:parser] fixture manifest emission failed" >&2
        cat "$PERGYRA_TOOL_BUILD_DIR/parser_fixture_manifest.err" >&2
        exit 1
    fi

    while IFS= read -r line; do
        line="${line%$'\r'}"
        [[ -n "$line" ]] || continue
        SOURCE_PAIRS+=("$line")
    done <"$PARSER_FIXTURE_MANIFEST_FILE"

    if [[ "${#SOURCE_PAIRS[@]}" -ne 188 ]]; then
        echo "[self-host-parity:parser] fixture manifest count drifted: ${#SOURCE_PAIRS[@]} != 188" >&2
        exit 1
    fi
}

check_live_fixture_drift() {
    local any_drift_guard_ran="no"

    for pair in "${SOURCE_PAIRS[@]}"; do
        local src="${pair%%:*}"
        local base="${pair##*:}"
        local expected_fixture="$FIXTURE_DIR/${base}_ast.txt"

        if [[ ! -f "$ROOT_DIR/$src" ]]; then
            echo "[self-host-parity:parser] missing source: $src" >&2
            exit 1
        fi
        if [[ ! -f "$expected_fixture" ]]; then
            echo "[self-host-parity:parser] missing AST fixture: $expected_fixture" >&2
            exit 1
        fi

        local live_out="$PERGYRA_TOOL_BUILD_DIR/live_${base}_ast.txt"
        local live_err="$PERGYRA_TOOL_BUILD_DIR/live_${base}_ast.err"
        local live_text
        local live_rc
        set +e
            live_text="$(cd "$ROOT_DIR" && "$PGY_EXEC" --ast "$src" 2>"$live_err")"
        live_rc=$?
        set -e
        printf '%s' "$live_text" > "$live_out"
        if [[ "$live_rc" -eq 0 && -n "$live_text" ]]; then
            compare_parser_ast_with_owner "live:$base" "$expected_fixture" "$live_out"
            any_drift_guard_ran="yes"
        fi
    done

    printf '%s\n' "$any_drift_guard_ran"
}

compile_parser_backend() {
    local backend="$1"
    local tool_bin="$2"
    local compile_log="$PERGYRA_TOOL_BUILD_DIR/main_${backend}.compile.log"

    echo "[self-host-parity:parser] compiling parser backend=$backend..."
    if ! pgy_selfhost_compile_parser_tool \
        "self-host-parity:parser" "$PERGYRA_TOOL_SOURCE" "$backend" \
        "$tool_bin" "$compile_log"; then
        cat "$compile_log" >&2
        exit 1
    fi
}

run_parser_backend() {
    local backend="$1"
    local tool_bin="$2"

    for pair in "${SOURCE_PAIRS[@]}"; do
        local src="${pair%%:*}"
        local base="${pair##*:}"
        local expected_fixture="$FIXTURE_DIR/${base}_ast.txt"
        local pergyra_out="$PERGYRA_TOOL_BUILD_DIR/parser_${backend}_${base}.out"
        local pergyra_err="$PERGYRA_TOOL_BUILD_DIR/parser_${backend}_${base}.err"
        local pergyra_text
        local p_rc

        set +e
        pergyra_text="$(cd "$ROOT_DIR" && "$tool_bin" "$src" 2>"$pergyra_err")"
        p_rc=$?
        set -e
        printf '%s' "$pergyra_text" > "$pergyra_out"

        if [[ "$p_rc" -ne 0 ]]; then
            echo "[self-host-parity:parser] backend=$backend $src: exit-code FAIL (pergyra=$p_rc)" >&2
            cat "$pergyra_out" "$pergyra_err" >&2
            exit 1
        fi

        compare_parser_ast_with_owner "backend:$backend:$base" \
            "$expected_fixture" "$pergyra_out"
    done

    echo "[self-host-parity:parser] backend=$backend byte-equal (${#SOURCE_PAIRS[@]} sources)"
}

BACKENDS="${PGY_SELFHOST_PARSER_BACKENDS:-c llvm}"
read_parser_fixture_manifest
ANY_DRIFT_GUARD_RAN="$(check_live_fixture_drift)"

for backend in $BACKENDS; do
    tool_bin="$PERGYRA_TOOL_BUILD_DIR/main_${backend}.exe"
    if [[ "$backend" != "c" || "$C_PARSER_COMPILED" -ne 1 ]]; then
        compile_parser_backend "$backend" "$tool_bin"
    fi
    run_parser_backend "$backend" "$tool_bin"
done

echo "[self-host-parity:parser] rung-1 parity ok (${#SOURCE_PAIRS[@]} sources byte-equal; backends=$BACKENDS; live-drift=$ANY_DRIFT_GUARD_RAN)"

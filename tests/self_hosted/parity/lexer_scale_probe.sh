#!/usr/bin/env bash
# Run the Pergyra-origin lexer against the examples corpus and backend-compare
# corpus, then count how many produce byte-equal output vs `pgy --tokens`. This
# is a coverage probe, not a parity gate. Output is a count summary plus lists
# of matching and failing files.
#
# Mirrors parser_scale_probe.sh. The status doc records the lexer at
# "191 of 195 historical sources byte-equal"; this probe re-measures that
# historical examples + backend_compare surface.

set -uo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v mkdir >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi

if [[ ! -x "$PGY" ]]; then
    echo "[scale-probe] missing pgy: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/lexer_scale}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/lexer_scale_harness_paths.txt"
COMPARATOR_LABEL="self-host-parity:lexer-scale"
PERGYRA_TOOL_SOURCE=""
PERGYRA_TOOL_ARG=""
COMPARATOR_SOURCE=""
SCALE_LIMIT="${PGY_SCALE_PROBE_LIMIT:-20}"
SHOW_FAILING=0

for arg in "$@"; do
    case "$arg" in
        --failing)
            SHOW_FAILING=1
            ;;
        --full)
            SCALE_LIMIT=0
            ;;
        --limit=*)
            SCALE_LIMIT="${arg#--limit=}"
            ;;
        *)
            echo "[scale-probe] unknown option: $arg" >&2
            exit 1
            ;;
    esac
done
if ! [[ "$SCALE_LIMIT" =~ ^[0-9]+$ ]]; then
    echo "[scale-probe] limit must be a non-negative integer: $SCALE_LIMIT" >&2
    exit 1
fi

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "$COMPARATOR_LABEL" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "lexer-parity-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 3 ]]; then
    echo "[scale-probe] TestHarness manifest expected 3 lexer paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"
COMPARATOR_SOURCE="$ROOT_DIR/${harness_paths[1]}"
if [[ ! -f "$PERGYRA_TOOL_SOURCE" || ! -f "$COMPARATOR_SOURCE" ]]; then
    echo "[scale-probe] missing TestHarness lexer/comparator source" >&2
    exit 1
fi

echo "[scale-probe] compiling lexer..."
(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" -o "$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_BUILD_DIR/main.exe")" >/dev/null)
pgy_selfhost_compile_backend_output_comparator "$COMPARATOR_LABEL" "$PERGYRA_TOOL_BUILD_DIR" "$COMPARATOR_SOURCE"

artifact_files_equal() {
    local left="$1"
    local right="$2"
    local left_rel
    local right_rel
    local comparator_bin

    left_rel="$(pgy_selfhost_path_relative_to_root "$left")"
    right_rel="$(pgy_selfhost_path_relative_to_root "$right")"
    comparator_bin="$(pgy_selfhost_backend_output_comparator_bin "$PERGYRA_TOOL_BUILD_DIR")"
    (cd "$ROOT_DIR" && "$comparator_bin" "$left_rel" "$right_rel" 0 2 run_output \
        > "$PERGYRA_TOOL_BUILD_DIR/scale_compare.out" \
        2> "$PERGYRA_TOOL_BUILD_DIR/scale_compare.err")
}

shopt -s nullglob globstar
TOTAL=0
MATCH=0
DIFFER=0
P_FAIL=0
C_SKIP=0
MATCH_LIST=()
FAIL_LIST=()
LIVE_FILE="$PERGYRA_TOOL_BUILD_DIR/live.tokens"
PERGYRA_FILE="$PERGYRA_TOOL_BUILD_DIR/pergyra.tokens"

for src in "$ROOT_DIR"/examples/*.pgy "$ROOT_DIR"/tests/cases/backend_compare/**/main.pgy; do
    rel="${src#$ROOT_DIR/}"
    TOTAL=$((TOTAL + 1))

    if ! (cd "$ROOT_DIR" && "$PGY" --tokens "$rel" 2>/dev/null | tr -d '\r' > "$LIVE_FILE"); then
        C_SKIP=$((C_SKIP + 1))
        continue
    fi
    if [[ ! -s "$LIVE_FILE" ]]; then
        C_SKIP=$((C_SKIP + 1))
        continue
    fi

    if ! (cd "$ROOT_DIR" && "$PERGYRA_TOOL_BUILD_DIR/main.exe" "$rel" 2>/dev/null \
        | tr -d '\r' \
        | sed '/^pgy: compiled /d' > "$PERGYRA_FILE"); then
        P_FAIL=$((P_FAIL + 1))
        FAIL_LIST+=("$rel (pergyra exit)")
        continue
    fi

    if artifact_files_equal "$PERGYRA_FILE" "$LIVE_FILE"; then
        MATCH=$((MATCH + 1))
        MATCH_LIST+=("$rel")
    else
        DIFFER=$((DIFFER + 1))
        FAIL_LIST+=("$rel (byte-drift)")
        cp "$LIVE_FILE" "$PERGYRA_TOOL_BUILD_DIR/fail_live.tokens"
        cp "$PERGYRA_FILE" "$PERGYRA_TOOL_BUILD_DIR/fail_pergyra.tokens"
        printf '%s\n' "$rel" > "$PERGYRA_TOOL_BUILD_DIR/fail_source.txt"
    fi
    if [[ "$SCALE_LIMIT" -gt 0 && "$TOTAL" -ge "$SCALE_LIMIT" ]]; then
        break
    fi
done

echo "[scale-probe] total=$TOTAL match=$MATCH differ=$DIFFER pergyra-fail=$P_FAIL c-skip=$C_SKIP limit=$SCALE_LIMIT"
if [[ "$SHOW_FAILING" -eq 1 ]]; then
    for f in "${FAIL_LIST[@]}"; do echo "  - $f"; done
elif [[ ${#MATCH_LIST[@]} -gt 0 ]]; then
    echo "[scale-probe] matching files:"
    for f in "${MATCH_LIST[@]}"; do
        echo "  + $f"
    done
fi

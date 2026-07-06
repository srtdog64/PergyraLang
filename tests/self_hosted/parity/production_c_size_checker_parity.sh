#!/usr/bin/env bash
# Rung 2 parity for the production C size checker (2026-05-27).
# Sister parity for tool 8. Asserts:
#   - clean repo: rc=0, JSON byte-equal vs expected/clean.json
#   - synthetic over-cap fixture (1001-line .c under src/runtime): rc=1,
#     JSON byte-equal vs expected/over_cap.json
# See tests/self_hosted/parity/README.md.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:production-c-size] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:production-c-size] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/production_c_size_checker}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/production_c_size_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:production-c-size" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "production-c-size-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 5 ]]; then
    echo "[self-host-parity:production-c-size] TestHarness manifest expected 5 production-c-size rows, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"
OVER_CAP_FIXTURE_PATH="${harness_paths[2]}"
OVER_CAP_LINE_COUNT="${harness_paths[3]}"
EXPECTED_OVER_CAP_JSON_FILE="$ROOT_DIR/${harness_paths[4]}"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$EXPECTED_OVER_CAP_JSON_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:production-c-size] missing input: $path" >&2
        exit 1
    fi
done
if [[ -z "$OVER_CAP_FIXTURE_PATH" || ! "$OVER_CAP_LINE_COUNT" =~ ^[0-9]+$ ]]; then
    echo "[self-host-parity:production-c-size] invalid TestHarness over-cap fixture row" >&2
    exit 1
fi

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"

CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/production_c_size_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/production_c_size_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:production-c-size] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:production-c-size" "$CLEAN_BIN"; then
    exit 1
fi

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:production-c-size] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.production-c-size.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:production-c-size] schema header missing" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.production-c-size.v1' \
    | tail -n 1)"
PERGYRA_JSON="${PERGYRA_JSON%$'\r'}"
# Tolerance: replace `"max_lines":N` in both expected and actual with
# `"max_lines":<NORM>` before comparing. The exact maximum line count
# legitimately shifts every time a production .c file grows or shrinks
# (a couple of bytes per dev cycle), and chasing it through the fixture on
# every commit is dev-cost without invariant gain. The synthetic over-cap
# fixture below proves the cap semantics without reimplementing the clean
# count in shell.
EXPECTED_JSON_NORM_FILE="$PERGYRA_TOOL_BUILD_DIR/expected.clean.normalized.json"
PERGYRA_JSON_NORM="$(printf '%s' "$PERGYRA_JSON" \
    | sed -E 's/"max_lines":[0-9]+/"max_lines":<NORM>/')"
sed -E 's/"max_lines":[0-9]+/"max_lines":<NORM>/' \
    "$EXPECTED_JSON_FILE" > "$EXPECTED_JSON_NORM_FILE"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:production-c-size" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_NORM_FILE" \
    "$PERGYRA_JSON_NORM" \
    "run_output"

# Synthetic over-cap fixture from the TestHarness owner row.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-pcs.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/$(dirname "$OVER_CAP_FIXTURE_PATH")"
mkdir -p "$NEG_ROOT/.tmp"
{
    for k in $(seq 1 "$OVER_CAP_LINE_COUNT"); do
        echo "/* synthetic line $k */"
    done
} > "$NEG_ROOT/$OVER_CAP_FIXTURE_PATH"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:production-c-size] over-cap fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
NEG_JSON="$(printf '%s\n' "$NEG_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.production-c-size.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:production-c-size" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_OVER_CAP_JSON_FILE" \
    "$NEG_JSON" \
    "run_output"

assert_llvm_leg "self-host-parity:production-c-size" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:production-c-size] rung-2 parity ok (expected-json clean; over-cap fixture rc=1)"

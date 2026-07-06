#!/usr/bin/env bash
# Rung 2 parity for the runtime boundary checker (2026-06-16).
#
# Pergyra is the origin (src/self_hosted/tools/runtime_boundary_checker/main.pgy).
# Shell grep over the same required terms is the parity backend.

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
        echo "[self-host-parity:runtime-boundary] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:runtime-boundary] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/runtime_boundary_checker}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/runtime_boundary_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:runtime-boundary" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "runtime-boundary-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 4 ]]; then
    echo "[self-host-parity:runtime-boundary] TestHarness manifest expected 4 runtime-boundary rows, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"
MISSING_TERM_FIXTURE_PATH="${harness_paths[2]}"
MISSING_TERM_FIXTURE_TERM="${harness_paths[3]}"

if [[ ! -f "$PERGYRA_TOOL_SOURCE" ]]; then
    echo "[self-host-parity:runtime-boundary] missing Pergyra tool: $PERGYRA_TOOL_SOURCE" >&2
    exit 1
fi
if [[ ! -f "$EXPECTED_JSON_FILE" ]]; then
    echo "[self-host-parity:runtime-boundary] missing expected JSON: $EXPECTED_JSON_FILE" >&2
    exit 1
fi
if [[ -z "$MISSING_TERM_FIXTURE_PATH" || -z "$MISSING_TERM_FIXTURE_TERM" ]]; then
    echo "[self-host-parity:runtime-boundary] missing negative fixture row from TestHarness manifest" >&2
    exit 1
fi

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"

CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/runtime_boundary_checker_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/runtime_boundary_checker_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:runtime-boundary] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:runtime-boundary" "$CLEAN_BIN"; then
    exit 1
fi

TERMS_FILE="$PERGYRA_TOOL_BUILD_DIR/runtime_boundary_required_terms.txt"
if ! (cd "$ROOT_DIR" && "$CLEAN_BIN" --terms >"$TERMS_FILE"); then
    echo "[self-host-parity:runtime-boundary] required-term manifest failed" >&2
    exit 1
fi
tr -d '\r' < "$TERMS_FILE" > "$TERMS_FILE.norm"
mv "$TERMS_FILE.norm" "$TERMS_FILE"

missing=0
required_count=0
while IFS= read -r pair; do
    [[ -n "$pair" ]] || continue
    required_count=$((required_count + 1))
    rel="${pair%%|*}"
    term="${pair#*|}"
    if [[ ! -f "$ROOT_DIR/$rel" ]]; then
        echo "[self-host-parity:runtime-boundary] missing input: $rel" >&2
        exit 1
    fi
    if ! grep -Fq "$term" "$ROOT_DIR/$rel"; then
        echo "[self-host-parity:runtime-boundary] shell missing term in $rel: $term" >&2
        missing=$((missing + 1))
    fi
done <"$TERMS_FILE"
if [[ "$required_count" -eq 0 ]]; then
    echo "[self-host-parity:runtime-boundary] required-term manifest is empty" >&2
    exit 1
fi
if [[ "$missing" -ne 0 ]]; then
    exit 1
fi

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" 2>/dev/null | tr -d '\r')"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:runtime-boundary] clean repo exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.runtime-boundary.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:runtime-boundary] schema header missing" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.runtime-boundary.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:runtime-boundary" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$PERGYRA_JSON" \
    "run_output"

NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-runtime-boundary.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT

strip_pair_found=0
while IFS= read -r pair; do
    [[ -n "$pair" ]] || continue
    rel="${pair%%|*}"
    term="${pair#*|}"
    mkdir -p "$NEG_ROOT/$(dirname "$rel")"
    if [[ ! -f "$NEG_ROOT/$rel" ]]; then
        cp "$ROOT_DIR/$rel" "$NEG_ROOT/$rel"
    fi
    if [[ "$rel" == "$MISSING_TERM_FIXTURE_PATH" && "$term" == "$MISSING_TERM_FIXTURE_TERM" ]]; then
        strip_pair_found=1
    fi
done <"$TERMS_FILE"
if [[ "$strip_pair_found" -ne 1 ]]; then
    echo "[self-host-parity:runtime-boundary] term manifest missing TestHarness negative fixture row" >&2
    exit 1
fi
grep -Fv "$MISSING_TERM_FIXTURE_TERM" "$ROOT_DIR/$MISSING_TERM_FIXTURE_PATH" > "$NEG_ROOT/$MISSING_TERM_FIXTURE_PATH"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" 2>&1 | tr -d '\r')"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:runtime-boundary] missing-term fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"ok":false' <<<"$NEG_OUT"; then
    echo "[self-host-parity:runtime-boundary] missing-term fixture expected ok:false" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"missing":1' <<<"$NEG_OUT"; then
    echo "[self-host-parity:runtime-boundary] missing-term fixture expected missing:1" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq "$MISSING_TERM_FIXTURE_TERM" <<<"$NEG_OUT"; then
    echo "[self-host-parity:runtime-boundary] missing-term fixture expected stripped term in findings" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:runtime-boundary" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:runtime-boundary] rung-2 parity ok (required=${required_count} missing-fixture rc=1)"

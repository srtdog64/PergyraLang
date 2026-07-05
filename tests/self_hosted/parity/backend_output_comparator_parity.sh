#!/usr/bin/env bash
# Rung 2 parity for the backend output comparator (2026-05-27).
#
# Pergyra is the origin
# (src/self_hosted/tools/backend_output_comparator/main.pgy).
# Shell text equivalence is the parity backend. Asserts:
#   - clean fixture (expected == actual): rc=0, JSON byte-equal vs
#     expected/clean.json
#   - synthetic mismatch fixture: rc=1, mismatch_lines >= 1, ok:false
#   - synthetic missing-input fixture: rc=1, input_error finding
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
        echo "[self-host-parity:backend-output-comparator] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:backend-output-comparator] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/backend_output_comparator}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/backend_output_comparator_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:backend-output-comparator" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "backend-output-comparator-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 4 ]]; then
    echo "[self-host-parity:backend-output-comparator] TestHarness manifest expected 4 comparator paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"
FIXTURE_EXPECTED_REL="${harness_paths[2]}"
FIXTURE_ACTUAL_REL="${harness_paths[3]}"
FIXTURE_EXPECTED="$ROOT_DIR/$FIXTURE_EXPECTED_REL"
FIXTURE_ACTUAL="$ROOT_DIR/$FIXTURE_ACTUAL_REL"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$FIXTURE_EXPECTED" "$FIXTURE_ACTUAL"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:backend-output-comparator] missing input: $path" >&2
        exit 1
    fi
done

mkdir -p "$PERGYRA_TOOL_BUILD_DIR/../../lib"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR/../../compiler"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"
cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$PERGYRA_TOOL_BUILD_DIR/../../lib/"
cp "$ROOT_DIR/src/self_hosted/compiler/artifact_zone_owner.pgy" \
    "$PERGYRA_TOOL_BUILD_DIR/../../compiler/artifact_zone_owner.pgy"
cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_owner.pgy" \
    "$PERGYRA_TOOL_BUILD_DIR/../../compiler/test_harness_owner.pgy"
cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_tool_paths_owner.pgy" \
    "$PERGYRA_TOOL_BUILD_DIR/../../compiler/test_harness_tool_paths_owner.pgy"
cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" \
    "$PERGYRA_TOOL_BUILD_DIR/../../compiler/test_harness_air_graph_paths_owner.pgy"
cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_driver_paths_owner.pgy" \
    "$PERGYRA_TOOL_BUILD_DIR/../../compiler/test_harness_driver_paths_owner.pgy"
cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy" \
    "$PERGYRA_TOOL_BUILD_DIR/../../compiler/test_harness_codegen_paths_owner.pgy"
cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_parser_paths_owner.pgy" \
    "$PERGYRA_TOOL_BUILD_DIR/../../compiler/test_harness_parser_paths_owner.pgy"
cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy" \
    "$PERGYRA_TOOL_BUILD_DIR/../../compiler/test_harness_semantic_paths_owner.pgy"
cp "$ROOT_DIR/src/self_hosted/compiler/test_harness_mir_json_paths_owner.pgy" \
    "$PERGYRA_TOOL_BUILD_DIR/../../compiler/test_harness_mir_json_paths_owner.pgy"
cp "$ROOT_DIR/src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "$PERGYRA_TOOL_BUILD_DIR/../../compiler/completeness_ledger_owner.pgy"
cp "$ROOT_DIR/src/self_hosted/compiler/subprocess_runner_owner.pgy" \
    "$PERGYRA_TOOL_BUILD_DIR/../../compiler/subprocess_runner_owner.pgy"
PERGYRA_TOOL_INPUT="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")"

# Phase 1 - clean (match) fixture.
set +e
CLEAN_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_INPUT" --run 2>/dev/null)"
CLEAN_RC=$?
set -e
if [[ "$CLEAN_RC" -ne 0 ]]; then
    echo "[self-host-parity:backend-output-comparator] clean fixture expected rc=0, got rc=$CLEAN_RC" >&2
    printf '%s\n' "$CLEAN_OUT" >&2
    exit 1
fi
CLEAN_JSON="$(printf '%s\n' "$CLEAN_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.backend-output-comparator.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$CLEAN_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:backend-output-comparator] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $CLEAN_JSON" >&2
    exit 1
fi

# Phase 1b - argv-driven pair/projection facts. This is the hard-self-host
# path: the comparator receives artifact paths and projection rows through
# Args() instead of relying only on fixed fixture owner paths.
ARG_BIN="$PERGYRA_TOOL_BUILD_DIR/main_args.exe"
ARG_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/main_args.compile.log"
ARG_EXPECTED_PATH="$FIXTURE_EXPECTED_REL"
ARG_ACTUAL_PATH="$FIXTURE_ACTUAL_REL"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_INPUT" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$ARG_BIN")" >"$ARG_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:backend-output-comparator] argv-mode C compile failed" >&2
    cat "$ARG_COMPILE_LOG" >&2
    exit 1
fi
ARG_OUT="$(cd "$ROOT_DIR" && "$ARG_BIN" "$ARG_EXPECTED_PATH" "$ARG_ACTUAL_PATH" 0 2 2>/dev/null | tr -d '\r')"
ARG_JSON="$(printf '%s\n' "$ARG_OUT" \
    | grep -F 'pgy.selfhost.backend-output-comparator.v1' \
    | tail -n 1)"
if ! grep -Fq '"expected_projection":"c_oracle"' <<<"$ARG_JSON" \
    || ! grep -Fq '"actual_projection":"self_hosted"' <<<"$ARG_JSON" \
    || ! grep -Fq '"ok":true' <<<"$ARG_JSON"; then
    echo "[self-host-parity:backend-output-comparator] argv-mode projection parity FAIL" >&2
    printf '%s\n' "$ARG_OUT" >&2
    exit 1
fi

LLVM_BACKEND_LABEL="llvm"
LLVM_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/main_llvm.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_INPUT" --backend=llvm \
    -o "$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_BUILD_DIR/main_llvm.exe")" \
    >"$LLVM_COMPILE_LOG" 2>&1); then
    if pgy_selfhost_log_reports_no_llvm "$LLVM_COMPILE_LOG"; then
        LLVM_BACKEND_LABEL="llvm skipped"
        echo "[self-host-parity:backend-output-comparator] LLVM backend unavailable; checking default/C path only"
    else
        echo "[self-host-parity:backend-output-comparator] LLVM backend compile failed" >&2
        cat "$LLVM_COMPILE_LOG" >&2
        exit 1
    fi
else
    LLVM_CLEAN_OUT="$(cd "$ROOT_DIR" && "$PERGYRA_TOOL_BUILD_DIR/main_llvm.exe" 2>/dev/null)"
    LLVM_CLEAN_JSON="$(printf '%s\n' "$LLVM_CLEAN_OUT" \
        | tr -d '\r' \
        | grep -F 'pgy.selfhost.backend-output-comparator.v1' \
        | tail -n 1)"
    if [[ "$LLVM_CLEAN_JSON" != "$EXPECTED_JSON" ]]; then
        echo "[self-host-parity:backend-output-comparator] LLVM backend JSON parity FAIL" >&2
        echo "expected: $EXPECTED_JSON" >&2
        echo "actual:   $LLVM_CLEAN_JSON" >&2
        exit 1
    fi
fi

# Shell drift detector for clean -- diff -q must agree. MSYS2 /
# Git-for-Windows checkouts can land text fixtures with CRLF endings
# depending on autocrlf and .gitattributes ordering (we saw
# --strip-trailing-cr still disagree in one Windows run, which means
# at least one byte is escaping that flag's normalization). The
# Pergyra tool already compares line-by-line after its own
# normalization, so we mirror that more aggressively here: strip all
# CRs (not just trailing-CR-per-line) from both files via tr before
# the shell diff. That matches what the Pergyra tool's
# line-iteration accepts.
# Shell drift detector for clean -- the previous run on ci-windows
# returned rc=127 (command not found) on `diff -q`. MSYS2's minimal
# Pergyra build environment doesn't ship GNU diff in the search PATH,
# so we can't rely on that binary. Switch to a pure-bash equivalence
# check via `$(<file)`: read both fixtures into shell strings after
# CR normalization (still required because the on-disk bytes can
# differ even when the rendered text is the same), then compare with
# `[[ == ]]`. No external binary, no rc=127 / rc=2 misreporting.
EXPECTED_CONTENT_RAW="$(<"$FIXTURE_EXPECTED")"
ACTUAL_CONTENT_RAW="$(<"$FIXTURE_ACTUAL")"
EXPECTED_CONTENT_NORM="${EXPECTED_CONTENT_RAW//$'\r'/}"
ACTUAL_CONTENT_NORM="${ACTUAL_CONTENT_RAW//$'\r'/}"
if [[ "$EXPECTED_CONTENT_NORM" != "$ACTUAL_CONTENT_NORM" ]]; then
    echo "[self-host-parity:backend-output-comparator] shell drift detector disagrees with Pergyra (clean)" >&2
    # Surface the exact bytes that diverged so a future run has a
    # concrete trace instead of just the error line.
    {
        echo "--- expected (size=$(wc -c <"$FIXTURE_EXPECTED")) ---"
        od -c "$FIXTURE_EXPECTED" | head -5
        echo "--- actual   (size=$(wc -c <"$FIXTURE_ACTUAL")) ---"
        od -c "$FIXTURE_ACTUAL" | head -5
    } >&2
    exit 1
fi

# Phase 2 - synthetic mismatch fixture (different middle line in actual.txt).
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-cmp.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/$(dirname "$FIXTURE_EXPECTED_REL")" \
    "$NEG_ROOT/$(dirname "$FIXTURE_ACTUAL_REL")"
mkdir -p "$NEG_ROOT/.tmp"
cp "$FIXTURE_EXPECTED" "$NEG_ROOT/$FIXTURE_EXPECTED_REL"
sed 's/gamma/GAMMA-DRIFT/' "$FIXTURE_ACTUAL" \
    > "$NEG_ROOT/$FIXTURE_ACTUAL_REL"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL_INPUT" --run 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:backend-output-comparator] mismatch fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"ok":false' <<<"$NEG_OUT"; then
    echo "[self-host-parity:backend-output-comparator] mismatch fixture expected ok:false" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Eq '"mismatch_lines":[1-9]' <<<"$NEG_OUT"; then
    echo "[self-host-parity:backend-output-comparator] mismatch fixture expected mismatch_lines >= 1" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"mismatch"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:backend-output-comparator] mismatch fixture expected a mismatch finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

# Phase 3 - synthetic missing-input fixture (delete actual.txt).
MISS_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-cmp-miss.XXXXXX")"
trap "rm -rf '$NEG_ROOT' '$MISS_ROOT'" EXIT
mkdir -p "$MISS_ROOT/$(dirname "$FIXTURE_EXPECTED_REL")"
mkdir -p "$MISS_ROOT/.tmp"
cp "$FIXTURE_EXPECTED" "$MISS_ROOT/$FIXTURE_EXPECTED_REL"

set +e
MISS_OUT="$(cd "$MISS_ROOT" && "$PGY" "$PERGYRA_TOOL_INPUT" --run 2>&1)"
MISS_RC=$?
set -e
if [[ "$MISS_RC" -ne 1 ]]; then
    echo "[self-host-parity:backend-output-comparator] missing-input fixture expected rc=1, got rc=$MISS_RC" >&2
    printf '%s\n' "$MISS_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"input_error"' <<<"$MISS_OUT"; then
    echo "[self-host-parity:backend-output-comparator] missing-input fixture expected input_error finding" >&2
    printf '%s\n' "$MISS_OUT" >&2
    exit 1
fi

echo "[self-host-parity:backend-output-comparator] rung-2 parity ok (clean rc=0; mismatch rc=1; missing-input rc=1; $LLVM_BACKEND_LABEL)"

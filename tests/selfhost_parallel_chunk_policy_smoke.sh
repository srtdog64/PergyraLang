#!/usr/bin/env bash
# Self-host owner gate for the parallel auto-chunk policy (docs/186 P-B3).
#
# The policy SoT is src/self_hosted/parallel/chunk_policy_owner.pgy. This gate
# proves, in order:
#   1. the runnable manifest reproduces the pinned golden table (C leg,
#      via the self-hosted backend-output comparator), i.e. the Pergyra
#      policy arithmetic and its in-language cover invariants hold;
#   2. the LLVM-compiled manifest is artifact-equal to the C-compiled one
#      (assert_llvm_leg), so the policy is backend-invariant;
#   3. every projection site (C runtime, exports, both backend emitters)
#      still carries the exact required terms the OWNER declares -- the pin
#      list is parsed out of the compiled manifest's require| rows, so it has
#      no second home in shell;
#   4. self-test: the pin check must be able to report absence (the owner's
#      known-missing sentinel term must NOT be found).
#
# Registered owner (docs/188 R2 repair): OWNERS.md rows, the component
# contract smoke section, and the parallel_chunk_policy artifact kind are
# in place, so the golden and LLVM-leg comparisons ride the self-hosted
# backend-output comparator (the same tool the other parity gates trust).
#
# Usage: PGY_BIN=bin/pgy.exe bash tests/selfhost_parallel_chunk_policy_smoke.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="selfhost-parallel-chunk-policy"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[$LABEL] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[$LABEL] missing compiler binary: $PGY" >&2
    exit 1
fi

MANIFEST_SOURCE="$ROOT_DIR/src/self_hosted/parallel/chunk_policy_manifest.pgy"
OWNER_SOURCE="$ROOT_DIR/src/self_hosted/parallel/chunk_policy_owner.pgy"
EXPECTED_FILE="$ROOT_DIR/src/self_hosted/parallel/expected_chunk_policy_manifest.txt"
for path in "$MANIFEST_SOURCE" "$OWNER_SOURCE" "$EXPECTED_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[$LABEL] missing input: $path" >&2
        exit 1
    fi
done

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/parallel_chunk_policy}"
mkdir -p "$BUILD_DIR"

TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$MANIFEST_SOURCE")"
C_BIN="$BUILD_DIR/chunk_policy_c.exe"
C_COMPILE_LOG="$BUILD_DIR/chunk_policy_c.compile.log"
C_OUT="$BUILD_DIR/chunk_policy_c.out"
C_ERR="$BUILD_DIR/chunk_policy_c.err"

if ! (cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$C_BIN")" >"$C_COMPILE_LOG" 2>&1); then
    echo "[$LABEL] C compile failed" >&2
    cat "$C_COMPILE_LOG" >&2
    exit 1
fi

set +e
(cd "$ROOT_DIR" && "$C_BIN" 2>"$C_ERR" | pgy_selfhost_normalize_text_artifact >"$C_OUT")
C_RC=$?
set -e
if [[ "$C_RC" -ne 0 ]]; then
    echo "[$LABEL] C-compiled manifest failed" >&2
    cat "$C_OUT" "$C_ERR" >&2
    exit 1
fi

# 1) golden diff through the self-hosted backend-output comparator
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "$LABEL" "$BUILD_DIR" "$EXPECTED_FILE" "$C_OUT" "parallel_chunk_policy"

# 2) backend invariance of the policy tool itself (C leg == LLVM leg)
assert_llvm_leg "$LABEL" "$TOOL_ARG" "$BUILD_DIR"

# 3) required-term pins, parsed from the OWNER's own require| rows
term_present() { # $1 = repo-relative path, $2 = exact term
    grep -qF -- "$2" "$ROOT_DIR/$1"
}

require_rows=0
selftest_path=""
# `|| [[ -n ... ]]`: the normalized artifact has no trailing newline, and a
# bare `read` loop would silently drop the final (unterminated) require row.
while IFS='|' read -r kind pin_path pin_term || [[ -n "${kind:-}" ]]; do
    [[ "$kind" == "require" ]] || continue
    if [[ -z "$pin_path" || -z "$pin_term" ]]; then
        echo "[$LABEL] malformed require row: $kind|$pin_path|$pin_term" >&2
        exit 1
    fi
    if [[ ! -f "$ROOT_DIR/$pin_path" ]]; then
        echo "[$LABEL] pinned projection file missing: $pin_path" >&2
        exit 1
    fi
    if ! term_present "$pin_path" "$pin_term"; then
        echo "[$LABEL] projection drift: '$pin_term' not found in $pin_path" >&2
        echo "[$LABEL] the chunk policy owner and this projection must move in lockstep" >&2
        exit 1
    fi
    if [[ -z "$selftest_path" ]]; then
        selftest_path="$pin_path"
    fi
    require_rows=$((require_rows + 1))
done <"$C_OUT"

if [[ "$require_rows" -lt 8 ]]; then
    echo "[$LABEL] expected at least 8 require rows, saw $require_rows" >&2
    exit 1
fi

# 4) self-test: the pin check must be able to report absence
SELFTEST_TERM="definitely_missing_parallel_chunk_policy_term"
if ! grep -qF -- "$SELFTEST_TERM" "$OWNER_SOURCE"; then
    echo "[$LABEL] self-test sentinel disappeared from the owner" >&2
    exit 1
fi
if [[ -z "$selftest_path" ]]; then
    echo "[$LABEL] self-test has no owner-provided projection path" >&2
    exit 1
fi
if term_present "$selftest_path" "$SELFTEST_TERM"; then
    echo "[$LABEL] self-test broken: known-missing term reported present" >&2
    exit 1
fi

echo "[$LABEL] ok (golden diff + llvm-leg parity + $require_rows projection pins + self-test)"

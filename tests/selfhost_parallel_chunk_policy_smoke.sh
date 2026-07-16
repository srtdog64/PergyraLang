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
# Standalone runner (WO-RT-3 precedent): registration rows in
# src/self_hosted/OWNERS.md, tests/self_hosted_component_contract_smoke.sh
# and the artifact-kind row in
# src/self_hosted/compiler/artifact_zone_owner.pgy (which would let this
# gate ride the self-hosted backend-output comparator) are deferred while
# those files are owned by a concurrent workstream; until then the golden
# and leg comparisons are plain byte diffs. The TODO board WO-RT-4 records
# that residue.
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

# 1) golden diff (byte-exact after newline normalization)
EXPECTED_NORM="$BUILD_DIR/expected.norm.txt"
pgy_selfhost_normalize_text_artifact <"$EXPECTED_FILE" >"$EXPECTED_NORM"
if ! diff -u "$EXPECTED_NORM" "$C_OUT" >"$BUILD_DIR/golden.diff" 2>&1; then
    echo "[$LABEL] manifest output diverges from the pinned golden table" >&2
    cat "$BUILD_DIR/golden.diff" >&2
    exit 1
fi

# 2) backend invariance of the policy tool itself (LLVM leg, byte-equal)
LLVM_BIN="$BUILD_DIR/chunk_policy_llvm.exe"
LLVM_COMPILE_LOG="$BUILD_DIR/chunk_policy_llvm.compile.log"
LLVM_OUT="$BUILD_DIR/chunk_policy_llvm.out"
LLVM_ERR="$BUILD_DIR/chunk_policy_llvm.err"
if ! (cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" --backend=llvm \
    -o "$(pgy_path_for_compiler "$PGY" "$LLVM_BIN")" \
    >"$LLVM_COMPILE_LOG" 2>&1); then
    if pgy_selfhost_log_reports_no_llvm "$LLVM_COMPILE_LOG"; then
        echo "[$LABEL] llvm-leg skipped (compiler built without LLVM backend support)"
    else
        echo "[$LABEL] LLVM leg compile failed" >&2
        cat "$LLVM_COMPILE_LOG" >&2
        exit 1
    fi
else
    set +e
    (cd "$ROOT_DIR" && "$LLVM_BIN" 2>"$LLVM_ERR" \
        | pgy_selfhost_normalize_text_artifact >"$LLVM_OUT")
    LLVM_RC=$?
    set -e
    if [[ "$LLVM_RC" -ne 0 ]]; then
        echo "[$LABEL] LLVM-compiled manifest failed" >&2
        cat "$LLVM_OUT" "$LLVM_ERR" >&2
        exit 1
    fi
    if ! cmp -s "$C_OUT" "$LLVM_OUT"; then
        echo "[$LABEL] LLVM-compiled manifest diverges from the C-compiled one" >&2
        diff -u "$C_OUT" "$LLVM_OUT" >&2 || true
        exit 1
    fi
fi

# 3) required-term pins, parsed from the OWNER's own require| rows
term_present() { # $1 = repo-relative path, $2 = exact term
    grep -qF -- "$2" "$ROOT_DIR/$1"
}

require_rows=0
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
if term_present "src/runtime/pgy_parallel.h" "$SELFTEST_TERM"; then
    echo "[$LABEL] self-test broken: known-missing term reported present" >&2
    exit 1
fi

echo "[$LABEL] ok (golden diff + llvm-leg parity + $require_rows projection pins + self-test)"

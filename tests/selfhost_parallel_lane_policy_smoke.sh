#!/usr/bin/env bash
# Self-host owner gate for the SEA execution-lane policy (docs/146, docs/186).
#
# The policy SoT is src/self_hosted/parallel/lane_policy_owner.pgy. This gate
# proves, in order:
#   1. the runnable manifest reproduces the pinned golden decision table (C leg,
#      via the self-hosted backend-output comparator) -- so the classification,
#      its in-language invariants (contradiction rejects, rule order is the
#      contract, M:N needs the full conjunction), and the declared codegen
#      reachability all hold;
#   2. the LLVM-compiled manifest is artifact-equal to the C-compiled one
#      (assert_llvm_leg), so the policy is backend-invariant.
#
# The reachable| rows are deliberately part of the golden. Since WO-PAR-NOVEL
# step 2, both spawn emitters consume the AIR-classified lane through the
# verified spawn-lane plan, so every classifier verdict except Inline (a spawn
# is always concurrent) and Reject (the plan producer fail-closes the compile)
# can reach a backend. Changing that reachability again MUST change this
# golden -- it can neither widen nor narrow silently.
#
# Usage: PGY_BIN=bin/pgy.exe bash tests/selfhost_parallel_lane_policy_smoke.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="selfhost-parallel-lane-policy"
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

MANIFEST_SOURCE="$ROOT_DIR/src/self_hosted/parallel/lane_policy_manifest.pgy"
EXPECTED_FILE="$ROOT_DIR/src/self_hosted/parallel/expected_lane_policy_manifest.txt"
for path in "$MANIFEST_SOURCE" "$EXPECTED_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[$LABEL] missing input: $path" >&2
        exit 1
    fi
done

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/parallel_lane_policy}"
mkdir -p "$BUILD_DIR"

TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$MANIFEST_SOURCE")"
C_BIN="$BUILD_DIR/lane_policy_c.exe"
C_COMPILE_LOG="$BUILD_DIR/lane_policy_c.compile.log"
C_OUT="$BUILD_DIR/lane_policy_c.out"
C_ERR="$BUILD_DIR/lane_policy_c.err"

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
    "$LABEL" "$BUILD_DIR" "$EXPECTED_FILE" "$C_OUT" "parallel_lane_policy"

# 2) backend invariance of the policy tool itself (C leg == LLVM leg)
assert_llvm_leg "$LABEL" "$TOOL_ARG" "$BUILD_DIR"

echo "[$LABEL] ok (golden decision table + llvm-leg parity)"

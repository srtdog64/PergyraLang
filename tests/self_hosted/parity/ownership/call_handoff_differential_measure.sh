#!/usr/bin/env bash
# G2 differential measurement of the call hand-off rows (slice S2 of the
# hand-off rule migration, docs/audits/unified_handoff_rule_study_2026-11-16.md
# section 3.4). It is a measurement, not a pass gate, until slice S5: it
# compiles the inputs with both front ends, compares the `call_handoff` rows
# of the programs both admit, and writes counts and per-program differences.
# It exits non-zero only when it cannot run (a missing binary, a malformed
# MIR document). The counts are written, never compared with a pin.
#
# Usage: call_handoff_differential_measure.sh OUT_DIR [INPUT_DIR...]
# Default inputs: the 868 lifetime fixtures and
# tests/cases/function_param_flow_summary. PGY_BIN and PGY_SELF_DRIVER_BIN
# name the binaries; PGY_G2_JOBS the parallel compiles (default 4).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="call-handoff-g2"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
JOBS="${PGY_G2_JOBS:-4}"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
[[ $# -ge 1 ]] || fail "usage: $0 OUT_DIR [INPUT_DIR...]"
OUT_DIR="$1"
shift
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
INPUTS=("$@")
if [[ ${#INPUTS[@]} -eq 0 ]]; then
    INPUTS=(tests/self_hosted/parity/fixture/collection_field_lifetime
            tests/self_hosted/parity/fixture/aggregate_storage_transfer
            tests/cases/function_param_flow_summary)
fi
mkdir -p "$OUT_DIR" "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/call-handoff-g2.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"

# One input: the native oracle MIR and the self-host verified MIR, each with
# its exit status. The self-host driver writes only inside the tree.
compile_one() {
    local source="$1" key
    key="$(printf '%s' "$source" | tr '/' '_')"
    (cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$source") \
        >"$WORK_DIR/$key.native.json" 2>"$WORK_DIR/$key.native.err" </dev/null
    echo $? >"$WORK_DIR/$key.native.rc"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source" \
        -o "$WORK_REL/$key.self.json") \
        >"$WORK_DIR/$key.self.out" 2>"$WORK_DIR/$key.self.err" </dev/null
    echo $? >"$WORK_DIR/$key.self.rc"
}
export -f compile_one
export ROOT_DIR PGY DRIVER WORK_DIR WORK_REL

: >"$WORK_DIR/inputs.txt"
for input in "${INPUTS[@]}"; do
    [[ -d "$ROOT_DIR/$input" ]] || fail "missing input directory: $input"
    (cd "$ROOT_DIR" && find "$input" -maxdepth 1 -name '*.pgy' | sort) \
        >>"$WORK_DIR/inputs.txt"
done
[[ -s "$WORK_DIR/inputs.txt" ]] || fail "no inputs"
xargs -P "$JOBS" -I{} bash -c 'compile_one "$@"' _ {} <"$WORK_DIR/inputs.txt" || true

python3 "$ROOT_DIR/tests/self_hosted/parity/ownership/call_handoff_differential_measure.py" \
    "$ROOT_DIR" "$WORK_DIR" "$WORK_DIR/inputs.txt" "$OUT_DIR" ||
    fail "the comparison could not read the MIR documents"
echo "[$LABEL] counts in $OUT_DIR/summary.json, differences in $OUT_DIR/differences.tsv"

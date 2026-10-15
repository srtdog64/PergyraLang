#!/usr/bin/env bash
# The installed driver's MIR to AST round trip must rebuild a program the
# semantic re-check reads as the source: the C it emits takes its cleanup
# from that re-check. Each fixture goes through the source to MIR and MIR to
# C routes and must print what the native build prints.
# - Code after an `if` one of whose arms always returns (a guard chain, or the
#   last `else { return }` of an else-if chain) is emitted once. A copy inside
#   the other arm moved a field out twice (member_use_after_move).
# - The end of a Void body is not a `return` statement. An aggregate field
#   release needs a straight-line body (aggregate_release_incomplete).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-mir-round-trip-semantic"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
FIXTURES="tests/concept_semantics/hashmap"
# fixture|native output|C text that must appear exactly once (space separated)
CASES=(
    "member_move_after_guard_chain_valid.pgy|4 1 1 2 2 3 0|pgy_as_push(&names pgy_as_push(&labels"
    "inout_field_drop_restore_valid.pgy|1 0|"
)

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/mir-round-trip-semantic.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"
cd "$ROOT_DIR"

for case_row in "${CASES[@]}"; do
    IFS='|' read -r fixture expected once <<<"$case_row"
    name="${fixture%.pgy}"
    source="$FIXTURES/$fixture"
    "$PGY" "$source" --native-pipeline --backend=c -o "$WORK_REL/$name.native.exe" \
        >"$WORK_DIR/$name.native.compile" 2>&1 || fail "native rejected $fixture"
    "$WORK_DIR/$name.native.exe" | tr -d '\r' | tr '\n' ' ' | sed 's/ $//' \
        >"$WORK_DIR/$name.native.out" || fail "$fixture failed at run time natively"
    [[ "$(cat "$WORK_DIR/$name.native.out")" == "$expected" ]] ||
        fail "native $fixture printed '$(cat "$WORK_DIR/$name.native.out")', expected '$expected'"
    "$DRIVER" --emit-mir-json-verified "$source" -o "$WORK_REL/$name.json" \
        >"$WORK_DIR/$name.mir.out" 2>&1 || {
        cat "$WORK_DIR/$name.mir.out" >&2
        fail "installed driver rejected the source of $fixture"
    }
    "$DRIVER" --mir-json "$WORK_REL/$name.json" -o "$WORK_REL/$name.c" \
        >"$WORK_DIR/$name.round-trip.out" 2>&1 || {
        cat "$WORK_DIR/$name.round-trip.out" >&2
        fail "installed driver round trip rejected the MIR of $fixture"
    }
    [[ -s "$WORK_DIR/$name.c" ]] || fail "round trip emitted no C for $fixture"
    for text in $once; do
        count="$(grep -cF "$text" "$WORK_DIR/$name.c" || true)"
        [[ "$count" == 1 ]] || fail "$fixture: round trip emitted '$text' $count times"
    done
    "$CC" -std=c11 -w -I src -I src/runtime -pthread -x c "$WORK_DIR/$name.c" \
        -o "$WORK_DIR/$name.round-trip.exe" -lm >"$WORK_DIR/$name.cc.out" 2>&1 || {
        sed -n '1,40p' "$WORK_DIR/$name.cc.out" >&2
        fail "round-trip C of $fixture did not compile"
    }
    "$WORK_DIR/$name.round-trip.exe" | tr -d '\r' | tr '\n' ' ' | sed 's/ $//' \
        >"$WORK_DIR/$name.round-trip.run" || fail "round-trip $fixture failed at run time"
    cmp -s "$WORK_DIR/$name.native.out" "$WORK_DIR/$name.round-trip.run" ||
        fail "round trip of $fixture printed '$(cat "$WORK_DIR/$name.round-trip.run")', native printed '$expected'"
done
echo "[$LABEL] ${#CASES[@]} fixtures admitted by the round trip, runtime-equal to native: PASS ($WORK_REL)"

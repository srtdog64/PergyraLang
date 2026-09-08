#!/usr/bin/env bash
# Descriptor completeness and element completeness are distinct C ABI facts.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/codegen_bootstrap_compile_leg.sh"
pgy_prepend_windows_runtime_paths
bash "$ROOT_DIR/tests/self_hosted/parity/codegen_bootstrap_status_smoke.sh"
PGY="${PGY_BIN:?native comparator is required}"
CODEGEN="${PGY_CODEGEN_BIN:?bootstrap codegen is required}"
CC="${PGY_SELFHOST_CC:-gcc}"
WORK_BASE="$ROOT_DIR/.tmp/self_hosted/codegen_nominal_array_declaration"
mkdir -p "$WORK_BASE"
B="$(mktemp -d "$WORK_BASE/run.XXXXXX")"
fail() { echo "[codegen-nominal-array-declaration] $* ($B)" >&2; exit 1; }
cd "$ROOT_DIR"
SOURCE=tests/self_hosted/parity/fixture/nominal_array_declaration.pgy
"$PGY" --native-pipeline --emit-c "$SOURCE" -o "$B/native.c" \
    >"$B/native.out" 2>"$B/native.err" || fail "native rejected the valid control"
if ! "$CODEGEN" --source "$SOURCE" >"$B/codegen.c" 2>"$B/codegen.err"; then
    sed -n '1,40p' "$B/codegen.c" "$B/codegen.err" >&2
    fail "codegen rejected array fields"
fi
printf '7\n24\n0\n' >"$B/expected.txt"
for producer in native codegen; do
    compile_c_artifact_with_bounded_log "$producer" "$B/$producer.c" \
        "$B/$producer.exe" || { sed -n '1,40p' "$B/${producer}_cc.log" >&2; fail "$producer C did not compile"; }
    "$B/$producer.exe" >"$B/$producer.actual" || fail "$producer execution failed"
    tr -d '\r' <"$B/$producer.actual" >"$B/$producer.normalized"
    cmp "$B/expected.txt" "$B/$producer.normalized" || fail "$producer value drifted"
done
SOURCE=tests/self_hosted/parity/fixture/mir_collection_receiver_root.pgy
"$PGY" --native-pipeline --emit-c "$SOURCE" -o "$B/root-native.c" \
    >"$B/root-native.out" 2>"$B/root-native.err" || fail "native rejected the MIR root control"
"$CODEGEN" --source "$SOURCE" >"$B/root-codegen.c" 2>"$B/root-codegen.err" ||
    fail "codegen rejected the MIR root control"
printf '1\ndeclaration:200:0\n1\nfalse\nfalse\nfalse\n' >"$B/root-expected.txt"
for producer in native codegen; do
    compile_c_artifact_with_bounded_log "root-$producer" "$B/root-$producer.c" \
        "$B/root-$producer.exe" || fail "$producer MIR root control did not compile"
    "$B/root-$producer.exe" | tr -d '\r' >"$B/root-$producer.actual"
    cmp "$B/root-expected.txt" "$B/root-$producer.actual" || fail "$producer MIR root identity drifted"
done
for fixture in cyclic_value_declarations cyclic_result_value_declaration cyclic_nested_option_result_value_declaration nominal_array_value_cycle; do
    source_path="src/self_hosted/codegen/reject_fixture/$fixture.pgy"
    if [[ "$fixture" == nominal_array_value_cycle ]]; then
        source_path="tests/self_hosted/parity/fixture/$fixture.pgy"
    fi
    if "$CODEGEN" --source "$source_path" \
        >"$B/$fixture.out" 2>"$B/$fixture.err"; then
        fail "codegen accepted $fixture"
    fi
    grep -Fq 'cyclic by-value type declaration dependency' "$B/$fixture.out" "$B/$fixture.err" ||
        fail "$fixture lost its cycle diagnosis"
    ! grep -Eq '^#include|^int main' "$B/$fixture.out" ||
        fail "$fixture published C before refusal"
    if "$PGY" --native-pipeline --emit-c "$source_path" -o "$B/$fixture.c" \
        >"$B/$fixture.native.out" 2>"$B/$fixture.native.err"; then
        fail "native accepted $fixture"
    fi
    [[ ! -e "$B/$fixture.c" ]] || fail "native published invalid C"
done
echo "[codegen-nominal-array-declaration] 4 executions and 8 pre-emission refusals: PASS ($B)"

#!/usr/bin/env bash
# A repaired outer digest cannot admit stale ownership coverage of typed flow.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL=self-host-owned-array-string-sealed-flow
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/owned-array-sealed-flow.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
echo "[$LABEL] evidence: $REL"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    tests/self_hosted/fixtures/direct_mir_owned_array_string_terminal_flow.pgy \
    -o "$REL/program.mir.json") >"$WORK/producer.log" 2>&1 || {
    cat "$WORK/producer.log" >&2; fail "MIR production failed";
}
(cd "$ROOT_DIR" && "$PGY" \
    tests/self_hosted/parity/fixture/owned_array_string_move_sealed_flow_probe.pgy \
    --native-pipeline --emit-c -o "$REL/probe.native.c") >"$WORK/native.emit.log" 2>&1 || {
    cat "$WORK/native.emit.log" >&2; fail "native-oracle probe emission failed";
}
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" tests/self_hosted/parity/fixture/owned_array_string_move_sealed_flow_probe.pgy \
    --emit-c -o "$REL/probe.c") >"$WORK/emit.log" 2>&1 || {
    cat "$WORK/emit.log" >&2; fail "Pergyra probe emission failed";
}
if grep -Fq '[pipeline timing]' "$WORK/emit.log"; then
    fail "probe used the native pipeline"
fi
for probe in probe probe.native; do
    command=("$CC" -x c -std=c11 -O0 -fwrapv -fno-strict-aliasing "$WORK/$probe.c")
    if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK/$probe.c"; then
        command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    command+=(-lm -o "$WORK/$probe.exe")
    "${command[@]}" >"$WORK/$probe.compile.log" 2>&1 || {
        cat "$WORK/$probe.compile.log" >&2; fail "$probe C compilation failed";
    }
    (cd "$ROOT_DIR" && "$WORK/$probe.exe" "$REL/program.mir.json") \
        >"$WORK/$probe.run.log" 2>&1 || {
        cat "$WORK/$probe.run.log" >&2; fail "$probe failed";
    }
    [[ "$(tr -d '\r' <"$WORK/$probe.run.log")" == \
        'owned move sealed flow: six stale-input mutations rejected' ]] ||
        fail "$probe verdict drifted"
done
echo "[$LABEL] public/native issued plan + repaired-digest mutations + exact restoration: PASS"

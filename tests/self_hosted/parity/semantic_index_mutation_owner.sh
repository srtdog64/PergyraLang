#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
LABEL=self-host-semantic-index-mutation
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
command -v "$CC" >/dev/null || fail 'missing C compiler'
pgy_selfhost_select_emitted_c_compile_profile || fail 'invalid emitted-C profile'
mkdir -p "$ROOT_DIR/.tmp/self_hosted/semantic_index_mutation"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/semantic_index_mutation/run.XXXXXX")"
PROBE="$ROOT_DIR/tests/self_hosted/parity/fixture/semantic_index_mutation_owner_probe.pgy"
if ! (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$(pgy_path_for_compiler "$PGY" "$PROBE")" \
        --emit-c -o "$(pgy_path_for_compiler "$PGY" "$WORK/probe.c")" \
        >"$WORK/emit.out" 2>"$WORK/emit.err"); then
    cat "$WORK/emit.out" "$WORK/emit.err" >&2
    fail 'probe self-hosted C emission failed'
fi
cat "$WORK/emit.err" >&2
if grep -Fq '[pipeline timing]' "$WORK/emit.out" "$WORK/emit.err"; then
    fail 'probe retried the native pipeline'
fi
compile=("$CC" -x c -std=c11 "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}")
if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK/probe.c"; then
    compile+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
compile+=("$WORK/probe.c" -lm -o "$WORK/probe.exe")
if ! "${compile[@]}" >"$WORK/compile.out" 2>"$WORK/compile.err"; then
    cat "$WORK/compile.out" "$WORK/compile.err" >&2
    fail 'emitted probe C compilation failed'
fi
cat "$WORK/compile.err" >&2
if ! "$WORK/probe.exe" >"$WORK/run.out" 2>"$WORK/run.err"; then
    cat "$WORK/run.out" "$WORK/run.err" >&2
    fail 'probe verdict failed'
fi
[[ "$(tr -d '\r' <"$WORK/run.out")" == 'semantic index mutation owner: ok' ]] ||
    fail 'unexpected probe output'
echo "[$LABEL] shared mutation policy, reads/comparisons and scoped modes PASS ($WORK)"

#!/usr/bin/env bash
# The public installed self-host C and LLVM routes consume the canonical
# intent-observability ABI row. Native C/LLVM are independent execution oracles.
# public installed/native C/LLVM consume zero-, one-, and two-argument registry rows without native re-entry
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
SOURCE="tests/self_hosted/fixtures/intent_observability_history_count.pgy"
WORK_DIR="$ROOT_DIR/.tmp/self_hosted/intent_observability_installed"

fail() {
    echo "[self-host-intent-observability] $*" >&2
    exit 1
}

run_compile_phase() {
    local phase="$1"
    local rc
    shift

    set +e
    (cd "$ROOT_DIR" && "$@") \
        >"$WORK_DIR/$phase.out" 2>"$WORK_DIR/$phase.err"
    rc=$?
    set -e
    if [[ "$rc" -eq 0 ]]; then
        return
    fi

    echo "[self-host-intent-observability] $phase compile failed (exit $rc)" >&2
    if [[ -s "$WORK_DIR/$phase.out" ]]; then
        echo "--- $phase stdout ---" >&2
        cat "$WORK_DIR/$phase.out" >&2
    fi
    if [[ -s "$WORK_DIR/$phase.err" ]]; then
        echo "--- $phase stderr ---" >&2
        cat "$WORK_DIR/$phase.err" >&2
    fi
    fail "$phase did not produce an executable"
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
    SELF_DRIVER="${SELF_DRIVER}.exe"
fi
[[ -x "$PGY" ]] || fail "missing public pgy launcher: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "missing installed self-host driver: $SELF_DRIVER"
PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
suffix=""
installed_name="pgy-self-driver"
if [[ "$PGY" == *.exe ]]; then
    suffix=".exe"
    installed_name="pgy-self-driver.exe"
fi
[[ "$SELF_DRIVER" == "$(dirname "$PGY")/$installed_name" ]] ||
    fail "self-host driver is not installed beside the public launcher"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
run_compile_phase installed-c env -u PGY_SELF_DRIVER_BIN \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$SOURCE" --backend=c -o \
    ".tmp/self_hosted/intent_observability_installed/installed-c$suffix"
! grep -Fq '[pipeline timing]' "$WORK_DIR/installed-c.err" ||
    fail "public C route re-entered the native compiler"
run_compile_phase native-c "$PGY" "$SOURCE" --native-pipeline --backend=c -o \
    ".tmp/self_hosted/intent_observability_installed/native-c$suffix"
run_compile_phase installed-llvm env -u PGY_SELF_DRIVER_BIN \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$SOURCE" --backend=llvm -o \
    ".tmp/self_hosted/intent_observability_installed/installed-llvm$suffix"
! grep -Fq '[pipeline timing]' "$WORK_DIR/installed-llvm.err" ||
    fail "public LLVM route re-entered the native compiler"
run_compile_phase native-llvm "$PGY" "$SOURCE" --native-pipeline \
    --backend=llvm -o \
    ".tmp/self_hosted/intent_observability_installed/native-llvm$suffix"

for backend in installed-c native-c installed-llvm native-llvm; do
    "$WORK_DIR/$backend$suffix" | tr -d '\r' >"$WORK_DIR/$backend.run"
done
printf '0\nfalse\n\n' >"$WORK_DIR/expected"
cmp -s "$WORK_DIR/expected" "$WORK_DIR/installed-c.run" ||
    fail "installed self-host C lost intent-observability arity carriage"
cmp -s "$WORK_DIR/expected" "$WORK_DIR/native-c.run" ||
    fail "native C oracle disagrees with the registry-owned runtime row"
cmp -s "$WORK_DIR/expected" "$WORK_DIR/installed-llvm.run" ||
    fail "installed self-host LLVM lost intent-observability arity carriage"
cmp -s "$WORK_DIR/expected" "$WORK_DIR/native-llvm.run" ||
    fail "native LLVM oracle disagrees with the registry-owned runtime row"
! grep -Fq 'IntentObservabilityAbiRowForSource(' \
    "$ROOT_DIR/src/self_hosted/codegen/emission/runtime_call_rewrite_owner.pgy" ||
    fail "C runtime rewrite reintroduced observability source lookup"
grep -Fq 'SemanticExpressionGraphRuntimeCallAbiId(graph, view.call_node)' \
    "$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy" ||
    fail "semantic C emitter stopped consuming the carried ABI ID"
grep -Fq 'IntentObservabilityAbiRowForCarriedIdentity(' \
    "$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy" ||
    fail "semantic C emitter stopped cross-sealing the carried ABI ID"
! grep -Fq 'IntentObservabilityAbiRowForSource(' \
    "$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy" ||
    fail "semantic C emitter reintroduced source-name ABI lookup"
grep -Fq 'CodegenUsageBuiltinGroupIntentObservability()' \
    "$ROOT_DIR/src/self_hosted/codegen/input/ast_expression_usage_owner.pgy" ||
    fail "codegen usage receipt omitted intent observability"
grep -Fq 'PGY_INTENT_OBSERVABILITY_ENABLED 1' \
    "$ROOT_DIR/src/self_hosted/codegen/runtime_abi/runtime_header_owner.pgy" ||
    fail "enabled runtime header boundary is missing"
! grep -Fq 'IntentHistoryCount' \
    "$ROOT_DIR/src/self_hosted/codegen/emission/runtime_call_rewrite_owner.pgy" ||
    fail "consumer-local intent observability spelling table returned"
grep -Fq 'DirectMirLiteralLogRouteClaimed(admitted)' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy" ||
    fail "direct backend no longer consumes the exact literal route claim"
! grep -Fq 'block_count == 1 && instruction_count == 1' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy" ||
    fail "generic one-instruction literal fallback returned"
BUILTIN_CALL_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_builtin_call_owner.pgy"
BUILTIN_SIGNATURE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_builtin_signature_projection_owner.pgy"
BUILTIN_SIGNATURE_FACT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_builtin_signature_fact_owner.pgy"
grep -Fq 'sequence.arena.identities.runtime_call_abi_ids[node]' \
    "$BUILTIN_CALL_OWNER" ||
    fail "direct MIR builtin call stopped consuming the carried ABI ID"
grep -Fq 'signature.runtime_call_abi_id' \
    "$BUILTIN_CALL_OWNER" ||
    fail "direct MIR builtin call stopped cross-sealing the carried ABI ID"
grep -Fq 'let runtime_call_abi_id: Int;' "$BUILTIN_SIGNATURE_FACT_OWNER" ||
    fail "builtin signature fact stopped owning the canonical ABI ID"
! grep -Fq 'IntentObservabilityAbiRowForSource(' "$BUILTIN_CALL_OWNER" ||
    fail "direct MIR builtin call reintroduced observability name lookup"
! grep -Fq 'CompilerRuntimeValueCallAbiFactForSource(' "$BUILTIN_CALL_OWNER" ||
    fail "direct MIR builtin call reintroduced runtime-value name lookup"
! grep -Fq 'func DirectMirScalarProgramBuiltinRuntimeCallAbiIdFromCarriedIdentity(' \
    "$BUILTIN_CALL_OWNER" ||
    fail "direct MIR builtin call reintroduced name-only ABI reconstruction"
for owner in \
    direct_mir_scalar_program_c_external_runtime_expression_owner.pgy \
    direct_mir_scalar_program_llvm_external_runtime_expression_owner.pgy; do
    owner_path="$ROOT_DIR/src/self_hosted/compiler/$owner"
    grep -Fq 'IntentObservabilityAbiRowForId(' "$owner_path" ||
        fail "$owner no longer resolves the carried ABI ID through the row owner"
    grep -Fq 'row.runtime_name' "$owner_path" ||
        fail "$owner reintroduced a backend-local runtime symbol decision"
    ! grep -Fq 'pgy_intent_history_count_export' "$owner_path" ||
        fail "$owner reintroduced a literal IntentHistoryCount runtime symbol"
done
echo "[self-host-intent-observability] registry row owns installed C/LLVM runtime execution"

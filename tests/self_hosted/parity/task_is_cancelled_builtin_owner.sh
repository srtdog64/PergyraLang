#!/usr/bin/env bash
# One IsCancelled meaning reaches native/public/direct C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL=self-host-task-is-cancelled-builtin
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
VALID=tests/concept_semantics/authority_effect/effect_is_cancelled_valid.pgy
MISSING_EFFECT=tests/concept_semantics/authority_effect/effect_is_cancelled_rejected.pgy
WRONG_ARITY=tests/concept_semantics/authority_effect/effect_is_cancelled_wrong_arity_rejected.pgy
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/task_is_cancelled_builtin_mutations.py"
TASK_ABI="$ROOT_DIR/src/self_hosted/codegen/runtime_abi/task_runtime_owner.pgy"
TASK_BUILTIN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_task_builtin_owner.pgy"
SIGNATURES="$ROOT_DIR/src/self_hosted/semantic/builtin_signature_owner.pgy"
IDENTITY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_builtin_runtime_call_identity_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_external_runtime_expression_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_external_runtime_expression_owner.pgy"
SEMANTIC_C_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for path in "$TASK_ABI" "$TASK_BUILTIN" "$SIGNATURES" "$IDENTITY" \
        "$C_OWNER" "$LLVM_OWNER" "$SEMANTIC_C_OWNER" "$MUTATIONS"; do
    [[ -f "$path" ]] || fail "missing owner: ${path#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
[[ -n "$PYTHON_BIN" ]] || fail "Python is required"
for tool in "$CC" "$CLANG" timeout; do
    command -v "$tool" >/dev/null || fail "missing $tool"
done

grep -Fq 'func TaskRuntimeIsCancelledSymbol()' "$TASK_ABI" &&
    grep -Fq 'return "pgy_task_is_cancelled_export";' "$TASK_ABI" ||
    fail "task ABI owner lost the one export identity"
grep -Fq 'static inline bool pgy_task_is_cancelled_export(void) { return pgy_task_is_cancelled(); }' \
        "$TASK_ABI" || fail "self-contained C projection lost its runtime adapter"
grep -Fq 'declare i1 @pgy_task_is_cancelled_export()' "$TASK_ABI" ||
    fail "LLVM projection lost the task declaration"
grep -Fq 'func SemanticBuiltinIsCancelledName() -> String { return "IsCancelled"; }' \
        "$SIGNATURES" &&
    grep -Fq 'Concat(SemanticBuiltinIsCancelledName(), "^Bool^none")' \
        "$SIGNATURES" ||
    fail "semantic builtin registry lost the zero-argument Bool signature owner"
grep -Fq 'CompilerRuntimeCallAbiTaskIsCancelledFactReady(runtime)' \
        "$TASK_BUILTIN" || fail "task expression bypasses its structured ABI fact"
grep -Fq 'DirectMirScalarProgramTaskExpressionKind(signature.expression_kind)' \
        "$IDENTITY" || fail "builtin identity admission omitted the task expression"
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    grep -Fq 'DirectMirScalarProgramTaskRuntimeFact()' "$owner" ||
        fail "$(basename "$owner") bypasses the task ABI fact"
    ! grep -Fq 'pgy_task_is_cancelled_export' "$owner" ||
        fail "$(basename "$owner") hard-codes the physical task symbol"
done
grep -Fq 'CompilerRuntimeCallAbiTaskIsCancelledFact()' "$SEMANTIC_C_OWNER" ||
    fail "semantic C emission bypasses the task ABI fact"
! grep -Fq 'pgy_task_is_cancelled_export' "$SEMANTIC_C_OWNER" ||
    fail "semantic C emission hard-codes the physical task symbol"

mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/task-is-cancelled.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
printf 'false\n' >"$WORK/expected.run"
echo "[$LABEL] evidence: $REL"

run_binary() {
    local stem="$1"
    timeout 30s "$WORK/$stem.exe" | tr -d '\r' >"$WORK/$stem.run" ||
        fail "$stem execution failed"
    cmp -s "$WORK/expected.run" "$WORK/$stem.run" || {
        diff -u "$WORK/expected.run" "$WORK/$stem.run" >&2 || true
        fail "$stem output drifted"
    }
}

for origin in native public; do
    for backend in c llvm; do
        stem="$origin-$backend"
        command=("$PGY" "$VALID")
        [[ "$origin" == native ]] && command+=(--native-pipeline)
        command+=("--backend=$backend" --opt=dev -o "$REL/$stem.exe")
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
            PGY_SELF_DRIVER_BIN="$DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
            timeout 90s "${command[@]}") >"$WORK/$stem.compile.log" 2>&1 || {
            cat "$WORK/$stem.compile.log" >&2
            fail "$stem compilation failed"
        }
        if [[ "$origin" == public ]]; then
            ! grep -Fq '[pipeline timing]' "$WORK/$stem.compile.log" ||
                fail "$stem re-entered the native pipeline"
        fi
        run_binary "$stem"
    done
done

for negative in missing-effect wrong-arity; do
    source="$MISSING_EFFECT"; code=PGY_SEM_EFFECT_CONFLICT
    if [[ "$negative" == wrong-arity ]]; then
        source="$WRONG_ARITY"; code=PGY_SEM_BUILTIN_ARGS_INVALID
    fi
    for origin in native public; do
        for backend in c llvm; do
            stem="$negative-$origin-$backend"
            command=("$PGY" "$source" "--backend=$backend" \
                --error-format=json -o "$REL/$stem.exe")
            [[ "$origin" == native ]] && command+=(--native-pipeline)
            if (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
                    PGY_SELF_DRIVER_BIN="$DRIVER" timeout 90s "${command[@]}") \
                    >"$WORK/$stem.log" 2>&1; then
                fail "$stem was accepted"
            fi
            [[ ! -e "$WORK/$stem.exe" ]] || fail "$stem published an artifact"
            grep -Fq "\"code\":\"$code\"" "$WORK/$stem.log" || {
                cat "$WORK/$stem.log" >&2
                fail "$stem lost diagnostic $code"
            }
        done
    done
done

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$VALID" \
    -o "$REL/program.json") >"$WORK/producer.log" 2>&1 || {
    cat "$WORK/producer.log" >&2
    fail "verified MIR production failed"
}
mir_sha="$(sha256sum "$WORK/program.json" | cut -d' ' -f1)"
"$PYTHON_BIN" "$MUTATIONS" "$WORK/program.json" "$WORK"

runtime_obj="$WORK/runtime.o"
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
    >"$WORK/runtime.compile.log" 2>&1 || fail "runtime object did not compile"

for backend in c llvm; do
    suffix=c; [[ "$backend" == llvm ]] && suffix=ll
    artifact="$WORK/program.$suffix"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$REL/program.json" -o "$REL/program.$suffix") \
        >"$WORK/direct-$backend.project.log" 2>&1 || {
        cat "$WORK/direct-$backend.project.log" >&2
        fail "direct $backend projection failed"
    }
    [[ -s "$artifact" ]] || fail "direct $backend emitted no artifact"
    [[ "$(grep -Fc 'pgy_task_is_cancelled_export' "$artifact")" == "2" ]] ||
        fail "direct $backend lost the one declaration and one call"
    if [[ "$backend" == c ]]; then
        grep -Fq 'static inline bool pgy_task_is_cancelled_export(void) { return pgy_task_is_cancelled(); }' \
            "$artifact" || fail "direct C lost the self-contained task adapter"
        command=("$CC" -x c -std=c11 -fwrapv "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$WORK/direct-$backend.exe")
    else
        grep -Fq 'declare i1 @pgy_task_is_cancelled_export()' "$artifact" ||
            fail "direct LLVM lost the external task declaration"
        command=("$CLANG" -x ir "$artifact" -x none "$runtime_obj" \
            -pthread -lm -o "$WORK/direct-$backend.exe")
    fi
    "${command[@]}" >"$WORK/direct-$backend.compile.log" 2>&1 || {
        cat "$WORK/direct-$backend.compile.log" >&2
        fail "direct $backend artifact compilation failed"
    }
    run_binary "direct-$backend"

    for mutation in wrong-runtime-id wrong-target missing-callee-edge; do
        mutated="$WORK/$mutation.$suffix"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$REL/$mutation.json" -o "$REL/$mutation.$suffix") \
                >"$WORK/$mutation.$backend.log" 2>&1; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$mutated" ]] || fail "$backend published $mutation"
        grep -Fq 'CODEGEN ERROR' "$WORK/$mutation.$backend.log" || {
            cat "$WORK/$mutation.$backend.log" >&2
            fail "$backend $mutation lacked an explicit codegen diagnostic"
        }
    done
done

[[ "$(sha256sum "$WORK/program.json" | cut -d' ' -f1)" == "$mir_sha" ]] ||
    fail "projection mutated the admitted MIR input"
echo "[$LABEL] native/public/direct C+LLVM, 8 semantic refusals, and 6 MIR mutation refusals: PASS"

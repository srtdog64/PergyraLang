#!/usr/bin/env bash
# Real checked MIR -> graph admission -> changed owned-result owner -> C/LLVM.
# This does not claim source-producer replacement, public dispatch or fixed point.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-owned-string-scalar-projection"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
PRODUCER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
MANIFEST="${PGY_SELFHOST_MACHINE_LAYER_DECLARATION:-$(pgy_self_driver_machine_manifest_path "$PRODUCER")}"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
BUILD_TIMEOUT="${PGY_SELFHOST_PROBE_BUILD_TIMEOUT:-300}s"
STEP_TIMEOUT="${PGY_SELFHOST_PROBE_TIMEOUT:-45}s"
FIXTURES="tests/self_hosted/fixtures"
PROBE="$FIXTURES/direct_mir_owned_string_scalar_projection_probe.pgy"
CASES=(minimal unused_tail reordered long_chain local_alias shared_callee)
fail() { echo "[$LABEL] $*; evidence: ${WORK_DIR:-not-created}" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$PRODUCER" || exit 1
[[ -s "$MANIFEST" ]] || fail "missing checked machine declaration"
for tool in "$CC" "$CLANG" timeout mktemp sha256sum cmp realpath; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing tool: $tool"
done
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/owned_string_scalar_projection.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
echo "[$LABEL] evidence: $WORK_DIR"
cd "$ROOT_DIR"
# Runtime ReadFile accepts project-relative paths without weakening its absolute
# path policy. Compiler CLI path conversion is not this runtime boundary.
MANIFEST_REL="$(realpath --relative-to="$ROOT_DIR" "$MANIFEST")"
[[ "$MANIFEST_REL" != ../* && "$MANIFEST_REL" != .. ]] || fail "machine declaration is outside project"
run_checked() {
    local stage="$1" budget="$2"; shift 2
    if ! timeout "$budget" "$@" >"$WORK_DIR/$stage.out" 2>"$WORK_DIR/$stage.err"; then
        cat "$WORK_DIR/$stage.out" "$WORK_DIR/$stage.err" >&2
        fail "$stage failed"
    fi
}
sha256sum "$PROBE" "$PGY" "$PRODUCER" "$MANIFEST" \
    src/self_hosted/compiler/direct_mir_scalar_program_owned_string_result_fact_owner.pgy \
    src/common/compiler_internal_builtin_caller_registry.def >"$WORK_DIR/source-before.sha256"
run_checked probe.emit "$BUILD_TIMEOUT" "$PGY" "$PROBE" --native-pipeline --emit-c \
    -o "$WORK_REL/probe.c"
sha256sum "$PROBE" "$PGY" "$PRODUCER" "$MANIFEST" \
    src/self_hosted/compiler/direct_mir_scalar_program_owned_string_result_fact_owner.pgy \
    src/common/compiler_internal_builtin_caller_registry.def >"$WORK_DIR/source-after.sha256"
cmp -s "$WORK_DIR/source-before.sha256" "$WORK_DIR/source-after.sha256" || fail "probe inputs changed"
export PGY_SELFHOST_CC_PROFILE=test
pgy_selfhost_select_emitted_c_compile_profile
run_checked probe.compile "$BUILD_TIMEOUT" "$CC" -x c -std=c11 \
    "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}" -Isrc -Isrc/runtime -pthread \
    "$WORK_DIR/probe.c" -lm -o "$WORK_DIR/probe.exe"
sha256sum "$WORK_DIR/probe.c" "$WORK_DIR/probe.exe" >"$WORK_DIR/artifacts.sha256"
run_checked runtime.compile "$STEP_TIMEOUT" "$CLANG" -DPGY_LLVM_ENABLED -Isrc \
    -Isrc/runtime -c src/runtime/pgy_runtime_lib.c -o "$WORK_DIR/runtime.o"
printf 'owned-string-result-composition-ready\n' >"$WORK_DIR/expected.run"
for case_name in "${CASES[@]}"; do
    # Each unique source is produced once and that admitted MIR is reused twice.
    run_checked "$case_name.produce" "$STEP_TIMEOUT" "$PRODUCER" --emit-mir-json-verified \
        "$FIXTURES/direct_mir_owned_string_result_composition_$case_name.pgy" \
        -o "$WORK_REL/$case_name.mir.json"
    for backend in c llvm; do
        extension=c; [[ "$backend" == c ]] || extension=ll
        artifact="$WORK_DIR/$case_name.$extension"
        run_checked "$case_name.$backend.project" "$STEP_TIMEOUT" "$WORK_DIR/probe.exe" \
            "$WORK_REL/$case_name.mir.json" "$MANIFEST_REL" \
            "$backend" "$WORK_REL/$case_name.$extension"
        [[ -s "$artifact" ]] || fail "$case_name/$backend emitted no artifact"
        if [[ "$backend" == c ]]; then
            run_checked "$case_name.c.compile" "$STEP_TIMEOUT" "$CC" -x c -std=c11 \
                -Isrc -Isrc/runtime -pthread "$artifact" -lm -o "$WORK_DIR/$case_name-$backend.exe"
        else
            run_checked "$case_name.llvm.compile" "$STEP_TIMEOUT" "$CLANG" -x ir "$artifact" \
                -x none "$WORK_DIR/runtime.o" -pthread -lm -o "$WORK_DIR/$case_name-$backend.exe"
        fi
        run_checked "$case_name.$backend.run" "$STEP_TIMEOUT" "$WORK_DIR/$case_name-$backend.exe"
        tr -d '\r' <"$WORK_DIR/$case_name.$backend.run.out" >"$WORK_DIR/$case_name.$backend.normalized"
        cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$case_name.$backend.normalized" ||
            fail "$case_name/$backend runtime output drift"
    done
done
echo "[$LABEL] six admitted programs / twelve exact C+LLVM runtime outputs: PASS"

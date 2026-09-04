#!/usr/bin/env bash
# One call-duration readonly String temporary through installed source and MIR C/LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-region-user-callee"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/region_user_callee_installed"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/cases/backend_compare/region_user_callee/main.pgy"
BAD_SOURCE_REL="tests/cases/backend_compare/region_user_callee_bad/main.pgy"
ADDRESSABLE_REL="tests/self_hosted/fixtures/readonly_string_addressable_call.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/region_user_callee_mutations.py"
TARGET_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_void_readonly_string_target_owner.pgy"
PARAMETER_POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_policy_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
[[ -n "$PYTHON_BIN" ]] || fail "python is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "LLVM compiler is unavailable"

[[ "$(wc -l <"$TARGET_OWNER")" -le 40 ]] || fail "target owner exceeded 40 lines"
for anchor in 'routines.roles[routine] != DirectMirScalarCfgRoutineRoleCallable()' \
        'routines.return_types[routine] != CompilerAbiLayoutVoidTypeName()' \
        'routines.parameter_counts[routine] != 1' 'ordinal != 0' \
        'CompilerAbiLayoutReadonlyRefPassesDirect(' \
        'routines.parameter_carriages[row] == "readonly-ref"'; do
    require_text "$TARGET_OWNER" "$anchor"
done
for anchor in 'signature.param_count == 1' \
        'signature.return_type == CompilerAbiLayoutVoidTypeName()' \
        'signature.parameters.resources[ordinal] == "none"' \
        'signature.parameters.pass_shapes[ordinal] == "direct"' \
        '!signature.parameters.abi_required[ordinal]' \
        'signature.parameters.abi_layout_ids[ordinal] == 0'; do
    require_text "$PARAMETER_POLICY" "$anchor"
done
for forbidden in Sink region_user_callee readonly_string_value_carriage_coercion \
        readonly_string_type_only_admission unchecked_void_callable_admission; do
    reject_text "$TARGET_OWNER" "$forbidden"
done

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/mir.out" 2>"$WORK_DIR/mir.err" || fail "source-MIR production failed"
for fact in '"name":"Sink"' '"type":"String"' '"carriage":"readonly-ref"' \
        '"resource":"none"' '"pass":"direct"' '"return":"Void"'; do
    require_text "$MIR" "$fact"
done
"$PYTHON_BIN" "$MUTATIONS" "$MIR" "$WORK_DIR"
printf 'leftright\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    suffix=c; [[ "$backend" == llvm ]] && suffix=ll
    artifact_rel="$WORK_REL/direct.$suffix"; artifact="$ROOT_DIR/$artifact_rel"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" \
        -o "$artifact_rel") >"$WORK_DIR/direct-$backend.out" \
        2>"$WORK_DIR/direct-$backend.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
done
require_text "$WORK_DIR/direct.c" 'static void pgy_scalar_routine_1(const char* pgy_param_0)'
require_text "$WORK_DIR/direct.c" 'pgy_scalar_routine_1(pgy_concat("left", "right"))'
require_text "$WORK_DIR/direct.ll" 'call void @pgy.scalar.routine.1(ptr %pgy.expr.0.2)'
require_text "$WORK_DIR/direct.ll" 'define internal void @pgy.scalar.routine.1(ptr %pgy.param.0)'

(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" --emit-c-verified) \
    >"$WORK_DIR/source.c" 2>"$WORK_DIR/source-c.err" || fail "source C failed"
(cd "$ROOT_DIR" && "$DRIVER" --emit-c-artifact-verified "$SOURCE_REL" \
    "$WORK_REL/source-artifact.c") >"$WORK_DIR/source-artifact.out" \
    2>"$WORK_DIR/source-artifact.err" || fail "source C artifact failed"
(cd "$ROOT_DIR" && "$DRIVER" --emit-source-llvm-ir-verified "$SOURCE_REL" \
    -o "$WORK_REL/source.ll") >"$WORK_DIR/source-llvm.out" \
    2>"$WORK_DIR/source-llvm.err" || fail "source LLVM failed"
(cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN; \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$SOURCE_REL" --emit-c \
    -o "$WORK_REL/public.c") >"$WORK_DIR/public-c.out" \
    2>"$WORK_DIR/public-c.err" || fail "public C failed"
(cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN; \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$SOURCE_REL" --emit-llvm \
    -o "$WORK_REL/public.ll") >"$WORK_DIR/public-llvm.out" \
    2>"$WORK_DIR/public-llvm.err" || fail "public LLVM failed"
for file in source.c source-artifact.c public.c; do
    tr -d '\r' <"$WORK_DIR/$file" >"$WORK_DIR/$file.norm"
done
cmp -s "$WORK_DIR/source.c.norm" "$WORK_DIR/source-artifact.c.norm" || fail "source C modes differ"
cmp -s "$WORK_DIR/source.c.norm" "$WORK_DIR/public.c.norm" || fail "public C bypassed source intent"
cmp -s "$WORK_DIR/direct.ll" "$WORK_DIR/source.ll" || fail "source LLVM differs from direct MIR"
cmp -s "$WORK_DIR/direct.ll" "$WORK_DIR/public.ll" || fail "public LLVM differs from direct MIR"
! grep -Fq '[pipeline timing]' "$WORK_DIR/public-c.err" "$WORK_DIR/public-llvm.err" || fail "public route retried native pipeline"
require_text "$WORK_DIR/source.c" 'void Sink(const char* value)'
require_text "$WORK_DIR/source.c" 'Sink(pgy_concat("left", "right"))'
reject_text "$WORK_DIR/source.c" 'Sink(&pgy_concat('
(cd "$ROOT_DIR" && "$DRIVER" "$ADDRESSABLE_REL" --emit-c-verified) \
    >"$WORK_DIR/addressable.c" 2>"$WORK_DIR/addressable.err" || fail "addressable source C failed"
require_text "$WORK_DIR/addressable.c" 'void SinkAddressable(const char* value)'
require_text "$WORK_DIR/addressable.c" 'SinkAddressable(value)'
reject_text "$WORK_DIR/addressable.c" 'SinkAddressable(&value)'

for stem in direct source public; do
    "$CC" -x c -std=c11 -fwrapv -fno-strict-aliasing -I"$ROOT_DIR/src" \
        -I"$ROOT_DIR/src/runtime" -pthread "$WORK_DIR/$stem.c" \
        -o "$WORK_DIR/$stem-c.exe" >/dev/null 2>"$WORK_DIR/$stem-c.compile.err" || fail "$stem C did not compile"
    "$WORK_DIR/$stem-c.exe" | tr -d '\r' >"$WORK_DIR/$stem-c.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$stem-c.run" || fail "$stem C execution drifted"
done
"$CC" -x c -std=c11 -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$WORK_DIR/addressable.c" -o "$WORK_DIR/addressable.exe" >/dev/null \
    2>"$WORK_DIR/addressable.compile.err" || fail "addressable C did not compile"
"$WORK_DIR/addressable.exe" | tr -d '\r' >"$WORK_DIR/addressable.run"
printf 'addressable\n' >"$WORK_DIR/addressable.expected"
cmp -s "$WORK_DIR/addressable.expected" "$WORK_DIR/addressable.run" || fail "addressable ref execution drifted"
"$CLANG" -x ir "$WORK_DIR/public.ll" -o "$WORK_DIR/public-llvm.exe" \
    >/dev/null 2>"$WORK_DIR/public-llvm.compile.err" || fail "public LLVM did not compile"
"$WORK_DIR/public-llvm.exe" | tr -d '\r' >"$WORK_DIR/public-llvm.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/public-llvm.run" || fail "public LLVM execution drifted"

for mutation in pass-indirect resource-region abi-required carriage-value-result return-string; do
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"; rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/$mutation.mir.json" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
        grep -Fq 'direct MIR scalar program route rejected' "$WORK_DIR/$mutation.$backend.out" \
            "$WORK_DIR/$mutation.$backend.err" || fail "$backend $mutation escaped route owner"
    done
done

reject_bad_source() {
    local mode="$1" artifact="$2"; shift 2; rm -f "$artifact"
    if (cd "$ROOT_DIR" && "$@") >"$WORK_DIR/bad-$mode.out" 2>"$WORK_DIR/bad-$mode.err"; then
        fail "$mode accepted escaping readonly String temporary"
    fi
    [[ ! -e "$artifact" ]] || fail "$mode published escaping artifact"
}
reject_bad_source source-c "$WORK_DIR/bad-source.c" "$DRIVER" \
    --emit-c-artifact-verified "$BAD_SOURCE_REL" "$WORK_REL/bad-source.c"
reject_bad_source source-llvm "$WORK_DIR/bad-source.ll" "$DRIVER" \
    --emit-source-llvm-ir-verified "$BAD_SOURCE_REL" -o "$WORK_REL/bad-source.ll"
reject_bad_source public-c "$WORK_DIR/bad-public.c" env -u PGY_NATIVE_PIPELINE \
    -u PGY_SELF_DRIVER_BIN PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$BAD_SOURCE_REL" \
    --emit-c -o "$WORK_REL/bad-public.c"
reject_bad_source public-llvm "$WORK_DIR/bad-public.ll" env -u PGY_NATIVE_PIPELINE \
    -u PGY_SELF_DRIVER_BIN PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$BAD_SOURCE_REL" \
    --emit-llvm -o "$WORK_REL/bad-public.ll"
grep -Fq 'ref argument must be addressable' "$WORK_DIR/bad-source-c.out" || fail "source C lost escape diagnostic"
grep -Fq 'callable-route-envelope' "$WORK_DIR/bad-source-llvm.out" || fail "source LLVM lost route diagnostic"
! grep -Fq '[pipeline timing]' "$WORK_DIR/bad-public-c.err" "$WORK_DIR/bad-public-llvm.err" || fail "bad public route retried native pipeline"

echo "[$LABEL] one call-duration readonly String temporary is exact across installed C/LLVM"

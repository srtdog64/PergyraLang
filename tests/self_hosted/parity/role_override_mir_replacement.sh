#!/usr/bin/env bash
# Installed source and direct-MIR role override replacement. An ordinary
# subject member call stays subject-bound while the override body remains a
# distinct emitted callable.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-role-override-replacement"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/driver/role_override_replacement"
WORK="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/cases/backend_compare/role_override_mir/main.pgy"
SOURCE="$ROOT_DIR/$SOURCE_REL"
EXPECTED="$ROOT_DIR/tests/cases/backend_compare/role_override_mir/expected.stdout"
MUTATOR="$ROOT_DIR/tests/self_hosted/parity/role_override_mir_mutations.py"
MIR="$WORK/role-override.mir.json"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require() { [[ -f "$1" ]] || fail "missing file: ${1#"$ROOT_DIR/"}"; }
rel() { pgy_selfhost_path_relative_to_root "$1"; }
hash_file() { sha256sum "$1" | awk '{print $1}'; }
normalize() { pgy_selfhost_normalize_text_artifact <"$1" >"$2"; }

project() {
    local target="$1" input="$2" output="$3"
    rm -f "$output" "$output.out" "$output.err"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$(rel "$input")" -o "$(rel "$output")" \
        >"$output.out" 2>"$output.err") || {
        cat "$output.out" "$output.err" >&2 || true
        fail "$target rejected ${input##*/}"
    }
    [[ -s "$output" ]] || fail "$target emitted no artifact"
}

compile_run() {
    local target="$1" artifact="$2" stem="$3" family="$4" bin actual expected
    bin="$WORK/$stem.exe"
    rm -f "$bin" "$bin.build"
    if [[ "$target" == c ]]; then
        if [[ "$family" == source ]]; then
            local command=("$CC" -x c -std=c11)
            command+=("${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}")
            if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
                command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
            fi
            command+=("$artifact" -o "$bin")
            "${command[@]}" >"$bin.build" 2>&1 || {
                cat "$bin.build" >&2; fail "C compile failed: $stem";
            }
        else
            "$CC" -x c -std=c11 -Wall -Wextra -Werror "$artifact" \
                -o "$bin" >"$bin.build" 2>&1 || {
                cat "$bin.build" >&2; fail "C compile failed: $stem";
            }
        fi
    else
        if [[ "$family" == source ]]; then
            "$CLANG" -x ir "$artifact" -x none "$SOURCE_LLVM_RUNTIME_OBJECT" \
                -pthread -lm -o "$bin" >"$bin.build" 2>&1 || {
                    cat "$bin.build" >&2
                    fail "LLVM compile failed: $stem"
                }
        else
            "$CLANG" -x ir "$artifact" -o "$bin" >"$bin.build" 2>&1 || {
                cat "$bin.build" >&2; fail "LLVM compile failed: $stem";
            }
        fi
    fi
    actual="$("$bin" | tr -d '\r')" || fail "execution failed: $stem"
    expected="${5:-$(tr -d '\r\n' <"$EXPECTED")}"
    [[ "$actual" == "$expected" ]] ||
        fail "$stem output '$actual', expected '$expected'"
}

reject_mir() {
    local input="$1" target output
    for target in c llvm; do
        output="$WORK/rejected-$target-${input##*/}"
        rm -f "$output" "$output.out" "$output.err"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$(rel "$input")" -o "$(rel "$output")" \
            >"$output.out" 2>"$output.err"); then
            fail "$target accepted ${input##*/}"
        fi
        [[ ! -e "$output" ]] || fail "$target published a rejected artifact"
        grep -Eiq 'role override|three-routine|multi-routine|MIR machine-layer|direct MIR' \
            "$output.out" "$output.err" || {
            cat "$output.out" "$output.err" >&2 || true
            fail "$target rejection escaped the direct-MIR owner"
        }
    done
}

reject_source() {
    local name="$1" artifact="$2"
    shift 2
    [[ -z "$artifact" ]] || rm -f "$artifact"
    rm -f "$WORK/$name.out" "$WORK/$name.err"
    set +e
    (cd "$ROOT_DIR" && "$@") >"$WORK/$name.out" 2>"$WORK/$name.err"
    local rc=$?
    set -e
    [[ "$rc" -ne 0 ]] || fail "$name accepted malformed override syntax"
    [[ -z "$artifact" || ! -e "$artifact" ]] ||
        fail "$name published a malformed-source artifact"
    grep -Fiq "expected 'func' after 'override'" \
        "$WORK/$name.out" "$WORK/$name.err" ||
        fail "$name lost the stable override diagnostic"
    ! grep -Eq '^Program$|"schema":"pgy.mir.v1"' "$WORK/$name.out" ||
        fail "$name emitted a successful AST/MIR payload"
    ! grep -Fq '[pipeline timing]' "$WORK/$name.err" ||
        fail "$name re-entered an undeclared native pipeline"
}

reject_receiver_source() {
    local name="$1" source_rel="$2" rc
    local artifact="$WORK/$name.c"
    rm -f "$artifact" "$WORK/$name.out" "$WORK/$name.err"
    set +e
    (cd "$ROOT_DIR" && "$DRIVER" --emit-c-artifact-verified \
        "$source_rel" "$WORK_REL/$name.c") \
        >"$WORK/$name.out" 2>"$WORK/$name.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 ]] || fail "$name accepted an invalid source receiver"
    [[ ! -e "$artifact" ]] || fail "$name published a rejected receiver"
    grep -Fq 'Code: function_signature_invalid' \
        "$WORK/$name.out" "$WORK/$name.err" ||
        fail "$name lost the signature diagnostic identity"
    grep -Fq 'missing_fact: receiver_source_parameter_shape' \
        "$WORK/$name.out" "$WORK/$name.err" ||
        fail "$name escaped the receiver source-parameter owner"
}

for file in "$PGY" "$DRIVER" "$SOURCE" "$EXPECTED" "$MUTATOR"; do
    require "$file"
done
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing clang"
pgy_selfhost_select_emitted_c_compile_profile || fail "invalid emitted-C profile"
mkdir -p "$WORK"
rm -f "$WORK"/*

while IFS='|' read -r owner cap; do
    require "$ROOT_DIR/$owner"
    [[ "$(wc -l <"$ROOT_DIR/$owner" | tr -d ' ')" -le "$cap" ]] ||
        fail "$owner exceeds $cap lines"
done <<'OWNERS'
src/self_hosted/compiler/direct_mir_role_override_program_identity_owner.pgy|800
src/self_hosted/compiler/direct_mir_role_override_plan_owner.pgy|120
src/self_hosted/compiler/direct_mir_role_override_target_projection_owner.pgy|110
src/self_hosted/compiler/direct_mir_role_override_emission_owner.pgy|220
src/self_hosted/codegen/emission/implicit_receiver_c_parameter_owner.pgy|60
src/self_hosted/codegen/emission/function_emit.pgy|500
src/self_hosted/codegen/emission/function_prototype_block_owner.pgy|270
OWNERS
family_lines=$(wc -l \
    <"$ROOT_DIR/src/self_hosted/compiler/direct_mir_role_override_program_identity_owner.pgy")
family_lines=$((family_lines + $(wc -l \
    <"$ROOT_DIR/src/self_hosted/compiler/direct_mir_role_override_plan_owner.pgy")))
family_lines=$((family_lines + $(wc -l \
    <"$ROOT_DIR/src/self_hosted/compiler/direct_mir_role_override_target_projection_owner.pgy")))
family_lines=$((family_lines + $(wc -l \
    <"$ROOT_DIR/src/self_hosted/compiler/direct_mir_role_override_emission_owner.pgy")))
[[ "$family_lines" -le 1250 ]] || fail "role override owner family exceeds 1250 lines"
! grep -Eiq '"OverrideTarget"|"OverrideSurface"|role_override_mir|"base"|"override"' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_role_override_"*.pgy ||
    fail "fixture identity leaked into role override owners"
grep -Fq 'CodegenImplicitReceiverCParameterOrDie(' \
    "$ROOT_DIR/src/self_hosted/codegen/emission/function_emit.pgy" ||
    fail "function definitions bypass the implicit-receiver owner"
grep -Fq 'CodegenImplicitReceiverCParameterOrDie(' \
    "$ROOT_DIR/src/self_hosted/codegen/emission/function_prototype_block_owner.pgy" ||
    fail "function prototypes bypass the implicit-receiver owner"

(cd "$ROOT_DIR" && "$DRIVER" --ast "$SOURCE_REL") \
    >"$WORK/direct.ast" 2>"$WORK/direct.ast.err"
(cd "$ROOT_DIR" && env -u PGY_SELF_DRIVER_BIN PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" --ast "$SOURCE_REL") >"$WORK/public.ast" 2>"$WORK/public.ast.err"
(cd "$ROOT_DIR" && "$PGY" --native-pipeline --ast "$SOURCE_REL") \
    >"$WORK/native.ast" 2>"$WORK/native.ast.err"
cmp -s "$WORK/direct.ast" "$WORK/public.ast" ||
    fail "public AST differs from the installed parser"
normalize "$WORK/direct.ast" "$WORK/direct.ast.norm"
normalize "$WORK/native.ast" "$WORK/native.ast.norm"
cmp -s "$WORK/direct.ast.norm" "$WORK/native.ast.norm" ||
    fail "installed AST differs from the native oracle"
grep -Fq 'Role: OverrideSurface for OverrideTarget' "$WORK/direct.ast" ||
    fail "AST lost the role target"
grep -Fq 'Override' "$WORK/direct.ast" || fail "AST lost the override wrapper"

(cd "$ROOT_DIR" && PGY_IO_ALLOW_ABSOLUTE=1 "$DRIVER" \
    --emit-mir-json-verified "$SOURCE") >"$MIR" 2>"$WORK/direct.mir.err"
(cd "$ROOT_DIR" && env -u PGY_SELF_DRIVER_BIN PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" --mir-json "$SOURCE_REL") >"$WORK/public.mir.json" \
    2>"$WORK/public.mir.err"
cmp -s "$MIR" "$WORK/public.mir.json" ||
    fail "public MIR differs from the installed producer"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$SOURCE_REL") \
    >"$WORK/native.mir.json" 2>"$WORK/native.mir.err"
(cd "$ROOT_DIR" && "$DRIVER" --canonicalize-mir-json \
    "$WORK_REL/role-override.mir.json") >"$WORK/self.canonical.json"
(cd "$ROOT_DIR" && "$DRIVER" --canonicalize-oracle-mir-json \
    "$WORK_REL/native.mir.json") >"$WORK/native.canonical.json"
cmp -s "$WORK/self.canonical.json" "$WORK/native.canonical.json" ||
    fail "native and installed canonical MIR differ"
grep -Fq '"call_target_name":"OverrideTarget_Name"' "$MIR" ||
    fail "MIR rebound the ordinary member call"
mir_hash="$(hash_file "$MIR")"
"$PYTHON_BIN" "$MUTATOR" "$MIR" "$WORK"

for required_negative in \
    direct_override_as_ability_impl direct_override_method_drop; do
    [[ -f "$WORK/$required_negative.negative.json" ]] ||
        fail "missing required MIR negative: $required_negative"
done

project c "$MIR" "$WORK/direct-mir.c"
project llvm "$MIR" "$WORK/direct-mir.ll"
[[ "$(hash_file "$MIR")" == "$mir_hash" ]] || fail "projection mutated MIR"
compile_run c "$WORK/direct-mir.c" direct-mir-c mir
compile_run llvm "$WORK/direct-mir.ll" direct-mir-llvm mir
grep -Fq 'OverrideTarget_Name(OverrideTarget *self)' "$WORK/direct-mir.c" ||
    fail "direct C lost the subject receiver"
grep -Fq 'OverrideSurface_Name(void *self)' "$WORK/direct-mir.c" ||
    fail "direct C lost the erased role receiver"
cp "$WORK/direct-mir.c" "$WORK/direct-mir-role-slot.c"
printf '\nconst char *(*pgy_role_slot)(void *) = OverrideSurface_Name;\n' \
    >>"$WORK/direct-mir-role-slot.c"
"$CC" -x c -std=c11 -Wall -Wextra -Werror \
    "$WORK/direct-mir-role-slot.c" -o "$WORK/direct-mir-role-slot.exe" \
    >"$WORK/direct-mir-role-slot.build" 2>&1 || {
        cat "$WORK/direct-mir-role-slot.build" >&2
        fail "direct C role slot does not preserve its erased receiver ABI"
    }
grep -Fq 'OverrideTarget_Name(&target)' "$WORK/direct-mir.c" ||
    fail "direct C did not call the subject method"
! grep -Fq 'OverrideSurface_Name(&target)' "$WORK/direct-mir.c" ||
    fail "direct C rebound the ordinary call to the override"
grep -Fq 'call ptr @OverrideTarget_Name(ptr %pgy.subject)' "$WORK/direct-mir.ll" ||
    fail "direct LLVM did not call the subject method"
! grep -Fq 'call ptr @OverrideSurface_Name(ptr %pgy.subject)' "$WORK/direct-mir.ll" ||
    fail "direct LLVM rebound the ordinary call to the override"

for variant in declaration-order routine-order both-orders; do
    project c "$WORK/$variant.positive.json" "$WORK/$variant.c"
    project llvm "$WORK/$variant.positive.json" "$WORK/$variant.ll"
    cmp -s "$WORK/direct-mir.c" "$WORK/$variant.c" ||
        fail "$variant changed direct C"
    cmp -s "$WORK/direct-mir.ll" "$WORK/$variant.ll" ||
        fail "$variant changed direct LLVM"
done
negative_count=0
for input in "$WORK"/*.negative.json; do
    reject_mir "$input"
    negative_count=$((negative_count + 1))
done
[[ "$negative_count" -eq 10 ]] || fail "expected 10 MIR negatives, got $negative_count"

(cd "$ROOT_DIR" && "$DRIVER" --emit-c-artifact-verified \
    "$SOURCE_REL" "$WORK_REL/direct-source.c") \
    >"$WORK/direct-source.c.out" 2>"$WORK/direct-source.c.err"
(cd "$ROOT_DIR" && env -u PGY_SELF_DRIVER_BIN PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" "$SOURCE_REL" --emit-c -o "$WORK_REL/public-source.c") \
    >"$WORK/public-source.c.out" 2>"$WORK/public-source.c.err"
(cd "$ROOT_DIR" && "$PGY" --native-pipeline "$SOURCE_REL" --emit-c \
    -o "$WORK_REL/native-source.c") >"$WORK/native-source.c.out" \
    2>"$WORK/native-source.c.err"
cmp -s "$WORK/direct-source.c" "$WORK/public-source.c" ||
    fail "public source C differs from the installed driver"
grep -Fq 'OverrideTarget_Name(OverrideTarget *self)' "$WORK/direct-source.c" ||
    fail "source C lost the implicit subject receiver"
grep -Fq 'OverrideSurface_Name(void *_pgy_raw_self)' \
    "$WORK/direct-source.c" || fail "source C lost the erased role receiver"

(cd "$ROOT_DIR" && "$DRIVER" --emit-source-llvm-ir-verified \
    "$SOURCE_REL" -o "$WORK_REL/direct-source.ll") \
    >"$WORK/direct-source.ll.out" 2>"$WORK/direct-source.ll.err"
(cd "$ROOT_DIR" && env -u PGY_SELF_DRIVER_BIN PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" "$SOURCE_REL" --emit-llvm -o "$WORK_REL/public-source.ll") \
    >"$WORK/public-source.ll.out" 2>"$WORK/public-source.ll.err"
(cd "$ROOT_DIR" && "$PGY" --native-pipeline "$SOURCE_REL" --emit-llvm \
    -o "$WORK_REL/native-source.ll") >"$WORK/native-source.ll.out" \
    2>"$WORK/native-source.ll.err"
cmp -s "$WORK/direct-source.ll" "$WORK/public-source.ll" ||
    fail "public source LLVM differs from the installed driver"

SOURCE_LLVM_RUNTIME_OBJECT="$WORK/source-llvm-runtime.o"
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" \
    -o "$SOURCE_LLVM_RUNTIME_OBJECT" \
    >"$WORK/source-llvm-runtime.build" 2>&1 || {
        cat "$WORK/source-llvm-runtime.build" >&2
        fail "source LLVM runtime ABI object did not compile"
    }

for mode in direct-source public-source native-source; do
    compile_run c "$WORK/$mode.c" "$mode-c" source
    compile_run llvm "$WORK/$mode.ll" "$mode-llvm" source
done
while IFS='|' read -r fixture stem expected_signature; do
    (cd "$ROOT_DIR" && "$DRIVER" --emit-c-artifact-verified \
        "tests/self_hosted/parity/fixture/$fixture" "$WORK_REL/$stem.c")
    compile_run c "$WORK/$stem.c" "$stem-c" source 42
    grep -Fq "$expected_signature" "$WORK/$stem.c" ||
        fail "$stem lost its exact physical receiver signature"
done <<'RECEIVER_FIXTURES'
implicit_mutable_receiver_inout.pgy|implicit-inout|Counter_Increment(Counter *self, int32_t *_pgy_inout_value)
explicit_mutable_receiver_no_duplicate.pgy|explicit-self|ExplicitCounter_Read(ExplicitCounter *self)
implicit_value_receiver_no_duplicate.pgy|implicit-value|ValueBox_Read(ValueBox self)
RECEIVER_FIXTURES
reject_receiver_source typed-self \
    tests/self_hosted/parity/fixture/typed_self_parameter_rejected.pgy
reject_receiver_source late-self \
    tests/self_hosted/parity/fixture/late_self_parameter_rejected.pgy
if grep -Fq '[pipeline timing]' "$WORK"/public*.err; then
    fail "public source/AST/MIR path re-entered the native pipeline"
fi

BAD_REL="$WORK_REL/missing-override-func.pgy"
"$PYTHON_BIN" - "$WORK/missing-override-func.pgy" <<'PY'
import pathlib, sys
pathlib.Path(sys.argv[1]).write_text("""subject BadTarget {}
role BadRole for BadTarget {
    override Name() -> String { return \"bad\"; }
}
func Main() -> Void {}
""", encoding="utf-8")
PY
reject_source direct-bad-ast "" "$DRIVER" --ast "$BAD_REL"
reject_source direct-bad-mir "" "$DRIVER" --emit-mir-json-verified "$BAD_REL"
reject_source direct-bad-c "$WORK/direct-bad.c" "$DRIVER" \
    --emit-c-artifact-verified "$BAD_REL" "$WORK_REL/direct-bad.c"
reject_source direct-bad-llvm "$WORK/direct-bad.ll" "$DRIVER" \
    --emit-source-llvm-ir-verified "$BAD_REL" -o "$WORK_REL/direct-bad.ll"
reject_source public-bad-ast "" env -u PGY_SELF_DRIVER_BIN \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" --ast "$BAD_REL"
reject_source public-bad-mir "" env -u PGY_SELF_DRIVER_BIN \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" --mir-json "$BAD_REL"
reject_source public-bad-c "$WORK/public-bad.c" env -u PGY_SELF_DRIVER_BIN \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$BAD_REL" --emit-c \
    -o "$WORK_REL/public-bad.c"
reject_source public-bad-llvm "$WORK/public-bad.ll" env -u PGY_SELF_DRIVER_BIN \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$BAD_REL" --emit-llvm \
    -o "$WORK_REL/public-bad.ll"
reject_source native-bad-ast "" "$PGY" --native-pipeline --ast "$BAD_REL"
reject_source native-bad-mir "" "$PGY" --native-pipeline --mir-json "$BAD_REL"
reject_source native-bad-c "$WORK/native-bad.c" "$PGY" --native-pipeline \
    "$BAD_REL" --emit-c -o "$WORK_REL/native-bad.c"
reject_source native-bad-llvm "$WORK/native-bad.ll" "$PGY" --native-pipeline \
    "$BAD_REL" --emit-llvm -o "$WORK_REL/native-bad.ll"

echo "[$LABEL] PASS: AST/MIR parity; 8 role C/LLVM runtime legs + 3 receiver runtimes + 14 source negatives; 3 receiver runtimes and typed/late self artifact-free negatives; 3 permutations + 10 MIR negatives"

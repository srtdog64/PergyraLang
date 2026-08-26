#!/usr/bin/env bash
# Callable values cross C semantic re-entry and LLVM direct-MIR through one exact identity lane.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:driver-rung2-callable-parameter-identity"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/driver_rung2_callable_parameter_identity"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/cases/backend_compare/compose_two_functions/main.pgy"
SHADOW_SOURCE_REL="tests/self_hosted/fixtures/callable_parameter_builtin_shadow.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/driver_rung2_callable_parameter_identity_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
command -v python >/dev/null 2>&1 || fail "missing Python"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
printf '16\n13\n6\n' >"$WORK_DIR/expected.run"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"type":"func(Int) -> Int"' "$MIR" ||
    fail "producer omitted the canonical callable signature"
grep -Fq '"binding_kind":"formal_parameter","binding_ordinal":1' "$MIR" ||
    fail "producer omitted the formal callable identity"
grep -Fq '"binding_kind":"declared_callable"' "$MIR" ||
    fail "producer omitted the declared callable value identity"

# The production/default C route deliberately re-enters the semantic graph;
# it must preserve and validate carried identities instead of rebuilding them.
C_ARTIFACT="$WORK_DIR/program.c"
(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL") >"$C_ARTIFACT" \
    2>"$WORK_DIR/c.project.err" || {
        cat "$WORK_DIR/c.project.err" >&2
        fail "source C projection failed"
    }
grep -Fq 'int32_t (*a)(int32_t)' "$C_ARTIFACT" ||
    fail "C signature omitted the callable parameter"
grep -Fq 'ApplyTwo(Plus3, Times2, 5)' "$C_ARTIFACT" ||
    fail "C call omitted the declared callable values"
C_BIN="$WORK_DIR/program-c.exe"
command=("$CC" -x c -std=c11 -O0 -fwrapv -fno-strict-aliasing)
if pgy_selfhost_emitted_c_uses_runtime_headers "$C_ARTIFACT"; then
    command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
command+=("$C_ARTIFACT" -lm -o "$C_BIN")
"${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" || {
    cat "$WORK_DIR/c.compile.err" >&2
    fail "C artifact did not compile"
}
"$C_BIN" | tr -d '\r' >"$WORK_DIR/c.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" ||
    fail "C runtime output drifted"

# LLVM consumes the same verified MIR directly through GraphPlan.
LLVM_REL="$WORK_REL/program.ll"
LLVM_ARTIFACT="$ROOT_DIR/$LLVM_REL"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
    "$MIR_REL" -o "$LLVM_REL") >"$WORK_DIR/llvm.project.out" \
    2>"$WORK_DIR/llvm.project.err" || {
        cat "$WORK_DIR/llvm.project.out" "$WORK_DIR/llvm.project.err" >&2
        fail "LLVM projection failed"
    }
grep -Eq 'define internal i64 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, ptr %pgy\.param\.1, i64 %pgy\.param\.2\)' \
    "$LLVM_ARTIFACT" || fail "LLVM signature omitted callable parameters"
grep -Eq 'call i64 %pgy\.param\.[01]\(i64 ' "$LLVM_ARTIFACT" ||
    fail "LLVM artifact omitted the indirect callable invocation"
LLVM_BIN="$WORK_DIR/program-llvm.exe"
"$CLANG" -x ir "$LLVM_ARTIFACT" -o "$LLVM_BIN" \
    >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" || {
        cat "$WORK_DIR/llvm.compile.err" >&2
        fail "LLVM artifact did not compile"
    }
"$LLVM_BIN" | tr -d '\r' >"$WORK_DIR/llvm.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/llvm.run" ||
    fail "LLVM runtime output drifted"

# A callable formal owns its carried identity even when its source name shadows
# a builtin table row. Call-target admission selects that lane before any
# spelling-based table lookup.
SHADOW_MIR_REL="$WORK_REL/shadow.mir.json"
SHADOW_MIR="$ROOT_DIR/$SHADOW_MIR_REL"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SHADOW_SOURCE_REL" -o "$SHADOW_MIR_REL") \
    >"$WORK_DIR/shadow.producer.out" 2>"$WORK_DIR/shadow.producer.err" ||
    fail "builtin-shadow MIR production failed"
grep -Fq '"binding_kind":"formal_parameter"' "$SHADOW_MIR" ||
    fail "builtin-shadow producer lost the formal callable identity"

SHADOW_C="$WORK_DIR/shadow.c"
(cd "$ROOT_DIR" && "$DRIVER" "$SHADOW_SOURCE_REL") >"$SHADOW_C" \
    2>"$WORK_DIR/shadow.c.err" || fail "builtin-shadow C projection failed"
shadow_c_command=("$CC" -x c -std=c11 -O0 -fwrapv -fno-strict-aliasing)
if pgy_selfhost_emitted_c_uses_runtime_headers "$SHADOW_C"; then
    shadow_c_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
shadow_c_command+=("$SHADOW_C" -lm -o "$WORK_DIR/shadow-c.exe")
"${shadow_c_command[@]}" >"$WORK_DIR/shadow.c.compile.out" \
    2>"$WORK_DIR/shadow.c.compile.err" || fail "builtin-shadow C did not compile"
"$WORK_DIR/shadow-c.exe" | tr -d '\r' >"$WORK_DIR/shadow.c.run"
printf '6\n' >"$WORK_DIR/shadow.expected"
cmp -s "$WORK_DIR/shadow.expected" "$WORK_DIR/shadow.c.run" ||
    fail "builtin-shadow C runtime output drifted"

SHADOW_LLVM_REL="$WORK_REL/shadow.ll"
SHADOW_LLVM="$ROOT_DIR/$SHADOW_LLVM_REL"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
    "$SHADOW_MIR_REL" -o "$SHADOW_LLVM_REL") \
    >"$WORK_DIR/shadow.llvm.out" 2>"$WORK_DIR/shadow.llvm.err" ||
    fail "builtin-shadow LLVM projection failed"
"$CLANG" -x ir "$SHADOW_LLVM" -o "$WORK_DIR/shadow-llvm.exe" \
    >"$WORK_DIR/shadow.llvm.compile.out" \
    2>"$WORK_DIR/shadow.llvm.compile.err" ||
    fail "builtin-shadow LLVM did not compile"
"$WORK_DIR/shadow-llvm.exe" | tr -d '\r' >"$WORK_DIR/shadow.llvm.run"
cmp -s "$WORK_DIR/shadow.expected" "$WORK_DIR/shadow.llvm.run" ||
    fail "builtin-shadow LLVM runtime output drifted"

mutations=(
    formal-missing-binding-id
    formal-target-zero
    formal-target-crosswire
    formal-binding-crosswire
    formal-binding-ordinal
    formal-forged-target
    formal-target-declared-routine
    formal-name-mismatch
    formal-param-source-missing
    formal-type-missing
    formal-carriage-missing
    formal-carriage-forged
    formal-malformed-signature
    formal-unsupported-shape
    declared-missing-binding-id
    declared-binding-id-swap
    declared-binding-kind
    declared-binding-ordinal
    declared-name-mismatch
)

for mutation in "${mutations[@]}"; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    mutated="$ROOT_DIR/$mutated_rel"
    python "$MUTATIONS" "$MIR" "$mutation" "$mutated"

    c_rel="$WORK_REL/$mutation.c"
    c_output="$ROOT_DIR/$c_rel"
    rm -f "$c_output"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json "$mutated_rel" -o "$c_rel") \
        >"$WORK_DIR/$mutation.c.out" 2>"$WORK_DIR/$mutation.c.err"; then
        fail "C semantic re-entry accepted $mutation"
    fi
    [[ ! -e "$c_output" ]] ||
        fail "C semantic re-entry published an artifact for $mutation"

    llvm_rel="$WORK_REL/$mutation.ll"
    llvm_output="$ROOT_DIR/$llvm_rel"
    rm -f "$llvm_output"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
        "$mutated_rel" -o "$llvm_rel") >"$WORK_DIR/$mutation.llvm.out" \
        2>"$WORK_DIR/$mutation.llvm.err"; then
        fail "LLVM direct-MIR accepted $mutation"
    fi
    [[ ! -e "$llvm_output" ]] ||
        fail "LLVM direct-MIR published an artifact for $mutation"
done

echo "[$LABEL] callable identity C/LLVM runtime parity + exact negative ratchet: PASS"

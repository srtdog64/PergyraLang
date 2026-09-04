#!/usr/bin/env bash
# One installed MIR artifact carries tagged-enum constructors and direct
# payload-member projections through the shared scalar plan to C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-tagged-enum-payload"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/driver/tagged_enum_payload"
WORK="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/cases/backend_compare/tagged_union/main.pgy"
MIR_REL="$WORK_REL/tagged.one.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
STALE_SOURCE_REL="tests/self_hosted/parity/fixture/tagged_enum_stale_ssa_use.pgy"
STALE_MIR_REL="$WORK_REL/stale.baseline.mir.json"
STALE_MIR="$ROOT_DIR/$STALE_MIR_REL"
MUTATOR="$ROOT_DIR/tests/self_hosted/parity/one_mir_tagged_enum_payload_mutations.py"
ROUTE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_enum_value_match_route_owner.pgy"
REFERENCE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_referenced_enum_fact_owner.pgy"
MEMBER_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_payload_enum_expression_owner.pgy"
BACKEND_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq '"AST_MATCH_CASE"' "$ROUTE_OWNER" ||
    fail "payload-free enum-match route lacks match-case evidence"
grep -Fq 'instruction_abi_type_names' "$REFERENCE_OWNER" ||
    fail "referenced enum owner omits local ABI type evidence"
grep -Fq 'DirectMirScalarProgramPayloadEnumMemberFromGraph(' "$MEMBER_OWNER" ||
    fail "tagged-enum payload member has no graph owner"
grep -Fq 'scalar_program_route.referenced_enum.payload_present' "$BACKEND_OWNER" ||
    fail "backend route does not consume the sealed payload-enum fact"
! grep -Eq 'tagged_union|"Shape"|"Circle"|"Rect"' \
    "$ROUTE_OWNER" "$REFERENCE_OWNER" "$MEMBER_OWNER" "$BACKEND_OWNER" ||
    fail "fixture spelling leaked into the tagged-enum direct MIR route"

mkdir -p "$WORK"
rm -f "$WORK"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$MIR_REL") >"$WORK/producer.out" 2>"$WORK/producer.err" || {
        cat "$WORK/producer.out" "$WORK/producer.err" >&2
        fail "installed MIR production failed"
    }
[[ -s "$MIR" ]] || fail "installed producer emitted no MIR"
mir_hash="$(sha256sum "$MIR" | awk '{print $1}')"
printf '10\n4\n5\n7\n' >"$WORK/expected.run"

for backend in c llvm; do
    artifact="$WORK/tagged.$backend"
    binary="$WORK/tagged-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$WORK_REL/tagged.$backend") \
        >"$WORK/$backend.out" 2>"$WORK/$backend.err" || {
            cat "$WORK/$backend.out" "$WORK/$backend.err" >&2
            fail "$backend rejected the installed MIR"
        }
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$binary")
        "${command[@]}" >"$WORK/c.compile.out" 2>"$WORK/c.compile.err" ||
            fail "C artifact did not compile"
    else
        "$CLANG" -x ir "$artifact" -o "$binary" \
            >"$WORK/llvm.compile.out" 2>"$WORK/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$binary" | tr -d '\r' >"$WORK/$backend.run"
    cmp -s "$WORK/expected.run" "$WORK/$backend.run" ||
        fail "$backend runtime output drifted"
done

[[ "$mir_hash" == "$(sha256sum "$MIR" | awk '{print $1}')" ]] ||
    fail "backend projection changed the admitted MIR artifact"
cmp -s "$WORK/c.run" "$WORK/llvm.run" || fail "C/LLVM output diverged"

for mutation in variant-name payload-count payload-type payload-ordinal member-valid-variant future-ssa-use graph-edge ssa-use; do
    mutated="$WORK/$mutation.mir.json"
    python "$MUTATOR" "$MIR" "$mutation" "$mutated"
    for backend in c llvm; do
        output="$WORK/rejected-$mutation.$backend"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/$mutation.mir.json" \
            -o "$WORK_REL/rejected-$mutation.$backend") \
            >"$WORK/rejected-$mutation.$backend.out" \
            2>"$WORK/rejected-$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation artifact"
        grep -Eiq 'enum|scalar|expression|MIR machine-layer' \
            "$WORK/rejected-$mutation.$backend.out" \
            "$WORK/rejected-$mutation.$backend.err" ||
            fail "$backend $mutation escaped the owned rejection path"
    done
done

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$STALE_SOURCE_REL" \
    -o "$STALE_MIR_REL") >"$WORK/stale.producer.out" \
    2>"$WORK/stale.producer.err" || fail "stale-SSA baseline MIR production failed"
[[ -s "$STALE_MIR" ]] || fail "stale-SSA baseline emitted no MIR"
for backend in c llvm; do
    baseline="$WORK/stale.baseline.$backend"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$STALE_MIR_REL" -o "$WORK_REL/stale.baseline.$backend") \
        >"$WORK/stale.baseline.$backend.out" \
        2>"$WORK/stale.baseline.$backend.err" ||
        fail "$backend rejected the valid stale-SSA baseline"
    [[ -s "$baseline" ]] || fail "$backend emitted no stale-SSA baseline"
    binary="$WORK/stale.baseline-$backend.exe"
    if [[ "$backend" == c ]]; then
        "$CC" -x c -std=c11 "$baseline" -I"$ROOT_DIR/src" \
            -I"$ROOT_DIR/src/runtime" -pthread -lm -o "$binary" ||
            fail "C stale-SSA baseline did not compile"
    else
        "$CLANG" -x ir "$baseline" -o "$binary" ||
            fail "LLVM stale-SSA baseline did not compile"
    fi
    "$binary" | tr -d '\r' >"$WORK/stale.baseline.$backend.run"
    grep -Fxq '7' "$WORK/stale.baseline.$backend.run" ||
        fail "$backend stale-SSA baseline runtime drifted"
done
python "$MUTATOR" "$STALE_MIR" stale-ssa-use "$WORK/stale-ssa-use.mir.json"
for backend in c llvm; do
    output="$WORK/rejected-stale-ssa-use.$backend"
    rm -f "$output"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$WORK_REL/stale-ssa-use.mir.json" \
        -o "$WORK_REL/rejected-stale-ssa-use.$backend") \
        >"$WORK/rejected-stale-ssa-use.$backend.out" \
        2>"$WORK/rejected-stale-ssa-use.$backend.err"; then
        fail "$backend accepted stale-ssa-use"
    fi
    [[ ! -e "$output" ]] || fail "$backend published stale-ssa-use artifact"
done

echo "[$LABEL] unchanged MIR C/LLVM 10+4+5+7 and nine artifact-free negatives: PASS"

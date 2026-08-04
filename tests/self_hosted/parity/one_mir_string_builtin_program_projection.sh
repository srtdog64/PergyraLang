#!/usr/bin/env bash
# Nested String builtins through one typed scalar-program GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-string-builtin-program"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_string_builtin_program"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/string_concat_op.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

[[ -x "$DRIVER" ]] || fail "current-source self-host driver is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"

while IFS='|' read -r owner cap; do
    [[ -z "$owner" || "$owner" == \#* ]] && continue
    lines="$(wc -l <"$ROOT_DIR/$owner")"
    [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: $owner=$lines/$cap"
done <"$ROOT_DIR/tests/self_hosted/parity/scalar_program_owner_caps.tsv"

PROGRAM="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_graph_admission_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_owner.pgy"
BUILTIN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_builtin_call_owner.pgy"
BUILTIN_SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_builtin_signature_projection_owner.pgy"
DIRECT_CALL="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_call_expression_admission_owner.pgy"
ABI="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_runtime_abi_fact_owner.pgy"
GRAPH="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
[[ "$(grep -Fc 'DirectMirScalarCfgProgramAppendRoutine(' "$PROGRAM")" -eq 1 ]] ||
    fail "one loop must own every scalar-program routine admission"
[[ "$(grep -Fc 'DirectMirScalarCfgSealGraphPlan(' "$PROGRAM")" -eq 1 ]] ||
    fail "program graph must seal exactly once"
require_text "$GRAPH" 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v23'
reject_text "$GRAPH" 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v18'
require_text "$BUILTIN" 'direct_mir_scalar_program_builtin_signature_projection_owner.pgy'
require_text "$BUILTIN_SIGNATURE" '../semantic/builtin_signature_owner.pgy'
reject_text "$BUILTIN" '../semantic/builtin_signature_owner.pgy'
require_text "$ADMISSION" 'DirectMirScalarProgramCallsComplete('
reject_text "$DIRECT_CALL" 'ToString'
for field in string_compare_id string_concat_id string_to_string_id \
    string_to_upper_id string_to_lower_id; do require_text "$ABI" "$field"; done
for owner in "$ADMISSION" "$BUILTIN" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_expression_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_expression_owner.pgy"; do
    for term in string_concat_op.pgy foobar 'Hello, Pergyra!' 'foo/bar/end'; do
        reject_text "$owner" "$term"
    done
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/producer.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
mir_sha="$(sha256sum "$WORK_DIR/producer.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$mir_sha" == "994A28363848AD3F60504BFA95B71C25D02842CACBC0C88CC89342AB9B3A1DF7" ]] ||
    fail "source MIR identity changed: $mir_sha"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_builtin_program_mutations.py" \
    "$WORK_DIR/producer.json" "$WORK_DIR"

project() {
    local input="$1" stem="$2" target="$3" suffix="$4" code=0
    rm -f "$WORK_DIR/$stem.$suffix"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") \
        >"$WORK_DIR/$stem.$target.out" 2>"$WORK_DIR/$stem.$target.err" || code=$?
    [[ "$code" -ne 0 ]] && return "$code"
    ! grep -Fq 'CODEGEN ERROR' "$WORK_DIR/$stem.$target.out" \
        "$WORK_DIR/$stem.$target.err"
}

goods=(program display-only semantic-change builtin-semantic-change)
bads=(bad-to-string-syntax bad-call-marker-edge bad-call-argument-edge \
    bad-to-string-argument-type bad-to-upper-argument-type \
    bad-unregistered-builtin bad-duplicate-call-consumption)
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in "${goods[@]}"; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || fail "$target rejected $good"
    done
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" ||
        fail "$target display text changed the artifact"
    for changed in semantic-change builtin-semantic-change; do
        ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/$changed.$suffix" ||
            fail "$target $changed did not change the artifact"
    done
    for bad in "${bads[@]}"; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Fq -- 'direct MIR scalar CFG program' "$WORK_DIR/$bad.$target.out" \
            "$WORK_DIR/$bad.$target.err" || fail "$target $bad escaped scalar owner"
        ! grep -Fq -- 'condition fact is invalid' "$WORK_DIR/$bad.$target.out" \
            "$WORK_DIR/$bad.$target.err" || fail "$target $bad retried generic CFG"
    done
done

for symbol in pgy_concat pgy_tostr pgy_toupper pgy_tolower; do
    require_text "$WORK_DIR/base.c" "$symbol"
    require_text "$WORK_DIR/base.ll" "$symbol"
done
expected_base=$'foobar\nxyz\nHello, Pergyra!\nfoo/bar/end\nn=7\nABcd'
expected_semantic=$'zapbar\nxyz\nHello, Pergyra!\nzap/bar/end\nn=7\nABcd'
expected_builtin=$'foobar\nxyz\nHello, Pergyra!\nfoo/bar/end\nn=8\nAZxy'
for stem in base display-only semantic-change builtin-semantic-change; do
    "$CC" -std=c11 "$WORK_DIR/$stem.c" -o "$WORK_DIR/$stem.c.exe" || fail "C compile failed: $stem"
    "$CLANG" "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem.llvm.exe" || fail "LLVM compile failed: $stem"
    expected="$expected_base"
    [[ "$stem" == semantic-change ]] && expected="$expected_semantic"
    [[ "$stem" == builtin-semantic-change ]] && expected="$expected_builtin"
    c_out="$("$WORK_DIR/$stem.c.exe" | sed 's/\r$//')" || fail "C execution failed: $stem"
    llvm_out="$("$WORK_DIR/$stem.llvm.exe" | sed 's/\r$//')" || fail "LLVM execution failed: $stem"
    [[ "$c_out" == "$expected" ]] || fail "C stdout changed: $stem=$c_out"
    [[ "$llvm_out" == "$expected" ]] || fail "LLVM stdout changed: $stem=$llvm_out"
done
final_sha="$(sha256sum "$WORK_DIR/producer.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$final_sha" == "$mir_sha" ]] || fail "projection mutated admitted MIR"
echo "[$LABEL] ok: nested String builtins execute from one typed GraphPlan in C and LLVM"

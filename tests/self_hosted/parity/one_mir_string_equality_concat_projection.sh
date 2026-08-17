#!/usr/bin/env bash
# Single-routine String concat/equality through the shared scalar GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-string-equality-concat"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_string_equality_concat"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/string_equality_concat.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

[[ -x "$DRIVER" ]] || fail "current-source self-host driver is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"

while IFS='|' read -r owner cap; do
    cap="${cap%$'\r'}"
    [[ -z "$owner" || "$owner" == \#* ]] && continue
    [[ -f "$ROOT_DIR/$owner" ]] || fail "missing owner: $owner"
    lines="$(wc -l <"$ROOT_DIR/$owner")"
    [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: $owner=$lines/$cap"
done <"$ROOT_DIR/tests/self_hosted/parity/scalar_program_owner_caps.tsv"

PROGRAM="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_graph_admission_owner.pgy"
ROUTE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_single_string_route_owner.pgy"
C_EMIT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_emission_owner.pgy"
LLVM_EMIT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_emission_owner.pgy"
GRAPH="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
[[ "$(grep -Fc 'DirectMirScalarCfgProgramAppendRoutine(' "$PROGRAM")" -eq 1 ]] ||
    fail "one loop must own every scalar-program routine admission"
[[ "$(grep -Fc 'DirectMirScalarCfgSealGraphPlan(' "$PROGRAM")" -eq 1 ]] ||
    fail "program graph must seal exactly once"
require_text "$GRAPH" 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v78'
reject_text "$GRAPH" 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v16'
for term in string_equality_concat.pgy routine_block_counts 'let block_count:'; do
    reject_text "$ROUTE" "$term"
done
for owner in "$C_EMIT" "$LLVM_EMIT"; do
    for term in admitted source_json JsonObjectFactTable BuildMir FromAdmitted \
        string_equality_concat.pgy; do reject_text "$owner" "$term"; done
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
mir_sha="$(sha256sum "$WORK_DIR/program.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$mir_sha" == "85E6A08A02F7C6DB568455793D7EF777847C17C9A56366782DFB38B6D8014538" ]] ||
    fail "source MIR identity changed: $mir_sha"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_equality_concat_mutations.py" \
    "$WORK_DIR/program.json" "$WORK_DIR"

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

goods=(program display-only semantic-fail)
bads=(bad-first-concat-child bad-second-concat-kind \
    bad-equality-literal-kind bad-equality-root-child bad-use-identity \
    bad-leaf-identity)
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in "${goods[@]}"; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || fail "$target rejected $good"
    done
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" ||
        fail "$target display text changed the artifact"
    ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/semantic-fail.$suffix" ||
        fail "$target semantic String change did not change the artifact"
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

require_text "$WORK_DIR/base.c" 'pgy_concat('
require_text "$WORK_DIR/base.c" 'strcmp('
require_text "$WORK_DIR/base.ll" 'define internal ptr @pgy_concat'
require_text "$WORK_DIR/base.ll" 'call i32 @strcmp'
for stem in base display-only semantic-fail; do
    "$CC" -std=c11 "$WORK_DIR/$stem.c" -o "$WORK_DIR/$stem.c.exe" || fail "C compile failed: $stem"
    "$CLANG" "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem.llvm.exe" || fail "LLVM compile failed: $stem"
    expected="concat_eq_ok"; [[ "$stem" == semantic-fail ]] && expected="concat_eq_fail"
    c_out="$("$WORK_DIR/$stem.c.exe" | tr -d '\r')" || fail "C execution failed: $stem"
    llvm_out="$("$WORK_DIR/$stem.llvm.exe" | tr -d '\r')" || fail "LLVM execution failed: $stem"
    [[ "$c_out" == "$expected" ]] || fail "C stdout changed: $stem=$c_out"
    [[ "$llvm_out" == "$expected" ]] || fail "LLVM stdout changed: $stem=$llvm_out"
done
final_sha="$(sha256sum "$WORK_DIR/program.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$final_sha" == "$mir_sha" ]] || fail "projection mutated admitted MIR"
echo "[$LABEL] ok: one GraphPlan executes typed String concat/equality in C and LLVM"

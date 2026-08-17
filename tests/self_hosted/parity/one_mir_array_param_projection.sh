#!/usr/bin/env bash
# Generic CollectionPlan carriage across return, call result, and parameter.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-array-param"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_array_param"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/array_param.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

[[ -x "$DRIVER" ]] || fail "current-source self-host driver is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"

while IFS='|' read -r owner cap; do
    [[ -f "$ROOT_DIR/$owner" ]] || fail "missing owner: $owner"
    lines="$(wc -l <"$ROOT_DIR/$owner")"
    [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: $owner=$lines/$cap"
done <<'EOF'
src/self_hosted/compiler/direct_mir_collection_local_context_fact_owner.pgy|100
src/self_hosted/compiler/direct_mir_collection_program_route_fact_owner.pgy|100
src/self_hosted/compiler/direct_mir_collection_program_identity_owner.pgy|130
src/self_hosted/compiler/direct_mir_collection_program_graph_fact_owner.pgy|185
src/self_hosted/compiler/direct_mir_collection_program_routine_fact_owner.pgy|190
src/self_hosted/compiler/direct_mir_collection_program_producer_admission_owner.pgy|210
src/self_hosted/compiler/direct_mir_collection_program_consumer_admission_owner.pgy|230
src/self_hosted/compiler/direct_mir_collection_program_entrypoint_admission_owner.pgy|135
src/self_hosted/compiler/direct_mir_collection_program_local_plan_owner.pgy|250
src/self_hosted/compiler/direct_mir_collection_program_local_join_owner.pgy|90
src/self_hosted/compiler/direct_mir_collection_program_edge_fact_owner.pgy|135
src/self_hosted/compiler/direct_mir_collection_program_plan_owner.pgy|220
src/self_hosted/compiler/direct_mir_collection_program_c_emission_owner.pgy|140
src/self_hosted/compiler/direct_mir_collection_program_llvm_emission_owner.pgy|180
src/self_hosted/compiler/direct_mir_collection_program_projection_owner.pgy|45
src/self_hosted/compiler/direct_mir_multi_routine_terminal_projection_owner.pgy|35
EOF

ROUTE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy"
ROUTE_FACT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_collection_program_route_fact_owner.pgy"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_collection_program_plan_owner.pgy"
C_EMIT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_collection_program_c_emission_owner.pgy"
LLVM_EMIT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_collection_program_llvm_emission_owner.pgy"
ABI="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_int_abi_projection_owner.pgy"
require_text "$PLAN" 'DirectMirScalarCfgCollectionPlan'
require_text "$PLAN" 'DirectMirCollectionProgramPlanMutationRejected'
require_text "$PLAN" 'producer_reallocating_array_value_main_single_cleanup'
require_text "$ROUTE_FACT" 'let claimed: Bool = routine_count == 3 && entrypoints == 1 &&'
require_text "$ROUTE_FACT" 'producers == 1 && consumers == 1;'
require_text "$ABI" 'StringRuntimeCLongPrintArgumentType()'
route_line="$(grep -n 'if collection_route.claimed' "$ROUTE" | cut -d: -f1)"
scalar_line="$(grep -n 'scalar-route:start' "$ROUTE" | cut -d: -f1)"
[[ "$route_line" -lt "$scalar_line" ]] || fail "collection route follows broad scalar admission"
for owner in "$C_EMIT" "$LLVM_EMIT"; do
    for term in admitted source_json JsonObjectFactTable BuildMir FromAdmitted \
        array_param.pgy DirectMirArrayArgument; do
        reject_text "$owner" "$term"
    done
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
mir_sha="$(sha256sum "$WORK_DIR/program.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$mir_sha" == "C473FF46F33690E8C47231F959C9DE45A2856E1B7B9FA5D60F58D561926A968C" ]] || \
    fail "source MIR identity changed: $mir_sha"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_param_mutations.py" \
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

goods=(program alternate display-only routine-order-cycle raw-value-collision)
bads=(bad-repaired-param-abi bad-call-target bad-return-use \
    bad-cross-routine-raw-collision)
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in "${goods[@]}"; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || fail "$target rejected $good"
    done
    for equal in display-only routine-order-cycle raw-value-collision; do
        cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/$equal.$suffix" || \
            fail "$target $equal changed the artifact"
    done
    ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/alternate.$suffix" || \
        fail "$target ignored the changed producer argument"
    for bad in "${bads[@]}"; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Fq 'direct MIR collection' "$WORK_DIR/$bad.$target.out" \
            "$WORK_DIR/$bad.$target.err" || fail "$target lost $bad diagnostic"
        ! grep -Fq 'direct MIR Array argument' "$WORK_DIR/$bad.$target.out" \
            "$WORK_DIR/$bad.$target.err" || fail "$target retried $bad through legacy"
    done
done

compile_run() {
    local stem="$1" expected="$2"
    "$CC" -std=c11 -Wall -Wextra -Werror \
        "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}" "$WORK_DIR/$stem.c" \
        -o "$WORK_DIR/$stem-c.exe" >"$WORK_DIR/$stem-c.compile" 2>&1 || \
        fail "$stem C did not compile"
    "$CLANG" -x ir "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem-llvm.exe" \
        >"$WORK_DIR/$stem-llvm.compile" 2>&1 || fail "$stem LLVM did not compile"
    "$WORK_DIR/$stem-c.exe" | tr -d '\r' >"$WORK_DIR/$stem-c.run"
    "$WORK_DIR/$stem-llvm.exe" | tr -d '\r' >"$WORK_DIR/$stem-llvm.run"
    cmp -s "$WORK_DIR/$stem-c.run" "$WORK_DIR/$stem-llvm.run" || \
        fail "$stem C/LLVM stdout differs"
    printf '%b' "$expected" >"$WORK_DIR/$stem.expected"
    cmp -s "$WORK_DIR/$stem-c.run" "$WORK_DIR/$stem.expected" || \
        fail "$stem stdout differs from expectation"
}

compile_run base '12\n4\n'
compile_run alternate '20\n5\n'
grep -Fq 'realloc(' "$WORK_DIR/base.c" || fail "C lost dynamic growth"
[[ "$(grep -Fc 'free(value.data)' "$WORK_DIR/base.c")" -eq 1 ]] || fail "C cleanup is not single-owner"
grep -Fq '@realloc' "$WORK_DIR/base.ll" || fail "LLVM lost dynamic growth"
[[ "$(grep -Fc 'call void @free(ptr %data)' "$WORK_DIR/base.ll")" -eq 1 ]] || \
    fail "LLVM cleanup is not single-owner"

echo "[$LABEL] collection return/parameter projection ok"

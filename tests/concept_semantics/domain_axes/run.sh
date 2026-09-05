#!/usr/bin/env bash
# Bounded concept substitutions and their current admission contracts.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here domain-axes "$PGY"
pgy_require_runnable_binary_here domain-axes-self-mir "$DRIVER"
SCRATCH="$ROOT_DIR/.tmp/self_hosted/concept_semantics_20260905/domain_axes"
mkdir -p "$SCRATCH"
RUN_DIR="$(mktemp -d "$SCRATCH/run.XXXXXX")"
FIXTURES="$ROOT_DIR/tests/concept_semantics/domain_axes"

fail() { echo "[domain-axes] $* (evidence: $RUN_DIR)" >&2; exit 1; }

native_runs() {
    local case_name="$1" expected="$2" backend="$3"
    local source="${4:-$FIXTURES/$case_name.pgy}"
    local exe="$RUN_DIR/${case_name}_${backend}.exe"
    (cd "$ROOT_DIR" && "$PGY" "$source" \
        --native-pipeline --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$exe")") \
        >"$exe.compile.log" 2>&1 || fail "$case_name/$backend compile"
    "$exe" >"$exe.out" 2>"$exe.err" || fail "$case_name/$backend execution"
    [[ ! -s "$exe.err" ]] || fail "$case_name/$backend unexpected stderr"
    [[ "$(tr -d '\r' <"$exe.out")" == "$expected" ]] || fail "$case_name/$backend output"
}

native_rejects() {
    local name="$1" source="$2" diagnostic="$3" backend="$4"
    local exe="$RUN_DIR/${name}_${backend}.exe"
    if (cd "$ROOT_DIR" && "$PGY" "$source" --native-pipeline \
        --backend="$backend" -o "$(pgy_path_for_compiler "$PGY" "$exe")") \
        >"$exe.compile.log" 2>&1; then
        fail "$name/$backend should reject"
    fi
    grep -Fq "$diagnostic" "$exe.compile.log" || fail "$name/$backend diagnostic"
    [[ ! -e "$exe" ]] || fail "$name/$backend published an executable"
}

for backend in c llvm; do
    native_runs action_state $'2\n2' "$backend"
    native_runs function_state $'2\n2' "$backend"
    native_runs action_zone_contract 2 "$backend"
    native_runs role_dispatch $'10\n77' "$backend"
    native_runs function_dispatch $'10\n77' "$backend"
    native_runs where_explicit true "$backend"
    native_runs where_inferred true "$backend"
    native_rejects function_zone_contract "$FIXTURES/function_zone_contract_rejected.pgy" \
        "'within' clause is only valid on 'action' declarations" "$backend"
    native_rejects where_conflict "$FIXTURES/where_conflict_rejected.pgy" \
        "using binding must match zone type 'OtherZone'" "$backend"
    native_rejects generic_ability_bound \
        "$ROOT_DIR/tests/cases/generic_falsification/f_where_ability_bad.pgy" \
        "does not satisfy constraint 'Sortable'" "$backend"
    native_rejects world_zone_escape \
        "$ROOT_DIR/tests/cases/axis_composition/comp_world_intent/cross.pgy" \
        'cannot escape as a live binding' "$backend"
done

# These controls establish semantic admission. Generic subject calls have a
# separate observed LLVM specialization gap; this runner makes no execution
# parity claim for that existing fixture.
for control in \
    tests/cases/generic_falsification/f_where_ability_ok.pgy \
    tests/cases/axis_composition/comp_world_intent/control.pgy; do
    name="$(basename "$control" .pgy)"
    (cd "$ROOT_DIR" && "$PGY" --native-pipeline --mir-json "$control") \
        >"$RUN_DIR/$name.native.mir.json" 2>"$RUN_DIR/$name.native.err" \
        || fail "$name native semantic control"
    grep -Fq '"schema":"pgy.mir.v1"' "$RUN_DIR/$name.native.mir.json" \
        || fail "$name native MIR missing"
done

for case_name in action_state function_state action_zone_contract role_dispatch \
    function_dispatch where_explicit where_inferred; do
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "tests/concept_semantics/domain_axes/$case_name.pgy") \
        >"$RUN_DIR/$case_name.mir.json" 2>"$RUN_DIR/$case_name.mir.err" \
        || fail "$case_name self-host MIR production"
    "${PYTHON_BIN:-python3}" "$FIXTURES/verify_mir.py" "$case_name" \
        "$RUN_DIR/$case_name.mir.json"
done
for case_name in function_zone_contract_rejected where_conflict_rejected; do
    if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "tests/concept_semantics/domain_axes/$case_name.pgy") \
        >"$RUN_DIR/$case_name.mir.out" 2>"$RUN_DIR/$case_name.mir.err"; then
        fail "$case_name self-host MIR should reject"
    fi
    if grep -Fq '"schema":"pgy.mir.v1"' "$RUN_DIR/$case_name.mir.out"; then
        fail "$case_name published MIR after rejection"
    fi
done
echo "[domain-axes] PASS: 3 substitution pairs, native C/LLVM execution and rejection, supported self-host MIR facts"
echo "[domain-axes] evidence: $RUN_DIR"

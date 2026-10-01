#!/usr/bin/env bash
# One admitted owner owns ArrayLength, indexed reads, and bounded String ArraySet.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-string-array-mutation"
DRIVER="$(pgy_select_optional_exe_binary "$(pgy_path_for_bash_tool "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")")"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

pgy_require_runnable_binary_here "$LABEL" "$DRIVER"
[[ -n "$PYTHON_BIN" ]] || fail "python is required for structured falsifiers"

PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_fact_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_admission_owner.pgy"
DOMINANCE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_dominance_owner.pgy"
GUARD="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_array_guard_dominance_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_c_emission_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_emission_owner.pgy"
require_text "$PLAN" 'DirectMirScalarCfgStringArrayAccessSetKind()'
require_text "$ADMISSION" 'DirectMirScalarCfgStringArrayWhileLengthGraphFactFrom(graph)'
require_text "$ADMISSION" 'DirectMirScalarCfgStringArrayAccessSetKind()'
require_text "$DOMINANCE" 'DirectMirScalarCfgArrayGuardEdgeReady('
require_text "$GUARD" 'MirRoutineBlockDominates('
require_text "$GUARD" 'predecessor_count == 1'
require_text "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_graph_readiness_owner.pgy" \
    'fact.index.value_row >= ArrayLength(plan.value_types)'
require_text "$C_OWNER" '.length'
require_text "$LLVM_OWNER" 'icmp ult i64'
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    reject_text "$owner" 'source_json'
    reject_text "$owner" 'JsonObjectFactTable'
    reject_text "$owner" 'MirExpressionGraphSequence'
    reject_text "$owner" 'str_array.pgy'
done
reject_text "$C_OWNER" '.capacity)'
reject_text "$LLVM_OWNER" '.capacity = extractvalue'

WORK_BASE="$ROOT_DIR/.tmp/self_hosted"
mkdir -p "$WORK_BASE"
WORK_BASE="$(cd "$WORK_BASE" && pwd -P)"
WORK_DIR="$(mktemp -d "$WORK_BASE/one-mir-string-array-mutation.XXXXXX")"
WORK_DIR="$(cd "$WORK_DIR" && pwd -P)"
[[ "$WORK_DIR" == "$WORK_BASE"/one-mir-string-array-mutation.* ]] || fail "evidence path escaped its owner"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"
[[ "$WORK_REL" != "$WORK_DIR" ]] || fail "evidence path is not below the repository"
echo "[$LABEL] evidence=$WORK_DIR driver=$DRIVER"
sha256sum "$DRIVER" >"$WORK_DIR/driver.before.sha256"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified src/self_hosted/codegen/fixture/str_array.pgy \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2 || true
        fail "installed producer rejected the active source"
    }
grep -Fq '"uses":["i.3","names.1"]' "$WORK_DIR/program.json" || fail "while use identity is absent"
grep -Fq '"arg0":"ArraySet"' "$WORK_DIR/program.json" || fail "ArraySet is absent"
PRODUCER_HASH="$(sha256sum "$WORK_DIR/program.json" | cut -d' ' -f1)"
sha256sum "$WORK_DIR/program.json" >"$WORK_DIR/producer.sha256"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_array_mutations.py" \
    "$WORK_DIR/program.json" "$WORK_DIR"

project() {
    local input="$1" stem="$2" target="$3" suffix="$4"
    local input_hash status=0
    [[ -s "$WORK_DIR/$input.json" ]] || fail "$input MIR is absent before projection"
    input_hash="$(sha256sum "$WORK_DIR/$input.json" | cut -d' ' -f1)" || fail "$input MIR hash is unreadable"
    [[ "$input_hash" =~ ^[0-9a-f]{64}$ ]] || fail "$input MIR hash is invalid"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") \
        >"$WORK_DIR/$stem.$target.out" 2>"$WORK_DIR/$stem.$target.err" || status=$?
    [[ "$(sha256sum "$WORK_DIR/$input.json" | cut -d' ' -f1)" == "$input_hash" ]] || \
        fail "$target rewrote $input MIR"
    return "$status"
}

printf 'case\tbackend\tstatus\tprior_output\tinput_unchanged\n' >"$WORK_DIR/refusals.tsv"
refusals=0
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    project program base "$target" "$suffix" || fail "$target rejected String-array plan"
    project display-only display-only "$target" "$suffix" || fail "$target read display text"
    project graph-values graph-values "$target" "$suffix" || fail "$target rejected graph values"
    project empty-set-value empty-set-value "$target" "$suffix" || fail "$target lost empty String value"
    project graph-set-value graph-set-value "$target" "$suffix" || fail "$target rejected graph set value"
    project set-log-reordered set-log-reordered "$target" "$suffix" || fail "$target lost operation order"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" || fail "$target display text changed artifact"
    for bad in bad-branch-index-use bad-branch-collection-use bad-length-target \
        bad-length-edge bad-log-index-use bad-log-index-edge bad-while-init \
        bad-while-step bad-set-receiver \
        bad-set-index-oob bad-set-index-negative bad-set-value-kind \
        bad-post-read-oob bad-guard-edge bad-guard-bypass bad-array-layout \
        stale-collection; do
        printf 'prior-output:%s:%s\n' "$bad" "$target" >"$WORK_DIR/$bad.$suffix"
        cp "$WORK_DIR/$bad.$suffix" "$WORK_DIR/$bad.$target.sentinel"
        status=0
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        else
            status=$?
        fi
        cmp -s "$WORK_DIR/$bad.$suffix" "$WORK_DIR/$bad.$target.sentinel" || \
            fail "$target replaced the prior output for $bad"
        grep -Eq '(direct MIR|MIR-LOWER ERROR)' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || \
            fail "$target lost owned diagnostic for $bad"
        ! grep -Fq 'direct MIR Option match' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || \
            fail "$target retried $bad as Option"
        printf '%s\t%s\t%s\tpreserved\tyes\n' "$bad" "$target" "$status" >>"$WORK_DIR/refusals.tsv"
        ((refusals+=1))
    done
done

# Function-scoped checks cannot borrow a cleanup/setter guard as a read proof.
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_array_emitted_contract.py" \
    "$WORK_DIR" >"$WORK_DIR/emitted-contract.log" 2>&1 || {
        cat "$WORK_DIR/emitted-contract.log" >&2
        fail "general-route emission contract is invalid"
    }
source "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_array_execution_contract.sh"

[[ "$refusals" == 34 ]] || fail "existing refusal matrix is incomplete"
[[ "$(sha256sum "$WORK_DIR/program.json" | cut -d' ' -f1)" == "$PRODUCER_HASH" ]] || fail "producer MIR changed"
sha256sum "$DRIVER" >"$WORK_DIR/driver.after.sha256"
cmp -s "$WORK_DIR/driver.before.sha256" "$WORK_DIR/driver.after.sha256" || fail "driver changed during the gate"
echo "[$LABEL] general C/LLVM while-read-static-set parity; 34 preserved-output refusals; immutable producer MIR"

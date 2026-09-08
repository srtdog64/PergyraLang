#!/usr/bin/env bash
# The bootstrap codegen must use graph-owned argument types independently of
# the callee's result shape. Invalid sources stop before C publication.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/codegen_bootstrap_compile_leg.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:?native comparator is required}"
CODEGEN="${PGY_CODEGEN_BIN:?bootstrap codegen is required}"
PARSER="${PGY_PARSER_BIN:?bootstrap parser is required}"
CC="${PGY_SELFHOST_CC:-gcc}"
WORK_BASE="$ROOT_DIR/.tmp/self_hosted/codegen_call_argument_graph"
mkdir -p "$WORK_BASE"
B="$(mktemp -d "$WORK_BASE/run.XXXXXX")"
REL="${B#"$ROOT_DIR"/}"
FIXTURE="tests/self_hosted/parity/fixture/call_argument_graph"
fail() { echo "[codegen-call-argument-graph] $* ($B)" >&2; exit 1; }
cd "$ROOT_DIR"

# A dotted receiver must not reopen whole-spelling local/type lookup. Both
# consumers reuse the existing graph and retain missing-binding refusal.
! grep -Fq 'let target_type: String = EnvLookup(names, types, target);' \
    src/self_hosted/semantic/ast_statement_type_fact_owner.pgy ||
    fail "collection receiver reopened name-only admission"
! grep -Eq 'LookupKindType\(env, [apq]_arr, "v"\)' \
    src/self_hosted/codegen/emission/stmt_emit.pgy ||
    fail "collection receiver reopened name-only C type lookup"

for case_name in nominal_return_valid nominal_return_bad scalar_return_bad collection_field_bad readonly_escape_bad readonly_field_bad readonly_call_bad; do
    source_rel="${FIXTURE}_${case_name}.pgy"
    "$PARSER" "$source_rel" >"$B/$case_name.ast" 2>"$B/$case_name.parse.err" ||
        fail "parser rejected $case_name"
    if [[ "$case_name" == nominal_return_valid ]]; then
        if ! "$CODEGEN" "$REL/$case_name.ast" >"$B/codegen.c" 2>"$B/codegen.err"; then
            sed -n '1,80p' "$B/codegen.c" "$B/codegen.err" >&2
            fail "codegen rejected the valid nominal argument/result control"
        fi
        "$PGY" --native-pipeline --emit-c "$source_rel" -o "$B/native.c" \
            >"$B/native.out" 2>"$B/native.err" || fail "native rejected the valid control"
        printf 'result=Array<Int>Int;\n7\n2\n1\n' >"$B/expected.txt"
        for producer in native codegen; do
            compile_c_artifact_with_bounded_log "$producer" "$B/$producer.c" \
                "$B/$producer.exe" || fail "$producer C did not compile"
            "$B/$producer.exe" >"$B/$producer.actual" || fail "$producer execution failed"
            tr -d '\r' <"$B/$producer.actual" >"$B/$producer.normalized"
            cmp "$B/expected.txt" "$B/$producer.normalized" || fail "$producer value drifted"
        done
    else
        if "$CODEGEN" "$REL/$case_name.ast" \
            >"$B/$case_name.out" 2>"$B/$case_name.err"; then
            fail "codegen accepted $case_name"
        fi
        diagnostic=call_arg_type_mismatch
        case "$case_name" in
            readonly_escape_bad) diagnostic=borrow_boundary_escape ;;
            readonly_field_bad|readonly_call_bad) diagnostic=value_param_collection_mutation ;;
        esac
        grep -Fq "Code: $diagnostic" "$B/$case_name.out" "$B/$case_name.err" ||
            fail "$case_name lost its $diagnostic diagnosis"
        ! grep -Eq '^#include|^int main' "$B/$case_name.out" ||
            fail "$case_name published C before refusal"
        if "$PGY" --native-pipeline --emit-c "$source_rel" -o "$B/$case_name.c" \
            >"$B/$case_name.native.out" 2>"$B/$case_name.native.err"; then
            fail "native accepted $case_name"
        fi
        [[ ! -e "$B/$case_name.c" ]] || fail "native published invalid C"
    fi
done
echo "[codegen-call-argument-graph] 2 executions and 12 pre-emission refusals: PASS ($B)"

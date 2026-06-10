#!/usr/bin/env bash
# Rung 1 parity for the minimal Pergyra-origin parser (2026-05-28).
# Each source pair is committed under fixture/<source>.pgy +
# fixture/<source>_ast.txt. The Pergyra binary reads `fixture/source.txt`
# (one-line repo-relative path) to pick which source to parse.
# See src/self_hosted/parity/README.md.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:parser] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:parser] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/parser/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/parser}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
FIXTURE_DIR="$ROOT_DIR/src/self_hosted/parser/fixture"
SOURCE_OVERRIDE="$FIXTURE_DIR/source.txt"
EXPECTED_FILE="$ROOT_DIR/src/self_hosted/parser/expected/clean.txt"

if [[ ! -f "$PERGYRA_TOOL_SOURCE" ]]; then
    echo "[self-host-parity:parser] missing Pergyra tool: $PERGYRA_TOOL_SOURCE" >&2
    exit 1
fi
if [[ ! -f "$EXPECTED_FILE" ]]; then
    echo "[self-host-parity:parser] missing expected: $EXPECTED_FILE" >&2
    exit 1
fi

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"

echo "[self-host-parity:parser] compiling parser..."
(cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")" -o "$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_BUILD_DIR/main.exe")" >/dev/null)

# Sources: each pair is "<source.pgy path relative to repo root>:<fixture base>"
# where fixture base resolves to fixture/<base>_ast.txt.
SOURCE_PAIRS=(
    "examples/hello.pgy:hello"
    "src/self_hosted/parser/fixture/multi_log.pgy:multi_log"
    "src/self_hosted/parser/fixture/with_param.pgy:with_param"
    "src/self_hosted/parser/fixture/no_arg.pgy:no_arg"
    "src/self_hosted/parser/fixture/with_let.pgy:with_let"
    "src/self_hosted/parser/fixture/let_mixed.pgy:let_mixed"
    "src/self_hosted/parser/fixture/multi_func.pgy:multi_func"
    "src/self_hosted/parser/fixture/with_return.pgy:with_return"
    "src/self_hosted/parser/fixture/return_void.pgy:return_void"
    "src/self_hosted/parser/fixture/arith_let.pgy:arith_let"
    "src/self_hosted/parser/fixture/arith_complex.pgy:arith_complex"
    "src/self_hosted/parser/fixture/arith_parens.pgy:arith_parens"
    "src/self_hosted/parser/fixture/arith_return.pgy:arith_return"
    "src/self_hosted/parser/fixture/cmp.pgy:cmp"
    "src/self_hosted/parser/fixture/if_stmt.pgy:if_stmt"
    "src/self_hosted/parser/fixture/if_else.pgy:if_else"
    "src/self_hosted/parser/fixture/while_stmt.pgy:while_stmt"
    "src/self_hosted/parser/fixture/if_nested.pgy:if_nested"
    "src/self_hosted/parser/fixture/logic.pgy:logic"
    "src/self_hosted/parser/fixture/for_loop.pgy:for_loop"
    "src/self_hosted/parser/fixture/for_arith.pgy:for_arith"
    "src/self_hosted/parser/fixture/call_expr.pgy:call_expr"
    "src/self_hosted/parser/fixture/call_args_arith.pgy:call_args_arith"
    "src/self_hosted/parser/fixture/not.pgy:not"
    "src/self_hosted/parser/fixture/assign.pgy:assign"
    "src/self_hosted/parser/fixture/export_func.pgy:export_func"
    "src/self_hosted/parser/fixture/subject_decl.pgy:subject_decl"
    "src/self_hosted/parser/fixture/subject_empty.pgy:subject_empty"
    "src/self_hosted/parser/fixture/class_decl.pgy:class_decl"
    "src/self_hosted/parser/fixture/enum_decl.pgy:enum_decl"
    "src/self_hosted/parser/fixture/enum_single.pgy:enum_single"
    "src/self_hosted/parser/fixture/namespace_decl.pgy:namespace_decl"
    "src/self_hosted/parser/fixture/namespace_subject.pgy:namespace_subject"
    "src/self_hosted/parser/fixture/namespace_nested.pgy:namespace_nested"
    "src/self_hosted/parser/fixture/subject_method.pgy:subject_method"
    "src/self_hosted/parser/fixture/class_method.pgy:class_method"
    "src/self_hosted/parser/fixture/class_only_method.pgy:class_only_method"
    "src/self_hosted/parser/fixture/class_method_param.pgy:class_method_param"
    "src/self_hosted/parser/fixture/break_continue.pgy:break_continue"
    "src/self_hosted/parser/fixture/match_case.pgy:match_case"
    "src/self_hosted/parser/fixture/match_expr.pgy:match_expr"
    "src/self_hosted/parser/fixture/unary_neg.pgy:unary_neg"
    "src/self_hosted/parser/fixture/arr_literal.pgy:arr_literal"
    "src/self_hosted/parser/fixture/generic_type.pgy:generic_type"
    "src/self_hosted/parser/fixture/let_inferred.pgy:let_inferred"
    "src/self_hosted/parser/fixture/member_access.pgy:member_access"
    "src/self_hosted/parser/fixture/toplevel_stmt.pgy:toplevel_stmt"
    "src/self_hosted/parser/fixture/vessel_decl.pgy:vessel_decl"
    "src/self_hosted/parser/fixture/struct_decl.pgy:struct_decl"
    "src/self_hosted/parser/fixture/mod_op.pgy:mod_op"
    "src/self_hosted/parser/fixture/else_if.pgy:else_if"
    "src/self_hosted/parser/fixture/for_in_arr.pgy:for_in_arr"
    "src/self_hosted/parser/fixture/member_assign.pgy:member_assign"
    "src/self_hosted/parser/fixture/event_decl.pgy:event_decl"
    "src/self_hosted/parser/fixture/defer_stmt.pgy:defer_stmt"
    "src/self_hosted/parser/fixture/turbofish.pgy:turbofish"
    "src/self_hosted/parser/fixture/func_generic_ret.pgy:func_generic_ret"
    "src/self_hosted/parser/fixture/event_subscribe.pgy:event_subscribe"
    "src/self_hosted/parser/fixture/event_unsubscribe.pgy:event_unsubscribe"
    "src/self_hosted/parser/fixture/generic_func.pgy:generic_func"
    "src/self_hosted/parser/fixture/lambda_simple.pgy:lambda_simple"
    "src/self_hosted/parser/fixture/self_param.pgy:self_param"
    "src/self_hosted/parser/fixture/action_method.pgy:action_method"
    "src/self_hosted/parser/fixture/ability_decl.pgy:ability_decl"
    "src/self_hosted/parser/fixture/role_impl.pgy:role_impl"
    "src/self_hosted/parser/fixture/async_func.pgy:async_func"
    "src/self_hosted/parser/fixture/channel_ops.pgy:channel_ops"
    "src/self_hosted/parser/fixture/spawn_await.pgy:spawn_await"
    "src/self_hosted/parser/fixture/zone_decl.pgy:zone_decl"
    "src/self_hosted/parser/fixture/parallel_stmt.pgy:parallel_stmt"
    "src/self_hosted/parser/fixture/object_decl.pgy:object_decl"
    "src/self_hosted/parser/fixture/generic_class.pgy:generic_class"
    "src/self_hosted/parser/fixture/vessel_field.pgy:vessel_field"
    "src/self_hosted/parser/fixture/pipe_op.pgy:pipe_op"
    "src/self_hosted/parser/fixture/try_op.pgy:try_op"
    "src/self_hosted/parser/fixture/destructure_let.pgy:destructure_let"
    "src/self_hosted/parser/fixture/with_slot.pgy:with_slot"
    "src/self_hosted/parser/fixture/tobject_decl.pgy:tobject_decl"
    "src/self_hosted/parser/fixture/spawn_blocking.pgy:spawn_blocking"
    "src/self_hosted/parser/fixture/import_simple.pgy:import_simple"
    "src/self_hosted/parser/fixture/walrus_op.pgy:walrus_op"
    "src/self_hosted/parser/fixture/enum_data.pgy:enum_data"
    "src/self_hosted/parser/fixture/intent_basic.pgy:intent_basic"
    "src/self_hosted/parser/fixture/option_test.pgy:option_test"
    "src/self_hosted/parser/fixture/array_literal.pgy:array_literal"
    "src/self_hosted/parser/fixture/slot_sugar.pgy:slot_sugar"
    "src/self_hosted/parser/fixture/transfer_move_minimal.pgy:transfer_move_minimal"
    "src/self_hosted/parser/fixture/result_test.pgy:result_test"
    "src/self_hosted/parser/fixture/queue.pgy:queue"
    "src/self_hosted/parser/fixture/stack.pgy:stack"
    "src/self_hosted/parser/fixture/binary_search.pgy:binary_search"
    "src/self_hosted/parser/fixture/fizzbuzz.pgy:fizzbuzz"
    "src/self_hosted/parser/fixture/insertion_sort.pgy:insertion_sort"
    "src/self_hosted/parser/fixture/hash_map.pgy:hash_map"
    "src/self_hosted/parser/fixture/linked_list.pgy:linked_list"
    "src/self_hosted/parser/fixture/word_count.pgy:word_count"
    "src/self_hosted/parser/fixture/math_lib.pgy:math_lib"
    "src/self_hosted/parser/fixture/match_test.pgy:match_test"
    "src/self_hosted/parser/fixture/enum_test.pgy:enum_test"
    "src/self_hosted/parser/fixture/minimal.pgy:minimal"
    "src/self_hosted/parser/fixture/generic_test.pgy:generic_test"
    "src/self_hosted/parser/fixture/basic.pgy:basic"
    "src/self_hosted/parser/fixture/async_demo.pgy:async_demo"
    "src/self_hosted/parser/fixture/string_ops.pgy:string_ops"
    "src/self_hosted/parser/fixture/string_utils.pgy:string_utils"
    "src/self_hosted/parser/fixture/io_test.pgy:io_test"
    "src/self_hosted/parser/fixture/stdlib_test.pgy:stdlib_test"
    "src/self_hosted/parser/fixture/math_builtins.pgy:math_builtins"
    "src/self_hosted/parser/fixture/lambda_test.pgy:lambda_test"
    "src/self_hosted/parser/fixture/deque.pgy:deque"
    "src/self_hosted/parser/fixture/heap.pgy:heap"
    "src/self_hosted/parser/fixture/spawn_test.pgy:spawn_test"
    "src/self_hosted/parser/fixture/select_test.pgy:select_test"
    "src/self_hosted/parser/fixture/defer_test.pgy:defer_test"
    "src/self_hosted/parser/fixture/for_test.pgy:for_test"
    "src/self_hosted/parser/fixture/union_find.pgy:union_find"
    "src/self_hosted/parser/fixture/zone_lifecycle.pgy:zone_lifecycle"
    "src/self_hosted/parser/fixture/tagged_union.pgy:tagged_union"
    "src/self_hosted/parser/fixture/graph_bfs.pgy:graph_bfs"
    "src/self_hosted/parser/fixture/ownership_demo.pgy:ownership_demo"
    "src/self_hosted/parser/fixture/concurrency_demo.pgy:concurrency_demo"
    "src/self_hosted/parser/fixture/event_basic.pgy:event_basic"
    "src/self_hosted/parser/fixture/pipe_and_try.pgy:pipe_and_try"
    "src/self_hosted/parser/fixture/walrus_test.pgy:walrus_test"
    "src/self_hosted/parser/fixture/walrus_test2.pgy:walrus_test2"
    "src/self_hosted/parser/fixture/operator_overload.pgy:operator_overload"
    "src/self_hosted/parser/fixture/notebook_style_analysis.pgy:notebook_style_analysis"
    "src/self_hosted/parser/fixture/battle_minimal.pgy:battle_minimal"
    "src/self_hosted/parser/fixture/transfer_move_typed_minimal.pgy:transfer_move_typed_minimal"
    "src/self_hosted/parser/fixture/transfer_contract_pair_minimal.pgy:transfer_contract_pair_minimal"
    "src/self_hosted/parser/fixture/zone_context_minimal.pgy:zone_context_minimal"
    "src/self_hosted/parser/fixture/class_test.pgy:class_test"
    "src/self_hosted/parser/fixture/class_method_test.pgy:class_method_test"
    "src/self_hosted/parser/fixture/channel_test.pgy:channel_test"
    "src/self_hosted/parser/fixture/spawn_blocking_test.pgy:spawn_blocking_test"
    "src/self_hosted/parser/fixture/qubit_test.pgy:qubit_test"
    "src/self_hosted/parser/fixture/qubit_quantum_ext.pgy:qubit_quantum_ext"
    "src/self_hosted/parser/fixture/remote_future_result.pgy:remote_future_result"
    "src/self_hosted/parser/fixture/for_in_array.pgy:for_in_array"
    "src/self_hosted/parser/fixture/generic_class.pgy:generic_class"
    "src/self_hosted/parser/fixture/subject_object_tobject.pgy:subject_object_tobject"
    "src/self_hosted/parser/fixture/vessel_method_test.pgy:vessel_method_test"
    "src/self_hosted/parser/fixture/test_parallel.pgy:test_parallel"
)

cleanup_source_override() {
    rm -f "$SOURCE_OVERRIDE"
}
trap cleanup_source_override EXIT

ANY_DRIFT_GUARD_RAN="no"

for pair in "${SOURCE_PAIRS[@]}"; do
    src="${pair%%:*}"
    base="${pair##*:}"
    expected_fixture="$FIXTURE_DIR/${base}_ast.txt"

    if [[ ! -f "$ROOT_DIR/$src" ]]; then
        echo "[self-host-parity:parser] missing source: $src" >&2
        exit 1
    fi
    if [[ ! -f "$expected_fixture" ]]; then
        echo "[self-host-parity:parser] missing AST fixture: $expected_fixture" >&2
        exit 1
    fi

    printf '%s' "$src" > "$SOURCE_OVERRIDE"

    PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PERGYRA_TOOL_BUILD_DIR/main.exe" 2>/dev/null \
        | tr -d '\r')"
    P_RC=$?
    set -e

    if [[ "$P_RC" -ne 0 ]]; then
        echo "[self-host-parity:parser] $src: exit-code FAIL (pergyra=$P_RC)" >&2
        printf '%s\n' "$PERGYRA_OUT" >&2
        exit 1
    fi

    EXPECTED_NORM="$(tr -d '\r' < "$expected_fixture")"
    if [[ "$PERGYRA_OUT" != "$EXPECTED_NORM" ]]; then
        echo "[self-host-parity:parser] $src: BYTE-DRIFT vs $expected_fixture" >&2
        diff <(printf '%s\n' "$EXPECTED_NORM") <(printf '%s\n' "$PERGYRA_OUT") | head -30 >&2
        exit 1
    fi

    # Live drift guard for this source pair.
    set +e
    LIVE_OUT="$(cd "$ROOT_DIR" && "$PGY" --ast "$src" 2>/dev/null)"
    LIVE_RC=$?
    set -e
    if [[ "$LIVE_RC" -eq 0 && -n "$LIVE_OUT" ]]; then
        LIVE_NORM="$(printf '%s' "$LIVE_OUT" | tr -d '\r')"
        if [[ "$LIVE_NORM" != "$EXPECTED_NORM" ]]; then
            echo "[self-host-parity:parser] $src: committed AST fixture drifted from live pgy --ast" >&2
            echo "regenerate: pgy --ast $src > $expected_fixture" >&2
            exit 1
        fi
        ANY_DRIFT_GUARD_RAN="yes"
    fi
done

echo "[self-host-parity:parser] rung-1 parity ok (${#SOURCE_PAIRS[@]} sources byte-equal; live-drift=$ANY_DRIFT_GUARD_RAN)"

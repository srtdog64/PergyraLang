#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[self-host-parity:mir-expression-graph-read] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "mir-expression-graph-read" "$PGY" \
    || fail "PGY_BIN is not runnable"

BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/mir_expression_graph_read"
KIND_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_kind_code_owner.pgy"
NODE_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_persisted_node_read_owner.pgy"
SEQUENCE_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_sequence_owner.pgy"
mkdir -p "$BUILD_DIR"

[[ "$(wc -l <"$KIND_OWNER" | tr -d ' ')" -le 100 ]] ||
    fail "persisted graph kind-code owner exceeds 100 lines"
[[ "$(wc -l <"$NODE_OWNER" | tr -d ' ')" -le 300 ]] ||
    fail "persisted graph node owner exceeds 300 lines"
grep -Fq 'func MirExpressionGraphKindCodeWithin(' "$KIND_OWNER" ||
    fail "node kind vocabulary does not consume exact JSON bounds"
grep -Fq 'func MirExpressionCallTargetKindCodeWithin(' "$KIND_OWNER" ||
    fail "call-target vocabulary does not consume exact JSON bounds"
grep -Fq 'func MirExpressionBindingKindCodeWithin(' "$KIND_OWNER" ||
    fail "binding vocabulary does not consume exact JSON bounds"
grep -Fq 'inout string_end_scratch: Array<Int>' "$NODE_OWNER" ||
    fail "node reader omitted reusable string-end scratch"
grep -Fq 'let string_end_scratch: Array<Int> = [0];' "$SEQUENCE_OWNER" ||
    fail "graph append omitted its bounded string-end scratch owner"
grep -Fq 'graph.json, cursor, header.nodes_end, string_end_scratch' \
    "$SEQUENCE_OWNER" || fail "graph append did not pass the owned scratch"
for required in \
    'MirExpressionGraphKindCodeWithin(' \
    'MirExpressionCallTargetKindCodeWithin(' \
    'MirExpressionBindingKindCodeWithin('; do
    grep -Fq "$required" "$NODE_OWNER" ||
        fail "node reader omitted allocation-free vocabulary read: $required"
done
for forbidden in \
    'let kind: String;' 'let target_kind: String;' \
    'let binding_kind: String;' 'MirExpressionGraphKindCode(' \
    'MirExpressionCallTargetKindCode(' \
    'let decoded_end: Array<Int> = [0];'; do
    grep -Fq "$forbidden" "$NODE_OWNER" "$SEQUENCE_OWNER" &&
        fail "persisted graph reintroduced decoded enum String: $forbidden"
done

run_backend() {
    local backend="$1"
    local probe="$BUILD_DIR/probe_${backend}.exe"
    local compile_log="$BUILD_DIR/compile_${backend}.log"
    local pipeline_args=()
    if [[ "$backend" == "llvm" ]]; then
        pipeline_args+=(--native-pipeline)
    fi
    (cd "$ROOT_DIR" && "$PGY" \
        "${pipeline_args[@]}" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/tools/expression_graph_persisted_read_probe/main.pgy")" \
        --backend="$backend" -o "$(pgy_path_for_compiler "$PGY" "$probe")" \
        >"$compile_log" 2>&1) \
        || { cat "$compile_log" >&2; fail "$backend probe build failed"; }
    pgy_require_runnable_binary_here \
        "mir-expression-graph-read:$backend" "$probe" || return 1

    local positive="$($probe | tr -d '\r')"
    [[ "$positive" == *"persisted-graph-read=one-pass-exact"* ]] \
        || fail "$backend positive graph reconstruction drifted"
    local reordered="$($probe --reordered-fields | tr -d '\r')"
    [[ "$reordered" == *"persisted-graph-read=one-pass-exact"* ]] \
        || fail "$backend order-independent graph reconstruction drifted"
    local extended="$($probe --extended-identity | tr -d '\r')"
    [[ "$extended" == *"persisted-graph-read=one-pass-exact"* ]] \
        || fail "$backend extended identity reconstruction drifted"

    local mode
    local negative
    for mode in \
        --duplicate-node-field \
        --unknown-header-field \
        --unreachable-node \
        --overflow-root \
        --unknown-node-kind \
        --unknown-target-kind \
        --unknown-binding-kind; do
        negative="$($probe "$mode" | tr -d '\r')"
        [[ "$negative" == *"persisted-graph-negative=closed"* ]] \
            || fail "$backend negative graph was not rejected: $mode"
    done
}

run_backend c
run_backend llvm

echo "[self-host-parity:mir-expression-graph-read] C/native LLVM exact-span vocabulary parity ok"

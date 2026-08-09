#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[self-host-parity:expression-graph-identity-prefix] missing compiler: $PGY" >&2
    exit 1
fi

SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/expression_graph_identity_prefix_owner.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/expression_graph_identity_prefix}"
mkdir -p "$BUILD_DIR"

SEQUENCE_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_sequence_owner.pgy"
for extension_owner in \
    destructure_expression_projection_owner.pgy \
    expression_graph_option_match_owner.pgy \
    expression_graph_tagged_enum_match_owner.pgy; do
    path="$ROOT_DIR/src/self_hosted/mir_lower/$extension_owner"
    grep -Fq 'MirExpressionGraphSequenceFromExtendedRows(' "$path" || {
        echo "[self-host-parity:expression-graph-identity-prefix] $extension_owner bypassed the identity-preserving extension owner" >&2
        exit 1
    }
    ! grep -Fq 'SemanticExpressionGraphArenaFromRows(' "$path" || {
        echo "[self-host-parity:expression-graph-identity-prefix] $extension_owner rebuilt the cumulative identity universe" >&2
        exit 1
    }
done
grep -Fq 'func MirExpressionGraphSequenceFromExtendedRows(' \
    "$SEQUENCE_OWNER" || exit 1

run_backend() {
    local backend="$1"
    local bin="$BUILD_DIR/expression_graph_identity_prefix_${backend}.exe"
    local log="$BUILD_DIR/expression_graph_identity_prefix_${backend}.compile.log"
    local pipeline_args=()
    if [[ "$backend" == "llvm" ]]; then
        # The installed self-host LLVM projector remains bounded to small
        # routine families. Keep an LLVM execution oracle without hiding that
        # independent integration blocker as an unavailable-backend skip.
        pipeline_args+=(--native-pipeline)
    fi
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "${pipeline_args[@]}" \
        "$(pgy_path_for_compiler "$PGY" "$SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" \
        >"$log" 2>&1); then
        cat "$log" >&2
        return 1
    fi
    pgy_require_runnable_binary_here \
        "self-host-parity:expression-graph-identity-prefix:$backend" \
        "$bin" || return 1
    (cd "$ROOT_DIR" && "$bin") | tr -d '\r'
}

C_OUT="$(run_backend c)"
[[ "$C_OUT" == "expression-graph-identity-prefix-ok" ]] || {
    echo "[self-host-parity:expression-graph-identity-prefix] C mismatch: $C_OUT" >&2
    exit 1
}

LLVM_OUT="$(run_backend llvm)"
[[ "$LLVM_OUT" == "$C_OUT" ]] || {
    echo "[self-host-parity:expression-graph-identity-prefix] LLVM mismatch: $LLVM_OUT" >&2
    exit 1
}

echo "[self-host-parity:expression-graph-identity-prefix] installed self-host C/native LLVM prefix identity parity ok"

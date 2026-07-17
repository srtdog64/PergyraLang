#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[hir-routine-identity] missing compiler binary: $PGY" >&2
    exit 1
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_hir_routine_identity.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

require_text() {
    local file="$1"
    local text="$2"
    if ! grep -Fq -- "$text" "$ROOT_DIR/$file"; then
        echo "[hir-routine-identity] missing $file contract: $text" >&2
        exit 1
    fi
}

reject_text() {
    local file="$1"
    local text="$2"
    if grep -Fq -- "$text" "$ROOT_DIR/$file"; then
        echo "[hir-routine-identity] forbidden $file path returned: $text" >&2
        exit 1
    fi
}

reject_name_join() {
    local file="$1"
    if grep -Eq 'hir_(lookup|build)_routine_(index_)?by_name|direct_calls\[[^]]+\]' "$file"; then
        echo "[hir-routine-identity] textual routine join returned: $file" >&2
        grep -En 'hir_(lookup|build)_routine_(index_)?by_name|direct_calls\[[^]]+\]' \
            "$file" >&2 || true
        return 1
    fi
}

require_text "src/parser/ast.h" "uint32_t       semantic_callee_decl_id;"
require_text "src/parser/ast_api.h" "ast_call_semantic_callee_decl_id"
require_text "src/semantic/type_checker_helpers_late.c" \
    "expr, sym->decl_syntax_id"
require_text "src/compiler/hir.h" "uint32_t         routine_id;"
require_text "src/compiler/hir.h" "uint32_t        *direct_call_decl_ids;"
require_text "src/compiler/hir_callgraph.c" "HIRRoutineSourceIndex"
require_text "src/compiler/hir_callgraph.c" \
    "routine->direct_call_decl_ids[j]"
require_text "src/compiler/hir_public.c" "hir_find_routine_by_id"
require_text "src/compiler/hir_callgraph.c" \
    "HIR direct-call identity facts are incomplete"
require_text "src/compiler/hir_callgraph.c" \
    "HIR internal call target has no RoutineId"
require_text "src/compiler/rir.h" "uint32_t      source_syntax_id;"
require_text "src/compiler/rir_flow.c" \
    "Source-backed HIR routines must not fall back to a name join"
require_text "src/compiler/rir_flow.c" \
    "RIR resource fact has no matching ResourceFlow stable identity"
require_text "src/parser/ast_identity.c" \
    "field->stable_id = ast_take_stable_id(next_id);"
require_text "src/semantic/type_checker_class_decl.c" \
    "fields[i].declaration_syntax_id"
require_text "src/semantic/type_checker_ownership_destructure.c" \
    "ownership_destructure_binding_syntax_id"
require_text "src/semantic/type_checker_ownership_destructure.c" \
    "ast_let_destructure_binding_stable_id"
reject_text "src/semantic/type_checker_ownership_destructure.c" \
    "ast_class_fields("
reject_text "src/semantic/type_checker_ownership_destructure.c" \
    "pgy_class_decl_field_declaration_syntax_id"
require_text "src/compiler/rir_builder.c" "case AST_ROLE_DECL:"
require_text "src/compiler/rir_builder.c" "case AST_ENUM_DECL:"
require_text "src/compiler/rir_builder.c" "case AST_PARTY_DECL:"
require_text "src/compiler/rir_builder.c" "case AST_ROSTER_DECL:"
require_text "src/compiler/rir_builder.c" \
    "rir_collect_hosted_method_scopes("
require_text "src/compiler/rir_builder.c" "ast_impl_ability_method_count"
require_text "src/compiler/rir_validation.c" \
    "RIR scope '%s' resource fact '%s' is missing HIR stable identity"
require_text "src/test_rir.c" \
    "RIR validation rejects a verified scope with missing HIR identity"
reject_name_join "$ROOT_DIR/src/compiler/hir_callgraph.c"

# The detector must reject the old name-carried edge shape, not merely accept
# the current file. This mutation never touches the worktree.
cp "$ROOT_DIR/src/compiler/hir_callgraph.c" "$WORK_DIR/hir_callgraph.c"
sed 's/routine->direct_call_decl_ids\[j\]/routine->direct_calls[j]/' \
    "$WORK_DIR/hir_callgraph.c" > "$WORK_DIR/hir_callgraph.mutated.c"
if reject_name_join "$WORK_DIR/hir_callgraph.mutated.c" >/dev/null 2>&1; then
    echo "[hir-routine-identity] negative mutation escaped the detector" >&2
    exit 1
fi

source="$ROOT_DIR/tests/cases/hir_routine_identity/main.pgy"
source_arg="$(pgy_path_for_compiler "$PGY" "$source")"
if ! "$PGY" --hir "$source_arg" >"$WORK_DIR/hir.out" 2>"$WORK_DIR/hir.err"; then
    cat "$WORK_DIR/hir.err" >&2
    exit 1
fi

if ! grep -Eq 'Helper.*hosted=true.*reachable=false' "$WORK_DIR/hir.out"; then
    echo "[hir-routine-identity] hosted Helper was selected by the callgraph" >&2
    cat "$WORK_DIR/hir.out" >&2
    exit 1
fi
if ! grep -Eq 'Helper.*hosted=false.*reachable=true' "$WORK_DIR/hir.out"; then
    echo "[hir-routine-identity] top-level Helper was not reached" >&2
    cat "$WORK_DIR/hir.out" >&2
    exit 1
fi

resource_source="$ROOT_DIR/tests/cases/slot_contract/positive/plain_read_write_release/main.pgy"
resource_arg="$(pgy_path_for_compiler "$PGY" "$resource_source")"
if ! "$PGY" --rir "$resource_arg" >"$WORK_DIR/rir.out" 2>"$WORK_DIR/rir.err"; then
    cat "$WORK_DIR/rir.err" >&2
    exit 1
fi
if ! grep -Eq 'identity=verified.*stable=0' "$WORK_DIR/rir.out"; then
    echo "[hir-routine-identity] RIR did not carry a verified HIR stable resource identity" >&2
    cat "$WORK_DIR/rir.out" >&2
    exit 1
fi

echo "[hir-routine-identity] semantic declaration target joins HIR RoutineId without a name fallback"

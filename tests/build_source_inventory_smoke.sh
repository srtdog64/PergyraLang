#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAKE_BIN="${MAKE:-}"

if [[ -z "$MAKE_BIN" ]]; then
    if command -v make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v make)"
    elif command -v mingw32-make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v mingw32-make)"
    else
        echo "[build-source-inventory] missing make" >&2
        exit 1
    fi
fi

inventory="$("$MAKE_BIN" -C "$ROOT_DIR" -s __pgy_build_source_inventory_print)"
missing=0

duplicate_sources="$(
    printf '%s\n' "$inventory" \
        | sed '/^$/d' \
        | sort \
        | uniq -d
)"
if [[ -n "$duplicate_sources" ]]; then
    printf '%s\n' "$duplicate_sources" >&2
    echo "[build-source-inventory] duplicate source inventory entries" >&2
    missing=1
fi

ignored_inventory=""
if command -v git >/dev/null 2>&1 \
    && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    ignored_inventory="$(
        printf '%s\n' "$inventory" \
            | git -C "$ROOT_DIR" check-ignore --stdin --no-index 2>/dev/null \
            || true
    )"
fi

while IFS= read -r src; do
    [[ -n "$src" ]] || continue

    if [[ ! -f "$ROOT_DIR/$src" ]]; then
        echo "[build-source-inventory] missing required build file: $src" >&2
        missing=1
        continue
    fi

    if [[ -n "$ignored_inventory" ]] \
        && grep -Fxq "$src" <<< "$ignored_inventory"; then
        echo "[build-source-inventory] required build file is ignored: $src" >&2
        missing=1
    fi

    if [[ "${PGY_REQUIRE_TRACKED_SOURCES:-0}" == "1" ]] \
        && command -v git >/dev/null 2>&1 \
        && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1 \
        && ! git -C "$ROOT_DIR" ls-files --error-unmatch "$src" >/dev/null 2>&1; then
        echo "[build-source-inventory] required build file is not tracked: $src" >&2
        missing=1
    fi
done <<< "$inventory"

if command -v git >/dev/null 2>&1 \
    && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    deleted_examples="$(
        git -C "$ROOT_DIR" ls-files --deleted -- 'tests/cases' 'examples'
    )"
    while IFS= read -r tracked; do
        [[ -n "$tracked" ]] || continue
        if [[ -n "$deleted_examples" ]] \
            && grep -Fxq "$tracked" <<< "$deleted_examples"; then
            continue
        fi
        case "$tracked" in
            *.exe|*.dll|*.so|*.dylib|*.o|*.obj|*.a|*.lib|*.wasm)
                echo "[build-source-inventory] tracked executable artifact is not allowed: $tracked" >&2
                missing=1
                ;;
        esac
    done < <(git -C "$ROOT_DIR" ls-files 'tests/cases' 'examples')
fi

typo_tokens="$(
    grep -RInE '\b(retun|stncmp|retun_type|infer_spawn_retun)\b' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
typo_tokens="$(
    printf '%s\n' "$typo_tokens" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [[ -n "$typo_tokens" ]]; then
    printf '%s\n' "$typo_tokens" >&2
    echo "[build-source-inventory] production source contains typo-like C tokens" >&2
    missing=1
fi

for header in \
    src/compiler/compiler_process.h \
    src/compiler/mir.h \
    src/parser/ast_types.h \
    src/runtime/slot_manager.h
do
    if grep -Eq '\bsize_t\b' "$ROOT_DIR/$header" \
        && ! grep -Eq '#include[[:space:]]*<stddef\.h>' "$ROOT_DIR/$header"; then
        echo "[build-source-inventory] public header using size_t must include <stddef.h>: $header" >&2
        missing=1
    fi
done

if ! grep -Fq '$(BUILD_DIR)/compiler/hir_callgraph.o' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] HIR callgraph source is not linked by HIR_CORE_OBJECTS" >&2
    missing=1
fi

if ! grep -Fq '$(COMPILER_DIR)/mir_cfg_contract_validate_cleanup.c' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] MIR cleanup validator source is not linked by the compiler source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(BUILD_DIR)/compiler/mir_cfg_contract_validate_cleanup.o' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] MIR cleanup validator object is not linked by MIR_CORE_OBJECTS" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_expr_party_instance_emit.h"' \
    "$ROOT_DIR/src/codegen/transpiler_expr_emitters.h"; then
    echo "[build-source-inventory] party-instance expression emitter is not linked by the C expression emitter include chain" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_let_type_register_emit.h"' \
    "$ROOT_DIR/src/codegen/transpiler_base_a_emitters.h"; then
    echo "[build-source-inventory] let type-registration emitter is not linked by the C let emitter include chain" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_collection_runtime_suffix.h"' \
    "$ROOT_DIR/src/codegen/transpiler_helpers_core_b.h"; then
    echo "[build-source-inventory] collection runtime suffix helper is not linked by the C helper include chain" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_roster_decl_emit.h"' \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.h"; then
    echo "[build-source-inventory] roster declaration emitter is not linked by the C domain nominal include chain" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_zone_decl_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] zone declaration emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_zone_methods_emit.h"' \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_emit.h"; then
    echo "[build-source-inventory] zone hosted-method emitter is not linked by the C domain role include chain" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_overlay_zone_bind.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] zone effect bind owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_overlay_zone_relation_bind.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] zone relation bind owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_stdlib_queue_builtin.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] stdlib queue builtin emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_domain_query_builtin.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] domain query builtin emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_io_builtin.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] I/O builtin emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_builtin_dispatch.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] expression builtin dispatch owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_core_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] expression core emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_composite_literal_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] composite literal emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_array_access_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] array access emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_let_channel_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] channel let emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_let_box_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] Box/Rc let emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_let_type_register_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] let type-register owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_future_type_query.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] future type query owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_control_flow_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] control-flow emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_block_intent_helpers.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent zone bind helper owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_block_intent_rebind_helpers.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent zone rebind helper owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_intent_cleanup_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent cleanup tail owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_intent_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent declaration emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_intent_prologue_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent prologue emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_mir_cfg_control_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] MIR CFG control emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_mir_match_condition_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] MIR match condition emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_let_collection_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] collection let emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_let_slot_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] slot let emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_domain_constructor_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] domain constructor emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_zone_specialization_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] zone specialization emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_zone_struct_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] zone struct emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_intent_zone_binding_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent zone-binding emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/llvm_decl_authority.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] LLVM declaration authority owner is not linked by the LLVM backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/llvm_decl_routines.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] LLVM function-routine inventory owner is not linked by the LLVM backend source inventory" >&2
    missing=1
fi

grep -Fq "emit_builtin_domain_query" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.c" || {
    echo "[build-source-inventory] C builtin dispatch must delegate domain queries to the compiled owner" >&2
    missing=1
}

grep -Fq "emit_builtin_io" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.c" || {
    echo "[build-source-inventory] C builtin dispatch must delegate I/O builtins to the compiled owner" >&2
    missing=1
}

grep -Fq "emit_builtin_has_zone_state" \
    "$ROOT_DIR/src/codegen/transpiler_expr_domain_query_builtin.c" || {
    echo "[build-source-inventory] domain query builtin lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_builtin_file_open" \
    "$ROOT_DIR/src/codegen/transpiler_expr_io_builtin.c" || {
    echo "[build-source-inventory] I/O builtin lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_binary" \
    "$ROOT_DIR/src/codegen/transpiler_expr_core_emit.c" || {
    echo "[build-source-inventory] expression core lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_composite_literal_expression" \
    "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c" || {
    echo "[build-source-inventory] composite literal lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_array_access_expression" \
    "$ROOT_DIR/src/codegen/transpiler_expr_array_access_emit.c" || {
    echo "[build-source-inventory] array access lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_try_emit_channel_let" \
    "$ROOT_DIR/src/codegen/transpiler_let_channel_emit.c" || {
    echo "[build-source-inventory] channel let lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_try_emit_box_family_let" \
    "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c" || {
    echo "[build-source-inventory] Box/Rc let lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_register_let_type_after_emit" \
    "$ROOT_DIR/src/codegen/transpiler_let_type_register_emit.c" || {
    echo "[build-source-inventory] let type registration must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "infer_spawn_return_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_future_type_query.c" || {
    echo "[build-source-inventory] Future<T> type query must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_for_loop" \
    "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c" || {
    echo "[build-source-inventory] control-flow lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_mir_render_branch_condition" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c" || {
    echo "[build-source-inventory] MIR CFG control lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_intent_step_bind_bound_zone" \
    "$ROOT_DIR/src/codegen/transpiler_block_intent_helpers.c" || {
    echo "[build-source-inventory] intent zone bind helpers must stay in their compiled owner" >&2
    missing=1
}

grep -Fq "emit_intent_step_rebind_bound_zone_aliases" \
    "$ROOT_DIR/src/codegen/transpiler_block_intent_rebind_helpers.c" || {
    echo "[build-source-inventory] intent zone rebind helpers must stay in their compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_emit_intent_cleanup_tail" \
    "$ROOT_DIR/src/codegen/transpiler_intent_cleanup_emit.c" || {
    echo "[build-source-inventory] intent cleanup tail lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_intent_decl" \
    "$ROOT_DIR/src/codegen/transpiler_intent_emit.c" || {
    echo "[build-source-inventory] intent declaration lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_zone_decl" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.c" || {
    echo "[build-source-inventory] zone declaration lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_emit_intent_signature_and_entry" \
    "$ROOT_DIR/src/codegen/transpiler_intent_prologue_emit.c" || {
    echo "[build-source-inventory] intent prologue lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_mir_render_match_case_condition" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c" || {
    echo "[build-source-inventory] MIR match condition lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_intent_forward_decl" \
    "$ROOT_DIR/src/codegen/transpiler_intent_zone_binding_emit.c" || {
    echo "[build-source-inventory] intent zone-binding lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_try_emit_collection_ctor_let" \
    "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c" || {
    echo "[build-source-inventory] collection let lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_emit_domain_constructor_for_decl" \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c" || {
    echo "[build-source-inventory] domain constructor lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_emit_zone_required_specializations" \
    "$ROOT_DIR/src/codegen/transpiler_zone_specialization_emit.c" || {
    echo "[build-source-inventory] zone specialization lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_current_overlay_domain_slot_decl" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c" || {
    echo "[build-source-inventory] overlay domain-slot query must be owned by transpiler_projection.c" >&2
    missing=1
}

grep -Fq "transpiler_domain_slot_is_projection_target" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c" || {
    echo "[build-source-inventory] projection-target query must be owned by transpiler_projection.c" >&2
    missing=1
}

grep -Fq "transpiler_find_world_state_decl" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c" || {
    echo "[build-source-inventory] world-state query must be owned by transpiler_projection.c" >&2
    missing=1
}

if grep -Fq "domain_slot_is_projection_target_local" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_projection.h" \
    "$ROOT_DIR/src/codegen/transpiler_call_constructor_result_emit.h"; then
    echo "[build-source-inventory] projection-target query regressed to implementation-header local helper" >&2
    missing=1
fi

if grep -Eq '(^|[^A-Za-z0-9_])current_overlay_domain_slot_decl\(' \
    "$ROOT_DIR/src/codegen/transpiler_overlay_projection.h" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.h"; then
    echo "[build-source-inventory] overlay domain-slot query regressed to implementation-header local helper" >&2
    missing=1
fi

if grep -Eq '(^|[^A-Za-z0-9_])find_world_state_decl\(' \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.h" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.h" \
    "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.h"; then
    echo "[build-source-inventory] world-state query regressed to implementation-header local helper" >&2
    missing=1
fi

if [[ "$missing" -ne 0 ]]; then
    exit 1
fi

echo "[build-source-inventory] Makefile source inventory ok"

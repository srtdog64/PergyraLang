#!/usr/bin/env bash
# Regression gate for C/LLVM declaration-side MIR inventory usage.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/beta_checklist_shards.sh"

fail() {
    echo "[mir-decl-inventory] FAIL" >&2
    echo "  - $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing required file: $rel"
}

require_term() {
    local rel="$1"
    local term="$2"
    local text

    if [[ "$rel" == "docs/100_beta_readiness_checklist.md" ]]; then
        pgy_beta_checklist_contains "$term" ||
            fail "$rel shards missing term: $term"
        return 0
    fi

    text="$(<"$ROOT_DIR/$rel")"
    [[ "$text" == *"$term"* ]] ||
        fail "$rel missing term: $term"
}

require_term_any() {
    local term="$1"
    shift

    local rel
    for rel in "$@"; do
        if [[ -f "$ROOT_DIR/$rel" ]] \
            && grep -Fq "$term" "$ROOT_DIR/$rel"; then
            return 0
        fi
    done
    fail "missing term in expected file set: $term"
}

for rel in \
    "src/codegen/llvm_internal.h" \
    "src/codegen/llvm_internal_api.h" \
    "src/codegen/llvm_inventory_internal.c" \
    "src/codegen/llvm_inventory_internal.h" \
    "src/codegen/llvm_inventory_decl_lookup.c" \
    "src/codegen/llvm_inventory_field_view.c" \
    "src/codegen/llvm_inventory_slot_view.c" \
    "src/codegen/llvm_inventory_decl_lookup.h" \
    "src/codegen/host_decl_compat.c" \
    "src/codegen/host_decl_compat.h" \
    "src/codegen/llvm_inventory_host_methods.h" \
    "src/codegen/llvm_pipeline.c" \
    "src/codegen/llvm_main_wrapper.c" \
    "src/codegen/llvm_domain.c" \
    "src/codegen/llvm_domain_method_helpers.c" \
    "src/codegen/llvm_domain_method_emit.c" \
    "src/codegen/llvm_domain_forward.c" \
    "src/codegen/llvm_domain_forward.h" \
    "src/codegen/llvm_backend_type_map.c" \
    "src/codegen/llvm_registry.c" \
    "src/codegen/llvm_decl_authority.c" \
    "src/codegen/llvm_decl_authority.h" \
    "src/codegen/llvm_decl_routines.c" \
    "src/codegen/llvm_backend.h" \
    "src/codegen/llvm_register.c" \
    "src/codegen/transpiler.h" \
    "src/codegen/transpiler_inventory_view.c" \
    "src/codegen/transpiler_inventory_view.h" \
    "src/codegen/transpiler.c" \
    "src/codegen/transpiler_decl_host_lookup.c" \
    "src/codegen/transpiler_decl_field_view.c" \
    "src/codegen/transpiler_decl_slot_view.c" \
    "src/codegen/transpiler_domain_receiver_query.c" \
    "src/codegen/transpiler_mir_stmt_emit.c" \
    "src/codegen/transpiler_overlay_zone_bind.c" \
    "src/codegen/transpiler_projection_sync.c" \
    "src/codegen/transpiler_projection_sync.h" \
    "src/codegen/transpiler_domain_role_ability_emit.c" \
    "src/codegen/transpiler_domain_role_ability_emit.h" \
    "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "src/codegen/transpiler_mir_ssa_names.h" \
    "src/compiler/mir.h" \
    "src/compiler/mir_lower_public_api.h" \
    "src/compiler/mir_program_inventory.c" \
    "src/compiler/mir_public_surface.c" \
    "src/compiler/mir_decl_headers.c" \
    "src/compiler/mir_decl_headers.h" \
    "src/parser/ast_api.h" \
    "src/parser/ast_decl_accessors.c" \
    "docs/100_beta_readiness_checklist.md" \
    "TODO.md"; do
    require_file "$rel"
done

require_term "src/codegen/transpiler_projection_sync.h" \
    "void emit_zone_action_effect_runtime(CodeBuf *out"
require_term "src/codegen/transpiler_mir_stmt_emit.c" \
    "emit_zone_action_effect_runtime(buf, stmt, ctx)"
require_term "src/codegen/transpiler_statement_dispatch.c" \
    "emit_zone_action_effect_runtime(ctx->out, node, ctx)"
require_term "src/codegen/transpiler_projection_sync.c" \
    "write_indent_to(out, ctx->indent)"
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "write_indent_to(out, ctx->indent)"
require_term "src/codegen/transpiler_domain_receiver_query.c" \
    "transpiler_resolve_nominal_host_expr_type_name(ctx, receiver)"
require_term "src/codegen/transpiler_domain_receiver_query.c" \
    "transpiler_zone_subject_slot_type_name(ctx, zone_decl"
if grep -Fq "type_name = NULL; /* revert to NULL" \
    "$ROOT_DIR/src/codegen/transpiler_domain_receiver_query.c"; then
    fail "zone subject receiver resolution must not keep temporary NULL receiver fallback"
fi
if grep -Fq "codebuf_write(ctx->out, \"self->__layer_active_" \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.c"; then
    fail "zone action effect sync must emit through the caller CodeBuf sink"
fi

for term in \
    "llvm_hosted_zone_layer_slot_view_is_relation" \
    "llvm_hosted_zone_layer_slot_view_is_pool" \
    "llvm_hosted_zone_layer_slot_view_pool_capacity"; do
    require_term "src/codegen/llvm_inventory_decl_lookup.h" "$term"
done
for term in \
    "mir_decl_field_is_relation_layer(field)" \
    "mir_decl_field_is_pool_layer(field)" \
    "mir_decl_field_pool_capacity(field)"; do
    require_term "src/codegen/llvm_inventory_slot_view.c" "$term"
done

for term in \
    "llvm_active_inventory" \
    "llvm_find_decl_header_in_context" \
    "llvm_find_host_decl_header_in_context" \
    "llvm_find_decl_in_active_inventory" \
    "llvm_find_host_decl_in_active_inventory"; do
    require_term "src/codegen/llvm_inventory_decl_lookup.h" "$term"
done

for term in \
    "kPgyHostDeclCompatTypes" \
    "AST_PARTY_DECL" \
    "AST_ROLE_DECL" \
    "AST_ROSTER_DECL" \
    "pgy_host_decl_compat_types" \
    "pgy_host_decl_compat_is_type" \
    "pgy_host_decl_compat_name" \
    "case AST_PARTY_DECL" \
    "case AST_ROLE_DECL" \
    "case AST_ROSTER_DECL"; do
    require_term "src/codegen/host_decl_compat.c" "$term"
done
for term in \
    "ast_declaration_generic_params" \
    "case AST_FUNC_DECL" \
    "case AST_CLASS_DECL" \
    "case AST_ABILITY_DECL" \
    "case AST_ROLE_DECL" \
    "case AST_PARTY_DECL" \
    "case AST_ROSTER_DECL"; do
    require_term "src/parser/ast_decl_accessors.c" "$term"
done
require_term "src/parser/ast_api.h" \
    "GenericParams* ast_declaration_generic_params"
require_term "src/codegen/llvm_backend_type_map.c" \
    "ast_declaration_generic_params(decl)"
if grep -Eq 'ast_(func|class|ability|role|party|roster)_generic_params\(decl\)' \
    "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"; then
    fail "LLVM generic default lookup must consume ast_declaration_generic_params"
fi
for rel in \
    "src/codegen/llvm_expr_constructor_calls.c" \
    "src/codegen/llvm_domain_projection_value_helpers.c" \
    "src/codegen/llvm_expr_projection_path_helpers.c"; do
    require_term "$rel" "llvm_class_field_type_at_index("
    if grep -Eq 'fields\[[^]]+\]\.index == field_index' "$ROOT_DIR/$rel"; then
        fail "$rel must consume llvm_class_field_type_at_index for index-to-type recovery"
    fi
done
for rel in \
    "src/codegen/llvm_domain_projection_value_helpers.c" \
    "src/codegen/llvm_expr_projection_path_helpers.c"; do
    require_term "$rel" "pgy_arena_alloc(&ctx->scratch"
    if grep -Fq "pergyra_strdup(field_name)" "$ROOT_DIR/$rel"; then
        fail "$rel must allocate projection path scratch strings from LLVM scratch arena"
    fi
done
require_term "src/codegen/llvm_domain_projection_value_helpers.c" \
    "LLVM domain projection nested path metadata is missing"
require_term "src/codegen/llvm_expr_projection_path_helpers.c" \
    "LLVM projection nested path requires field declaration metadata"
require_term "src/codegen/llvm_internal_api.h" \
    "LLVMTypeRef         llvm_class_field_type_at_index"
require_term "src/codegen/llvm_internal.h" \
    "LLVMVarEntry *entries"
require_term "src/codegen/llvm_internal.h" \
    "int           capacity"
require_term "src/codegen/llvm_limits_internal.h" \
    "LLVM_SCOPE_INITIAL_CAPACITY"
require_term "src/codegen/llvm_registry.c" \
    "frame->capacity"
require_term "src/codegen/llvm_stmt_parallel_async.c" \
    "capture_count"
require_term "src/codegen/llvm_stmt_parallel_async.c" \
    "LLVM parallel wrapper registry allocation overflow"
require_term "src/codegen/llvm_stmt_parallel_async.c" \
    "out of memory allocating LLVM parallel wrapper registry"
require_term "src/codegen/llvm_stmt_parallel_async.c" \
    "LLVM parallel handle registry allocation overflow"
require_term "src/codegen/llvm_stmt_parallel_async.c" \
    "out of memory allocating LLVM parallel handle registry"
if grep -R --include='llvm*.c' --include='llvm*.h' -Fq \
    "MAX_SCOPE_VARS" "$ROOT_DIR/src/codegen"; then
    fail "LLVM scope/capture registries must stay dynamically sized, not MAX_SCOPE_VARS bounded"
fi
for term in \
    "llvm_class_field_count" \
    "llvm_class_field_name_at" \
    "llvm_class_field_type_at" \
    "llvm_class_field_struct_index_at" \
    "llvm_class_field_is_subject_slot_at"; do
    require_term "src/codegen/llvm_internal_api.h" "$term"
    require_term "src/codegen/llvm_registry.c" "$term"
done
require_term "src/codegen/llvm_registry.c" \
    "llvm_class_field_type_at_index(LLVMClassTypeEntry *entry, int struct_index)"
for rel in \
    "src/codegen/llvm_expr_constructor_calls.c" \
    "src/codegen/llvm_domain_projection_value_helpers.c" \
    "src/codegen/llvm_expr_projection_path_helpers.c" \
    "src/codegen/llvm_mir_emit.c" \
    "src/codegen/llvm_intent_zone.c" \
    "src/codegen/llvm_expr_host_spawn_literal_helpers.c"; do
    require_term "$rel" "llvm_class_field_count("
    require_term "$rel" "llvm_class_field_name_at("
    require_term "$rel" "llvm_class_field_struct_index_at("
    if grep -Fq -- "->fields[" "$ROOT_DIR/$rel"; then
        fail "$rel must consume LLVM class-field registry accessors instead of raw fields[]"
    fi
done
require_term "src/codegen/llvm_intent_zone.c" \
    "llvm_class_field_is_subject_slot_at("
require_term "src/codegen/llvm_intent_zone.c" \
    "llvm_class_field_type_at("
raw_llvm_class_field_hits="$(
    grep -R --include='llvm*.c' --include='llvm*.h' -F -- "->fields[" \
        "$ROOT_DIR/src/codegen" |
        grep -Fv "/src/codegen/llvm_registry.c:" |
        sed "s#^$ROOT_DIR/##" ||
        true
)"
if [[ -n "$raw_llvm_class_field_hits" ]]; then
    fail "LLVM class-field storage must stay behind llvm_registry.c:
$raw_llvm_class_field_hits"
fi
require_term "src/codegen/llvm_internal_api.h" \
    "bool                llvm_enum_type_exists"
require_term "src/codegen/llvm_registry_aux.c" \
    "llvm_enum_type_exists(LLVMGenCtx *ctx, const char *enum_name)"
require_term "src/codegen/llvm_type.c" \
    "llvm_enum_type_exists(ctx, type_name)"
if grep -Eq 'ctx->enum_variant_count|ctx->enum_variants\[' \
    "$ROOT_DIR/src/codegen/llvm_type.c"; then
    fail "LLVM source type resolution must consume enum registry APIs instead of raw enum variant arrays"
fi
require_term "src/codegen/llvm_expr_common.c" \
    "return llvm_lookup_class_by_struct_type(ctx, ty)"
require_term "src/codegen/llvm_stmt_type_infer.c" \
    "return llvm_lookup_class_by_struct_type(ctx, type)"
require_term "src/codegen/llvm_internal_api.h" \
    "LLVMClassTypeEntry *llvm_lookup_vtable_class_with_method"
require_term "src/codegen/llvm_registry.c" \
    "llvm_lookup_vtable_class_with_method(LLVMGenCtx *ctx,"
require_term "src/codegen/llvm_expr_call_methods_vtable_dispatch.c" \
    "llvm_lookup_vtable_class_with_method(ctx, method_name"
require_term "src/codegen/llvm_internal_api.h" \
    "int llvm_event_type_count"
require_term "src/codegen/llvm_internal_api.h" \
    "LLVMEventTypeEntry *llvm_event_type_at"
require_term "src/codegen/llvm_event.c" \
    "llvm_event_type_at(LLVMGenCtx *ctx, int index)"
require_term "src/codegen/llvm_main_wrapper.c" \
    "llvm_event_type_count(ctx)"
require_term "src/codegen/llvm_main_wrapper.c" \
    "llvm_event_type_at(ctx, i)"
require_term "src/codegen/llvm_mir_emit.c" \
    "llvm_lookup_function(ctx, fn_name)"
if grep -Fq "llvm_lookup_or_declare_function(ctx, fn_name" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"; then
    fail "LLVM MIR routine emission must consume registered inventory functions instead of synthesizing declarations"
fi
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "MIR-only LLVM path missing registered intent call target"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "MIR-only LLVM path missing registered function call target"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "llvm_active_has_mir(ctx)"
require_term "src/codegen/llvm_intent_forward.c" \
    "MIR-only LLVM path missing intent participant type metadata"
require_term "src/codegen/llvm_intent_forward.c" \
    "MIR-only LLVM path missing intent value type metadata"
require_term "src/codegen/llvm_mir_local_emit.c" \
    "MIR-only LLVM path missing local type metadata"
require_term "src/codegen/llvm_mir_local_emit.c" \
    "llvm_mir_find_result_instruction"
require_term "src/codegen/llvm_mir_local_emit.c" \
    "inst->phi_incomings[i].value_name"
require_term "src/codegen/llvm_stmt_type_infer.c" \
    "channel receive requires registered Channel<T> metadata"
require_term "src/codegen/llvm_stmt_type_infer.c" \
    "call result requires registered function or expected type metadata"
require_term "src/codegen/llvm_stmt_type_infer.c" \
    "identifier requires registered LLVM local metadata"
require_term "src/codegen/llvm_stmt_type_infer.c" \
    "llvm_current_host_class_name(ctx)"
require_term "src/codegen/llvm_stmt_type_infer.c" \
    "expression requires typed MIR result facts"
require_term "src/codegen/llvm_stmt_array_type_infer.c" \
    "array or slice element type requires registered Array<T>/Slice<T> metadata"
if grep -Fq "elem_type = ctx->type_i32;" \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"; then
    fail "LLVM array/slice element inference must not default missing metadata to Int"
fi
require_term "src/codegen/llvm_expr.c" \
    "LLVM await expression requires registered Future<T> result metadata"
require_term "src/codegen/llvm_expr_await_task.c" \
    "LLVM await expression requires registered Future<T> result metadata"
if grep -Fq "return llvm_emit_expression(inner_expr);" \
    "$ROOT_DIR/src/codegen/llvm_expr.c"; then
    fail "LLVM await expression must fail closed when Future<T> metadata is missing"
fi
if grep -Fq "poison i32" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"; then
    fail "LLVM expression type inference must not keep silent poison i32 fallbacks"
fi
if grep -Fq "ctx->class_type_count" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_vtable_dispatch.c"; then
    fail "LLVM vtable dispatch must consume registry vtable lookup"
fi
if grep -Eq 'ctx->event_type_count|ctx->event_types\[' \
    "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"; then
    fail "LLVM main wrapper must consume event registry accessors instead of raw event arrays"
fi
if grep -R --include='llvm*.c' -n 'LLVMAppendBasicBlock(' \
    "$ROOT_DIR/src/codegen" >/dev/null; then
    fail "LLVM backend must use LLVMAppendBasicBlockInContext, not deprecated global-context block append"
fi
for rel in \
    "src/codegen/llvm_expr.c" \
    "src/codegen/llvm_stmt.c"; do
    require_term "$rel" "llvm_class_field_index("
    if grep -Eq 'strcmp\([^,]*fields\[[^]]+\]\.field_name' "$ROOT_DIR/$rel"; then
        fail "$rel must consume llvm_class_field_index for class field lookup"
    fi
done
raw_llvm_field_type_hits="$(
    grep -R --include='llvm*.c' -EHIn \
        'fields\[[A-Za-z_][A-Za-z0-9_]*(idx|_idx|index)\]\.field_type' \
        "$ROOT_DIR/src/codegen" | sed "s#^$ROOT_DIR/##" || true
)"
if [[ -n "$raw_llvm_field_type_hits" ]]; then
    fail "LLVM consumers must not index fields[] with struct field indexes for type lookup:
$raw_llvm_field_type_hits"
fi
for term in \
    "PgyHostClassFieldsCompatView" \
    "pgy_host_class_fields_compat_view_from_decl" \
    "pgy_host_shared_fields_compat_view_from_decl" \
    "pgy_host_class_field_compat_find" \
    "pgy_host_shared_field_compat_find"; do
    require_term "src/codegen/host_decl_compat.h" "$term"
done
require_term "src/codegen/host_decl_compat.c" \
    "pgy_host_class_fields_compat_view_from_decl"
require_term "src/codegen/host_decl_compat.c" \
    "pgy_host_class_field_compat_find"
require_term "src/codegen/host_decl_compat.c" \
    "pgy_host_shared_field_compat_find"
require_term "src/codegen/host_decl_compat.c" \
    "ast_class_fields(decl, &view.count)"
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "transpiler_constructor_find_mir_channel_field"
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "transpiler_active_decl_header(ctx, host_name)"
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "mir_decl_header_field_count(header)"
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "transpiler_mir_decl_field_type_name(field)"
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "transpiler_type_name_is_channel(type_name)"
for term in \
    "TranspilerHostedFieldView fields" \
    "transpiler_hosted_class_field_view_from_decl(ctx, decl_name, decl)" \
    "transpiler_hosted_field_view_missing_mir_metadata(&fields)" \
    "transpiler_hosted_field_view_type(view, i)" \
    "transpiler_hosted_field_view_name(view, i)"; do
    require_term "src/codegen/transpiler_constructor_channel_guard.c" "$term"
done
if grep -Fq "pgy_host_class_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/transpiler_constructor_channel_guard.c"; then
    fail "C constructor channel guard must consume TranspilerHostedFieldView for class fields"
fi
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "TranspilerHostedSharedFieldView shared"
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, decl)"
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "transpiler_hosted_shared_field_view_missing_mir_metadata(&shared)"
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "transpiler_hosted_shared_field_view_type(view, i)"
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "TranspilerHostedDomainSlotView slot_view"
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "transpiler_hosted_domain_slot_view_from_decl(ctx, decl_name"
require_term "src/codegen/transpiler_constructor_channel_guard.c" \
    "transpiler_hosted_domain_slot_view_type(view, i)"
if grep -Fq "pgy_host_shared_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/transpiler_constructor_channel_guard.c"; then
    fail "C constructor channel guard must consume TranspilerHostedSharedFieldView"
fi
if grep -Eq 'ast_relation_slots|ast_effect_slots|ast_zone_slots|ast_domain_slot_(name|type)\(' \
    "$ROOT_DIR/src/codegen/transpiler_constructor_channel_guard.c"; then
    fail "C constructor channel guard must consume TranspilerHostedDomainSlotView"
fi
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "llvm_constructor_find_mir_channel_field"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "llvm_find_host_decl_header_in_context(ctx, host_name)"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "mir_decl_header_field(header, i)"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "llvm_mir_decl_field_type(field)"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "mir_decl_header_field_count(header)"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "llvm_mir_decl_field_type_name(field)"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "pgy_classify_type(type_name) == PGY_TK_CHANNEL"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "LLVMHostedFieldView class_fields"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "llvm_hosted_class_field_view_from_decl("
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "llvm_hosted_field_view_missing_mir_metadata("
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "llvm_hosted_field_view_type("
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "LLVMHostedSharedFieldView shared"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "llvm_hosted_shared_field_view_from_decl(ctx, decl_name, decl)"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "llvm_hosted_shared_field_view_missing_mir_metadata(&shared)"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "LLVMHostedDomainSlotView slot_view"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, decl)"
require_term "src/codegen/llvm_expr_constructor_channel_guard.c" \
    "llvm_hosted_domain_slot_view_type(view, i)"
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_hosted_shared_field_view_source_ast(view, i)"
if grep -Fq "pgy_host_shared_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_channel_guard.c"; then
    fail "LLVM constructor calls must consume LLVMHostedSharedFieldView"
fi
if grep -Fq "pgy_host_class_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_channel_guard.c"; then
    fail "LLVM constructor calls must consume LLVMHostedFieldView"
fi
if grep -Eq 'ast_relation_slots|ast_effect_slots|ast_zone_slots|ast_domain_slot_(name|type)\(' \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_channel_guard.c"; then
    fail "LLVM constructor calls must consume LLVMHostedDomainSlotView"
fi
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_shared_field_view_from_decl(ctx, name, node)"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_shared_field_view_missing_mir_metadata("
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "TranspilerHostedDomainSlotView slot_view"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_domain_slot_view_from_decl(ctx, name, node)"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_domain_slot_view_missing_mir_metadata("
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_emit_zone_required_specializations(ctx"
require_term "src/codegen/transpiler_zone_specialization_emit.c" \
    "const TranspilerHostedDomainSlotView *slot_view"
require_term "src/codegen/transpiler_zone_specialization_emit.c" \
    "transpiler_hosted_domain_slot_view_type(slot_view, i)"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "emit_domain_projection_sync_loop_from_view(ctx"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "TranspilerHostedZoneLayerSlotView layer_view"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_from_decl(ctx, name, node)"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_missing_mir_metadata("
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_is_relation(&layer_view, i)"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_is_pool(&layer_view, i)"
if grep -Eq 'transpiler_hosted_zone_layer_slot_view_source_ast\(|ast_zone_layer_slot_' \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.c"; then
    fail "C zone sync emission must consume zone layer-slot metadata, not source AST slots"
fi
if grep -Fq "ast_zone_layer_slots" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.c"; then
    fail "C zone sync emission must consume TranspilerHostedZoneLayerSlotView"
fi
if grep -Eq 'ast_zone_slots|ast_domain_slot_type\(' \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_zone_specialization_emit.c"; then
    fail "C zone specialization emission must consume TranspilerHostedDomainSlotView"
fi
require_term "src/codegen/transpiler_zone_frontier_emit.c" \
    "const TranspilerHostedZoneLayerSlotView *layer_view"
require_term "src/codegen/transpiler_zone_frontier_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_name(layer_view, i)"
require_term "src/codegen/transpiler_zone_specialization_emit.c" \
    "transpiler_hosted_shared_field_view_type(shared_view, i)"
if grep -Fq "pgy_host_shared_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.c"; then
    fail "C zone declaration emission must consume TranspilerHostedSharedFieldView"
fi
for rel in \
    "src/codegen/transpiler_relation_effect_emit.c" \
    "src/codegen/transpiler_world_select_event_emit.c" \
    "src/codegen/transpiler_zone_struct_emit.c"; do
    require_term "$rel" "transpiler_hosted_shared_field_view_from_decl(ctx, name, node)"
    require_term "$rel" "transpiler_hosted_shared_field_view_missing_mir_metadata(&shared_view)"
    require_term "$rel" "transpiler_hosted_shared_field_view_name(&shared_view, i)"
    require_term "$rel" "transpiler_hosted_shared_field_view_type(&shared_view, i)"
    if grep -Fq "pgy_host_shared_fields_compat_view_from_decl" "$ROOT_DIR/$rel"; then
        fail "$rel must consume TranspilerHostedSharedFieldView, not shared field compatibility view"
    fi
done
require_term "src/codegen/transpiler_zone_struct_emit.c" \
    "TranspilerHostedZoneLayerSlotView layer_view"
require_term "src/codegen/transpiler_zone_struct_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_from_decl(ctx, name, node)"
require_term "src/codegen/transpiler_zone_struct_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_missing_mir_metadata("
require_term "src/codegen/transpiler_zone_struct_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/transpiler_zone_struct_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_type_name(&layer_view, i)"
require_term "src/codegen/transpiler_zone_struct_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_is_pool(&layer_view, i)"
require_term "src/codegen/transpiler_zone_struct_emit.c" \
    "transpiler_hosted_zone_layer_slot_view_pool_capacity("
if grep -Eq 'transpiler_hosted_zone_layer_slot_view_source_ast\(|ast_zone_layer_slot_' \
    "$ROOT_DIR/src/codegen/transpiler_zone_struct_emit.c"; then
    fail "C zone struct emission must consume zone layer-slot metadata, not source AST slots"
fi
if grep -Fq "ast_zone_layer_slots" \
    "$ROOT_DIR/src/codegen/transpiler_zone_struct_emit.c"; then
    fail "C zone struct emission must consume TranspilerHostedZoneLayerSlotView"
fi
require_term "src/codegen/llvm_domain_struct_register.c" \
    "LLVMHostedSharedFieldView shared_view"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_shared_field_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_shared_field_view_missing_mir_metadata("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_shared_field_view_type(&shared_view, j)"
if grep -Fq "pgy_host_shared_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c"; then
    fail "LLVM domain struct registration must consume LLVMHostedSharedFieldView"
fi
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_shared_field_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_shared_field_view_missing_mir_metadata(&shared_view)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_shared_field_view_name(shared_view, j)"
if grep -Fq "pgy_host_shared_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register_fields.c"; then
    fail "LLVM domain struct field registration must consume LLVMHostedSharedFieldView"
fi
for term in \
    "transpiler_hosted_field_view_find_index" \
    "transpiler_hosted_field_view_is_subject_like"; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
    require_term "src/codegen/transpiler_decl_field_view.c" "$term"
done
for term in \
    "llvm_hosted_field_view_find_index" \
    "llvm_hosted_field_view_is_subject_like"; do
    require_term "src/codegen/llvm_inventory_decl_lookup.h" "$term"
    require_term "src/codegen/llvm_inventory_field_view.c" "$term"
done
for rel in \
    "src/codegen/llvm_channel_target.c" \
    "src/codegen/llvm_domain_lookup.c"; do
    require_term "$rel" "llvm_hosted_class_field_view_from_decl("
    require_term "$rel" "llvm_hosted_field_view_missing_mir_metadata("
    require_term "$rel" "llvm_hosted_field_view_find_index("
    require_term "$rel" "llvm_hosted_field_view_type("
done
for rel in \
    "src/codegen/transpiler_nominal.c" \
    "src/codegen/transpiler_overlay_host_fields.c" \
    "src/codegen/transpiler_projection_field_path.c"; do
    require_term "$rel" "transpiler_hosted_class_field_view_from_decl("
    require_term "$rel" "transpiler_hosted_field_view_missing_mir_metadata("
    require_term "$rel" "transpiler_hosted_field_view_find_index("
done
require_term "src/codegen/transpiler_projection_field_path.c" \
    "transpiler_hosted_field_view_is_subject_like("
require_term "src/codegen/transpiler_projection_method_invalidation.c" \
    "host_projection_subject_field_type_name("
for term in \
    "transpiler_find_decl_field_metadata(ctx, host_type_name" \
    "transpiler_mir_decl_field_is_subject_like(mir_field)"; do
    require_term "src/codegen/transpiler_projection_field_path.c" "$term"
done
require_term "src/codegen/transpiler_decl_lookup.c" \
    "transpiler_mir_decl_field_is_subject_like"
for rel in \
    "src/codegen/transpiler_expr_type_infer.c" \
    "src/codegen/transpiler_mir_local_type_lookup.c"; do
    require_term "$rel" "transpiler_lookup_nominal_host_member_type_name("
    if grep -Fq "pgy_host_class_field_compat_find" "$ROOT_DIR/$rel"; then
        fail "$rel must consume nominal host member type lookup instead of reopening class fields"
    fi
done
for rel in \
    "src/codegen/transpiler_overlay_host_fields.c" \
    "src/codegen/transpiler_projection.c"; do
    require_term "$rel" "transpiler_hosted_shared_field_view_from_decl("
    require_term "$rel" \
        "transpiler_hosted_shared_field_view_missing_mir_metadata("
    require_term "$rel" "transpiler_hosted_shared_field_view_name("
    if grep -Fq "pgy_host_shared_field_compat_find" "$ROOT_DIR/$rel"; then
        fail "$rel must consume TranspilerHostedSharedFieldView for shared-field presence"
    fi
done
for term in \
    "transpiler_hosted_shared_field_view_from_decl(" \
    "transpiler_hosted_shared_field_view_missing_mir_metadata(" \
    "transpiler_hosted_shared_field_view_metadata(&shared_view, i)" \
    "transpiler_hosted_shared_field_view_type(&shared_view, i)"; do
    require_term "src/codegen/transpiler_nominal.c" "$term"
done
if grep -Fq "pgy_host_shared_field_compat_find" \
    "$ROOT_DIR/src/codegen/transpiler_nominal.c"; then
    fail "C nominal shared member type lookup must consume TranspilerHostedSharedFieldView"
fi
for rel in \
    "src/codegen/llvm_domain_projection_value_helpers.c" \
    "src/codegen/llvm_expr_projection_path_helpers.c"; do
    require_term "$rel" "llvm_hosted_class_field_view_from_decl("
    require_term "$rel" "llvm_hosted_field_view_missing_mir_metadata("
    require_term "$rel" "llvm_hosted_field_view_name("
    require_term "$rel" "llvm_hosted_field_view_metadata("
    require_term "$rel" "llvm_hosted_field_view_type("
    if grep -Fq "pgy_host_class_fields_compat_view_from_decl" "$ROOT_DIR/$rel"; then
        fail "$rel must consume LLVMHostedFieldView"
    fi
done
require_term "src/codegen/transpiler_let_emit.c" \
    "transpiler_emit_class_constructor_with_type("
if grep -Fq "pgy_host_class_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/transpiler_let_emit.c"; then
    fail "C annotated let class constructor lowering must consume the shared class-constructor owner"
fi
for term in \
    "host_projection_class_field_info" \
    "TranspilerHostedFieldView field_view" \
    "transpiler_hosted_class_field_view_from_decl(" \
    "transpiler_hosted_field_view_missing_mir_metadata(&field_view)" \
    "transpiler_hosted_field_view_find_index(" \
    "transpiler_hosted_field_view_metadata(view, index)" \
    "transpiler_mir_decl_field_type_name(field)" \
    "transpiler_hosted_field_view_type(view, index)"; do
    require_term "src/codegen/transpiler_projection_field_path.c" "$term"
done
for term in \
    "TranspilerHostedZoneLayerSlotView layer_view" \
    "transpiler_hosted_zone_layer_slot_view_from_decl(" \
    "transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(" \
    "transpiler_hosted_zone_layer_slot_view_name(&layer_view, i)" \
    "transpiler_zone_has_layer_slot("; do
    require_term "src/codegen/transpiler_projection.c" "$term"
done
if grep -Fq "pgy_host_class_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C projection class-field iteration must consume TranspilerHostedFieldView"
fi
if grep -Eq 'transpiler_hosted_(zone_layer|world_zone)_slot_view_source_ast\(|ast_zone_layer_slots' \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C projection zone/world slot lookup must answer metadata queries without source AST slots"
fi
for term in \
    "overlay_projection_field_view" \
    "transpiler_hosted_class_field_view_from_decl(" \
    "transpiler_hosted_field_view_missing_mir_metadata(view)" \
    "transpiler_hosted_field_view_name(&view, index)"; do
    require_term "src/codegen/transpiler_overlay_projection.c" "$term"
done
if grep -Fq "pgy_host_class_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_projection.c"; then
    fail "C overlay projection field iteration must consume TranspilerHostedFieldView"
fi
for term in \
    "transpiler_hosted_class_field_view_from_decl(" \
    "transpiler_hosted_field_view_missing_mir_metadata(&field_view)" \
    "transpiler_hosted_field_view_type(&field_view, i)" \
    "transpiler_hosted_field_view_name(&field_view, i)"; do
    require_term "src/codegen/transpiler_class_constructor_emit.c" "$term"
done
if grep -Fq "pgy_host_class_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/transpiler_class_constructor_emit.c"; then
    fail "C domain constructor class-field emission must consume TranspilerHostedFieldView"
fi
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, decl)"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, party_decl)"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, roster_decl)"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, zone_decl)"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, world_decl)"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_shared_field_view_missing_mir_metadata(&shared_view)"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_shared_field_view_source_ast("
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_shared_field_view_type("
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_domain_slot_view_is_projection_slot"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_domain_slot_view_is_projection_slot("
if grep -Fq "ast_domain_slot_is_tobject" \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"; then
    fail "C domain constructor dirty initialization must consume TranspilerHostedDomainSlotView"
fi
if grep -Fq "pgy_host_shared_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"; then
    fail "C domain constructor emission must consume TranspilerHostedSharedFieldView"
fi
if grep -Eq 'ast_(party|roster)_shared_count|ast_(party|roster)_shared\(|ast_party_shared_(name|type)\(' \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"; then
    fail "C party/roster constructors must consume TranspilerHostedSharedFieldView for shared-field name/type/count"
fi
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "transpiler_hosted_shared_field_view_from_decl("
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "transpiler_hosted_shared_field_view_missing_mir_metadata("
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "transpiler_hosted_shared_field_view_name(&shared_view, i)"
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "TranspilerHostedZoneLayerSlotView layer_view"
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "transpiler_hosted_zone_layer_slot_view_from_decl("
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "transpiler_hosted_zone_layer_slot_view_missing_mir_metadata("
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "transpiler_hosted_zone_layer_slot_view_name("
if grep -Fq "ast_zone_layer_slots" \
    "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"; then
    fail "C MIR SSA implicit zone layer-field recovery must consume TranspilerHostedZoneLayerSlotView"
fi
if grep -Fq "pgy_host_shared_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"; then
    fail "C MIR SSA implicit zone shared-field recovery must consume TranspilerHostedSharedFieldView"
fi
require_term "src/codegen/llvm_domain_decl_parts_helpers.c" \
    "llvm_domain_decl_refreshes(ASTNode *stmt"
require_term "src/codegen/llvm_domain_decl_parts_helpers.c" \
    "*decl_name = llvm_decl_node_name(stmt)"
if grep -Fq "pgy_host_shared_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/llvm_domain_decl_parts_helpers.c"; then
    fail "LLVM domain decl parts must not reopen shared-field compatibility views"
fi
if grep -Eq 'ast_(relation|effect|zone)_slots\(' \
    "$ROOT_DIR/src/codegen/llvm_domain_decl_parts_helpers.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_decl_parts_helpers.h"; then
    fail "LLVM domain decl refresh helper must not expose raw domain slot arrays"
fi
for rel in \
    "src/codegen/llvm_domain_struct_register.c" \
    "src/codegen/llvm_domain_struct_register_fields.c"; do
    if grep -Fq "ast_roster_shared" "$ROOT_DIR/$rel"; then
        fail "$rel must consume LLVMHostedSharedFieldView for roster shared fields"
    fi
done
require_term "src/codegen/transpiler_decl_lookup.h" \
    "TranspilerHostedRosterSlotView"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_roster_slot_view_from_decl"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_roster_slot_view_type_name"
require_term "src/codegen/transpiler_roster_decl_emit.c" \
    "TranspilerHostedRosterSlotView roster_view"
require_term "src/codegen/transpiler_roster_decl_emit.c" \
    "transpiler_hosted_roster_slot_view_from_decl("
require_term "src/codegen/transpiler_roster_decl_emit.c" \
    "transpiler_hosted_roster_slot_view_type_name("
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "TranspilerHostedRosterSlotView roster_view"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_roster_slot_view_name("
require_term "src/codegen/transpiler_overlay_host_fields.c" \
    "TranspilerHostedRosterSlotView roster_view"
require_term "src/codegen/transpiler_overlay_host_fields.c" \
    "transpiler_hosted_roster_slot_view_name(&roster_view, i)"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "LLVMHostedRosterSlotView"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_roster_slot_view_from_decl"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_roster_slot_view_type_name"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "LLVMHostedRosterSlotView roster_view"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_roster_slot_view_from_decl("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_roster_slot_view_type_name("
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "LLVMHostedRosterSlotView roster_view"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_roster_slot_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_roster_slot_view_name(&roster_view, j)"
roster_slot_consumer_hits="$(
    grep -RInE 'ast_roster_party|ast_roster_slot_(name|party_type)' \
        "$ROOT_DIR/src/codegen" | \
        grep -v 'src/codegen/transpiler_decl_slot_view.c' | \
        grep -v 'src/codegen/transpiler_decl_role_roster_slot_view.c' | \
        grep -v 'src/codegen/llvm_inventory_role_roster_slot_view.c' || true
)"
if [[ -n "$roster_slot_consumer_hits" ]]; then
    fail "Codegen roster-slot consumers must use hosted roster-slot views:
$roster_slot_consumer_hits"
fi
require_term "src/codegen/transpiler_decl_lookup.h" \
    "TranspilerHostedDomainSlotView"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_domain_slot_view_from_decl"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_domain_slot_view_is_subject_like"
require_term "src/codegen/transpiler_relation_effect_emit.c" \
    "TranspilerHostedDomainSlotView slot_view"
require_term "src/codegen/transpiler_relation_effect_emit.c" \
    "transpiler_hosted_domain_slot_view_from_decl("
require_term "src/codegen/transpiler_relation_effect_emit.c" \
    "transpiler_hosted_domain_slot_view_type("
require_term "src/codegen/transpiler_relation_effect_emit.c" \
    "transpiler_hosted_domain_slot_view_is_subject_like("
require_term "src/codegen/transpiler_zone_struct_emit.c" \
    "TranspilerHostedDomainSlotView slot_view"
require_term "src/codegen/transpiler_zone_struct_emit.c" \
    "transpiler_hosted_domain_slot_view_from_decl("
require_term "src/codegen/transpiler_zone_struct_emit.c" \
    "transpiler_hosted_domain_slot_view_type("
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "TranspilerHostedDomainSlotView slot_view"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_domain_slot_view_from_decl("
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_domain_slot_view_is_projection_slot("
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "LLVMHostedDomainSlotView"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_domain_slot_view_from_decl"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_domain_slot_view_is_subject_like"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "LLVMHostedDomainSlotView domain_slot_view"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_domain_slot_view_from_decl("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_domain_slot_view_type("
if grep -Eq 'llvm_hosted_(domain|roster|world_roster|world_zone)_slot_view_source_ast\(' \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c"; then
    fail "LLVM domain struct type registration must consume hosted slot metadata, not source AST slot anchors"
fi
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "LLVMHostedDomainSlotView slot_view"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_domain_slot_view_is_subject_like(&slot_view, j)"
require_term "src/codegen/llvm_domain_lookup.c" \
    "LLVMHostedDomainSlotView slot_view"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_domain_slot_view_from_decl(ctx, zone_name"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_domain_slot_view_name(&slot_view, i)"
require_term "src/codegen/llvm_expr_assignment_projection.c" \
    "LLVMHostedDomainSlotView slot_view"
require_term "src/codegen/llvm_expr_assignment_projection.c" \
    "llvm_hosted_domain_slot_view_name(&slot_view, i)"
require_term "src/codegen/llvm_internal_api.h" \
    "bool llvm_zone_has_domain_slot(LLVMGenCtx *ctx"
require_term "src/codegen/llvm_expr_assignment_projection.c" \
    "llvm_zone_has_domain_slot(ctx, zone_decl,"
domain_slot_layout_consumer_hits="$(
    grep -RInE 'ast_relation_slots|ast_effect_slots|ast_zone_slots|ast_domain_slot_(name|type)\(' \
        "$ROOT_DIR/src/codegen/transpiler_relation_effect_emit.c" \
        "$ROOT_DIR/src/codegen/transpiler_zone_struct_emit.c" \
        "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c" \
        "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c" \
        "$ROOT_DIR/src/codegen/llvm_domain_struct_register_fields.c" \
        "$ROOT_DIR/src/codegen/llvm_domain_lookup.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_assignment_projection.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_domain_query_calls.c" || true
)"
if [[ -n "$domain_slot_layout_consumer_hits" ]]; then
    fail "C/LLVM domain layout consumers must use hosted domain-slot views:
$domain_slot_layout_consumer_hits"
fi
require_term "src/codegen/transpiler_nominal.c" \
    "TranspilerHostedDomainSlotView slot_view"
require_term "src/codegen/transpiler_nominal.c" \
    "transpiler_hosted_domain_slot_view_metadata(&slot_view, i)"
require_term "src/codegen/transpiler_nominal.c" \
    "transpiler_hosted_domain_slot_view_type(&slot_view, i)"
require_term "src/codegen/transpiler_overlay_host_fields.c" \
    "TranspilerHostedDomainSlotView slot_view"
require_term "src/codegen/transpiler_overlay_host_fields.c" \
    "transpiler_hosted_domain_slot_view_name(&slot_view, i)"
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "TranspilerHostedDomainSlotView slot_view"
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "transpiler_hosted_domain_slot_view_name(&slot_view, i)"
domain_slot_c_lookup_consumer_hits="$(
    grep -RInE 'ast_relation_slots|ast_effect_slots|ast_zone_slots|ast_domain_slot_(name|type)\(' \
        "$ROOT_DIR/src/codegen/transpiler_nominal.c" \
        "$ROOT_DIR/src/codegen/transpiler_overlay_host_fields.c" \
        "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c" || true
)"
if [[ -n "$domain_slot_c_lookup_consumer_hits" ]]; then
    fail "C domain lookup/implicit-field consumers must use hosted domain-slot views:
$domain_slot_c_lookup_consumer_hits"
fi
require_term "src/codegen/llvm_domain_struct_register.c" \
    "LLVMHostedZoneLayerSlotView layer_view"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_zone_layer_slot_view_is_pool("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_zone_layer_slot_view_pool_capacity("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_zone_layer_slot_view_type_name("
if grep -Fq "ast_zone_layer_slots" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c"; then
    fail "LLVM zone struct type registration must consume LLVMHostedZoneLayerSlotView"
fi
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "LLVMHostedZoneLayerSlotView layer_view"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_zone_layer_slot_view_name(&layer_view, j)"
if grep -Fq "ast_zone_layer_slots" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register_fields.c"; then
    fail "LLVM zone struct field registration must consume LLVMHostedZoneLayerSlotView"
fi
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "LLVMHostedWorldZoneSlotView"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_world_zone_slot_view_from_decl"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_world_zone_slot_view_type_name"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "LLVMHostedWorldRosterSlotView"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_world_roster_slot_view_from_decl"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_world_roster_slot_view_type_name"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "LLVMHostedWorldRosterSlotView roster_view"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_world_roster_slot_view_from_decl("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_world_roster_slot_view_missing_mir_metadata("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_world_roster_slot_view_type_name("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "LLVMHostedWorldZoneSlotView zone_view"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_world_zone_slot_view_from_decl(ctx, decl_name"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_world_zone_slot_view_missing_mir_metadata("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_world_zone_slot_view_type_name("
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "LLVMHostedWorldZoneSlotView zone_view"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "LLVMHostedWorldRosterSlotView roster_view"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_world_roster_slot_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_world_roster_slot_view_missing_mir_metadata("
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_world_roster_slot_view_name(&roster_view, j)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_world_zone_slot_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_world_zone_slot_view_missing_mir_metadata(&zone_view)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_world_zone_slot_view_name(&zone_view, j)"
llvm_world_zone_consumer_hits="$(
    grep -RInE 'ast_world_zones|ast_world_zone_(slot_name|type_name)' \
        "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c" \
        "$ROOT_DIR/src/codegen/llvm_domain_struct_register_fields.c" || true
)"
if [[ -n "$llvm_world_zone_consumer_hits" ]]; then
    fail "LLVM world zone-slot struct registration must consume LLVMHostedWorldZoneSlotView:
$llvm_world_zone_consumer_hits"
fi
require_term "src/codegen/transpiler_mir_local_type_lookup.c" \
    "transpiler_decl_name_local(host_decl)"
require_term "src/codegen/transpiler_nominal.c" \
    "TranspilerHostedZoneLayerSlotView layer_view"
require_term "src/codegen/transpiler_nominal.c" \
    "transpiler_hosted_zone_layer_slot_view_from_decl("
require_term "src/codegen/transpiler_nominal.c" \
    "transpiler_hosted_zone_layer_slot_view_missing_mir_metadata("
require_term "src/codegen/transpiler_nominal.c" \
    "transpiler_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/transpiler_nominal.c" \
    "transpiler_hosted_zone_layer_slot_view_type_name("
if grep -Fq "ast_zone_layer_slots" \
    "$ROOT_DIR/src/codegen/transpiler_nominal.c"; then
    fail "C nominal zone member lookup must consume TranspilerHostedZoneLayerSlotView"
fi
require_term "src/codegen/transpiler_decl_lookup.h" \
    "TranspilerHostedWorldZoneSlotView"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_world_zone_slot_view_from_decl"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_world_zone_slot_view_type_name"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "TranspilerHostedWorldRosterSlotView"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_world_roster_slot_view_from_decl"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_world_roster_slot_view_type_name"
require_term "src/codegen/transpiler_nominal.c" \
    "TranspilerHostedWorldRosterSlotView roster_view"
require_term "src/codegen/transpiler_nominal.c" \
    "transpiler_hosted_world_roster_slot_view_from_decl("
require_term "src/codegen/transpiler_nominal.c" \
    "transpiler_hosted_world_roster_slot_view_type_name("
require_term "src/codegen/transpiler_nominal.c" \
    "TranspilerHostedWorldZoneSlotView zone_view"
require_term "src/codegen/transpiler_nominal.c" \
    "transpiler_hosted_world_zone_slot_view_from_decl(ctx, world_name, decl)"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "TranspilerHostedWorldRosterSlotView roster_view"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_world_roster_slot_view_name("
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "TranspilerHostedWorldZoneSlotView zone_view"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_world_zone_slot_view_missing_mir_metadata("
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "TranspilerHostedWorldRosterSlotView roster_view"
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "transpiler_hosted_world_roster_slot_view_type_name("
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "TranspilerHostedWorldZoneSlotView zone_view"
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "transpiler_hosted_world_zone_slot_view_type_name(&zone_view, i)"
require_term "src/codegen/transpiler_projection.c" \
    "TranspilerHostedWorldRosterSlotView roster_view"
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_hosted_world_roster_slot_view_name(&roster_view, i)"
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "pgy_domain_world_embedded_frontier_count_from_zone_types("
if grep -Fq "pgy_domain_world_embedded_frontier_count(" \
    "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.c"; then
    fail "C world frontier pass-limit selection must consume zone-slot metadata, not the AST world-zone wrapper"
fi
require_term "src/codegen/transpiler_projection.c" \
    "TranspilerHostedWorldZoneSlotView zone_view"
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_world_zone_slot_type_name"
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_hosted_world_zone_slot_view_name(&zone_view, i)"
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_hosted_world_zone_slot_view_type_name("
if grep -Eq 'ast_world_zones|ast_world_zone_(slot_name|type_name)' \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C world zone-slot projection lookup must consume TranspilerHostedWorldZoneSlotView"
fi
require_term "src/codegen/llvm_domain_world_frontier.c" \
    "LLVMHostedWorldZoneSlotView zone_view"
require_term "src/codegen/llvm_domain_world_frontier.c" \
    "llvm_hosted_world_zone_slot_view_from_decl(ctx,"
require_term "src/codegen/llvm_domain_world_frontier.c" \
    "pgy_domain_world_embedded_frontier_count_from_zone_types("
if grep -Fq "pgy_domain_world_embedded_frontier_count(" \
    "$ROOT_DIR/src/codegen/llvm_domain_world_frontier.c"; then
    fail "LLVM world frontier pass-limit selection must consume zone-slot metadata, not the AST world-zone wrapper"
fi
require_term "src/codegen/llvm_domain_lookup.c" \
    "LLVMHostedWorldZoneSlotView zone_view"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_world_zone_slot_view_from_decl(ctx, world_name"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_world_zone_slot_view_name(&zone_view, i)"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_world_zone_slot_view_type_name(&zone_view, i)"
require_term "src/codegen/llvm_domain_lookup.c" \
    "MIR-only LLVM path missing world zone-slot type metadata for"
if grep -Eq 'llvm_hosted_world_zone_slot_view_source_ast\(|ast_world_zones|ast_world_zone_(slot_name|type_name)' \
    "$ROOT_DIR/src/codegen/llvm_domain_lookup.c"; then
    fail "LLVM world zone-slot lookup must consume LLVMHostedWorldZoneSlotView metadata, not source AST slots"
fi
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "LLVMHostedWorldZoneSlotView zone_view"
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_hosted_world_zone_slot_view_from_decl(ctx, world_name"
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_hosted_world_zone_slot_view_name(&zone_view, i)"
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "LLVMHostedDomainSlotView slot_view"
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, decl)"
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_domain_slot_view_is_projection_slot(&slot_view, i"
if grep -Eq 'ast_world_zones|ast_world_zone_(slot_name|type_name)' \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    fail "LLVM world constructor dirty initialization must consume LLVMHostedWorldZoneSlotView"
fi
if grep -Eq 'ast_relation_slots|ast_effect_slots|ast_zone_slots|ast_domain_slot_(name|type|is_subject|is_tobject)\(' \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    fail "LLVM domain constructor dirty initialization must consume LLVMHostedDomainSlotView"
fi
require_term "src/codegen/llvm_domain_world_sync.c" \
    "LLVMHostedWorldZoneSlotView zone_view"
require_term "src/codegen/llvm_domain_world_sync.c" \
    "llvm_hosted_world_zone_slot_view_from_decl(ctx, world_name, stmt)"
require_term "src/codegen/llvm_domain_world_sync.c" \
    "llvm_hosted_world_zone_slot_view_name(&zone_view, i)"
require_term "src/codegen/llvm_domain_world_sync_directives.c" \
    "LLVMHostedWorldZoneSlotView zone_view"
require_term "src/codegen/llvm_domain_world_sync_directives.c" \
    "llvm_hosted_world_zone_slot_view_from_decl(ctx, world_name"
require_term "src/codegen/llvm_domain_world_frontier_derived.c" \
    "llvm_world_sync_has_zone_slot(ctx, stmt, input_name)"
for rel in \
    "src/codegen/llvm_domain_world_sync.c" \
    "src/codegen/llvm_domain_world_sync_directives.c"; do
    if grep -Eq 'ast_world_zones|ast_world_zone_(slot_name|type_name)' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must consume LLVMHostedWorldZoneSlotView for world zone-slot lookup"
    fi
done
require_term "src/codegen/llvm_domain_world_frontier_internal.h" \
    "const LLVMHostedWorldZoneSlotView *zone_view"
require_term "src/codegen/llvm_domain_world_frontier_zones.c" \
    "const LLVMHostedWorldZoneSlotView *zone_view"
require_term "src/codegen/llvm_domain_world_frontier_zones.c" \
    "llvm_hosted_world_zone_slot_view_name(zone_view, i)"
require_term "src/codegen/llvm_domain_world_frontier_zones.c" \
    "llvm_hosted_world_zone_slot_view_type_name(zone_view, i)"
for rel in \
    "src/codegen/llvm_domain_world_frontier.c" \
    "src/codegen/llvm_domain_world_frontier_zones.c"; do
    if grep -Eq 'ast_world_zones|ast_world_zone_(slot_name|type_name)' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must consume LLVMHostedWorldZoneSlotView for world frontier zone-slot emission"
    fi
done
c_world_zone_consumer_hits="$(
    grep -RInE 'ast_world_zones|ast_world_zone_(slot_name|type_name)' \
        "$ROOT_DIR/src/codegen/transpiler_nominal.c" \
        "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c" \
        "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.c" || true
)"
if [[ -n "$c_world_zone_consumer_hits" ]]; then
    fail "C world zone-slot consumers must consume TranspilerHostedWorldZoneSlotView:
$c_world_zone_consumer_hits"
fi
world_roster_consumer_hits="$(
    grep -RInE 'ast_world_rosters|ast_world_roster_(slot_name|type_name)' \
        "$ROOT_DIR/src/codegen" | \
        grep -v 'src/codegen/transpiler_decl_slot_view.c' | \
        grep -v 'src/codegen/transpiler_decl_role_roster_slot_view.c' | \
        grep -v 'src/codegen/llvm_inventory_role_roster_slot_view.c' || true
)"
if [[ -n "$world_roster_consumer_hits" ]]; then
    fail "Codegen world roster-slot consumers must use hosted roster-slot views:
$world_roster_consumer_hits"
fi
require_term "src/codegen/transpiler_overlay_host_fields.c" \
    "transpiler_decl_name_local(host_decl)"
require_term "src/codegen/transpiler_overlay_host_fields.c" \
    "TranspilerHostedZoneLayerSlotView layer_view"
require_term "src/codegen/transpiler_overlay_host_fields.c" \
    "transpiler_hosted_zone_layer_slot_view_from_decl("
require_term "src/codegen/transpiler_overlay_host_fields.c" \
    "transpiler_hosted_zone_layer_slot_view_missing_mir_metadata("
require_term "src/codegen/transpiler_overlay_host_fields.c" \
    "transpiler_hosted_zone_layer_slot_view_name(&layer_view, i)"
if grep -Fq "ast_zone_layer_slots" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_host_fields.c"; then
    fail "C overlay zone field lookup must consume TranspilerHostedZoneLayerSlotView"
fi
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "transpiler_find_zone_layer_slot_local("
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "TranspilerHostedZoneLayerSlotView layer_view"
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "transpiler_hosted_zone_layer_slot_view_from_decl("
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "transpiler_hosted_zone_layer_slot_view_missing_mir_metadata("
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "transpiler_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "transpiler_hosted_zone_layer_slot_view_type_name("
if grep -Fq "ast_zone_layer_slots" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_bind.c"; then
    fail "C overlay zone effect bind must consume TranspilerHostedZoneLayerSlotView"
fi
require_term "src/codegen/transpiler_overlay_zone_relation_bind.c" \
    "transpiler_find_zone_layer_slot_local(ctx, zone, layer_slot_name"
if grep -Fq "ast_zone_layer_slots" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_relation_bind.c"; then
    fail "C overlay zone relation bind must consume TranspilerHostedZoneLayerSlotView"
fi
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "effect_name = llvm_decl_node_name(effect_decl)"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "relation_name = llvm_decl_node_name(relation_decl)"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "LLVMHostedZoneLayerSlotView layer_view"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, zone_decl)"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "llvm_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "llvm_hosted_zone_layer_slot_view_type_name(&layer_view, i)"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "llvm_hosted_zone_layer_slot_view_is_relation(&layer_view, i)"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "llvm_hosted_zone_layer_slot_view_is_pool(&layer_view, i)"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "llvm_hosted_zone_layer_slot_view_pool_capacity("
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "LLVMHostedDomainSlotView effect_slot_view"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "LLVMHostedDomainSlotView relation_slot_view"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "llvm_domain_slot_view_bindable_name("
if grep -Eq 'ast_zone_layer_slots|ast_zone_layer_slot_|ast_effect_slots|ast_relation_slots|ast_domain_slot_name\(|llvm_find_nth_bindable_domain_slot' \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_bind_lowering.c"; then
    fail "LLVM zone bind emission must consume hosted zone/domain slot views"
fi
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, zone_decl)"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/llvm_domain_lookup.c" \
    "MIR-only LLVM path missing zone layer-slot metadata for"
require_term "src/codegen/llvm_internal_api.h" \
    "bool llvm_zone_has_layer_slot(LLVMGenCtx *ctx"
require_term "src/codegen/llvm_expr_domain_query_calls.c" \
    "llvm_zone_has_layer_slot(ctx, zone_decl, detail_name)"
if grep -Eq 'llvm_hosted_(world_zone|domain|zone_layer)_slot_view_source_ast\(|ast_zone_layer_slots' \
    "$ROOT_DIR/src/codegen/llvm_domain_lookup.c"; then
    fail "LLVM domain lookup must answer slot existence/type queries from metadata, not source AST slots"
fi
require_term "src/codegen/llvm_domain_zone_sync_clauses.c" \
    "LLVMHostedZoneLayerSlotView layer_view"
require_term "src/codegen/llvm_domain_zone_sync_clauses.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, stmt)"
require_term "src/codegen/llvm_domain_zone_sync_clauses.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)"
require_term "src/codegen/llvm_domain_zone_sync_clauses.c" \
    "llvm_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/llvm_domain_zone_sync_clauses.c" \
    "llvm_hosted_zone_layer_slot_view_is_relation(&layer_view, i)"
if grep -Eq 'llvm_hosted_zone_layer_slot_view_source_ast\(&layer_view, i\)|ast_zone_layer_slot_|ast_zone_layer_slots' \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_sync_clauses.c"; then
    fail "LLVM zone sync clauses must consume LLVMHostedZoneLayerSlotView metadata, not source AST slots"
fi
require_term "src/codegen/llvm_domain_zone_sync.c" \
    "LLVMHostedZoneLayerSlotView layer_view"
require_term "src/codegen/llvm_domain_zone_sync.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_zone_sync.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)"
require_term "src/codegen/llvm_domain_zone_sync.c" \
    "pgy_domain_zone_frontier_pass_limit_from_counts("
if grep -Fq "pgy_domain_zone_frontier_pass_limit(stmt)" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_sync.c"; then
    fail "LLVM zone sync frontier limit must consume metadata counts"
fi
require_term "src/codegen/llvm_domain_zone_frontier_state.c" \
    "LLVMHostedZoneLayerSlotView layer_view"
require_term "src/codegen/llvm_domain_zone_frontier_state.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, stmt)"
require_term "src/codegen/llvm_domain_zone_frontier_state.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)"
require_term "src/codegen/llvm_domain_zone_frontier_state.c" \
    "llvm_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/llvm_domain_zone_frontier_state.c" \
    "llvm_hosted_zone_layer_slot_view_is_pool(&layer_view, i)"
require_term "src/codegen/llvm_domain_zone_frontier_state.c" \
    "llvm_hosted_zone_layer_slot_view_pool_capacity(&layer_view, i)"
if grep -Eq 'llvm_hosted_zone_layer_slot_view_source_ast\(&layer_view, i\)|ast_zone_layer_slot_|ast_zone_layer_slots' \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_frontier_state.c"; then
    fail "LLVM zone frontier state must consume LLVMHostedZoneLayerSlotView metadata, not source AST slots"
fi
require_term "src/codegen/llvm_intent_effect.c" \
    "LLVMHostedZoneLayerSlotView layer_view"
require_term "src/codegen/llvm_intent_effect.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, zone_decl)"
require_term "src/codegen/llvm_intent_effect.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)"
require_term "src/codegen/llvm_intent_effect.c" \
    "llvm_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/llvm_intent_effect.c" \
    "llvm_hosted_zone_layer_slot_view_type_name(&layer_view, i)"
require_term "src/codegen/llvm_intent_effect.c" \
    "llvm_hosted_zone_layer_slot_view_is_relation(&layer_view, i)"
if grep -Eq 'llvm_hosted_zone_layer_slot_view_source_ast\(&layer_view, i\)|ast_zone_layer_slot_|ast_zone_layer_slots' \
    "$ROOT_DIR/src/codegen/llvm_intent_effect.c"; then
    fail "LLVM intent effect emission must consume LLVMHostedZoneLayerSlotView metadata, not source AST slots"
fi
require_term "src/codegen/llvm_decl_authority.c" \
    "zone_name = llvm_decl_node_name(zone_decl)"
require_term "src/codegen/llvm_register.c" \
    "enum_name = llvm_decl_node_name(stmt)"
require_term "src/codegen/llvm_register.c" \
    "cls_name = llvm_decl_node_name(stmt)"
require_term "src/codegen/llvm_register.c" \
    "llvm_hosted_class_field_view_from_decl(ctx, cls_name, stmt)"
require_term "src/codegen/llvm_register.c" \
    "llvm_hosted_field_view_missing_mir_metadata(&field_view)"
require_term "src/codegen/llvm_register.c" \
    "llvm_hosted_field_view_type(&field_view, j)"
require_term "src/codegen/llvm_register.c" \
    "llvm_hosted_field_view_name(&field_view, j)"
if grep -Fq "pgy_host_class_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/llvm_register.c"; then
    fail "LLVM nominal class registration must consume LLVMHostedFieldView"
fi
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_hosted_class_field_view_from_decl(ctx, name, node)"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_hosted_class_field_view_from_decl(ctx, base_class_name"
require_term "src/codegen/transpiler_class_constructor_emit.c" \
    "transpiler_hosted_class_field_view_from_decl("
for rel in \
    "src/codegen/transpiler_class_decl_emit.c" \
    "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "src/codegen/transpiler_class_constructor_emit.c"; do
    require_term "$rel" "transpiler_hosted_field_view_missing_mir_metadata(&field_view)"
    require_term "$rel" "transpiler_hosted_field_view_name(&field_view, i)"
    require_term "$rel" "transpiler_hosted_field_view_type(&field_view, i)"
    if grep -Fq "pgy_host_class_fields_compat_view_from_decl" "$ROOT_DIR/$rel"; then
        fail "$rel must consume TranspilerHostedFieldView, not class field compatibility view"
    fi
done
require_term "src/codegen/llvm_domain_forward_role.c" \
    "role_name = llvm_decl_node_name(stmt)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "const char *role_name = llvm_decl_node_name(stmt)"
require_term "src/codegen/llvm_registry.c" \
    "if (ctx == NULL || class_name == NULL)"
require_term "src/codegen/llvm_expr_assignment_projection.c" \
    "world_name = llvm_decl_node_name(host_decl)"
require_term "src/codegen/llvm_expr_assignment_projection.c" \
    "zone_name = llvm_decl_node_name(zone_decl)"
require_term "src/codegen/llvm_stmt_zone_action.c" \
    "zone_name = llvm_decl_node_name(zone_decl)"
require_term "src/codegen/llvm_stmt_zone_action.c" \
    "LLVMHostedZoneLayerSlotView layer_view"
require_term "src/codegen/llvm_stmt_zone_action.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, zone_decl)"
require_term "src/codegen/llvm_stmt_zone_action.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)"
require_term "src/codegen/llvm_stmt_zone_action.c" \
    "llvm_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/llvm_stmt_zone_action.c" \
    "llvm_hosted_zone_layer_slot_view_type_name(&layer_view, i)"
require_term "src/codegen/llvm_stmt_zone_action.c" \
    "llvm_hosted_zone_layer_slot_view_is_relation(&layer_view, i)"
require_term "src/codegen/llvm_stmt_zone_action.c" \
    "LLVMHostedDomainSlotView slot_view"
require_term "src/codegen/llvm_stmt_zone_action.c" \
    "llvm_hosted_domain_slot_view_is_subject_like(&slot_view, i)"
require_term "src/codegen/llvm_stmt_zone_action.c" \
    "llvm_stmt_first_effect_subject_slot_name(ctx,"
if grep -Eq 'ast_zone_slots|ast_effect_slots|ast_domain_slot_(name|type|is_subject)\(|ast_zone_layer_slot_(is_relation|is_pool|pool_capacity)' \
    "$ROOT_DIR/src/codegen/llvm_stmt_zone_action.c"; then
    fail "LLVM zone action emission must consume hosted domain/zone metadata views"
fi
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "LLVMHostedZoneLayerSlotView layer_view"
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, zone_decl)"
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)"
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_hosted_zone_layer_slot_view_type_name(&layer_view, i)"
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_hosted_zone_layer_slot_view_is_relation(&layer_view, i)"
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_hosted_zone_layer_slot_view_is_pool(&layer_view, i)"
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_hosted_zone_layer_slot_view_pool_capacity(&layer_view, i)"
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_call_find_first_effect_subject_slot_name(ctx,"
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_hosted_domain_slot_view_is_subject_like(&slot_view, i)"
if grep -Eq 'ast_effect_slots|ast_domain_slot_(name|is_subject)\(|ast_zone_layer_slot_(is_relation|is_pool|pool_capacity)' \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_world_effect_sync.c"; then
    fail "LLVM world effect sync must consume hosted domain/zone metadata views"
fi
for term in \
    "llvm_domain_slot_view_is_projection_slot" \
    "llvm_count_domain_projection_slots_in_view"; do
    require_term "src/codegen/llvm_domain_projection_count_helpers.h" "$term"
    require_term "src/codegen/llvm_domain_projection_count.c" "$term"
done
require_term "src/codegen/llvm_domain_projection_count.c" \
    "llvm_hosted_domain_slot_view_is_tobject_like(slot_view, index)"
require_term "src/codegen/llvm_domain_projection_count.c" \
    "llvm_hosted_domain_slot_view_name(slot_view, index)"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_domain_slot_view_is_tobject_like"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_domain_slot_view_is_binding_like"
require_term "src/codegen/llvm_inventory_slot_view.c" \
    "llvm_hosted_domain_slot_view_is_tobject_like"
require_term "src/codegen/llvm_inventory_slot_view.c" \
    "llvm_hosted_domain_slot_view_is_binding_like"
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_hosted_domain_slot_view_is_tobject_like(slot_view, index)"
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_hosted_domain_slot_view_name(slot_view, index)"
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_hosted_domain_slot_view_is_binding_like(slot_view, i)"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "llvm_hosted_domain_slot_view_is_binding_like(slot_view, i)"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_domain_slot_view_is_binding_like"
require_term "src/codegen/transpiler_decl_slot_view.c" \
    "transpiler_hosted_domain_slot_view_is_binding_like"
if grep -Fq "hosted_domain_slot_view_source_ast(slot_view, index)" \
    "$ROOT_DIR/src/codegen/llvm_domain_projection_count.c" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C/LLVM projection-slot counting must use domain-slot metadata names, not source AST slots"
fi
if grep -Fq "hosted_domain_slot_view_source_ast(slot_view, i)" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_bind_lowering.c" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C/LLVM bind-target classification must use hosted domain-slot metadata, not source AST slots"
fi
if grep -Fq "ast_domain_slot_is_tobject" \
    "$ROOT_DIR/src/codegen/llvm_domain_projection_count.c" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C/LLVM projection-slot classification must consume hosted domain-slot metadata"
fi
if grep -Fq "ast_domain_slot_is_binding" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_bind_lowering.c" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C/LLVM bind-target classification must consume hosted domain-slot metadata"
fi
require_term "src/compiler/mir.h" "is_binding_like"
require_term "src/compiler/mir_decl_headers.h" \
    "mir_decl_field_is_binding_like"
require_term "src/compiler/mir_decl_header_access.c" \
    "mir_decl_field_is_binding_like"
require_term "src/compiler/mir_decl_headers.c" \
    "meta->is_binding_like = ast_domain_slot_is_binding(slot)"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_count_domain_projection_slots_in_view("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_domain_decl_refreshes(stmt, &decl_name, &refreshes,"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_domain_slot_view_is_projection_slot("
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_domain_add_projection_state_fields(ctx, entry, ftypes"
require_term "src/codegen/llvm_domain_struct_fields.c" \
    "llvm_hosted_domain_slot_view_name(slot_view, j)"
projection_view_consumer_hits="$(
    grep -RInE 'ast_compat_slots|llvm_count_domain_projection_slots\(|ast_domain_slot_is_tobject|llvm_domain_slot_is_projection_target|ast_zone_layer_slot_(is_pool|pool_capacity)' \
        "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c" \
        "$ROOT_DIR/src/codegen/llvm_domain_struct_register_fields.c" \
        "$ROOT_DIR/src/codegen/llvm_domain_struct_fields.c" || true
)"
if [[ -n "$projection_view_consumer_hits" ]]; then
    fail "LLVM domain struct projection consumers must consume hosted slot views:
$projection_view_consumer_hits"
fi
if grep -Eq 'ASTNode \*\*slots|size_t slot_count|llvm_domain_decl_parts\(' \
    "$ROOT_DIR/src/codegen/llvm_domain_method_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c"; then
    fail "LLVM domain method/struct emit must not request raw slot arrays from decl-parts"
fi
require_term "src/codegen/transpiler_domain_receiver_query.c" \
    "zone_type_name = transpiler_decl_name_local(zone_decl)"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "const char *name = transpiler_decl_name_local(node)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "role_name = transpiler_decl_name_local(role)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "find_callable_decl(ctx, fn_name)"
if grep -Fq "find_function_decl(ctx, fn_name)" \
        "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"; then
    fail "C role operator alias collision checks must consume find_callable_decl"
fi
require_term "src/codegen/transpiler_overlay_projection.c" \
    "world_name = transpiler_decl_name_local(host_decl)"
require_term "src/codegen/transpiler_overlay_projection.c" \
    "transpiler_resolve_world_zone_decl(ctx, host_decl"
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "effect_name = transpiler_decl_name_local(effect_decl)"
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "TranspilerHostedDomainSlotView effect_slot_view"
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "transpiler_domain_slot_view_bindable_name("
if grep -Eq 'ast_effect_slots|ast_domain_slot_name\(|find_nth_bindable_domain_slot_local' \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_bind.c"; then
    fail "C zone effect bind must consume hosted domain-slot view for bind targets"
fi
require_term "src/codegen/transpiler_overlay_zone_relation_bind.c" \
    "relation_name = transpiler_decl_name_local(relation_decl)"
require_term "src/codegen/transpiler_overlay_zone_relation_bind.c" \
    "TranspilerHostedDomainSlotView relation_slot_view"
require_term "src/codegen/transpiler_overlay_zone_relation_bind.c" \
    "transpiler_domain_slot_view_bindable_name("
if grep -Eq 'ast_relation_slots|ast_domain_slot_name\(|find_nth_bindable_domain_slot_local' \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_relation_bind.c"; then
    fail "C zone relation bind must consume hosted domain-slot view for bind targets"
fi
if grep -Fq "find_nth_bindable_domain_slot_local" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_bind.h"; then
    fail "C zone bind header must not expose raw AST bind-target finder"
fi
require_term "src/codegen/transpiler_projection_sync.c" \
    "active_zone_name = transpiler_decl_name_local(host_decl)"
require_term "src/codegen/transpiler_projection_sync.c" \
    "transpiler_resolve_world_zone_decl(ctx, world_decl"
require_term "src/codegen/transpiler_projection_sync.c" \
    "TranspilerHostedZoneLayerSlotView layer_view"
require_term "src/codegen/transpiler_projection_sync.c" \
    "transpiler_hosted_zone_layer_slot_view_from_decl("
require_term "src/codegen/transpiler_projection_sync.c" \
    "transpiler_hosted_zone_layer_slot_view_missing_mir_metadata("
require_term "src/codegen/transpiler_projection_sync.c" \
    "transpiler_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/transpiler_projection_sync.c" \
    "transpiler_hosted_zone_layer_slot_view_type_name("
require_term "src/codegen/transpiler_projection_sync.c" \
    "transpiler_hosted_zone_layer_slot_view_is_relation(&layer_view, i)"
require_term "src/codegen/transpiler_projection_sync.c" \
    "transpiler_hosted_zone_layer_slot_view_is_pool(&layer_view, i)"
require_term "src/codegen/transpiler_projection_sync.c" \
    "TranspilerHostedDomainSlotView effect_slot_view"
require_term "src/codegen/transpiler_projection_sync.c" \
    "transpiler_hosted_domain_slot_view_from_decl(ctx, effect_type_name"
require_term "src/codegen/transpiler_projection_sync.c" \
    "transpiler_domain_slot_view_bindable_name("
if grep -Eq 'ast_zone_layer_slots|ast_effect_slots|ast_domain_slot_name\(' \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.c"; then
    fail "C projection sync layer and effect source iteration must consume typed slot views"
fi
require_term "src/codegen/transpiler_generic_class_naming.c" \
    "base_class_name = transpiler_decl_name_local(class_decl)"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "base_class_name = transpiler_decl_name_local(class_decl)"
for rel in \
    "src/codegen/transpiler_class_decl_emit.c" \
    "src/codegen/transpiler_domain_nominal_emit.c" \
    "src/codegen/transpiler_enum_decl_emit.c" \
    "src/codegen/transpiler_relation_effect_emit.c" \
    "src/codegen/transpiler_roster_decl_emit.c" \
    "src/codegen/transpiler_world_select_event_emit.c" \
    "src/codegen/transpiler_zone_decl_emit.c"; do
    require_term "$rel" "transpiler_decl_name_local(node)"
done
for rel in \
    "src/codegen/transpiler_domain_nominal_emit.c" \
    "src/codegen/transpiler_roster_decl_emit.c"; do
    require_term "$rel" "transpiler_hosted_shared_field_view_from_decl(ctx, name, node)"
    require_term "$rel" "transpiler_hosted_shared_field_view_missing_mir_metadata("
    require_term "$rel" "transpiler_hosted_shared_field_view_name(&shared_view, i)"
    require_term "$rel" "transpiler_hosted_shared_field_view_type(&shared_view, i)"
    if grep -Eq 'ast_(party|roster)_shared_count|ast_(party|roster)_shared\(|ast_party_shared_(name|type)\(' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must consume TranspilerHostedSharedFieldView for shared-field declaration name/type/count"
    fi
done
host_name_hits="$(
    grep -RInE 'ast_(class|enum|party|role|roster|relation|effect|zone|world)_name\(' \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' |
        grep -v 'src/codegen/host_decl_compat.c:' || true
)"
if [[ -n "$host_name_hits" ]]; then
    fail "backend host declaration names must flow through host_decl_compat.c:
$host_name_hits"
fi
class_field_compat_hits="$(
    grep -RIn "pgy_host_class_fields_compat_view_from_decl" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' |
        grep -Ev 'src/codegen/(host_decl_compat\.[ch]|llvm_inventory_field_view\.c|transpiler_decl_field_view\.c):' || true
)"
if [[ -n "$class_field_compat_hits" ]]; then
    fail "class-field compatibility views must stay behind declaration inventory owners:
$class_field_compat_hits"
fi
class_field_find_hits="$(
    grep -RIn "pgy_host_class_field_compat_find" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' |
        grep -Ev 'src/codegen/host_decl_compat\.[ch]:' || true
)"
if [[ -n "$class_field_find_hits" ]]; then
    fail "class-field compatibility lookup must stay behind host_decl_compat.c:
$class_field_find_hits"
fi
shared_field_compat_hits="$(
    grep -RInE "pgy_host_shared_fields_compat_view_from_decl|pgy_host_shared_field_compat_find" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' |
        grep -Ev 'src/codegen/(host_decl_compat\.[ch]|llvm_inventory_field_view\.c|transpiler_decl_field_view\.c):' || true
)"
if [[ -n "$shared_field_compat_hits" ]]; then
    fail "shared-field compatibility views must stay behind declaration inventory owners:
$shared_field_compat_hits"
fi
zone_layer_slot_hits="$(
    grep -RIn "ast_zone_layer_slots" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' |
        grep -Ev 'src/codegen/(domain_frontier_policy\.c|llvm_inventory_slot_view\.c|transpiler_decl_slot_view\.c):' || true
)"
if [[ -n "$zone_layer_slot_hits" ]]; then
    fail "zone layer-slot AST child-list access must stay behind frontier policy or declaration inventory owners:
$zone_layer_slot_hits"
fi
for rel in \
    "src/codegen/llvm_channel_target.c" \
    "src/codegen/llvm_domain_decl_parts_helpers.c" \
    "src/codegen/llvm_domain_lookup.c" \
    "src/codegen/llvm_domain_projection_value_helpers.c" \
    "src/codegen/llvm_expr_constructor_calls.c" \
    "src/codegen/llvm_expr_projection_path_helpers.c" \
    "src/codegen/transpiler_domain_constructor_emit.c" \
    "src/codegen/transpiler_expr_type_infer.c" \
    "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "src/codegen/transpiler_let_emit.c" \
    "src/codegen/transpiler_mir_local_type_lookup.c" \
    "src/codegen/transpiler_mir_ssa_names.c" \
    "src/codegen/transpiler_nominal.c" \
    "src/codegen/transpiler_overlay_host_fields.c" \
    "src/codegen/transpiler_overlay_projection.c" \
    "src/codegen/transpiler_projection.c" \
    "src/codegen/transpiler_projection_field_path.c"; do
    if grep -Eq 'ast_class_fields|ast_(party|roster|relation|effect|zone|world)_shared_fields' \
            "$ROOT_DIR/$rel"; then
        fail "$rel must consume host_decl_compat field lookup helpers instead of reopening class/shared field arrays"
    fi
done
for rel in \
    "src/codegen/llvm_register.c" \
    "src/codegen/llvm_decl_authority.c" \
    "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "src/codegen/llvm_expr_assignment_projection.c" \
    "src/codegen/llvm_stmt_zone_action.c" \
    "src/codegen/transpiler_domain_receiver_query.c" \
    "src/codegen/transpiler_overlay_projection.c" \
    "src/codegen/transpiler_overlay_zone_bind.c" \
    "src/codegen/transpiler_overlay_zone_relation_bind.c" \
    "src/codegen/transpiler_projection_sync.c"; do
    if grep -Eq 'ast_(world|zone|effect|relation)_name\((host_decl|world_decl|zone_decl|effect_decl|relation_decl)\)' \
            "$ROOT_DIR/$rel"; then
        fail "$rel must consume backend declaration-name owners in sync/query paths"
    fi
done
for rel in \
    "src/codegen/transpiler_overlay_projection.c" \
    "src/codegen/transpiler_projection_sync.c"; do
    if grep -Fq "transpiler_find_decl_in_inventory_local(ctx, AST_ZONE_DECL" \
            "$ROOT_DIR/$rel"; then
        fail "$rel must resolve world-embedded zones through transpiler_resolve_world_zone_decl"
    fi
done
for rel in \
    "src/codegen/llvm_domain_decl_parts_helpers.c" \
    "src/codegen/transpiler_mir_local_type_lookup.c" \
    "src/codegen/transpiler_overlay_host_fields.c"; do
    if grep -Eq 'ast_(class|enum|party|role|roster|relation|effect|zone|world)_name\((stmt|host_decl)\)' \
            "$ROOT_DIR/$rel"; then
        fail "$rel must consume the host declaration name owner"
    fi
done

is_approved_decl_field_array_owner() {
    case "$1" in
        src/codegen/host_decl_compat.c) return 0 ;;
    esac
    return 1
}

while IFS= read -r hit; do
    rel="${hit%%:*}"
    rel="${rel#"$ROOT_DIR/"}"
    if ! is_approved_decl_field_array_owner "$rel"; then
        fail "$rel reopens declaration field arrays outside approved declaration/register owners"
    fi
done < <(grep -RInE 'ast_class_fields|ast_(party|roster|relation|effect|zone|world)_shared_fields' \
    "$ROOT_DIR/src/codegen" \
    --include='*.c' --include='*.h' || true)

for term in \
    "llvm_active_inventory" \
    "mir_find_decl_header(ctx->mir, name)" \
    "llvm_is_host_decl_type" \
    "pgy_host_decl_compat_is_type(decl_type)" \
    "pgy_host_decl_compat_types(&host_type_count)" \
    "host_types[i]" \
    "pgy_host_decl_compat_name(node)" \
    "return llvm_is_host_decl_type(decl->type)" \
    "llvm_decl_node_name(decl)"; do
    require_term "src/codegen/llvm_inventory_decl_lookup.c" "$term"
done

for term in \
    "llvm_find_host_method_metadata_in_context" \
    "llvm_hosted_method_view" \
    "llvm_hosted_method_view_metadata" \
    "llvm_hosted_method_view_source_ast" \
    "llvm_find_host_method_decl_in_context" \
    "llvm_mir_decl_method_name" \
    "llvm_mir_decl_method_source_ast" \
    "llvm_mir_decl_method_param_count" \
    "llvm_mir_decl_method_param" \
    "llvm_mir_decl_method_return_type" \
    "llvm_mir_decl_method_is_async" \
    "llvm_mir_decl_method_is_action_like" \
    "llvm_mir_decl_method_within_zone" \
    "llvm_mir_decl_method_causes_effect" \
    "llvm_mir_decl_method_routine"; do
    require_term "src/codegen/llvm_inventory_host_methods.h" "$term"
done
if grep -Fq "llvm_hosted_method_view_routine" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM hosted method routine lookup must consume llvm_mir_decl_method_routine directly"
fi
if grep -Fq "llvm_find_host_decl_methods_in_context" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h"; then
    fail "LLVM host method inventory must not expose AST method-array lookup helpers"
fi
if grep -Fq "llvm_host_decl_methods(" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h"; then
    fail "LLVM host method inventory must be MIRDeclMethod metadata-only"
fi
if grep -RInE 'llvm_(hosted_method_view|mir_decl_method)_ast' \
    "$ROOT_DIR/src/codegen"; then
    fail "LLVM declaration method source compatibility accessors must use *_source_ast names"
fi
require_term "src/codegen/llvm_inventory_host_methods.h" "ast_compat_methods"
require_term "src/codegen/llvm_inventory_host_methods.h" "ast_compat_count"
require_term "src/codegen/llvm_inventory_host_methods.c" \
    "view->count != view->ast_compat_count"
for term in \
    "mir_decl_header_source_ast" \
    "mir_decl_method_source_ast" \
    "mir_decl_method_name" \
    "mir_decl_method_param_count" \
    "mir_decl_method_param" \
    "mir_decl_method_return_type" \
    "mir_decl_method_is_action_like" \
    "mir_decl_method_routine_index"; do
    require_term "src/compiler/mir_decl_headers.h" "$term"
    require_term "src/compiler/mir_decl_header_access.c" "$term"
done
for term in \
    "MIRDeclField" \
    "MIRDeclFieldKind" \
    "field_metadata" \
    "field_metadata_count" \
    "MIR_DECL_FIELD_CLASS" \
    "MIR_DECL_FIELD_SHARED" \
    "MIR_DECL_FIELD_ROLE_SLOT" \
    "MIR_DECL_FIELD_ROSTER_SLOT" \
    "MIR_DECL_FIELD_WORLD_ROSTER_SLOT" \
    "MIR_DECL_FIELD_WORLD_ZONE_SLOT" \
    "MIR_DECL_FIELD_DOMAIN_SLOT" \
    "MIR_DECL_FIELD_ZONE_LAYER_SLOT"; do
    require_term "src/compiler/mir.h" "$term"
done
for term in \
    "mir_decl_header_field_count" \
    "mir_decl_header_field" \
    "mir_decl_field_source_ast" \
    "mir_decl_field_owner_name" \
    "mir_decl_field_name" \
    "mir_decl_field_type" \
    "mir_decl_field_type_name" \
    "mir_decl_field_kind_or" \
    "mir_decl_field_is_dynamic" \
    "mir_decl_field_is_subject_like" \
    "mir_decl_field_is_tobject_like"; do
    require_term "src/compiler/mir_decl_headers.h" "$term"
    require_term "src/compiler/mir_decl_header_access.c" "$term"
done
require_term "src/compiler/mir.h" \
    "is_tobject_like"
require_term "src/compiler/mir_decl_headers.c" \
    "meta->is_tobject_like = ast_domain_slot_is_tobject(slot)"
for term in \
    "MIR declaration header[%zu] '%s' has %zu hosted field(s) without MIRDeclField metadata" \
    "MIR declaration header[%zu] '%s' field metadata count %zu does not match AST compatibility count %zu" \
    "MIR declaration header[%zu] field[%zu] has owner metadata drift" \
    "MIR declaration header[%zu] field[%zu] has incomplete field metadata"; do
    require_term "src/compiler/mir_decl_header_validate.c" "$term"
done
for term in \
    "transpiler_find_decl_field_metadata" \
    "mir_decl_header_field_count(header)" \
    "mir_decl_header_field(header, i)" \
    "mir_decl_field_name(field)" \
    "mir_decl_field_type_name(field)" \
    "mir_decl_field_type(field)" \
    "transpiler_mir_decl_field_kind_or" \
    "transpiler_mir_decl_field_is_tobject_like"; do
    require_term "src/codegen/transpiler_decl_lookup.c" "$term"
done
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_mir_decl_field_is_tobject_like"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_domain_slot_view_is_tobject_like"
for term in \
    "transpiler_decl_header_shared_field_count" \
    "transpiler_decl_header_shared_field" \
    "pgy_host_shared_fields_compat_view_from_decl(decl)" \
    "transpiler_active_decl_header(ctx, host_name)" \
    "mir_decl_header_field_count(header)" \
    "mir_decl_header_field(header, i)" \
    "MIR_DECL_FIELD_SHARED" \
    "return transpiler_decl_header_shared_field(view->decl_header, index)" \
    "ast_party_shared_name(view->ast_compat_fields[index])" \
    "ast_party_shared_type(view->ast_compat_fields[index])" \
    "mir_decl_field_name(field)" \
    "mir_decl_field_type(field)"; do
    require_term "src/codegen/transpiler_decl_field_view.c" "$term"
done
for term in \
    "TranspilerHostedSharedFieldView" \
    "transpiler_hosted_shared_field_view_from_decl(" \
    "transpiler_hosted_shared_field_view_missing_mir_metadata(" \
    "transpiler_hosted_shared_field_view_metadata(" \
    "transpiler_hosted_shared_field_view_source_ast(" \
    "transpiler_hosted_shared_field_view_name(" \
    "transpiler_hosted_shared_field_view_type("; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
done
for term in \
    "transpiler_find_decl_field_metadata(ctx, host_name, field_name)" \
    "transpiler_find_decl_field_metadata(ctx, host_type_name, member_name)" \
    "render_mir_decl_field_type_name(ctx, field)" \
    "transpiler_mir_decl_field_kind_or(field, MIR_DECL_FIELD_UNKNOWN)"; do
    require_term "src/codegen/transpiler_nominal.c" "$term"
done
for term in \
    "projection_field_type_name" \
    "host_projection_class_field_info" \
    "transpiler_hosted_class_field_view_from_decl(" \
    "transpiler_hosted_field_view_find_index(" \
    "transpiler_hosted_field_view_metadata(view, index)" \
    "transpiler_mir_decl_field_type_name(field)"; do
    require_term "src/codegen/transpiler_projection_field_path.c" "$term"
done
for term in \
    "overlay_projection_field_count" \
    "overlay_projection_field_name" \
    "overlay_projection_field_view" \
    "transpiler_hosted_class_field_view_from_decl(" \
    "transpiler_hosted_field_view_name(&view, index)"; do
    require_term "src/codegen/transpiler_overlay_projection.c" "$term"
done
for term in \
    "llvm_find_decl_field_in_context" \
    "mir_decl_header_field_count(decl_header)" \
    "mir_decl_header_field(decl_header, i)" \
    "mir_decl_field_name(field)" \
    "llvm_mir_decl_field_type" \
    "llvm_mir_decl_field_type_name"; do
    require_term "src/codegen/llvm_inventory_decl_lookup.c" "$term"
done
for term in \
    "LLVMHostedFieldView" \
    "llvm_hosted_class_field_view_from_decl" \
    "llvm_hosted_field_view_missing_mir_metadata" \
    "llvm_hosted_field_view_metadata" \
    "llvm_hosted_field_view_name" \
    "llvm_hosted_field_view_type" \
    "pgy_host_class_fields_compat_view_from_decl(decl)" \
    "return mir_decl_header_field(view->decl_header, index)" \
    "LLVMHostedSharedFieldView" \
    "llvm_decl_header_shared_field_count" \
    "llvm_decl_header_shared_field" \
    "llvm_hosted_shared_field_view_from_decl" \
    "llvm_hosted_shared_field_view_missing_mir_metadata" \
    "llvm_hosted_shared_field_view_metadata" \
    "llvm_hosted_shared_field_view_source_ast" \
    "llvm_hosted_shared_field_view_name" \
    "llvm_hosted_shared_field_view_type" \
    "pgy_host_shared_fields_compat_view_from_decl(decl)" \
    "MIR_DECL_FIELD_SHARED" \
    "return llvm_decl_header_shared_field(view->decl_header, index)" \
    "mir_decl_field_name(field)"; do
    require_term "src/codegen/llvm_inventory_field_view.c" "$term"
done
for term in \
    "llvm_find_decl_field_in_context(ctx, host_name, field_name)" \
    "llvm_mir_decl_field_type_name(mir_field)" \
    "llvm_mir_decl_field_type(mir_field)"; do
    require_term "src/codegen/llvm_domain_lookup.c" "$term"
done
for term in \
    "llvm_projection_field_count" \
    "llvm_projection_field_name" \
    "llvm_projection_field_type_name" \
    "llvm_projection_field_view" \
    "llvm_hosted_field_view_metadata(&view, index)" \
    "llvm_mir_decl_field_type_name(field)"; do
    require_term "src/codegen/llvm_expr_projection_path_helpers.c" "$term"
done
for term in \
    "llvm_domain_projection_field_count" \
    "llvm_domain_projection_field_name" \
    "llvm_domain_projection_field_type_name" \
    "llvm_domain_projection_field_view" \
    "llvm_hosted_field_view_metadata(&view, index)" \
    "llvm_mir_decl_field_type_name(field)"; do
    require_term "src/codegen/llvm_domain_projection_value_helpers.c" "$term"
done
for rel in \
    "src/codegen/llvm_inventory_decl_lookup.c" \
    "src/codegen/transpiler_decl_lookup.c"; do
    require_term "$rel" "mir_decl_header_source_ast("
done
for rel in \
    "src/codegen/llvm_inventory_host_methods.c" \
    "src/codegen/transpiler_decl_host_lookup.c" \
    "src/codegen/transpiler_decl_method_view.c"; do
    require_term "$rel" "mir_decl_method_source_ast("
done
if grep -RInE 'decl_header->source_ast|method->source_ast' \
        "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h'; then
    fail "backend source/provenance compatibility must use MIR source_ast accessors"
fi
require_term "src/codegen/host_decl_compat.c" \
    "ast_role_impl_method_total_count"
require_term "src/codegen/host_decl_compat.c" \
    "view.count = (size_t)-1"
require_term "src/codegen/host_decl_compat.c" \
    "case AST_ROLE_DECL"
require_term "src/codegen/llvm_inventory_host_methods.c" \
    "pgy_host_method_compat_view_from_decl(decl, llvm_active_has_mir(ctx))"
for term in \
    "view.decl_header = decl_header" \
    "view.count = mir_decl_header_method_count(decl_header)" \
    "return mir_decl_header_method(view->decl_header, index)" \
    "llvm_hosted_method_view_missing_mir_method_row(" \
    "llvm_require_hosted_method_view_rows("; do
    require_term "src/codegen/llvm_inventory_host_methods.c" "$term"
done
require_term "src/codegen/llvm_inventory_host_methods.h" \
    "llvm_require_hosted_method_view_rows("
if grep -RInE 'const MIRDeclMethod \*metadata|view[.]metadata|view->metadata|llvm_host_decl_method_metadata' \
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h" \
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM hosted method view must keep MIRDeclMethod arrays behind compiler accessors"
fi
if grep -Fq "llvm_hosted_method_view(" \
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c" \
    && grep -Fq "NULL, 0)" \
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM hosted method view must preserve AST compatibility counts when a MIR header exists"
fi
if grep -Fq "fallback_methods" "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM hosted method view must name AST compatibility paths explicitly, not as fallback_methods"
fi
if grep -Fq "fallback_count" "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM hosted method view must name AST compatibility counts explicitly, not as fallback_count"
fi
for term in \
    "mir_decl_method_routine_index(method, &routine_index)" \
    "llvm_active_routine_inventory(ctx, &inventory)" \
    "llvm_routine_inventory_get(&inventory, routine_index)" \
    "llvm_mir_decl_method_routine("; do
    require_term "src/codegen/llvm_inventory_host_methods.c" "$term"
done
if grep -RInE 'method->(name|params|param_count|return_type|is_action_like|has_routine|routine_index)|methods\[[^]]+\]\.name' \
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM hosted method metadata view must consume compiler MIRDeclMethod accessors"
fi
if grep -RInE 'method(_meta)?->(has_routine|routine_index)' \
    "$ROOT_DIR/src/codegen"/llvm_*.c \
    "$ROOT_DIR/src/codegen"/llvm_*.h \
    | grep -v "src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM hosted method routine lookup must go through llvm_mir_decl_method_routine"
fi

for term in \
    "llvm_active_routine_inventory" \
    "llvm_mir_routine_inventory_from_program" \
    "llvm_routine_inventory_get" \
    "llvm_mir_routine_source_ast" \
    "llvm_mir_routine_source_ast_of_type" \
    "llvm_active_nominal_inventory" \
    "llvm_active_domain_inventory"; do
    require_term "src/codegen/llvm_inventory_internal.h" "$term"
done
for term in \
    "llvm_mir_routine_source_ast(const MIRRoutine *routine)" \
    "llvm_mir_routine_source_ast_of_type(const MIRRoutine *routine" \
    "return mir_routine_source_ast(routine)"; do
    require_term "src/codegen/llvm_inventory_internal.c" "$term"
done
require_term "src/compiler/mir.h" \
    "mir_routine_source_ast(const MIRRoutine *routine)"
require_term "src/compiler/mir_program_inventory.c" \
    "mir_routine_source_ast(const MIRRoutine *routine)"
for rel in \
    "src/codegen/llvm_decl_routines.c" \
    "src/codegen/llvm_intent.c" \
    "src/codegen/llvm_intent_forward.c"; do
    require_term "$rel" "llvm_routine_inventory_get(inventory, i)"
    if grep -Eq '(inventory|routine_inventory)->routines\[[^]]+\]' "$ROOT_DIR/$rel"; then
        fail "$rel must consume LLVM MIR routine inventory through llvm_routine_inventory_get"
    fi
done
for rel in \
    "src/codegen/llvm_intent_flow.c" \
    "src/codegen/llvm_mir_contract.c"; do
    require_term "$rel" "llvm_routine_inventory_get(&routine_inventory, i)"
    if grep -Eq '(inventory|routine_inventory)->routines\[[^]]+\]' "$ROOT_DIR/$rel"; then
        fail "$rel must consume LLVM MIR routine inventory through llvm_routine_inventory_get"
    fi
done

for term in "mir_active_inventory" "mir_active_externs"; do
    require_term "src/compiler/mir.h" "$term"
    require_term "src/compiler/mir_public_surface.c" "$term"
done
for term in \
    "mir_routine_inventory_from_program" \
    "mir_routine_inventory_get" \
    "mir_mutable_routine_inventory_from_program" \
    "mir_mutable_routine_inventory_get" \
    "mir_program_has_main_function" \
    "mir_program_has_top_level_exec"; do
    require_term "src/compiler/mir.h" "$term"
    require_term "src/compiler/mir_program_inventory.c" "$term"
done
require_term "src/compiler/mir_public_surface.c" \
    "mir_routine_inventory_from_program(mir, &inventory)"
require_term "src/compiler/mir_public_surface.c" \
    "mir_mutable_routine_inventory_from_program(mir, &inventory)"
require_term "src/compiler/mir_decl_headers.c" \
    "mir_routine_inventory_from_program(mir, &inventory)"
require_term "src/compiler/mir_decl_header_validate.c" \
    "mir_routine_inventory_from_program(mir, &inventory)"
require_term "src/compiler/mir_program_validate.c" \
    "mir_routine_inventory_from_program(mir, &inventory)"
for rel in \
    "src/compiler/mir_decl_headers.c" \
    "src/compiler/mir_decl_header_validate.c" \
    "src/compiler/mir_program_validate.c"; do
    if grep -Eq '\bmir->routine_count\b|\bmir->routines\b' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must consume MIR routine inventory accessors instead of raw MIRProgram routine fields"
    fi
done
if grep -Eq '\bmir->routine_count\b|\bmir->routines\b' \
    "$ROOT_DIR/src/compiler/mir_public_surface.c"; then
    fail "MIR public surface must consume routine inventory accessors instead of raw MIRProgram routine fields"
fi
for term in \
    "mir_find_function_decl" \
    "mir_active_inventory" \
    "mir_active_externs" \
    "mir_find_decl_header" \
    "mir_run_liveness_pass" \
    "mir_run_dce_pass"; do
    if grep -Eq "^[[:space:]]*(ASTNode[[:space:]*]+|const[[:space:]]+MIRDeclHeader[[:space:]*]+|void[[:space:]]*|bool[[:space:]]*)$term[[:space:]]*\\(" \
            "$ROOT_DIR/src/compiler/mir_lower_public_api.h"; then
        fail "MIR public query/pass wrapper '$term' must stay in mir_public_surface.c, not mir_lower_public_api.h"
    fi
done
if grep -Fq "ASTNode    **methods;" "$ROOT_DIR/src/compiler/mir.h"; then
    fail "MIRDeclHeader must not carry AST method-array pointers as inventory state"
fi
if grep -Fq "header->methods" \
    "$ROOT_DIR/src/compiler/mir_decl_header_validate.c" \
    "$ROOT_DIR/src/compiler/mir_decl_headers.c"; then
    fail "MIR declaration-header validation must consume method metadata, not AST method arrays"
fi

for rel in "src/codegen/llvm_inventory_decl_lookup.c" "src/codegen/transpiler_inventory_view.c"; do
    require_term "$rel" "mir_active_inventory(ctx->mir, decl_type, &nodes, &count)"
done
for rel in "src/codegen/llvm_inventory_internal.c" "src/codegen/transpiler_inventory_view.c"; do
    require_term "$rel" "mir_active_externs(ctx->mir, &nodes, &count)"
done

raw_ctx_mir_hits="$(
    grep -RIn 'ctx->mir' "$ROOT_DIR/src/codegen" |
        grep -Ev 'src/codegen/(llvm_api\.c|llvm_inventory_decl_lookup\.c|llvm_inventory_internal\.c|transpiler_entry\.c|transpiler_inventory_view\.c|transpiler_mir_emission_contract\.c):' || true
)"
if [[ -n "$raw_ctx_mir_hits" ]]; then
    fail "raw ctx->mir access must stay in backend entrypoints, inventory view/lookup owners, or MIR emission contract probes:
$raw_ctx_mir_hits"
fi

for term in \
    "llvm_register_active_nominal_types(ctx)" \
    "llvm_emit_class_method_bodies_from_inventory(ctx)" \
    "llvm_forward_declare_function_routines_from_inventory(" \
    "llvm_emit_function_routines_from_inventory(" \
    "llvm_validate_function_routine_bodies_from_inventory(" \
    "llvm_forward_declare_intent_routines_from_inventory(" \
    "llvm_emit_intent_routines_from_inventory(" \
    "llvm_emit_main_wrapper(ctx)" \
    "declaration inventory is still AST-carried inside MIRProgram"; do
    require_term "src/codegen/llvm_pipeline.c" "$term"
done
for term in \
    "llvm_active_synthetic_executable_func(ctx)" \
    "llvm_active_has_mir(ctx)" \
    "llvm_active_has_top_level_exec(ctx)" \
    "llvm_active_has_main_function(ctx)" \
    "llvm_active_main_function_name(ctx)" \
    "llvm_active_uses_thread_pool(ctx)" \
    "__pgy_user_main_lowercase" \
    "LLVM thread-pool entry requires registered runtime function" \
    "LLVM event initialization requires generated event function"; do
    require_term "src/codegen/llvm_main_wrapper.c" "$term"
done
require_term "src/codegen/llvm_api.c" \
    "llvm_active_uses_intent_observability(ctx)"
require_term "src/codegen/llvm_inventory_internal.h" \
    "llvm_active_has_mir"
require_term "src/codegen/llvm_inventory_internal.c" \
    "llvm_active_has_mir(const LLVMGenCtx *ctx)"
require_term "src/codegen/llvm_inventory_internal.h" \
    "llvm_active_uses_intent_observability"
require_term "src/codegen/llvm_inventory_internal.c" \
    "pgy_mir_program_uses_intent_observability(ctx->mir)"
require_term "src/codegen/llvm_inventory_internal.h" \
    "llvm_active_uses_thread_pool"
require_term "src/codegen/llvm_inventory_internal.c" \
    "pgy_mir_program_uses_thread_pool(ctx->mir)"
require_term "src/codegen/llvm_inventory_internal.c" \
    "mir_routine_inventory_from_program(mir, &mir_inventory)"
require_term "src/codegen/llvm_inventory_internal.c" \
    "mir_program_has_main_function(ctx->mir)"
require_term "src/codegen/llvm_inventory_internal.c" \
    "mir_program_main_function_name(ctx->mir)"
require_term "src/codegen/llvm_inventory_internal.c" \
    "mir_program_has_top_level_exec(ctx->mir)"
require_term "src/compiler/mir.h" \
    "mir_program_has_main_function"
require_term "src/compiler/mir.h" \
    "mir_program_main_function_name"
require_term "src/compiler/mir.h" \
    "mir_program_has_top_level_exec"
require_term "src/codegen/transpiler_inventory_view.c" \
    "mir_program_main_function_name(ctx->mir)"
require_term "src/codegen/transpiler.c" \
    "transpiler_active_main_function_name(ctx)"
require_term "src/codegen/transpiler.c" \
    "transpiler_c_executable_emitted_name"
require_term "src/codegen/transpiler.c" \
    "__pgy_user_main_lowercase"
for rel in \
    "src/codegen/llvm_inventory_internal.c" \
    "src/codegen/transpiler_inventory_view.c"; do
    if grep -Eq 'ctx->mir->has_(main_function|top_level_exec)|ctx->mir->routine(s|_count)|mir->routine(s|_count)' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must consume MIR routine/main/top-level accessors instead of raw MIRProgram fields"
    fi
done
for term in \
    "llvm_register_generic_template_decl(LLVMGenCtx *ctx, ASTNode *func_decl)" \
    "ctx->generic_templates[ctx->generic_template_count].name = name" \
    "llvm_lookup_generic_template(ctx, name)"; do
    require_term "src/codegen/llvm_backend_generic.c" "$term"
done
if grep -Fq "ctx->generic_templates" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must not mutate the generic-template registry directly"
fi
if grep -Fq "routine->ast" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must not reopen routine source AST for emit policy"
fi
if grep -Fq "MIR_SCOPE_" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must not classify routine kinds locally for emit policy"
fi
if grep -Fq "mir_find_function_decl(ctx->mir, \"__pgy_top_level_exec\")" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM main wrapper must use the active executable inventory seam"
fi
if grep -Fq "mir_find_function_decl(ctx->mir, \"__pgy_top_level_exec\")" \
    "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"; then
    fail "LLVM main wrapper must use the active executable inventory seam"
fi
if grep -Eq 'ctx->mir->has_(top_level_exec|main_function)' \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM main wrapper must use active top-level/main metadata helpers"
fi
if grep -Eq 'ctx->mir->has_(top_level_exec|main_function)' \
    "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"; then
    fail "LLVM main wrapper must use active top-level/main metadata helpers"
fi
for term in \
    "llvm_forward_declare_function_routines_from_inventory(" \
    "llvm_emit_function_routines_from_inventory(" \
    "llvm_validate_function_routine_bodies_from_inventory(" \
    "llvm_decl_require_function_source_ast(" \
    "routine->block_count > 0 && routine->blocks == NULL" \
    "llvm_mir_routine_source_ast_of_type(" \
    "llvm_register_generic_template_decl(ctx, func_decl)" \
    "llvm_emit_func_from_mir(routine, ctx)" \
    "MIR-only LLVM path missing routine for function" \
    "MIR-only LLVM path missing function source declaration metadata for routine" \
    "MIR-only LLVM path has invalid function routine inventory row"; do
    require_term "src/codegen/llvm_decl_routines.c" "$term"
done
require_term "src/codegen/llvm_decl.c" '#include "llvm_decl_authority.h"'
for term in \
    "llvm_decl_emit_zone_authority_check(LLVMGenCtx *ctx)" \
    "pgy_zone_authority_check_export" \
    "ast_zone_authorities(zone_decl" \
    "ast_zone_authority_subject_slot_name(authority)" \
    "llvm_set_mir_inventory_missing(ctx"; do
    require_term "src/codegen/llvm_decl_authority.c" "$term"
done
if grep -R "data\.zone_decl\.\(authorities\|authority_count\)" \
    "$ROOT_DIR/src/codegen/llvm_decl.c" \
    "$ROOT_DIR/src/codegen/llvm_decl_authority.c" >/dev/null; then
    fail "LLVM zone authority checks must use AST zone child accessors"
fi
for rel in \
    "src/codegen/llvm_decl.c" \
    "src/codegen/llvm_decl_authority.c" \
    "src/codegen/llvm_decl_routines.c" \
    "src/codegen/llvm_inventory_internal.c" \
    "src/codegen/llvm_intent.c" \
    "src/codegen/llvm_intent_forward.c" \
    "src/codegen/llvm_mir_emit.c"; do
    if grep -Fq "routine->ast" "$ROOT_DIR/$rel"; then
        fail "$rel must consume routine source AST through llvm_mir_routine_source_ast* accessors"
    fi
done
require_term "src/codegen/llvm_mir_emit.c" \
    "llvm_mir_routine_source_ast(routine)"
for rel in \
    "src/codegen/llvm_decl_routines.c" \
    "src/codegen/llvm_domain_method_emit.c" \
    "src/codegen/llvm_domain_role_emit.c"; do
    awk '
        /llvm_emit_func_from_mir\(/ { guard = 4; next }
        guard > 0 {
            if ($0 ~ /ctx->has_error/) {
                guard = 0
            } else {
                guard--
                if (guard == 0)
                    exit 1
            }
        }
        END {
            if (guard > 0)
                exit 1
        }
    ' "$ROOT_DIR/$rel" ||
        fail "$rel must check ctx->has_error after llvm_emit_func_from_mir"
done
if grep -Fq "MIR-only LLVM path missing routine for function" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline function residual diagnostics must stay in the decl owner"
fi
for term in \
    "llvm_forward_declare_intent_routines_from_inventory(" \
    "llvm_require_mir_intent_source_ast(ctx, routine, &intent_decl)" \
    "llvm_forward_declare_intent(intent_decl, ctx)" \
    "MIR-only LLVM path has invalid intent routine inventory row"; do
    require_term "src/codegen/llvm_intent_forward.c" "$term"
done
for term in \
    "llvm_emit_intent_routines_from_inventory(" \
    "llvm_require_mir_intent_source_ast(ctx, routine, &intent_decl)" \
    "llvm_emit_intent_decl(intent_decl, ctx)" \
    "MIR-only LLVM path has invalid intent routine inventory row"; do
    require_term "src/codegen/llvm_intent.c" "$term"
done
for term in \
    "llvm_require_mir_intent_source_ast(" \
    "routine->block_count > 0 && routine->blocks == NULL" \
    "llvm_mir_routine_source_ast_of_type(" \
    "MIR-only LLVM path missing intent source declaration metadata for routine"; do
    require_term "src/codegen/llvm_intent_flow.c" "$term"
done
if grep -Fq "llvm_forward_declare_intent(stmt, ctx)" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline intent forward declaration must stay in the intent owner"
fi
if grep -Fq "llvm_emit_intent_decl(stmt, ctx)" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline intent routine emission must stay in the intent owner"
fi
for term in \
    "llvm_emit_class_method_bodies_from_inventory(LLVMGenCtx *ctx)" \
    "llvm_active_nominal_inventory(ctx, &nominal_nodes, &nominal_count)" \
    "llvm_hosted_method_view_from_decl(ctx, cls_name, decl)" \
    "method_view.uses_mir_metadata" \
    "llvm_mir_decl_method_routine(ctx, method_meta)" \
    "llvm_set_mir_inventory_missing(ctx" \
    "MIR-only LLVM path missing routine for class method"; do
    require_term "src/codegen/llvm_domain_method_emit.c" "$term"
done
require_term "src/codegen/llvm_domain_method_emit.h" \
    "llvm_emit_class_method_bodies_from_inventory"
require_term "src/codegen/llvm_internal_api.h" \
    "bool llvm_emit_class_method_bodies_from_inventory(LLVMGenCtx *ctx);"
if grep -Fq '#include "llvm_domain_method_emit.h"' \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must consume class-method emission through llvm_internal_api.h"
fi
register_decl_body="$(
    awk '
        /emit_program_from_mir:register_decl_items/ { in_body = 1 }
        /emit_program_from_mir:emit_domain_passes/ { in_body = 0 }
        in_body { print }
    ' "$ROOT_DIR/src/codegen/llvm_pipeline.c"
)"
if ! grep -Fq "llvm_register_active_nominal_types(ctx)" \
        <<<"$register_decl_body"; then
    fail "LLVM pipeline nominal registration must call the register owner helper"
fi
if grep -Fq "llvm_active_nominal_inventory" <<<"$register_decl_body"; then
    fail "LLVM pipeline nominal registration must not reopen the active nominal inventory loop"
fi
if grep -Fq "llvm_active_nominal_inventory" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must not directly iterate the active nominal inventory"
fi
if grep -Fq "llvm_hosted_method_view_from_decl(ctx, cls_name, decl)" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline class-method emission must stay in the method emit owner"
fi
if grep -Fq "llvm_find_mir_method_routine" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline class-method emission must use linked MIRDeclMethod routine indexes, not local routine fallback search"
fi
if grep -Fq "llvm_find_host_decl_methods_in_context(ctx, cls_name" \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline class-method emission must consume MIRDeclMethod metadata, not AST method arrays"
fi
if grep -Eq 'decl->data\.class_decl\.method_count|decl->data\.class_decl\.methods\[[^]]+\]' \
    "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
    fail "LLVM pipeline must use LLVMHostedMethodView for class-method inventory guards"
fi

require_term "src/codegen/llvm_internal_api.h" "llvm_set_mir_inventory_missing"
require_term "src/codegen/llvm_internal_api.h" "llvm_set_mir_topology_invalid"
require_term "src/codegen/llvm_internal_api.h" "llvm_set_mir_intent_carrier_missing"
require_term "src/codegen/llvm_error.c" "llvm_set_mir_inventory_missing"
require_term "src/codegen/llvm_error.c" "llvm_set_mir_topology_invalid"
require_term "src/codegen/llvm_error.c" "llvm_set_mir_intent_carrier_missing"
require_term "src/codegen/llvm_error.c" "llvm_result_error_with_hints"
require_term "src/codegen/llvm_error.c" "llvm_result_error_fmt_with_hints"
require_term "src/codegen/llvm_error.c" "PGY_CODE_LLVM_MIR_ROUTINE_MISSING"
require_term "src/codegen/llvm_error.c" "PGY_CODE_MIR_TOPOLOGY_INVALID"
require_term "src/codegen/llvm_error.c" "PGY_CODE_MIR_INTENT_CARRIER_MISSING"
require_term "src/codegen/llvm_error.c" "PGY_FIX_INSPECT_MIR_INVENTORY"
require_term "src/codegen/llvm_error.c" "PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING"
require_term "src/codegen/llvm_error.c" "PGY_FIX_CHECK_INTENT_STEP_LOWERING"
legacy_mir_metadata_errors="$(
    grep -RIn --include='llvm_*.c' --include='llvm_*.h' \
        "missing MIR declaration metadata" \
        "$ROOT_DIR/src/codegen" || true
)"
if [[ -n "$legacy_mir_metadata_errors" ]]; then
    fail "LLVM MIR metadata diagnostics must use llvm_set_mir_inventory_missing:
$legacy_mir_metadata_errors"
fi
legacy_mir_inventory_errors="$(
    grep -RIn --include='llvm_*.c' --include='llvm_*.h' \
        "MIR declaration inventory missing" \
        "$ROOT_DIR/src/codegen" || true
)"
if [[ -n "$legacy_mir_inventory_errors" ]]; then
    fail "LLVM declaration-inventory diagnostics must use llvm_set_mir_inventory_missing:
$legacy_mir_inventory_errors"
fi
if grep -RIn "PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING" "$ROOT_DIR/src/codegen" \
    | grep -v "src/codegen/llvm_error.c"; then
    fail "LLVM MIR-missing diagnostics must route through llvm_set_mir_inventory_missing"
fi
if grep -A3 -F "LLVM MIR pin block cannot resolve" \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM MIR pin topology diagnostics must use llvm_set_mir_topology_invalid"
fi
if grep -A3 -F "MIR-only LLVM path missing owner metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM MIR owner topology diagnostics must use llvm_set_mir_topology_invalid"
fi
require_term "src/codegen/llvm_mir_contract.c" "llvm_set_mir_topology_invalid"
if grep -RIn "llvm_set_error_with_hints(ctx" \
    "$ROOT_DIR/src/codegen/llvm_mir_contract.c"; then
    fail "LLVM MIR contract diagnostics must use llvm_set_mir_topology_invalid"
fi
require_term "src/codegen/llvm_mir_contract.c" "llvm_validate_mir_for_codegen"
require_term "src/codegen/llvm_mir_contract.c" "llvm_result_error_with_hints(\"MIR program is NULL\""
require_term "src/codegen/llvm_mir_contract.c" "llvm_result_error_with_hints(\"MIR routine is missing name\""
require_term "src/codegen/llvm_mir_contract.c" "llvm_result_error_fmt_with_hints("
if grep -A10 -F "MIR routine '%s' emission topology invalid" \
    "$ROOT_DIR/src/codegen/llvm_mir_contract.c" | grep -Fq "llvm_result_error_fmt("; then
    fail "LLVM MIR topology preflight must attach structured diagnostic hints"
fi

if grep -A8 -F "MIR-only LLVM path missing intent routine" \
    "$ROOT_DIR/src/codegen/llvm_intent.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM intent MIR-missing diagnostics must use llvm_set_mir_inventory_missing"
fi
if grep -A8 -F "MIR-only LLVM path missing intent participant metadata" \
    "$ROOT_DIR/src/codegen/llvm_intent_flow.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM intent participant inventory diagnostics must use llvm_set_mir_inventory_missing"
fi
require_term "src/codegen/llvm_intent_cleanup.c" "llvm_set_mir_intent_carrier_missing"
require_term "src/codegen/llvm_intent_step_context.c" "llvm_set_mir_intent_carrier_missing"
require_term "src/codegen/llvm_intent_step_context.c" \
    "out->dispatch_aliases = (const char **)ast_intent_step_who_names(step, NULL)"
if grep -Fq "step->data.intent_step.who_names[j]" \
    "$ROOT_DIR/src/codegen/llvm_intent.c"; then
    fail "LLVM intent dispatch emission must consume LLVMIntentStepContext aliases"
fi
if grep -RIn "llvm_set_error_with_hints(ctx" \
    "$ROOT_DIR/src/codegen/llvm_intent_cleanup.c" \
    "$ROOT_DIR/src/codegen/llvm_intent_step_context.c"; then
    fail "LLVM intent carrier diagnostics must use llvm_set_mir_intent_carrier_missing"
fi
if grep -A8 -F "MIR-only LLVM path missing routine for function" \
    "$ROOT_DIR/src/codegen/llvm_decl_routines.c" | grep -Fq "llvm_set_error(ctx"; then
    fail "LLVM pipeline MIR-missing diagnostics must use llvm_set_mir_inventory_missing"
fi
if grep -RIn "llvm_set_error(ctx" "$ROOT_DIR/src/codegen"/llvm_mir*.c \
    "$ROOT_DIR/src/codegen"/llvm_mir*.h \
    "$ROOT_DIR/src/codegen/llvm_intent_flow.c" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"; then
    fail "LLVM MIR emission must route failures through MIR diagnostic helpers"
fi

require_term "src/codegen/llvm_domain.c" "llvm_active_domain_inventory(ctx, &inventory)"

for term in \
    "declaration / top-level inventory is carried through MIRProgram" \
    "dedicated declaration IR layer"; do
    require_term "src/codegen/llvm_backend.h" "$term"
done

for term in \
    "transpiler_active_inventory" \
    "TranspilerMIRRoutineInventory" \
    "transpiler_active_routine_inventory" \
    "transpiler_mir_routine_inventory_from_program" \
    "transpiler_routine_inventory_get" \
    "transpiler_mir_routine_source_ast" \
    "transpiler_mir_routine_source_ast_of_type" \
    "transpiler_active_routine_count" \
    "transpiler_active_decl_header" \
    "transpiler_active_externs" \
    "transpiler_active_executables" \
    "transpiler_active_synthetic_executable_func" \
    "transpiler_active_has_mir" \
    "transpiler_active_mir_identity" \
    "transpiler_active_has_main_function" \
    "transpiler_active_has_top_level_exec" \
    "transpiler_active_uses_intent_observability" \
    "transpiler_active_uses_thread_pool" \
    "transpiler_active_can_emit_intent_cleanup_from_mir"; do
    require_term "src/codegen/transpiler_inventory_view.h" "$term"
done
require_term "src/codegen/transpiler_inventory_view.c" \
    "transpiler_active_has_mir(const TranspilerCtx *ctx)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "transpiler_active_mir_identity(const TranspilerCtx *ctx)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "transpiler_active_decl_header(const TranspilerCtx *ctx, const char *name)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "mir_routine_inventory_from_program(mir, &mir_inventory)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "mir_program_has_main_function(ctx->mir)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "mir_program_has_top_level_exec(ctx->mir)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "pgy_mir_program_uses_intent_observability(ctx->mir)"
require_term "src/codegen/transpiler_entry.c" \
    "transpiler_active_uses_intent_observability(ctx)"
require_term "src/codegen/transpiler.c" \
    "transpiler_active_has_mir(ctx)"
if grep -Fq "ctx->mir" "$ROOT_DIR/src/codegen/transpiler.c"; then
    fail "C program emitter must use active MIR view helpers, not direct ctx->mir probes"
fi
require_term "src/codegen/transpiler_inventory_view.c" \
    "pgy_mir_program_uses_thread_pool(ctx->mir)"
for term in \
    "transpiler_mir_routine_source_ast(const MIRRoutine *routine)" \
    "transpiler_mir_routine_source_ast_of_type(" \
    "return mir_routine_source_ast(routine)"; do
    require_term "src/codegen/transpiler_inventory_view.c" "$term"
done
for rel in \
    "src/codegen/transpiler_inventory_view.c" \
    "src/codegen/transpiler_mir_emission_contract.c"; do
    if grep -Fq "routine->ast" "$ROOT_DIR/$rel"; then
        fail "$rel must consume routine source AST through transpiler_mir_routine_source_ast* accessors"
    fi
done
require_term "src/codegen/transpiler_mir_emission_contract.c" \
    "transpiler_mir_routine_source_ast(routine)"
require_term "src/codegen/transpiler_mir_emission_contract.c" \
    "transpiler_mir_routine_source_ast_of_type("

for term in \
    "transpiler_active_inventory(ctx, AST_ABILITY_DECL, &abilities, &ability_count)" \
    "transpiler_active_inventory(ctx, AST_CLASS_DECL, &types, &type_count)" \
    "transpiler_active_inventory(ctx, AST_FUNC_DECL, &functions, &function_count)" \
    "transpiler_active_inventory(ctx, AST_INTENT_DECL, &intents, &intent_count)" \
    "transpiler_active_inventory(ctx, AST_ROLE_DECL, &roles, &role_count)" \
    "transpiler_active_inventory(ctx, AST_PARTY_DECL, &parties, &party_count)" \
    "transpiler_active_inventory(ctx, AST_ROSTER_DECL, &rosters, &roster_count)" \
    "transpiler_active_synthetic_executable_func(ctx)" \
    "transpiler_active_has_main_function(ctx)" \
    "transpiler_active_has_top_level_exec(ctx)"; do
    require_term "src/codegen/transpiler.c" "$term"
done
if sed -n '/emit_c_nominal_forward_decls/,/Program emitter/p' \
    "$ROOT_DIR/src/codegen/transpiler.c" \
    | grep -Eq 'ast_(class|party|roster|relation|effect|zone|world)_name\('; then
    fail "C nominal forward declarations must consume the host declaration name owner"
fi
require_term "src/codegen/transpiler.c" \
    "transpiler_decl_name_local(type_decl)"
for term in \
    "host_name = pgy_host_decl_compat_name(decl)" \
    "stmt_name = transpiler_decl_name_local(stmt)" \
    "transpiler_is_host_decl_type" \
    "return pgy_host_decl_compat_is_type(decl_type)" \
    "pgy_host_decl_compat_is_type(decl_type)" \
    "pgy_host_decl_compat_nominal_lookup_types(&host_lookup_type_count)" \
    "host_lookup_types[i]" \
    "transpiler_find_domain_constructor_decl_local" \
    "pgy_host_decl_compat_constructor_domain_types(" \
    "&constructor_type_count" \
    "constructor_types[i]"; do
    require_term "src/codegen/transpiler_decl_lookup.c" "$term"
done
if grep -RInE 'ASTNode \*find_(zone|world|relation|effect)_decl\(' \
        "$ROOT_DIR/src/codegen/transpiler_decl_lookup.c" \
        "$ROOT_DIR/src/codegen/transpiler_decl_lookup.h"; then
    fail "C backend domain declaration shortcut wrappers were removed; use active inventory seams instead"
fi
require_term "src/codegen/llvm_stmt_zone_action.c" \
    "llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL, effect_name)"
if grep -Fq "llvm_stmt_find_effect_decl" \
        "$ROOT_DIR/src/codegen/llvm_stmt_zone_action.c"; then
    fail "LLVM zone action effect lookup must consume llvm_find_named_domain_decl instead of a local wrapper"
fi
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "effect_decl = llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL,"
require_term "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "relation_decl = llvm_find_named_domain_decl(ctx, AST_RELATION_DECL,"
if grep -Fq "llvm_find_named_domain_decl_local" \
        "$ROOT_DIR/src/codegen/llvm_domain_zone_bind_lowering.c"; then
    fail "LLVM zone bind helpers must consume llvm_find_named_domain_decl instead of a local wrapper"
fi
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_find_projection_nominal_decl(LLVMGenCtx *ctx"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_find_function_decl(LLVMGenCtx *ctx"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_find_intent_decl(LLVMGenCtx *ctx"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_find_callable_decl(LLVMGenCtx *ctx"
require_term "src/codegen/llvm_stmt.c" \
    "return llvm_find_function_decl(ctx, name)"
if grep -Fq "return llvm_find_decl_in_active_inventory(ctx, AST_FUNC_DECL, name)" \
        "$ROOT_DIR/src/codegen/llvm_stmt.c"; then
    fail "LLVM statement function lookup must consume llvm_find_function_decl"
fi
for rel in \
    "src/codegen/llvm_expr_call_variable.c" \
    "src/codegen/llvm_expr_identifier_slot_helpers.c"; do
    require_term "$rel" "current_decl = ctx->current_func_decl"
    if grep -Fq "LLVMGetValueName(ctx->current_function)" "$ROOT_DIR/$rel"; then
        fail "$rel must consume ctx->current_func_decl instead of rediscovering the current function declaration by name"
    fi
done
if grep -Eq 'llvm_find_(function|intent)_decl\(LLVMGenCtx \*ctx' \
        "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.h"; then
    fail "LLVM function/intent declaration lookup must not be owned by boundary projection helpers"
fi
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "ASTNode *callable_decl = llvm_find_callable_decl(ctx, callee_name)"
if grep -Eq 'llvm_find_(function|intent)_decl\(ctx, callee_name\)' \
        "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"; then
    fail "LLVM call dispatch must consume llvm_find_callable_decl instead of reopening callable lookup"
fi
require_term "src/codegen/transpiler_expr_type_infer.c" \
    "ASTNode *decl = find_callable_decl(ctx, name)"
if grep -Eq 'find_(intent|function)_decl\(ctx, name\)' \
        "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"; then
    fail "C call type inference must consume find_callable_decl instead of reopening callable lookup"
fi
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "ASTNode *decl = (callee->type == AST_IDENTIFIER)"
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "? find_callable_decl(ctx, callee_name) : NULL"
if grep -Fq "find_function_decl(ctx, callee_name)" \
        "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"; then
    fail "C user-call emission must consume find_callable_decl instead of reopening function lookup"
fi
require_term "src/codegen/transpiler_mir_local_type_lookup.c" \
    "ASTNode *callee_decl = find_callable_decl(ctx, callee_name)"
if grep -Fq "find_function_decl(ctx, callee_name)" \
        "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"; then
    fail "C MIR local call-type inference must consume find_callable_decl"
fi
require_term "src/codegen/llvm_expr_spawn_names.c" \
    "llvm_spawn_append_mangled_suffix"
require_term "src/codegen/llvm_expr_spawn_generic.c" \
    "llvm_spawn_append_mangled_suffix(mangled, sizeof(mangled), suf)"
if grep -Fq "llvm_append_mangled_suffix" \
        "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.h"; then
    fail "LLVM boundary projection helpers must not own spawn mangled-name utilities"
fi
if grep -Eq 'llvm_(boundary_slot_param|expr_projection_path_helpers)\.h' \
        "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.h"; then
    fail "LLVM boundary projection helper header must not re-export unrelated owner headers"
fi
require_term "src/codegen/llvm_domain_projection_sync_body_helpers.c" \
    "llvm_find_projection_nominal_decl(ctx, source_type_name)"
require_term "src/codegen/llvm_domain_projection_sync_body_helpers.c" \
    "LLVMHostedDomainSlotView slot_view"
require_term "src/codegen/llvm_domain_projection_sync_body_helpers.c" \
    "llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_projection_sync_body_helpers.c" \
    "llvm_projection_sync_slot_type_name("
if grep -Eq 'ast_domain_slot_(name|type)\(|ASTNode \*\*slots|size_t slot_count' \
        "$ROOT_DIR/src/codegen/llvm_domain_projection_sync_body_helpers.c"; then
    fail "LLVM projection sync body must consume hosted domain-slot view for slot metadata"
fi
for rel in \
    "src/codegen/llvm_domain_projection_value_helpers.c" \
    "src/codegen/llvm_expr_projection_path_helpers.c"; do
    if grep -Fq "llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, name)" \
            "$ROOT_DIR/$rel"; then
        fail "$rel must consume llvm_find_projection_nominal_decl for projection nominal lookup"
    fi
done
if grep -RInE 'llvm_find_(domain_projection_nominal_decl|projection_class_decl)' \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h'; then
    fail "LLVM projection nominal lookup must be owned by llvm_find_projection_nominal_decl"
fi
require_term "src/codegen/transpiler_call_constructor_result_emit.c" \
    "transpiler_find_domain_constructor_decl_local(ctx, fn)"
require_term "src/codegen/transpiler_mir_local_type_lookup.c" \
    "transpiler_find_domain_constructor_decl_local("
require_term "src/codegen/transpiler_let_emit.c" \
    "transpiler_find_domain_constructor_decl_local("
if grep -Fq "find_party_decl(ctx, fn)" \
    "$ROOT_DIR/src/codegen/transpiler_call_constructor_result_emit.c"; then
    fail "C constructor dispatch must consume the domain-constructor lookup seam instead of repeating host chains"
fi
if grep -Fq "find_zone_decl(ctx, callee_name)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"; then
    fail "C MIR local type lookup must consume the domain-constructor lookup seam instead of repeating host chains"
fi
if grep -Fq "find_zone_decl(ctx, ann_type_name)" \
    "$ROOT_DIR/src/codegen/transpiler_let_emit.c"; then
    fail "C annotated let constructor fallback must consume the domain-constructor lookup seam instead of repeating host chains"
fi
require_term "src/codegen/transpiler_intent_emit.c" \
    "find_zone_decl_in_program_view(ctx, step_zone_name)"
if grep -Fq "find_zone_decl(ctx, step_zone_name)" \
    "$ROOT_DIR/src/codegen/transpiler_intent_emit.c"; then
    fail "C intent step zone binding must consume the active inventory view instead of direct AST lookup"
fi
require_term "src/codegen/transpiler_block_intent_helpers.c" \
    "find_zone_decl_in_program_view(ctx, zone_type)"
require_term "src/codegen/transpiler_block_intent_helpers.c" \
    "TranspilerHostedZoneLayerSlotView layer_view"
require_term "src/codegen/transpiler_block_intent_helpers.c" \
    "transpiler_hosted_zone_layer_slot_view_from_decl("
require_term "src/codegen/transpiler_block_intent_helpers.c" \
    "transpiler_hosted_zone_layer_slot_view_missing_mir_metadata("
require_term "src/codegen/transpiler_block_intent_helpers.c" \
    "transpiler_hosted_zone_layer_slot_view_name(&layer_view, i)"
require_term "src/codegen/transpiler_block_intent_helpers.c" \
    "transpiler_hosted_zone_layer_slot_view_type_name("
require_term "src/codegen/transpiler_block_intent_helpers.c" \
    "transpiler_hosted_zone_layer_slot_view_is_relation("
if grep -Fq "find_zone_decl(ctx, zone_type)" \
    "$ROOT_DIR/src/codegen/transpiler_block_intent_helpers.c"; then
    fail "C intent block zone-effect helpers must consume the active inventory view instead of direct AST lookup"
fi
if grep -Eq 'transpiler_hosted_zone_layer_slot_view_source_ast\(|ast_zone_layer_slot_|ast_zone_layer_slots' \
    "$ROOT_DIR/src/codegen/transpiler_block_intent_helpers.c"; then
    fail "C intent block zone-effect helpers must consume TranspilerHostedZoneLayerSlotView metadata, not source AST slots"
fi
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_find_decl_in_inventory_local(ctx, AST_ZONE_DECL,"
if grep -Fq "return find_zone_decl(ctx, zone_type)" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C world-zone projection resolution must consume active inventory instead of direct AST lookup"
fi
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "transpiler_ctx, AST_ZONE_DECL, zone_name)"
if grep -Fq "return find_zone_decl((TranspilerCtx *)ctx, zone_name)" \
    "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.c"; then
    fail "C world frontier lookup must consume active inventory instead of direct AST lookup"
fi
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "transpiler_find_decl_in_inventory_local(ctx, AST_ZONE_DECL,"
if grep -Fq "find_zone_decl(ctx, host_name)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"; then
    fail "C MIR SSA host recovery must consume active inventory for zone lookup"
fi
require_term "src/codegen/transpiler_func_forward_policy.c" \
    "transpiler_find_decl_in_inventory_local(ctx, AST_WORLD_DECL,"
if grep -Fq "find_world_decl(ctx, name)" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"; then
    fail "C function forward policy must consume active inventory for world lookup"
fi
for rel in \
    "src/codegen/transpiler_projection_sync.c" \
    "src/codegen/transpiler_expr_call_member_emit.c"; do
    require_term "$rel" "transpiler_find_decl_in_inventory_local("
    if grep -Fq "find_zone_decl(ctx, zone_type_name)" "$ROOT_DIR/$rel"; then
        fail "$rel must consume active inventory for world-zone projection/action context lookup"
    fi
done
require_term "src/codegen/transpiler_projection_sync.c" \
    "AST_EFFECT_DECL"
if grep -Fq "find_effect_decl(ctx, effect_name)" \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.c"; then
    fail "C world action effect sync must consume active inventory for effect lookup"
fi
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "transpiler_find_decl_in_inventory_local("
if grep -Fq "find_effect_decl(ctx, ast_zone_layer_slot_layer_type(layer_slot))" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_bind.c"; then
    fail "C zone effect bind must consume active inventory for effect lookup"
fi
require_term "src/codegen/transpiler_overlay_zone_relation_bind.c" \
    "transpiler_find_decl_in_inventory_local("
if grep -Fq "find_relation_decl(ctx," \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_relation_bind.c"; then
    fail "C zone relation bind must consume active inventory for relation lookup"
fi
c_domain_lookup_hits="$(
    grep -RInE 'find_(zone|world|relation|effect)_decl\(ctx,' \
        "$ROOT_DIR/src/codegen" \
        --include='transpiler*.c' --include='transpiler*.h' \
        | grep -v 'src/codegen/transpiler_decl_lookup.c:' \
        | grep -v 'src/codegen/transpiler_decl_lookup.h:' || true
)"
if [[ -n "$c_domain_lookup_hits" ]]; then
    fail "C backend domain declaration recovery must consume active inventory seams outside decl_lookup owner:
$c_domain_lookup_hits"
fi
require_term "src/codegen/transpiler_expr_type_infer.c" \
    "transpiler_has_known_nominal_type(ctx, name)"
if grep -Fq "find_subject_host_decl(ctx, name)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"; then
    fail "C expression type inference must consume known-nominal policy instead of repeating host chains"
fi
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_find_nominal_host_decl_local(ctx, type_name)"
if grep -Fq "find_relation_decl(ctx, type_name) != NULL" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C nominal host type predicate must consume nominal host lookup instead of repeating domain host chains"
fi
for term in \
    "transpiler_find_nominal_host_decl_local(ctx, type_name)" \
    "pgy_host_decl_compat_uses_pointer_self(decl)"; do
    require_term "src/codegen/transpiler_host_self_policy.c" "$term"
done
require_term "src/codegen/transpiler_expr_dispatch_emit.c" \
    "transpiler_host_decl_uses_pointer_self(host_decl)"
if grep -Fq "host_decl->type == AST_PARTY_DECL" \
    "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"; then
    fail "C self-member dispatch must consume host pointer-self policy instead of repeating domain host chains"
fi
for term in \
    "transpiler_find_method_source_ast_in_mir_header" \
    "transpiler_decl_header_is_nominal_host(header)" \
    "transpiler_active_decl_header(ctx, host_type_name)" \
    "transpiler_active_mir_identity(ctx)" \
    "pgy_host_decl_compat_is_type(owner_ast_type)" \
    "owner_ast_type, owner_name" \
    "pgy_host_decl_compat_nominal_lookup_types(&host_lookup_type_count)" \
    "host_lookup_types[i]" \
    "AST_ROLE_DECL" \
    "transpiler_hosted_method_view_from_decl(ctx, host_type_name, decl)" \
    "mir_decl_header_method_count(header)" \
    "mir_decl_header_method(header, i)" \
    "transpiler_mir_decl_method_name(method)" \
    "mir_decl_method_source_ast(method)"; do
    require_term "src/codegen/transpiler_decl_host_lookup.c" "$term"
done
if grep -Fq "static const TranspilerHostOwnerLookup kTranspilerHostOwnerLookups[]" \
    "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c"; then
    fail "C host-owner lookup must consume host_decl_compat.c instead of reopening a local host-type table"
fi
if grep -Fq "lookup->lookup_type" \
    "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c"; then
    fail "C host-owner lookup must not route through a local owner/lookup table"
fi
if grep -Fq "ctx->mir" "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c"; then
    fail "C host-decl lookup cache must use active MIR identity helpers, not direct ctx->mir probes"
fi
if grep -RIn 'transpiler_decl_methods_local' "$ROOT_DIR/src/codegen"; then
    fail "C backend must not expose public AST method-array lookup seam"
fi
if grep -RInE 'transpiler_(hosted_method_view|mir_decl_method)_ast' \
    "$ROOT_DIR/src/codegen"; then
    fail "C declaration method source compatibility accessors must use *_source_ast names"
fi
for rel in \
    "src/codegen/transpiler_intent_context.c" \
    "src/codegen/transpiler_domain_receiver_query.c"; do
    require_term "$rel" "find_nominal_host_method_decl(ctx"
    if grep -Eq 'data\.class_decl\.methods\[[^]]+\]|data\.class_decl\.method_count' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must use the C backend MIR-aware host-method lookup seam"
    fi
done
for term in \
    "TranspilerHostedMethodView method_view" \
    "transpiler_hosted_method_view_from_decl(ctx, name, node)" \
    "transpiler_hosted_method_view_metadata(&method_view, i)" \
    "transpiler_mir_decl_method_routine(ctx, method_meta)" \
    "transpiler_hosted_method_view_source_ast(&method_view, i)" \
    "emit_role_method_impl(name, method_meta, mir_method, method, ctx)"; do
    require_term "src/codegen/transpiler_domain_nominal_emit.c" "$term"
done
for term in \
    "const MIRDeclMethod *method_meta" \
    "const MIRRoutine *mir_method" \
    "transpiler_mir_decl_method_name(method_meta)" \
    "transpiler_mir_routine_source_ast_of_type(" \
    "MIR-only C path missing declaration metadata for role method"; do
    require_term "src/codegen/transpiler_domain_role_methods_emit.c" "$term"
done
for term in \
    "mir_decl_method_routine_index(method, &routine_index)" \
    "mir_decl_header_ast_type_or(" \
    "pgy_host_method_compat_view_from_decl(" \
    "transpiler_active_routine_inventory(ctx, &inventory)" \
    "transpiler_routine_inventory_get(&inventory, routine_index)"; do
    require_term "src/codegen/transpiler_decl_method_view.c" "$term"
done
if grep -RInE 'header->(ast_type|method_metadata|method_metadata_count)|decl_header->ast_type' \
        "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c" \
        "$ROOT_DIR/src/codegen/transpiler_decl_lookup.c" \
        "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c" \
        "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c" \
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "backend declaration header views must consume compiler MIRDeclHeader accessors"
fi
if grep -RInE 'method->(name|params|param_count|return_type|is_action_like|has_routine|routine_index)' \
        "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"; then
    fail "C hosted method metadata view must consume compiler MIRDeclMethod accessors"
fi
for rel in \
    "src/codegen/llvm_inventory_host_methods.c" \
    "src/codegen/transpiler_decl_method_view.c"; do
    if grep -Fq "case AST_ROLE_DECL" "$ROOT_DIR/$rel"; then
        fail "$rel must delegate hosted-method AST compatibility classification to host_decl_compat.c"
    fi
done
for term in \
    "kPgyHostDeclCompatTypes[]" \
    "pgy_host_decl_compat_types" \
    "pgy_host_decl_compat_is_type" \
    "pgy_host_decl_compat_name" \
    "pgy_host_decl_compat_uses_pointer_self" \
    "pgy_host_decl_compat_has_projection_ready_flag" \
    "kPgyHostDeclCompatConstructorDomainTypes[]" \
    "pgy_host_decl_compat_constructor_domain_types" \
    "PgyHostMethodCompatView" \
    "pgy_host_method_compat_view_from_decl" \
    "PgyHostSharedFieldsCompatView" \
    "pgy_host_shared_fields_compat_view_from_decl" \
    "case AST_CLASS_DECL" \
    "case AST_ENUM_DECL" \
    "case AST_PARTY_DECL" \
    "case AST_ROSTER_DECL" \
    "case AST_ROLE_DECL" \
    "case AST_WORLD_DECL" \
    "case AST_RELATION_DECL" \
    "case AST_EFFECT_DECL" \
    "case AST_ZONE_DECL"; do
    require_term "src/codegen/host_decl_compat.c" "$term"
done
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_hosted_shared_field_view_from_decl(ctx, host_name, host_decl)"
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_find_domain_constructor_decl"
require_term "src/codegen/llvm_domain_lookup.c" \
    "pgy_host_decl_compat_constructor_domain_types("
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_find_domain_constructor_decl(ctx, callee_name)"
if grep -Fq "llvm_find_named_domain_decl(ctx, AST_PARTY_DECL, callee_name)" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    fail "LLVM constructor dispatch must consume the domain-constructor lookup seam instead of repeating host chains"
fi
for term in \
    "transpiler_party_slot_first_ability_tag" \
    "transpiler_party_slot_method_ability_tag"; do
    require_term "src/codegen/transpiler_role_ability_helpers.h" "$term"
    require_term "src/codegen/transpiler_role_ability.c" "$term"
done
require_term "src/codegen/transpiler_decl_lookup.h" \
    "TranspilerHostedRoleSlotView"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_role_slot_view_from_decl"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_role_slot_view_required_ability"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "transpiler_hosted_role_slot_view_from_decl(ctx, name, node)"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "transpiler_hosted_role_slot_view_missing_mir_metadata(&role_view)"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "transpiler_hosted_role_slot_view_required_ability("
require_term "src/codegen/transpiler_role_ability.c" \
    "transpiler_hosted_role_slot_view_from_decl(ctx, party_name, party_decl)"
require_term "src/codegen/transpiler_role_ability.c" \
    "transpiler_hosted_role_slot_view_missing_mir_metadata(&role_view)"
require_term "src/codegen/transpiler_role_ability.c" \
    "transpiler_hosted_role_slot_view_required_ability("
require_term "src/codegen/transpiler_statement_dispatch.c" \
    "transpiler_party_slot_first_ability_tag(ctx,"
require_term "src/codegen/transpiler_expr_call_member_emit.c" \
    "transpiler_party_slot_method_ability_tag("
for rel in \
    "src/codegen/transpiler_statement_dispatch.c" \
    "src/codegen/transpiler_expr_call_member_emit.c"; do
    if grep -Fq "ast_party_role_count(" "$ROOT_DIR/$rel"; then
        fail "$rel must consume party-slot ability helpers instead of repeating role-slot scans"
    fi
done
c_role_slot_consumer_hits="$(
    grep -RInE 'ast_party_role_count|ast_party_role\(|ast_role_slot_(name|is_dynamic|required_ability)' \
        "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c" \
        "$ROOT_DIR/src/codegen/transpiler_role_ability.c" || true
)"
if [[ -n "$c_role_slot_consumer_hits" ]]; then
    fail "C party role-slot emission must consume TranspilerHostedRoleSlotView:
$c_role_slot_consumer_hits"
fi
for rel in \
    "src/codegen/llvm_expr_domain_query_calls.c" \
    "src/codegen/transpiler_expr_domain_query_builtin.c"; do
    require_term "$rel" "pgy_host_decl_compat_has_projection_ready_flag(host_decl)"
    if grep -Fq "host_decl->type == AST_RELATION_DECL" "$ROOT_DIR/$rel"; then
        fail "$rel must consume host projection-ready policy instead of repeating relation/effect/zone chains"
    fi
done
for term in \
    "kPgyHostDeclCompatNominalLookupTypes[]" \
    "pgy_host_decl_compat_nominal_lookup_types"; do
    require_term "src/codegen/host_decl_compat.c" "$term"
done
if grep -Fq "kTranspilerNominalHostLookupTypes" \
    "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c"; then
    fail "C nominal host lookup must consume host_decl_compat.c lookup order"
fi
if grep -RIn "pgy_host_decl_compat_name(" "$ROOT_DIR/src/codegen" \
    --include='*.c' --include='*.h' |
    grep -Ev 'src/codegen/(host_decl_compat\.[ch]|llvm_inventory_decl_lookup\.c|transpiler_decl_lookup\.c):'; then
    fail "backend consumers must use llvm_decl_node_name/transpiler_decl_name_local instead of direct host name compatibility"
fi
for term in \
    "ast_class_name(stmt)" \
    "ast_enum_name(stmt)" \
    "return ast_role_name(decl)" \
    "ast_role_name(stmt)" \
    "return ast_party_name(decl)" \
    "ast_party_name(stmt)" \
    "return ast_roster_name(decl)" \
    "ast_roster_name(stmt)" \
    "ast_relation_name(stmt)" \
    "ast_effect_name(stmt)" \
    "ast_zone_name(stmt)" \
    "ast_world_name(stmt)"; do
    if grep -Fq "$term" "$ROOT_DIR/src/codegen/transpiler_decl_lookup.c"; then
        fail "C host declaration names must delegate to host_decl_compat.c"
    fi
done
for rel in \
    "src/codegen/transpiler_enum.c" \
    "src/codegen/transpiler_match_bindings.c"; do
    require_term "$rel" "transpiler_decl_name_local(stmt)"
    if grep -Fq "ast_enum_name(stmt)" "$ROOT_DIR/$rel"; then
        fail "$rel must consume transpiler_decl_name_local for enum host names"
    fi
done
if grep -Fq "ast_enum_name(stmt)" "$ROOT_DIR/src/codegen/llvm_expr_common.c"; then
    fail "LLVM enum declaration lookup must consume llvm_decl_node_name"
fi
if grep -Fq "llvm_active_inventory(ctx, AST_ENUM_DECL" \
        "$ROOT_DIR/src/codegen/llvm_expr_common.c"; then
    fail "LLVM enum lookup must not rescan active inventory after owner lookup"
fi
require_term "src/codegen/llvm_expr_common.c" \
    "return llvm_find_decl_in_active_inventory(ctx, AST_ENUM_DECL, enum_name)"
require_term "src/codegen/llvm_intent_effect.c" "llvm_decl_node_name(zone)"
if grep -Fq "ast_zone_name(zone)" "$ROOT_DIR/src/codegen/llvm_intent_effect.c"; then
    fail "LLVM intent effect zone lookup must consume llvm_decl_node_name"
fi
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_decl_node_name(item)"
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_decl_node_name(host_decl)"
require_term "src/codegen/llvm_expr_call_projection_sync.c" \
    "llvm_decl_node_name(host_decl)"
require_term "src/codegen/llvm_expr_call_projection_sync.c" \
    "llvm_decl_node_name(zone_decl)"
require_term "src/codegen/llvm_expr_call_projection_sync.c" \
    "LLVMHostedDomainSlotView slot_view"
require_term "src/codegen/llvm_expr_call_projection_sync.c" \
    "llvm_hosted_domain_slot_view_from_decl(ctx, zone_name, zone_decl)"
require_term "src/codegen/llvm_expr_call_projection_sync.c" \
    "llvm_hosted_domain_slot_view_name(&slot_view, i)"
if grep -Eq 'ast_zone_slots|ast_domain_slot_(name|type|is_subject)\(' \
        "$ROOT_DIR/src/codegen/llvm_expr_call_projection_sync.c"; then
    fail "LLVM projection sync call emission must consume hosted domain-slot metadata view"
fi
require_term "src/codegen/llvm_expr_domain_query_calls.c" \
    "llvm_decl_node_name(zone_decl)"
require_term "src/codegen/llvm_expr_domain_query_calls.c" \
    "llvm_zone_domain_slot_is_projection("
require_term "src/codegen/llvm_expr_domain_query_calls.c" \
    "llvm_hosted_domain_slot_view_from_decl(ctx, zone_decl_name"
require_term "src/codegen/llvm_expr_domain_query_calls.c" \
    "llvm_hosted_domain_slot_view_is_subject_like(&slot_view, i)"
if grep -Fq "ast_domain_slot_is_subject" \
        "$ROOT_DIR/src/codegen/llvm_expr_domain_query_calls.c"; then
    fail "LLVM domain query builtins must consume projection slot predicates, not AST slot subject checks"
fi
require_term "src/codegen/transpiler_expr_domain_query_builtin.c" \
    "transpiler_decl_name_local(zone_decl)"
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_current_overlay_domain_slot_is_projection"
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_zone_domain_slot_is_projection"
require_term "src/codegen/transpiler_expr_domain_query_builtin.c" \
    "transpiler_current_overlay_domain_slot_is_projection("
require_term "src/codegen/transpiler_expr_domain_query_builtin.c" \
    "transpiler_zone_domain_slot_is_projection("
if grep -Fq "ast_domain_slot_is_subject" \
        "$ROOT_DIR/src/codegen/transpiler_expr_domain_query_builtin.c"; then
    fail "C domain query builtins must consume projection slot predicates, not AST slot subject checks"
fi
if grep -Fq "llvm_count_domain_projection_slots(" \
        "$ROOT_DIR/src/codegen/llvm_domain_projection_count_helpers.h" \
        "$ROOT_DIR/src/codegen/llvm_domain_projection_count.c"; then
    fail "LLVM projection counting must not expose the raw AST slot-array API"
fi
require_term "src/codegen/transpiler_decl_lookup.c" \
    "transpiler_find_projection_nominal_decl_local(TranspilerCtx *ctx"
require_term "src/codegen/transpiler_decl_lookup.c" \
    "ASTNode *decl = transpiler_find_projection_nominal_decl_local("
require_term "src/codegen/transpiler_domain_receiver_query.c" \
    "decl = find_subject_host_decl(ctx, type_name)"
require_term "src/codegen/transpiler_projection.c" \
    "ASTNode *decl = find_subject_host_decl(ctx, type_name)"
require_term "src/codegen/transpiler_projection_emit.c" \
    "vessel_decl = transpiler_find_projection_nominal_decl_local("
require_term "src/codegen/transpiler_projection_field_path.c" \
    "transpiler_find_projection_nominal_decl_local("
require_term "src/codegen/transpiler_projection_method_invalidation.c" \
    "transpiler_find_projection_nominal_decl_local("
require_term "src/codegen/transpiler_expr_projection_builtin.c" \
    "target_decl = transpiler_find_projection_nominal_decl_local(ctx, target_name)"
require_term "src/codegen/transpiler_overlay_projection.c" \
    "target_decl = transpiler_find_projection_nominal_decl_local("
require_term "src/codegen/transpiler_overlay_projection.c" \
    "TranspilerHostedDomainSlotView slot_view"
require_term "src/codegen/transpiler_overlay_projection.c" \
    "transpiler_hosted_domain_slot_view_from_decl(ctx"
require_term "src/codegen/transpiler_overlay_projection.c" \
    "transpiler_hosted_domain_slot_view_name(&slot_view, i)"
if grep -Eq 'ast_zone_slots|ast_domain_slot_is_subject\(' \
        "$ROOT_DIR/src/codegen/transpiler_overlay_projection.c"; then
    fail "C overlay projection embedded zone sync must consume TranspilerHostedDomainSlotView"
fi
require_term "src/codegen/transpiler_intent_zone_slot.c" \
    "TranspilerHostedDomainSlotView slot_view"
require_term "src/codegen/transpiler_intent_zone_slot.c" \
    "transpiler_hosted_domain_slot_view_from_decl(ctx"
require_term "src/codegen/transpiler_intent_zone_slot.c" \
    "transpiler_hosted_domain_slot_view_is_subject_like(&slot_view, i)"
if grep -Eq 'ast_zone_slots|ast_domain_slot_(name|type|is_subject)\(' \
        "$ROOT_DIR/src/codegen/transpiler_intent_zone_slot.c"; then
    fail "C intent zone-slot resolution must consume TranspilerHostedDomainSlotView"
fi
require_term "src/codegen/transpiler_domain_provenance_emit.c" \
    "target_decl = transpiler_find_projection_nominal_decl_local("
for rel in \
    "src/codegen/transpiler_domain_provenance_emit.c" \
    "src/codegen/transpiler_expr_dispatch_emit.c" \
    "src/codegen/transpiler_expr_projection_builtin.c" \
    "src/codegen/transpiler_overlay_projection.c" \
    "src/codegen/transpiler_projection.c" \
    "src/codegen/transpiler_projection_field_path.c" \
    "src/codegen/transpiler_projection_method_invalidation.c"; do
    if grep -Eq 'find_class_decl\(ctx, (host_type_name|target_type_name|source_type_name|source_type|target_name|type_name|ast_type_name\(field->type\))\)' \
            "$ROOT_DIR/$rel"; then
        fail "$rel must consume nominal-host lookup instead of direct class lookup"
    fi
done
for rel in \
    "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "src/codegen/llvm_expr_call_projection_sync.c" \
    "src/codegen/llvm_expr_domain_query_calls.c" \
    "src/codegen/transpiler_expr_domain_query_builtin.c"; do
    if grep -Eq 'ast_(effect|zone|world)_name\((item|host_decl|zone_decl)\)' \
            "$ROOT_DIR/$rel"; then
        fail "$rel must consume the host declaration name owner"
    fi
done
for term in \
    "transpiler_mir_decl_method_param_count" \
    "transpiler_mir_decl_method_param" \
    "transpiler_mir_decl_method_return_type" \
    "transpiler_mir_decl_method_is_action_like"; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
done
for term in \
    "emit_hosted_method_forward_decl_from_metadata" \
    "method_meta == NULL" \
    "transpiler_mir_decl_method_param_count(method_meta)" \
    "transpiler_mir_decl_method_return_type(method_meta)" \
    "transpiler_mir_decl_method_param(method_meta, j)"; do
    require_term "src/codegen/transpiler_func_forward_metadata.c" "$term"
done
if grep -Fq "host_name == NULL || method == NULL || buf == NULL || ctx == NULL" \
        "$ROOT_DIR/src/codegen/transpiler_func_forward_metadata.c"; then
    fail "C hosted method forward declarations must not require source AST when MIRDeclMethod metadata exists"
fi
for rel in \
    "src/codegen/transpiler_class_decl_emit.c" \
    "src/codegen/transpiler_enum_decl_emit.c"; do
    require_term "$rel" "transpiler_hosted_method_view_from_decl(ctx"
    require_term "$rel" "transpiler_hosted_method_view_source_ast(&method_view, i)"
    require_term "$rel" "transpiler_mir_decl_method_routine(ctx, method_meta)"
    require_term "$rel" "method_meta == NULL"
    require_term "$rel" "emit_hosted_method_forward_decl_from_metadata"
done
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_hosted_method_view_from_decl(ctx, base_class_name"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_hosted_method_view_source_ast(&method_view, i)"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_mir_decl_method_routine(ctx, method_meta)"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_mir_routine_source_ast_of_type("
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "method_meta == NULL"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "emit_hosted_method_forward_decl_from_metadata"
if grep -Eq 'class_decl->data\.class_decl\.methods\[[^]]+\]' \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"; then
    fail "generic class specialization must consume TranspilerHostedMethodView, not index AST method arrays"
fi
for term in \
    "TranspilerHostedMethodView" \
    "TranspilerHostedFieldView" \
    "ast_compat_methods" \
    "ast_compat_count" \
    "ast_compat_fields" \
    "transpiler_hosted_method_view(" \
    "transpiler_hosted_method_view_metadata(" \
    "transpiler_find_host_method_metadata_in_context(" \
    "transpiler_mir_decl_method_name(" \
    "transpiler_mir_decl_method_source_ast(" \
    "transpiler_mir_decl_method_is_async(" \
    "transpiler_mir_decl_method_is_action_like(" \
    "transpiler_mir_decl_method_within_zone(" \
    "transpiler_mir_decl_method_causes_effect(" \
    "transpiler_mir_decl_method_routine(" \
    "transpiler_hosted_method_view_from_decl(" \
    "transpiler_hosted_method_view_source_ast(" \
    "transpiler_hosted_method_view_missing_mir_metadata(" \
    "transpiler_hosted_class_field_view_from_decl(" \
    "transpiler_hosted_field_view_metadata(" \
    "transpiler_hosted_field_view_name(" \
    "transpiler_hosted_field_view_type(" \
    "transpiler_hosted_field_view_missing_mir_metadata("; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
done
if grep -Fq "transpiler_hosted_method_view_routine" \
    "$ROOT_DIR/src/codegen/transpiler_decl_lookup.h" \
    "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"; then
    fail "C hosted method routine lookup must consume transpiler_mir_decl_method_routine directly"
fi
for term in \
    "transpiler_hosted_class_field_view_from_decl" \
    "pgy_host_class_fields_compat_view_from_decl(decl)" \
    "view.count = mir_decl_header_field_count(header)" \
    "return mir_decl_header_field(view->decl_header, index)" \
    "mir_decl_field_name(field)" \
    "mir_decl_field_type(field)"; do
    require_term "src/codegen/transpiler_decl_field_view.c" "$term"
done
require_term "src/codegen/transpiler_context.h" \
    "transpiler_set_mir_inventory_missing"
require_term "src/codegen/transpiler_context.h" \
    "transpiler_set_mir_topology_invalid"
require_term "src/codegen/transpiler_context.h" \
    "transpiler_set_mir_intent_carrier_missing"
for term in \
    "transpiler_set_mir_inventory_missing" \
    "transpiler_set_mir_topology_invalid" \
    "transpiler_set_mir_intent_carrier_missing" \
    "PGY_CODE_MIR_TOPOLOGY_INVALID" \
    "PGY_CODE_MIR_INTENT_CARRIER_MISSING" \
    "PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING" \
    "PGY_CAUSE_MIR_TOPOLOGY_INVALID" \
    "PGY_CAUSE_MIR_INTENT_CARRIER_MISSING" \
    "PGY_FIX_CHECK_INTENT_STEP_LOWERING" \
    "PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING"; do
    require_term "src/codegen/transpiler_context.c" "$term"
done
if grep -RIn "PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING" "$ROOT_DIR/src/codegen" \
    | grep -v "src/codegen/transpiler_context.c"; then
    fail "C backend MIR-missing diagnostics must route through transpiler_set_mir_inventory_missing"
fi
for term in \
    "view->count != view->ast_compat_count" \
    "if (view->requires_mir_metadata)"; do
    require_term "src/codegen/transpiler_decl_method_view.c" "$term"
done
for term in \
    "view.decl_header = header" \
    "view.count = mir_decl_header_method_count(header)" \
    "return mir_decl_header_method(view->decl_header, index)" \
    "transpiler_hosted_method_view_missing_mir_method_row(" \
    "transpiler_require_hosted_method_view_rows("; do
    require_term "src/codegen/transpiler_decl_method_view.c" "$term"
done
if grep -RInE 'const MIRDeclMethod \*metadata|view[.]metadata|view->metadata' \
        "$ROOT_DIR/src/codegen/transpiler_decl_lookup.h" \
        "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"; then
    fail "C hosted method view must keep MIRDeclMethod arrays behind compiler accessors"
fi
if grep -Fq "transpiler_hosted_method_view(" \
        "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c" \
    && grep -Fq "NULL, 0)" \
        "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"; then
    fail "C hosted method view must preserve AST compatibility counts when a MIR header exists"
fi
if grep -Fq "fallback_methods" "$ROOT_DIR/src/codegen/transpiler_decl_lookup.h" \
    "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"; then
    fail "C hosted method view must name AST compatibility paths explicitly, not as fallback_methods"
fi
if grep -Fq "fallback_count" "$ROOT_DIR/src/codegen/transpiler_decl_lookup.h" \
    "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"; then
    fail "C hosted method view must name AST compatibility counts explicitly, not as fallback_count"
fi
for rel in \
    "src/codegen/transpiler_domain_nominal_emit.c" \
    "src/codegen/transpiler_world_select_event_emit.c"; do
    require_term "$rel" "transpiler_hosted_method_view_from_decl(ctx"
    require_term "$rel" "transpiler_hosted_method_view_source_ast(&method_view, i)"
    require_term "$rel" "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
    require_term "$rel" "emit_hosted_method_forward_decl_from_metadata"
done
for rel in \
    "src/codegen/transpiler_class_decl_emit.c" \
    "src/codegen/transpiler_enum_decl_emit.c" \
    "src/codegen/transpiler_domain_nominal_emit.c" \
    "src/codegen/transpiler_relation_effect_emit.c" \
    "src/codegen/transpiler_roster_decl_emit.c" \
    "src/codegen/transpiler_world_select_event_emit.c" \
    "src/codegen/transpiler_zone_decl_emit.c"; do
    require_term "$rel" "transpiler_require_hosted_method_view_rows("
done
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_method_view_from_decl(ctx"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_zone_methods_emit.c" \
    "transpiler_hosted_method_view_source_ast(method_view, i)"
require_term "src/codegen/transpiler_zone_methods_emit.c" \
    "emit_hosted_method_forward_decl_from_metadata"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "MIR-only C path missing method declaration metadata for party"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for party"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for role"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "method_meta == NULL"
require_term "src/codegen/transpiler_roster_decl_emit.c" \
    "MIR-only C path missing method declaration metadata for roster"
require_term "src/codegen/transpiler_roster_decl_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for roster"
require_term "src/codegen/transpiler_roster_decl_emit.c" \
    "method_meta == NULL"
for term in \
    "MIR-only C path missing method declaration metadata for relation" \
    "MIR-only C path missing method declaration metadata for effect" \
    "MIR-only C path has invalid method declaration metadata row for relation" \
    "MIR-only C path has invalid method declaration metadata row for effect"; do
    require_term "src/codegen/transpiler_relation_effect_emit.c" "$term"
done
require_term "src/codegen/transpiler_relation_effect_emit.c" \
    "method_meta == NULL"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "MIR-only C path missing method declaration metadata for zone"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "transpiler_require_hosted_method_view_rows("
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for zone"
require_term "src/codegen/transpiler_zone_methods_emit.c" \
    "method_meta == NULL"
require_term "src/codegen/transpiler_zone_methods_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for zone"
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "MIR-only C path missing method declaration metadata for world"
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "method_meta == NULL"
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for world"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "MIR-only C path missing method declaration metadata for generic class"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for generic class"
if grep -RIn "emit_hosted_method_forward_decl_named" "$ROOT_DIR/src/codegen"; then
    fail "C hosted method forward declarations must use MIRDeclMethod metadata-first helper"
fi
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "const TranspilerHostedMethodView *method_view"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_hosted_method_view_metadata(method_view, i)"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_mir_decl_method_routine(ctx, method_meta)"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_mir_routine_source_ast_of_type("
if grep -Eq 'emit_hosted_methods_from_mir_or_error_local\([^)]*ASTNode \*\*methods|emit_hosted_methods_from_mir_or_error_local\([^)]*size_t method_count' \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.h"; then
    fail "hosted method body emission must accept TranspilerHostedMethodView, not AST method arrays"
fi
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_hosted_method_view_from_decl(ctx, name"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_mir_routine_source_ast_of_type("
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for class"
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_hosted_method_view_from_decl(ctx, ename"
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_mir_routine_source_ast_of_type("
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for enum"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(method_view)"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_hosted_method_view_missing_mir_method_row(method_view, i)"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_intent_emit.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_intent_emit_metadata_helpers.h" \
    "transpiler_set_mir_intent_carrier_missing("
require_term "src/codegen/transpiler_intent_cleanup_emit.c" \
    "transpiler_set_mir_intent_carrier_missing("
if grep -RIn "PGY_CODE_MIR_INTENT_CARRIER_MISSING" \
    "$ROOT_DIR/src/codegen/transpiler_intent_emit_metadata_helpers.h" \
    "$ROOT_DIR/src/codegen/transpiler_intent_cleanup_emit.c"; then
    fail "C intent carrier diagnostics must use transpiler_set_mir_intent_carrier_missing"
fi
require_term "src/codegen/transpiler_mir_emit_state.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_mir_func_emit.c" \
    "transpiler_set_mir_topology_invalid("
require_term "src/codegen/transpiler_mir_terminator_emit.c" \
    "transpiler_set_mir_topology_invalid("
require_term "src/codegen/transpiler_mir_reason_classifier.h" \
    "transpiler_classify_mir_function_reason"
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "PGY_CODE_MIR_UNRESOLVED_LOCAL"
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "PGY_CODE_MIR_SIGNATURE_UNSUPPORTED"
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "PGY_CODE_MIR_SSA_LIMIT"
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "kMirFunctionReasonPatterns"
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "\"MIR contract invalid\""
require_term "src/codegen/transpiler_mir_reason_classifier.c" \
    "\"no matching MIR routine\""
require_term "src/codegen/transpiler_func_class_flow_emit.c" \
    "transpiler_classify_mir_function_reason(reason)"
if grep -n "strstr(reason" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"; then
    fail "C function emitter must not classify MIR reason strings inline"
fi
for rel in \
    "src/codegen/transpiler_class_decl_emit.c" \
    "src/codegen/transpiler_enum_decl_emit.c" \
    "src/codegen/transpiler_generic_class_specialization_emit.c"; do
    if grep -Fq "transpiler_find_mir_method(ctx" "$ROOT_DIR/$rel"; then
        fail "$rel must use TranspilerHostedMethodView routine metadata, not a secondary method lookup"
    fi
done
if sed -n '1,110p' "$ROOT_DIR/src/codegen/transpiler_hosted_method_body_emit.c" \
    | grep -Fq "transpiler_find_mir_method(ctx"; then
    fail "hosted role/domain method emission must use MIRDeclMethod routine metadata, not a secondary method lookup"
fi
if grep -RIn "transpiler_find_mir_method" "$ROOT_DIR/src/codegen"; then
    fail "generic C method lookup helper name must not reappear"
fi
if grep -RIn "transpiler_find_role_impl_mir_method" "$ROOT_DIR/src/codegen"; then
    fail "C role method emission must consume TranspilerHostedMethodView metadata, not owner/name routine lookup"
fi
if grep -RInE 'method->name' \
        "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c"; then
    fail "C role/host method lookup must consume MIRDeclMethod name accessor"
fi
c_method_raw_hits="$(
    c_method_files=()
    for path in "$ROOT_DIR"/src/codegen/transpiler*.c \
        "$ROOT_DIR"/src/codegen/transpiler*.h; do
        [[ -e "$path" ]] || continue
        rel="${path#$ROOT_DIR/}"
        if [[ "$rel" == "src/codegen/transpiler_decl_lookup.h" ||
              "$rel" == "src/codegen/transpiler_decl_method_view.c" ]]; then
            continue
        fi
        c_method_files+=("$path")
    done
    if ((${#c_method_files[@]})); then
        grep -EHIn 'data\.(class_decl|enum_decl|relation_decl|effect_decl|zone_decl|world_decl|party_decl|roster_decl)\.methods\[[^]]+\]|data\.(class_decl|enum_decl|relation_decl|effect_decl|zone_decl|world_decl|party_decl|roster_decl)\.method_count' \
            "${c_method_files[@]}" | sed "s#^$ROOT_DIR/##" || true
    fi
)"
if [[ -n "$c_method_raw_hits" ]]; then
    fail "C backend hosted-method emission must use TranspilerHostedMethodView outside method-view owners:
$c_method_raw_hits"
fi

c_routine_raw_hits="$(
    c_routine_files=()
    for path in "$ROOT_DIR"/src/codegen/*.c \
        "$ROOT_DIR"/src/codegen/*.h; do
        [[ -e "$path" ]] || continue
        rel="${path#$ROOT_DIR/}"
        if [[ "$rel" == src/codegen/llvm* ||
              "$rel" == "src/codegen/transpiler.h" ||
              "$rel" == "src/codegen/transpiler_inventory_view.h" ||
              "$rel" == "src/codegen/transpiler_inventory_view.c" ||
              "$rel" == "src/codegen/transpiler_decl_lookup.h" ||
              "$rel" == "src/codegen/transpiler_decl_method_view.c" ]]; then
            continue
        fi
        c_routine_files+=("$path")
    done
    if ((${#c_routine_files[@]})); then
        grep -EHIn '\bctx->mir->routine_count\b|\bctx->mir->routines\b|\bmir->routine_count\b|\bmir->routines\b' \
            "${c_routine_files[@]}" | sed "s#^$ROOT_DIR/##" || true
    fi
)"
if [[ -n "$c_routine_raw_hits" ]]; then
    fail "C backend routine inventory must use TranspilerMIRRoutineInventory outside helper owners:
$c_routine_raw_hits"
fi

routine_raw_hits="$(
    for rel in \
        "src/codegen/llvm_pipeline.c" \
        "src/codegen/llvm_domain.c" \
        "src/codegen/llvm_intent.c"; do
        grep -EIn '\bctx->mir->routine_count\b|\bctx->mir->routines\b|\bmir->routine_count\b|\bmir->routines\b' \
            "$ROOT_DIR/$rel" | sed "s#^#$rel:#" || true
    done
)"
if [[ -n "$routine_raw_hits" ]]; then
    fail "LLVM routine inventory must go through llvm_active_routine_inventory outside the helper owner:
$routine_raw_hits"
fi

if grep -Fq "decl_header->source_ast == decl" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h"; then
    fail "LLVM host method inventory must be metadata-first; do not require source_ast identity"
fi
require_term "src/codegen/llvm_inventory_host_methods.c" \
    "llvm_active_has_mir(ctx)"
if grep -Fq "ctx->mir" \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM host method inventory must use active MIR view helpers, not direct ctx->mir probes"
fi

if grep -RIn "llvm_find_mir_method_routine_local" "$ROOT_DIR/src/codegen"; then
    fail "LLVM AST/name-based MIR method routine compatibility helper must not reappear"
fi

for term in \
    "return llvm_is_host_decl_type(decl->type)" \
    "llvm_decl_node_name(decl)" \
    "llvm_find_host_decl_header_in_context(ctx, type_name)" \
    "llvm_find_host_decl_in_active_inventory(ctx, type_name)" \
    "llvm_host_decl_uses_pointer_self"; do
    require_term "src/codegen/llvm_domain_lookup.c" "$term"
done
for term in \
    "pgy_host_decl_compat_types(&host_type_count)" \
    "if (ctx->mir != NULL)" \
    "host_types[i]" \
    "llvm_find_decl_in_active_inventory("; do
    require_term "src/codegen/llvm_inventory_decl_lookup.c" "$term"
done
if grep -Eq 'llvm_find_decl_in_active_inventory\([^,]+,[[:space:]]*AST_(CLASS|ENUM|RELATION|EFFECT|ZONE|WORLD)_DECL' \
    "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"; then
    fail "LLVM host-decl fallback must iterate host_decl_compat.c, not a partial hard-coded host chain"
fi
for term in \
    "llvm_member_call_adjust_pointer_self_arg" \
    "llvm_mir_decl_method_param_count(method_meta)" \
    "llvm_mir_decl_method_param(method_meta, pk)"; do
    require_term "src/codegen/llvm_member_call_support.c" "$term"
done
for term in \
    "llvm_find_host_method_metadata_in_context(ctx," \
    "llvm_mir_decl_method_source_ast(method_meta)" \
    "llvm_member_call_adjust_pointer_self_arg("; do
    require_term "src/codegen/llvm_member_call_emit.c" "$term"
done
if grep -Eq 'ast_func_param_count\(method_decl\)|ast_func_param\(method_decl' \
    "$ROOT_DIR/src/codegen/llvm_member_call_emit.c"; then
    fail "LLVM member-call emit must consume MIRDeclMethod metadata through llvm_member_call_adjust_pointer_self_arg"
fi
for term in \
    "llvm_find_host_method_metadata_in_context(ctx," \
    "llvm_mir_decl_method_within_zone(method_meta)" \
    "llvm_mir_decl_method_causes_effect(method_meta)" \
    "llvm_mir_decl_method_is_action_like(method_meta)" \
    "if (method_meta == NULL)"; do
    require_term "src/codegen/llvm_stmt_zone_action.c" "$term"
done
for term in \
    "const MIRDeclMethod *method_meta" \
    "llvm_mir_decl_method_is_async(method_meta)" \
    "llvm_mir_decl_method_within_zone(method_meta)" \
    "llvm_mir_decl_method_causes_effect(method_meta)" \
    "llvm_mir_decl_method_is_action_like(method_meta)" \
    "if (method_meta == NULL)"; do
    require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" "$term"
done
require_term "src/codegen/llvm_member_call_emit.c" \
    "obj_node, method_meta, method_decl"
for term in \
    "transpiler_find_host_method_metadata_in_context(ctx," \
    "transpiler_mir_decl_method_is_async(method_meta)" \
    "transpiler_mir_decl_method_within_zone(method_meta)" \
    "transpiler_mir_decl_method_causes_effect(method_meta)" \
    "transpiler_mir_decl_method_is_action_like(method_meta)" \
    "if (method_meta == NULL)"; do
    require_term "src/codegen/transpiler_projection_sync.c" "$term"
done
require_term "src/codegen/transpiler_expr_call_member_emit.c" \
    "ctx, obj, method_meta, method_decl"
if grep -Fq "llvm_decl_current_nominal_name" \
    "$ROOT_DIR/src/codegen/llvm_decl.c"; then
    fail "LLVM implicit self lowering must use the shared current host-name helper"
fi
if grep -Fq "routine->ast == method->source_ast" \
    "$ROOT_DIR/src/compiler/mir_decl_headers.c"; then
    fail "MIRDeclMethod routine linking must not use AST identity matching"
fi
for term in \
    "mir_decl_next_capacity" \
    "mir_decl_method_matches_routine" \
    "mir_decl_header_set_role_impl_methods" \
    "ast_role_impl_method_total_count" \
    "SIZE_MAX / sizeof(MIRDeclMethod)" \
    "case AST_ROLE_DECL"; do
    require_term "src/compiler/mir_decl_headers.c" "$term"
done
for term in \
    "hir->role_count" \
    "mir_record_decl_header(mir, hir->roles[i])"; do
    require_term "src/compiler/mir.c" "$term"
done
for term in \
    "llvm_mir_decl_method_param_count(method_meta)" \
    "llvm_mir_decl_method_return_type(method_meta)" \
    "llvm_mir_decl_method_is_action_like(method_meta)" \
    "llvm_hosted_method_view_from_decl(ctx, enum_name, stmt)" \
    "llvm_hosted_method_view_from_decl(ctx, cls_name, stmt)" \
    "llvm_hosted_method_view_missing_mir_metadata(&enum_method_view)" \
    "llvm_hosted_method_view_missing_mir_metadata(&class_method_view)" \
    "llvm_hosted_method_view_metadata(&enum_method_view, j)" \
    "llvm_hosted_method_view_metadata(&class_method_view, j)" \
    "llvm_require_hosted_method_view_rows(" \
    "llvm_mir_decl_method_source_ast(method_meta)" \
    "llvm_set_mir_inventory_missing(ctx" \
    "MIR-only LLVM path missing enum method declaration metadata" \
    "MIR-only LLVM path missing class method declaration metadata" \
    "MIR-only LLVM path has invalid method declaration metadata row for enum" \
    "MIR-only LLVM path has invalid method declaration metadata row for class"; do
    require_term "src/codegen/llvm_register.c" "$term"
done
if grep -Eq 'for[[:space:]]*\([^)]*stmt->data\.(enum_decl|class_decl)\.method_count' \
    "$ROOT_DIR/src/codegen/llvm_register.c"; then
    fail "LLVM nominal method registration must iterate MIRDeclMethod metadata, not AST method_count"
fi
if grep -Eq 'stmt->data\.(enum_decl|class_decl)\.method_count' \
    "$ROOT_DIR/src/codegen/llvm_register.c"; then
    fail "LLVM nominal method registration must use LLVMHostedMethodView for method-count guards"
fi
if grep -Eq 'stmt->data\.(enum_decl|class_decl)\.methods\[[^]]+\]' \
    "$ROOT_DIR/src/codegen/llvm_register.c"; then
    fail "LLVM nominal method registration must not index AST method arrays"
fi
if grep -Eq 'enum_method_metadata|class_method_metadata|[.]metadata' \
    "$ROOT_DIR/src/codegen/llvm_register.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_method_emit.c"; then
    fail "LLVM method consumers must use LLVMHostedMethodView accessors, not view metadata arrays"
fi

domain_method_forward_body="$(
    awk '
        /llvm_emit_domain_method_forward_decls\(LLVMGenCtx \*ctx,/ { in_body = 1 }
        /llvm_emit_domain_ability_vtables\(LLVMGenCtx \*ctx,/ { in_body = 0 }
        in_body { print }
    ' "$ROOT_DIR/src/codegen/llvm_domain_forward.c"
)"
for term in \
    "llvm_hosted_method_view_missing_mir_metadata(methods)" \
    "MIR-only LLVM path missing method forward metadata for domain" \
    "llvm_hosted_method_view_metadata(methods, j)" \
    "llvm_hosted_method_view_missing_mir_method_row(methods, j)" \
    "MIR-only LLVM path has invalid method forward metadata row for domain" \
    "llvm_domain_method_param_count_metadata_first" \
    "llvm_domain_method_param_metadata_first" \
    "llvm_domain_method_return_type_metadata_first"; do
    grep -Fq "$term" <<<"$domain_method_forward_body" ||
        fail "LLVM domain method forward declarations must be MIRDeclMethod metadata-first: missing $term"
done
if grep -Eq 'method->data\.func_decl\.(param_count|return_type)' \
    <<<"$domain_method_forward_body"; then
    fail "LLVM domain method forward declarations must not read AST method param_count/return_type directly"
fi
role_method_forward_body="$(
    awk '
        /llvm_emit_role_method_forward_decls_metadata_first/ { in_body = 1 }
        /llvm_emit_domain_role_forward_decls\(LLVMGenCtx \*ctx,/ { in_body = 0 }
        in_body { print }
    ' "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
)"
for term in \
    "llvm_hosted_method_view_missing_mir_metadata(methods)" \
    "MIR-only LLVM path missing method forward metadata for role" \
    "llvm_hosted_method_view_metadata(methods, j)" \
    "llvm_hosted_method_view_missing_mir_method_row(methods, j)" \
    "MIR-only LLVM path has invalid method forward metadata row for role" \
    "llvm_domain_method_param_count_metadata_first" \
    "llvm_domain_method_param_metadata_first" \
    "llvm_domain_method_return_type_metadata_first"; do
    grep -Fq "$term" <<<"$role_method_forward_body" ||
        fail "LLVM role method forward declarations must be MIRDeclMethod metadata-first: missing $term"
done
if grep -Eq 'method->data\.func_decl\.(param_count|return_type)' \
    <<<"$role_method_forward_body"; then
    fail "LLVM role method forward declarations must not read AST method param_count/return_type directly"
fi
ability_vtable_body="$(
    awk '
        /llvm_emit_domain_ability_vtables\(LLVMGenCtx \*ctx,/ { in_body = 1 }
        /#endif/ { in_body = 0 }
        in_body { print }
    ' "$ROOT_DIR/src/codegen/llvm_domain_forward_ability.c"
)"
role_operator_body="$(
    awk '
        /llvm_emit_role_operator_forward_decl/ { in_body = 1 }
        /llvm_emit_domain_role_forward_decls\(LLVMGenCtx \*ctx,/ { in_body = 0 }
        in_body { print }
    ' "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
)"
for body_name in ability_vtable_body role_operator_body; do
    body="${!body_name}"
    for term in \
        "llvm_domain_method_param_count_metadata_first" \
        "llvm_domain_method_param_metadata_first" \
        "llvm_domain_method_return_type_metadata_first"; do
        grep -Fq "$term" <<<"$body" ||
            fail "LLVM ${body_name} must route method signature reads through the shared method accessors: missing $term"
    done
    if grep -Eq 'method->data\.func_decl\.(param_count|return_type)' <<<"$body"; then
        fail "LLVM ${body_name} must not read AST method param_count/return_type directly"
    fi
done
require_term "src/codegen/llvm_domain_forward.h" \
    "const LLVMHostedMethodView *methods"
require_term "src/codegen/llvm_domain_forward.c" \
    "llvm_hosted_method_view_metadata(methods, j)"
require_term "src/codegen/llvm_domain_forward_role.c" \
    "llvm_emit_role_method_forward_decls_metadata_first"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "LLVMHostedMethodView method_view"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "llvm_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "MIR-only LLVM path missing method declaration metadata for domain"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "llvm_hosted_method_view_metadata(&method_view, j)"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "llvm_hosted_method_view_missing_mir_method_row(&method_view, j)"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "MIR-only LLVM path has invalid method declaration metadata row for domain"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "MIR-only LLVM path has invalid method declaration metadata row for class"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "LLVMHostedMethodView method_view"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path missing method declaration metadata for role"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_hosted_method_view_metadata(&method_view, j)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_hosted_method_view_missing_mir_method_row(&method_view, j)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path has invalid method declaration metadata row for role"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_mir_decl_method_routine(ctx, method_meta)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_role_method_name_from_ast"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "llvm_role_for_type_name"
require_term "src/codegen/llvm_domain_forward_role.c" \
    "llvm_role_for_type_node(stmt)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_role_for_type_name(stmt)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path missing registered function for role method"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path missing vtable function for role method"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "ctx != NULL && ctx->backend_error != NULL"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "ast_include_role_name(include_stmt)"
require_term "src/codegen/transpiler_operator.c" \
    "ast_include_role_name(include_stmt)"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "ast_include_role_name(inc)"
require_term "src/codegen/transpiler_operator.c" \
    "ast_impl_ability_method(impl, j)"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "ast_impl_ability_method(impl, j)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "ast_impl_ability_ref(impl)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "ast_impl_ability_method(impl, j)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "ast_impl_ability_name(impl)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "ast_impl_ability_method(impl, j)"
require_term "src/codegen/llvm_domain_role_helpers.h" \
    "llvm_party_slot_first_ability_name"
require_term "src/codegen/llvm_domain_role_helpers.h" \
    "llvm_lookup_role_vtable_global"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "LLVMHostedRoleSlotView"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_role_slot_view_from_decl"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_role_slot_view_is_dynamic"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_role_slot_view_required_ability"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_role_slot_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_role_slot_view_missing_mir_metadata(&role_view)"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_role_slot_view_is_dynamic(&role_view, j)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_role_slot_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_role_slot_view_missing_mir_metadata(&role_view)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_role_slot_view_name(&role_view, j)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_role_slot_view_is_dynamic(&role_view, j)"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "llvm_role_vtable_global_name"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "llvm_hosted_role_slot_view_from_decl("
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "llvm_hosted_role_slot_view_required_ability("
require_term "src/codegen/llvm_stmt.c" \
    "llvm_party_slot_first_ability_name("
require_term "src/codegen/llvm_stmt.c" \
    "llvm_lookup_role_vtable_global("
require_term "src/codegen/transpiler_statement_dispatch.c" \
    "lookup_typed_var(ctx, pvar)"
require_term "src/codegen/transpiler_statement_dispatch.c" \
    "transpiler_resolve_active_ssa_name(ctx, pvar)"
require_term "src/codegen/transpiler_statement_dispatch.c" \
    "transpiler_make_c_ssa_name(ctx, pvar_ssa)"
require_term "src/compiler/mir_decl_headers.c" \
    "ast_impl_ability_method(impl, j)"
require_term "src/parser/ast_domain_api.h" \
    "ast_role_impl_method_total_count"
require_term "src/parser/ast_role_type_accessors.c" \
    "ast_role_impl_method_total_count"
require_term "src/compiler/mir_decl_header_validate.c" \
    "ast_role_impl_method_total_count"
if grep -R "data\.include_stmt" "$ROOT_DIR/src/codegen" >/dev/null; then
    fail "C/LLVM codegen include payload consumers must use AST include accessors"
fi
if grep -R "data\.impl_ability" \
    "$ROOT_DIR/src/compiler/mir_decl_headers.c" >/dev/null; then
    fail "MIR role impl method metadata must use AST impl-ability accessors"
fi
if grep -R "data\.role_decl\.\(for_type\|includes\|include_count\|impl_abilities\|impl_count\)" \
    "$ROOT_DIR/src/compiler/mir_decl_headers.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_lookup.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c" \
    "$ROOT_DIR/src/codegen/transpiler_operator.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c" >/dev/null; then
    fail "MIR/C/LLVM role inventory paths must use AST role accessors"
fi
if grep -R "data\.ability_decl\.\(methods\|method_count\)" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.c" >/dev/null; then
    fail "C/LLVM ability method inventory paths must use AST ability accessors"
fi
if grep -R "data\.impl_ability" \
    "$ROOT_DIR/src/codegen/transpiler_operator.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_lookup.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c" >/dev/null; then
    fail "C/LLVM role emission compatibility paths must use AST impl-ability accessors"
fi
if grep -Fq "LLVMGetFirstGlobal(ctx->module)" \
    "$ROOT_DIR/src/codegen/llvm_stmt.c"; then
    fail "LLVM bind lowering must resolve role vtable globals by party-slot ability, not module-prefix scans"
fi
if grep -Fq "strstr(gname, \"_vtable_instance\")" \
    "$ROOT_DIR/src/codegen/llvm_stmt.c"; then
    fail "LLVM bind lowering must not recover role vtable ability from global-name substring scans"
fi
if grep -Eq 'ast_party_role_count|ast_party_role\(|ast_role_slot_(name|required_ability)' \
    "$ROOT_DIR/src/codegen/llvm_stmt.c"; then
    fail "LLVM bind lowering must consume llvm_party_slot_first_ability_name instead of scanning party role slots"
fi
role_slot_consumer_hits="$(
    grep -RInE 'ast_party_role_count|ast_party_role\(|ast_role_slot_(name|is_dynamic)' \
        "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c" \
        "$ROOT_DIR/src/codegen/llvm_domain_struct_register_fields.c" || true
)"
if [[ -n "$role_slot_consumer_hits" ]]; then
    fail "LLVM party role-slot struct registration must consume LLVMHostedRoleSlotView:
$role_slot_consumer_hits"
fi
llvm_role_slot_lookup_hits="$(
    grep -RInE 'ast_party_role_count|ast_party_role\(|ast_role_slot_(name|required_ability)' \
        "$ROOT_DIR/src/codegen/llvm_domain_role_lookup.c" || true
)"
if [[ -n "$llvm_role_slot_lookup_hits" ]]; then
    fail "LLVM party role-slot ability lookup must consume LLVMHostedRoleSlotView:
$llvm_role_slot_lookup_hits"
fi
if grep -Fq "llvm_role_vtable_global_name(" \
    "$ROOT_DIR/src/codegen/llvm_stmt.c"; then
    fail "LLVM bind lowering must consume llvm_lookup_role_vtable_global instead of constructing role vtable globals locally"
fi
if grep -Eq 'ctx->typed_vars\[[^]]+\]\.(name|type_name)' \
    "$ROOT_DIR/src/codegen/transpiler_statement_dispatch.c"; then
    fail "C bind lowering must consume typed-var and SSA name seams instead of scanning ctx->typed_vars locally"
fi
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "ctx != NULL && ctx->backend_error != NULL"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "ensure_ability_ref_vtable_decl(ability_ref, ctx)"
require_term "src/codegen/transpiler_decl_host_lookup.c" \
    "transpiler_role_subject_type_name_local"
require_term "src/codegen/transpiler_operator.c" \
    "transpiler_role_subject_type_name_local(role)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "transpiler_role_subject_type_node_local(role)"
if grep -Fq "llvm_find_mir_method_routine_local(ctx," \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"; then
    fail "LLVM role method body emission must use linked MIRDeclMethod routine indexes, not AST/name routine search"
fi
if grep -Fq "llvm_find_mir_method_routine_local(ctx," \
    "$ROOT_DIR/src/codegen/llvm_domain_method_emit.c"; then
    fail "LLVM hosted domain method body emission must use linked MIRDeclMethod routine indexes, not AST/name routine search"
fi
if grep -Eq 'role_decl\.for_type' \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"; then
    fail "LLVM role operator emission must read role target type through llvm_domain_role_lookup helpers"
fi
for rel in \
    "src/codegen/llvm_domain_role_lookup.c" \
    "src/codegen/llvm_domain_forward_role.c" \
    "src/codegen/llvm_domain_role_emit.c"; do
    require_term "$rel" "llvm_find_role_operator_method_metadata"
done
if grep -Fq "llvm_find_role_operator_method(ctx" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"; then
    fail "LLVM role operator bridge must consume MIRDeclMethod metadata, not AST method lookup"
fi
if grep -Eq 'role_decl\.for_type' \
    "$ROOT_DIR/src/codegen/transpiler_operator.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"; then
    fail "C role operator emission must read role target type through transpiler decl-host helpers"
fi
require_term "src/codegen/transpiler_operator.c" \
    "find_role_operator_method_metadata"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "find_role_operator_method_metadata(ctx, role, op, 0)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "!transpiler_active_has_mir(ctx)"
if grep -Eq 'llvm_emit_domain_method_forward_decls\([^)]*ASTNode \*\*methods|llvm_emit_domain_method_forward_decls\([^)]*size_t method_count' \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.h" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c"; then
    fail "LLVM domain method forward declarations must accept LLVMHostedMethodView, not AST method arrays"
fi
llvm_method_raw_hits="$(
    llvm_method_files=()
    for path in "$ROOT_DIR"/src/codegen/llvm*.[ch]; do
        [[ -e "$path" ]] || continue
        rel="${path#$ROOT_DIR/}"
        if [[ "$rel" == "src/codegen/llvm_inventory_host_methods.c" ||
              "$rel" == "src/codegen/llvm_inventory_host_methods.h" ||
              "$rel" == "src/codegen/llvm_domain_decl_parts_helpers.h" ]]; then
            continue
        fi
        llvm_method_files+=("$path")
    done
    if ((${#llvm_method_files[@]})); then
        grep -EHIn 'data\.(class_decl|enum_decl|relation_decl|effect_decl|zone_decl|world_decl|party_decl|roster_decl)\.methods\[[^]]+\]|data\.(class_decl|enum_decl|relation_decl|effect_decl|zone_decl|world_decl|party_decl|roster_decl)\.method_count' \
            "${llvm_method_files[@]}" | sed "s#^$ROOT_DIR/##" || true
    fi
)"
if [[ -n "$llvm_method_raw_hits" ]]; then
    fail "LLVM hosted-method emission must use LLVMHostedMethodView outside method-view owners:
$llvm_method_raw_hits"
fi

for forbidden in \
    "llvm_mir_decl_method_name(method_meta, method)" \
    "llvm_mir_decl_method_param_count(method_meta, method)" \
    "llvm_mir_decl_method_return_type(method_meta, method)" \
    "llvm_mir_decl_method_is_action_like(method_meta, method)"; do
    if grep -Fq "$forbidden" \
        "$ROOT_DIR/src/codegen/llvm_register.c" \
        "$ROOT_DIR/src/codegen/llvm_inventory_internal.h" \
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.h" \
        "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c" \
        "$ROOT_DIR/src/codegen/llvm_pipeline.c"; then
        fail "LLVM MIR method accessors must not fall back to AST method nodes: $forbidden"
    fi
done

require_term "src/codegen/llvm_inventory_host_methods.c" \
    "if (view->requires_mir_metadata)"

for term in \
    "MIRDeclMethod" \
    "method_metadata" \
    "method_metadata_count" \
    "mir_decl_header_set_methods" \
    "mir_link_decl_method_routines" \
    "params" \
    "param_count" \
    "return_type" \
    "has_routine" \
    "routine_index"; do
    if ! grep -Fq "$term" "$ROOT_DIR/src/compiler/mir.h" \
        && ! grep -Fq "$term" "$ROOT_DIR/src/compiler/mir_lower_public_api.h" \
        && ! grep -Fq "$term" "$ROOT_DIR/src/compiler/mir_public_surface.c" \
        && ! grep -Fq "$term" "$ROOT_DIR/src/compiler/mir_decl_headers.h" \
        && ! grep -Fq "$term" "$ROOT_DIR/src/compiler/mir_decl_headers.c"; then
        fail "MIR declaration method metadata missing term: $term"
    fi
done
require_term "src/compiler/mir_decl_headers.c" \
    "ast_type_name(ast_domain_slot_type(slot))"
for term in \
    "mir_validate_decl_header_ast_compat" \
    "mir_validate_decl_header_metadata" \
    "ast_role_impl_method_total_count" \
    "AST method-count compatibility drift" \
    "name metadata drift" \
    "duplicates declaration header" \
    "pointer-self ABI metadata drift" \
    "method metadata count" \
    "signature metadata drift" \
    "routine index" \
    "routine link metadata drift"; do
    require_term "src/compiler/mir_decl_header_validate.c" "$term"
done
if grep -Fq "header->ast_type != AST_ROLE_DECL" \
    "$ROOT_DIR/src/compiler/mir_decl_header_validate.c"; then
    fail "MIR declaration header validation must not keep role method-count exceptions"
fi
require_term "src/compiler/mir_program_validate.c" \
    "mir_validate_decl_header_metadata(mir, error_message)"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR validator rejects hosted method signature metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR validator rejects hosted method routine link metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR method routine linker requires owner metadata"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR validator rejects declaration header name metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR declaration headers preserve pointer-self ABI shape"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR validator rejects pointer-self ABI metadata drift"
require_term_any \
    "MIR validator rejects duplicate declaration header names" \
    "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "src/tests/mir/test_mir_lowering_part_d.cases.h"

if awk '/decl = decl_header->source_ast;/{exit} {print}' \
    "$ROOT_DIR/src/codegen/llvm_inventory_host_methods.c" |
    grep -Fq "method->data.func_decl.name != NULL"; then
    fail "LLVM host method lookup must compare MIRDeclMethod.name before AST func_decl name"
fi

for term in \
    "MIR Declaration Debt Removal" \
    "AST-carried declaration inventory" \
    "dedicated declaration metadata view" \
    "make mir-declaration-inventory-test-smoke"; do
    require_term "docs/100_beta_readiness_checklist.md" "$term"
done
for term in \
    "MIR/LLVM Declaration Bootstrap Proof Rows" \
    "Hosted method routine link" \
    "routine link metadata drift" \
    "Host field compatibility view" \
    "Dedicated declaration IR" \
    "Open beta blocker row"; do
    require_term "docs/125_source_of_truth_spine.md" "$term"
done

require_term "TODO.md" "declaration-side MIR-only debt"

domain_arrays=(
    functions intents abilities roles parties rosters worlds relations effects
    zones events types
)
allowed_raw_files=(
    "src/codegen/llvm_internal.h"
    "src/codegen/llvm_inventory_internal.c"
    "src/codegen/llvm_inventory_internal.h"
    "src/codegen/llvm_inventory_decl_lookup.c"
    "src/codegen/llvm_inventory_decl_lookup.h"
    "src/codegen/llvm_inventory_host_methods.c"
    "src/codegen/llvm_inventory_host_methods.h"
    "src/codegen/transpiler_inventory_view.c"
    "src/codegen/transpiler_inventory_view.h"
)
raw_hits=""
domain_array_pattern="$(
    IFS='|'
    printf '%s' "${domain_arrays[*]}"
)"
raw_scan_files=()
for path in "$ROOT_DIR"/src/codegen/*.[ch]; do
    [[ -e "$path" ]] || continue
    rel="${path#$ROOT_DIR/}"
    allowed=false
    for allowed_file in "${allowed_raw_files[@]}"; do
        if [[ "$rel" == "$allowed_file" ]]; then
            allowed=true
            break
        fi
    done
    [[ "$allowed" == true ]] && continue
    raw_scan_files+=("$path")
done
if ((${#raw_scan_files[@]})); then
    raw_hits="$(
        grep -EHIn "\\b(ctx->mir|mir)->($domain_array_pattern)\\b" \
            "${raw_scan_files[@]}" |
            sed "s#^$ROOT_DIR/##; s#:#: raw MIR declaration array access: #" ||
            true
    )"
fi
if [[ -n "$raw_hits" ]]; then
    fail "raw MIR declaration inventory array access outside allowed owner files:
$raw_hits"
fi

echo "[mir-decl-inventory] OK: C/LLVM declaration inventory use is helper-gated"

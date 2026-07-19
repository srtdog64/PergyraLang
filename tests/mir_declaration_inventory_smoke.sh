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

PGY_FILE_TEXT_VAR=""

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing required file: $rel"
}

load_file_text() {
    local rel="$1"
    local key
    local var

    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing required file: $rel"
    key="$rel"
    key="${key//\//__}"
    key="${key//./_}"
    key="${key//-/_}"
    var="PGY_FILE_TEXT_CACHE_${key}"
    if [[ -z "${!var+x}" ]]; then
        printf -v "$var" "%s" "$(<"$ROOT_DIR/$rel")"
    fi
    PGY_FILE_TEXT_VAR="$var"
}

require_term() {
    local rel="$1"
    local term="$2"

    if [[ "$rel" == "docs/100_beta_readiness_checklist.md" ]]; then
        pgy_beta_checklist_contains "$term" ||
            fail "$rel shards missing term: $term"
        return 0
    fi

    load_file_text "$rel"
    if [[ "${!PGY_FILE_TEXT_VAR}" == *"$term"* ]]; then
        return 0
    fi
    # MIR representation and declaration facts have dedicated type owners.
    # When the umbrella is requested, search those owners as one public surface.
    if [[ "$rel" == "src/compiler/mir.h" ]]; then
        load_file_text "src/compiler/mir_program.h"
        [[ "${!PGY_FILE_TEXT_VAR}" == *"$term"* ]] && return 0
        load_file_text "src/compiler/mir_types.h"
        [[ "${!PGY_FILE_TEXT_VAR}" == *"$term"* ]] && return 0
        load_file_text "src/compiler/mir_decl.h"
        [[ "${!PGY_FILE_TEXT_VAR}" == *"$term"* ]] && return 0
    fi
    fail "$rel missing term: $term"
}

reject_term() {
    local rel="$1"
    local term="$2"

    load_file_text "$rel"
    if [[ "${!PGY_FILE_TEXT_VAR}" == *"$term"* ]]; then
        fail "$rel must not contain retired term: $term"
    fi
}

require_term_any() {
    local term="$1"
    shift

    local rel
    for rel in "$@"; do
        if [[ -f "$ROOT_DIR/$rel" ]]; then
            load_file_text "$rel"
        else
            continue
        fi
        if [[ "${!PGY_FILE_TEXT_VAR}" == *"$term"* ]]; then
            return 0
        fi
    done
    fail "missing term in expected file set: $term"
}

require_each_following_term() {
    local rel="$1"
    local anchor="$2"
    local term="$3"
    local window="$4"
    local file="$ROOT_DIR/$rel"
    local lines
    local line

    [[ -f "$file" ]] || fail "missing required file: $rel"
    lines="$(grep -nF "$anchor" "$file" | cut -d: -f1 || true)"
    [[ -n "$lines" ]] || fail "$rel missing anchor: $anchor"
    while IFS= read -r line; do
        [[ -n "$line" ]] || continue
        sed -n "${line},$((line + window))p" "$file" | grep -Fq "$term" ||
            fail "$rel anchor '$anchor' at line $line is not followed by: $term"
    done <<< "$lines"
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
    "src/codegen/llvm_backend_type_render.c" \
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
    "src/codegen/transpiler_host_field_identifier.c" \
    "src/codegen/transpiler_host_field_identifier.h" \
    "src/codegen/transpiler_mir_ssa_names.h" \
    "src/codegen/transpiler_type_alias.c" \
    "src/compiler/mir.h" \
    "src/compiler/mir_lower_public_api.h" \
    "src/compiler/mir_program_inventory.c" \
    "src/compiler/mir_public_surface.c" \
    "src/compiler/mir_decl_method_projection.c" \
    "src/compiler/mir_decl_method_projection.h" \
    "src/compiler/mir_decl.h" \
    "src/compiler/mir_decl_header_access.c" \
    "src/compiler/mir_decl_header_generic_metadata.c" \
    "src/compiler/mir_decl_header_shape_validate.c" \
    "src/compiler/mir_decl_header_validate.c" \
    "src/compiler/mir_decl_headers.c" \
    "src/compiler/mir_decl_headers.h" \
    "src/compiler/mir_lifecycle.c" \
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
    "llvm_find_decl_header_in_context_of_type" \
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
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "llvm_generic_default_name_from_header"
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "mir_decl_header_generic_param_count(header)"
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "llvm_find_generic_default_name_in_mir_context"
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "llvm_find_decl_header_in_context_of_type("
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "llvm_active_decl_header_inventory(ctx, &headers)"
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "llvm_decl_header_inventory_get(&headers, i)"
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "pgy_host_decl_compat_types(&host_decl_type_count)"
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "llvm_active_has_mir(ctx)"
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "ctx->current_host_decl->type"
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "ast_declaration_generic_params(decl)"
require_term "src/compiler/mir_decl.h" \
    "char        *type_alias_target_type_name"
require_term "src/compiler/mir_decl.h" \
    "int          intent_retry_count"
require_term "src/compiler/mir_decl_headers.h" \
    "mir_decl_header_type_alias_target_type_name"
require_term "src/compiler/mir_decl_headers.h" \
    "mir_decl_header_intent_retry_count"
require_term "src/compiler/mir_decl_headers.h" \
    "mir_decl_header_inventory_resolve_type_alias_target_type_name"
require_term "src/compiler/mir_decl_headers.h" \
    "mir_decl_header_resolve_type_alias_target_type_name"
require_term "src/compiler/mir_decl_header_access.c" \
    "mir_decl_header_type_alias_target_type_name(const MIRDeclHeader *header)"
require_term "src/compiler/mir_decl_header_access.c" \
    "mir_decl_header_intent_retry_count(const MIRDeclHeader *header)"
require_term "src/compiler/mir_decl_header_access.c" \
    "mir_decl_header_inventory_resolve_type_alias_target_type_name("
require_term "src/compiler/mir_decl_header_access.c" \
    "mir_decl_header_resolve_type_alias_target_type_name(const MIRProgram *mir"
require_term "src/compiler/mir_decl_headers.c" \
    "header.type_alias_target_type_name ="
require_term "src/compiler/mir_decl_headers.c" \
    "header.intent_retry_count = ast_intent_decl_retry_count(decl)"
require_term "src/compiler/mir_decl_header_shape_validate.c" \
    "type-alias target metadata drift"
require_term "src/compiler/mir_decl_header_shape_validate.c" \
    "intent retry metadata drift"
require_term "src/compiler/mir_lifecycle.c" \
    "free(mir->decl_headers[i].type_alias_target_type_name)"
require_term "src/parser/ast_intent_constructors.c" \
    "node->data.intent_decl.retry_count = 0"
require_term "src/parser/ast_print_intent.c" \
    "IntentRetry: %d"
require_term "src/self_hosted/parser/decl_intent_owner.pgy" \
    "IntentRetry: "
require_term "src/semantic/type_checker_intent_decl.c" \
    "Intent retry(%d) is parsed and carried by MIR"
if grep -RIn "ast_intent_decl_retry_count" \
        "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h'; then
    fail "backend retry lowering must consume MIR declaration metadata, not reopen the AST retry accessor"
fi
require_term "src/codegen/llvm_backend_type_map.c" \
    "mir_decl_header_type_alias_target_type_name(alias_header)"
require_term "src/codegen/llvm_backend_type_map.c" \
    "llvm_active_has_mir(ctx)"
require_term "src/codegen/llvm_backend_type_map.c" \
    "MIR-only LLVM path missing type-alias target metadata"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_field_view_type_name("
require_term "src/codegen/llvm_inventory_field_view.c" \
    "llvm_hosted_field_view_type_name(const LLVMHostedFieldView *view"
require_term "src/codegen/llvm_backend_type_map.c" \
    "llvm_hosted_field_view_type_name(&fv, j)"
require_term "src/codegen/llvm_backend_type_map.c" \
    "generic_header != NULL && !fv.uses_mir_metadata"
require_each_following_term "src/codegen/llvm_backend_type_map.c" \
    "generic_header = llvm_find_decl_header_in_context_of_type(ctx," \
    "tmpl = llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, base)" \
    8
require_term "src/codegen/llvm_backend_type_render.c" \
    "llvm_render_alias_target_type_name_from_headers"
reject_term "src/codegen/llvm_backend_type_render.c" \
    "llvm_render_alias_target_type_name_scratch"
require_term "src/codegen/llvm_backend_type_render.c" \
    "mir_decl_header_type_alias_target_type_name(alias_header)"
require_term "src/codegen/llvm_backend_type_render.c" \
    "llvm_active_has_mir(ctx)"
require_term "src/codegen/llvm_backend_type_render.c" \
    "MIR-only LLVM type-name render missing type-alias target metadata"
require_term "src/codegen/transpiler_type_alias.c" \
    "transpiler_type_alias_target_type_name_from_headers("
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_type_alias_target_type_name_from_headers"
require_term "src/codegen/transpiler_decl_lookup.c" \
    "transpiler_type_alias_target_type_name_from_headers(TranspilerCtx *ctx"
require_term "src/codegen/transpiler_decl_lookup.c" \
    "transpiler_active_decl_header_inventory(ctx, &inventory)"
require_term "src/codegen/transpiler_decl_lookup.c" \
    "mir_decl_header_inventory_resolve_type_alias_target_type_name("
require_term "src/codegen/transpiler_inventory_view.h" \
    "transpiler_active_decl_header_inventory"
require_term "src/codegen/transpiler_inventory_view.c" \
    "mir_decl_header_inventory_from_program(ctx->mir, inventory)"
for rel in \
    "src/codegen/transpiler_specialization_type_name_scan.c" \
    "src/codegen/transpiler_specialization_registry.c" \
    "src/codegen/transpiler_channel_type_query.c" \
    "src/codegen/transpiler_expr_stdlib_builtin.c" \
    "src/codegen/transpiler_expr_stdlib_collection_support.c" \
    "src/codegen/transpiler_let_emit.c"; do
    require_term "$rel" \
        "transpiler_type_alias_target_type_name_from_headers("
done
require_term "src/compiler/mir_source_local_types.c" \
    "mir_decl_header_resolve_type_alias_target_type_name("
require_term "src/codegen/transpiler_type_alias.c" \
    "ensure_type_specializations_from_type_name_to("
require_term "src/codegen/transpiler_type_alias.c" \
    "transpiler_active_has_mir(ctx)"
if ! awk '
    /transpiler_emit_mir_source_local_let_def_inst\(/ { in_fn=1 }
    in_fn && /transpiler_mir_routine_source_local_type_name\(/ { source=NR }
    in_fn && /transpiler_render_effective_local_type_name\(ctx, let_type\)/ { rendered=NR }
    END { exit !(source > 0 && rendered > source) }
' "$ROOT_DIR/src/codegen/transpiler_mir_preserved_let_emit.c"; then
    fail "C MIR preserved let emit must consume source-local type facts before AST type rendering"
fi
require_term "src/codegen/transpiler_projection.c" \
    "ctx->generic_class_specs[i].specialized_name"
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_active_decl_header_of_type("
require_term "src/codegen/transpiler_host_self_policy.c" \
    "transpiler_generic_class_spec_base_decl("
require_term "src/codegen/transpiler_host_self_policy.c" \
    "mir_decl_header_uses_pointer_self(base_header)"
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "llvm_find_generic_default_name_in_mir_context"
require_term "src/codegen/llvm_backend_type_map_generics.c" \
    "llvm_find_generic_default_name_in_mir_inventory(ctx, type_name)"
if grep -Fq "llvm_generic_default_from_context_decl(" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_map_generics.c"; then
    fail "LLVM generic defaults must not route MIR headers through AST context-decl fallback"
fi
require_term "src/compiler/mir.c" \
    "mir_record_decl_header(mir, hir->functions[i])"
require_term "src/compiler/mir_decl_headers.c" \
    "case AST_FUNC_DECL"
require_term "src/compiler/mir.h" \
    "mir_find_decl_header_of_type"
require_term "src/compiler/mir_decl_header_access.c" \
    "mir_find_decl_header_of_type(const MIRProgram *mir"
require_term "src/codegen/llvm_inventory_decl_lookup.c" \
    "mir_find_decl_header_of_type(ctx->mir, decl_type, name)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "mir_find_decl_header_of_type(ctx->mir, decl_type, name)"
require_term "src/codegen/transpiler_decl_lookup.c" \
    "transpiler_active_decl_header_of_type(ctx, decl_type, name)"
if grep -Fq "llvm_active_inventory(ctx, AST_FUNC_DECL, &functions" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"; then
    fail "LLVM generic default MIR path must consume function declaration headers, not AST function inventory"
fi
if grep -Eq 'ast_(func|class|ability|role|party|roster)_generic_params\(decl\)' \
    "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"; then
    fail "LLVM generic default lookup must consume ast_declaration_generic_params"
fi
if grep -R -E "ast_(func|class|ability|role|party|roster)_generic_params[(]" \
    "$ROOT_DIR/src/codegen" >/dev/null 2>&1; then
    fail "codegen must consume declaration-level generic metadata"
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
    "MIR-only LLVM path missing intent routine for call target"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "llvm_collect_mir_intent_bindings"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "MIR-only LLVM path missing ordered intent binding metadata for call target"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "MIR-only LLVM path has incomplete ordered intent binding metadata for call target"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "MIR-only LLVM path has invalid ordered intent binding metadata for call target"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "!mir_only_intent && i < binding_count"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "MIR-only LLVM path missing registered function call target"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "MIR-only LLVM path missing user-call routine"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "MIR-only LLVM path missing user-call signature metadata"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "llvm_forward_declare_func_from_mir(callee_routine, decl, ctx)"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "llvm_active_function_routine_by_name(ctx,"
if grep -Fq "ast_func_body(decl)" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"; then
    fail "LLVM call dispatch must use MIR routine inventory, not AST body presence, for registered target fail-closed policy"
fi
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "llvm_active_has_mir(ctx)"
require_term "src/compiler/mir_intent.c" \
    "\"IntentValue\""
require_term "src/compiler/mir_intent_fact.c" \
    "\"IntentValue\""
require_term "src/compiler/mir_intent.c" \
    "\"IntentBinding\""
require_term "src/compiler/mir_intent_fact.c" \
    "\"IntentBinding\""
require_term "src/compiler/mir_intent.c" \
    "mir_append_intent_decl_contracts"
require_term "src/compiler/mir_intent.c" \
    "\"priority\""
require_term "src/compiler/mir_intent.c" \
    "\"success\""
require_term "src/codegen/llvm_intent.c" \
    "llvm_find_mir_intent_eval_expr("
require_term "src/codegen/llvm_intent.c" \
    "mir_routine, ctx, intent_name, \"priority\""
require_term "src/codegen/llvm_intent.c" \
    "llvm_find_mir_intent_check_expr("
require_term "src/codegen/llvm_intent.c" \
    "mir_routine, intent_name, \"success\""
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "transpiler_find_mir_intent_eval_expr("
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "mir_routine, intent_name, \"priority\""
require_term "src/codegen/transpiler_intent_emit.c" \
    "transpiler_find_mir_intent_check_expr("
require_term "src/codegen/transpiler_intent_emit.c" \
    "mir_routine, intent_name, \"success\""
require_term "src/codegen/transpiler_intent_emit.c" \
    "C intent '%s' cannot lower its success predicate; silent true fallback is disabled"
if grep -Fq 'success != NULL ? success : "true"' \
    "$ROOT_DIR/src/codegen/transpiler_intent_emit.c"; then
    fail "C intent success lowering must fail closed instead of silently falling back to true"
fi
require_term "src/codegen/llvm_intent_internal.h" \
    "llvm_collect_mir_intent_bindings"
require_term "src/codegen/intent_binding_metadata_view.h" \
    "typedef struct IntentBindingMetadataView"
require_term "src/codegen/intent_binding_metadata_view.h" \
    "bool owns_storage"
require_term "src/codegen/intent_binding_metadata_view.h" \
    "intent_binding_metadata_view_has_complete_row"
require_term "src/codegen/intent_binding_metadata_view.h" \
    "intent_binding_metadata_view_has_supported_row"
require_term "src/codegen/intent_binding_metadata_view.h" \
    "intent_binding_metadata_view_dispose"
require_term "src/codegen/intent_binding_metadata_view.h" \
    "intent_binding_metadata_view_kind_at"
require_term "src/codegen/intent_binding_metadata_view.h" \
    "intent_binding_metadata_view_alias_at"
require_term "src/codegen/intent_binding_metadata_view.h" \
    "intent_binding_metadata_view_type_at"
require_term "src/codegen/intent_binding_metadata_view.h" \
    "intent_binding_metadata_view_row_is_kind"
require_term "src/codegen/intent_binding_metadata_view.c" \
    "intent_binding_metadata_view_has_complete_row"
require_term "src/codegen/intent_binding_metadata_view.c" \
    "intent_binding_metadata_kind_is_supported"
require_term "src/codegen/intent_binding_metadata_view.c" \
    "intent_binding_metadata_view_dispose"
require_term "src/codegen/intent_binding_metadata_view.c" \
    "if (bindings->owns_storage)"
require_term "src/codegen/intent_binding_metadata_view.c" \
    "intent_binding_metadata_view_row_is_kind"
require_term "src/codegen/llvm_intent_internal.h" \
    "intent_binding_metadata_view.h"
require_term "src/codegen/transpiler_intent_context.h" \
    "intent_binding_metadata_view.h"
if grep -R -Fq "typedef struct LLVMIntentBindingMetadataView" \
        "$ROOT_DIR/src/codegen"; then
    fail "C and LLVM backends must share IntentBindingMetadataView instead of duplicating backend-local view structs"
fi
if grep -R -Fq "free((void *)binding_kinds)" "$ROOT_DIR/src/codegen" \
    || grep -R -Fq "free((void *)binding_aliases)" "$ROOT_DIR/src/codegen" \
    || grep -R -Fq "free((void *)binding_types)" "$ROOT_DIR/src/codegen"; then
    fail "Intent binding metadata storage must be released through IntentBindingMetadataView dispose"
fi
if grep -R -Fq "binding_metadata.kinds" "$ROOT_DIR/src/codegen" \
    || grep -R -Fq "binding_metadata.aliases" "$ROOT_DIR/src/codegen" \
    || grep -R -Fq "binding_metadata.types" "$ROOT_DIR/src/codegen"; then
    fail "Intent binding metadata rows must be read through IntentBindingMetadataView accessors"
fi
require_term "src/codegen/llvm_intent_mir_meta.c" \
    "llvm_collect_mir_intent_bindings"
require_term "src/codegen/llvm_intent_mir_meta.c" \
    "bindings_out->kinds"
require_term "src/codegen/llvm_intent_mir_meta.c" \
    "bindings_out->aliases"
require_term "src/codegen/llvm_intent_mir_meta.c" \
    "bindings_out->types"
require_term "src/codegen/llvm_intent_mir_meta.c" \
    "bindings_out->owns_storage = false"
if grep -Fq "const char ***kinds_out" \
        "$ROOT_DIR/src/codegen/llvm_intent_internal.h" \
    || grep -Fq "const char ***kinds_out" \
        "$ROOT_DIR/src/codegen/llvm_intent_mir_meta.c"; then
    fail "LLVM MIR intent binding collector must return one metadata view, not three parallel out-params"
fi
for rel in \
    "src/codegen/llvm_expr_call_dispatch.c" \
    "src/codegen/llvm_intent.c" \
    "src/codegen/llvm_intent_forward.c" \
    "src/codegen/llvm_mir_emit.c" \
    "src/codegen/llvm_mir_param_emit.c"; do
    require_term "$rel" "IntentBindingMetadataView binding_metadata"
    if grep -Fq "&binding_kinds, &binding_aliases" "$ROOT_DIR/$rel"; then
        fail "$rel must collect MIR intent bindings through IntentBindingMetadataView"
    fi
done
require_term "src/codegen/llvm_mir_emit.c" \
    "llvm_collect_mir_intent_bindings"
require_term "src/codegen/llvm_mir_param_emit.c" \
    "llvm_collect_mir_intent_bindings"
for file in \
    "src/codegen/llvm_mir_emit.c" \
    "src/codegen/llvm_mir_param_emit.c"; do
    if grep -Fq "ast_intent_decl_bindings" "$ROOT_DIR/$file" \
        || grep -Fq "ast_intent_decl_involves" "$ROOT_DIR/$file" \
        || grep -Fq "ast_intent_decl_values" "$ROOT_DIR/$file"; then
        fail "LLVM MIR intent param/type emission must consume ordered IntentBinding metadata instead of AST binding arrays: $file"
    fi
done
require_term "src/codegen/transpiler_mir_inventory_intent_collect.h" \
    "transpiler_collect_mir_intent_bindings"
require_term "src/codegen/transpiler_mir_inventory_intent_alias_collect.c" \
    "transpiler_collect_mir_intent_bindings"
require_term "src/codegen/transpiler_intent_participant.h" \
    "intent_type_name_is_subject_participant"
require_term "src/codegen/transpiler_intent_participant.h" \
    "intent_type_name_uses_pointer_self"
require_term "src/codegen/transpiler_intent_participant.c" \
    "intent_type_name_is_subject_participant"
require_term "src/codegen/transpiler_intent_participant.c" \
    "intent_type_name_uses_pointer_self"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "intent_type_name_uses_pointer_self"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "transpiler_require_type_name_c_type_copy(ctx"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "transpiler_can_forward_declare_type_name_early"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "transpiler_collect_mir_intent_bindings"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "MIR-backed C forward declaration has incomplete ordered intent binding metadata"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "MIR-backed C forward declaration has invalid ordered intent binding metadata"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "MIR-backed C forward declaration has missing value type metadata"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "mir_routine == NULL"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "subject_type = ast_intent_involves_subject_type(binding)"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "!mir_only_intent && value_type_name == NULL"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "intent_binding_metadata_view_type_at"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "transpiler_can_forward_declare_type_name_early("
if grep -Fq "transpiler_collect_mir_intent_values(" \
    "$ROOT_DIR/src/codegen/transpiler_intent_zone_binding_emit.c" \
    || grep -Fq "transpiler_collect_mir_intent_participants(" \
        "$ROOT_DIR/src/codegen/transpiler_intent_zone_binding_emit.c"; then
    fail "C intent forward/early eligibility must consume ordered binding rows, not separate participant/value collectors"
fi
if grep -Fq "participant_count + mir_value_count" \
    "$ROOT_DIR/src/codegen/transpiler_intent_zone_binding_emit.c"; then
    fail "C intent forward/early eligibility must not compare ordered binding rows against legacy participant/value counts"
fi
require_term "src/codegen/transpiler_intent_context.c" \
    "find_subject_action_metadata"
require_term "src/codegen/transpiler_intent_context.c" \
    "intent_action_metadata_has_only_self"
require_term "src/codegen/intent_binding_metadata_view.c" \
    "intent_binding_metadata_view_is_active"
require_term "src/codegen/intent_binding_metadata_view.c" \
    "intent_binding_metadata_view_has_supported_row(bindings, i)"
require_term "src/codegen/intent_binding_metadata_view.c" \
    "intent_binding_metadata_view_row_is_kind(bindings, i"
require_term "src/codegen/transpiler_intent_context.c" \
    "return NULL;"
require_term "src/codegen/transpiler_intent_context.c" \
    "return intent_zone_binding_type_name(intent, alias)"
require_term "src/codegen/transpiler_intent_zone_slot.c" \
    "has_binding_metadata"
require_term "src/codegen/transpiler_intent_zone_slot.c" \
    "MIR-only C path missing ordered intent binding metadata for zone-slot binding"
require_term "src/codegen/transpiler_intent_emit.c" \
    "IntentBindingMetadataView binding_metadata"
require_term "src/codegen/transpiler_intent_emit.c" \
    "resolve_intent_zone_slot_name_for_zone_with_bindings"
require_term "src/codegen/transpiler_intent_emit.c" \
    "find_subject_action_metadata"
require_term "src/codegen/transpiler_intent_emit.c" \
    "intent_action_metadata_has_only_self"
require_term "src/codegen/transpiler_func_forward_policy.c" \
    "transpiler_can_forward_declare_type_name_early"
for term in \
    "transpiler_find_mir_function(ctx, func)" \
    "transpiler_mir_routine_signature_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES" \
    "if (routine == NULL)" \
    "transpiler_mir_or_ast_function_is_generic(routine, func)" \
    "transpiler_mir_routine_return_type_name(routine)" \
    "transpiler_mir_routine_param_type_name(routine, i)" \
    "MIR-only C path missing function forward routine" \
    "MIR-only C path missing function forward signature metadata" \
    "transpiler_active_decl_header_of_type(" \
    "transpiler_can_forward_declare_type_name_after_zones"; do
    require_term "src/codegen/transpiler_func_forward_policy.c" "$term"
done
require_term "src/codegen/transpiler_decl_lookup.c" \
    "transpiler_active_decl_header_of_type("
if awk '
    /transpiler_has_known_nominal_type\(TranspilerCtx \*ctx/ { in_fn = 1 }
    in_fn && /transpiler_find_decl_in_inventory_local\(/ { saw_ast_lookup = 1 }
    in_fn && /transpiler_active_decl_header_of_type\(/ { saw_header = 1 }
    in_fn && /^}/ {
        if (saw_ast_lookup && !saw_header) bad = 1
        in_fn = 0
    }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_decl_lookup.c"; then
    fail "C nominal type existence checks must consume MIR declaration headers before AST declaration lookup"
fi
if ! awk '
    /transpiler_find_decl_in_inventory_local\(TranspilerCtx \*ctx/ { in_fn = 1 }
    in_fn && /if \(transpiler_active_has_mir\(ctx\)\)/ { saw_active = NR }
    in_fn && saw_active > 0 && NR == saw_active + 1 && /return NULL;/ { ok = 1 }
    in_fn && /^}/ { in_fn = 0 }
    END { exit ok ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_decl_lookup.c"; then
    fail "C inactive-inventory lookup must return NULL in MIR-active mode"
fi
for term in \
    "transpiler_mir_or_ast_function_is_generic" \
    "mir_routine_has_signature(routine)" \
    "transpiler_mir_routine_generic_param_count(routine)" \
    "ast_declaration_generic_params((ASTNode *)func_decl)"; do
    require_term "src/codegen/transpiler_mir_signature.c" "$term"
done
if grep -Fq "if (!transpiler_mir_routine_has_signature" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"; then
    fail "C forward policy must consume transpiler_mir_signature owner"
fi
if grep -Fq "ast_generic_param_count(ast_declaration_generic_params(func))" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"; then
    fail "C forward policy must consume MIR/AST generic owner"
fi
if grep -Fq "routine != NULL && transpiler_active_has_mir(ctx)" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"; then
    fail "C forward policy must use MIR signature facts whenever a routine exists"
fi
if grep -Fq "ast_func_generic_params(func)" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"; then
    fail "C forward policy must consume declaration-level generic metadata"
fi
if grep -Fq "p == NULL || p->type == NULL" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"; then
    fail "C forward policy must not require AST param->type before MIR routine param_type_name"
fi
for term in \
    "llvm_active_function_routine_by_name(ctx," \
    "if (routine == NULL)" \
    "llvm_mir_or_ast_function_is_generic(routine, func)" \
    "llvm_mir_routine_return_type_name(routine)" \
    "llvm_mir_routine_param_type_name(routine, i)" \
    "llvm_mir_routine_signature_metadata_complete(" \
    "MIR-only LLVM path missing function forward routine" \
    "MIR-only LLVM path missing function forward signature metadata" \
    "llvm_can_forward_declare_type_name_early"; do
    require_term "src/codegen/llvm_backend_forward_declare.c" "$term"
done
if grep -Fq "llvm_find_mir_function_for_forward_decl" \
    "$ROOT_DIR/src/codegen/llvm_backend_forward_declare.c"; then
    fail "LLVM forward policy reintroduced owner-local MIR routine lookup"
fi
if grep -Fq "routine != NULL && llvm_active_has_mir(ctx)" \
    "$ROOT_DIR/src/codegen/llvm_backend_forward_declare.c"; then
    fail "LLVM forward policy must use MIR signature facts whenever a routine exists"
fi
require_each_following_term "src/codegen/llvm_backend_forward_declare.c" \
    "llvm_mir_or_ast_function_is_generic(routine, func)" \
    "if (routine == NULL)" \
    4
if awk '
    /bool generic_func =/ { seen_generic_decl = NR }
    /ast_generic_param_count\(ast_declaration_generic_params\(node\)\)/ && seen_generic_decl && NR - seen_generic_decl <= 3 { bad = 1 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_decl.c"; then
    fail "LLVM function declaration must not read AST generic count before checking MIR routine availability"
fi
require_term "src/codegen/llvm_mir_signature.c" \
    "llvm_mir_or_ast_function_is_generic"
require_term "src/codegen/llvm_mir_signature.c" \
    "func_decl == NULL || func_decl->type != AST_FUNC_DECL"
require_term "src/codegen/llvm_mir_signature.c" \
    "ast_declaration_generic_params((ASTNode *)func_decl)"
require_term "src/codegen/llvm_decl_routines.c" \
    "llvm_mir_or_ast_function_is_generic(routine, NULL)"
require_term "src/codegen/llvm_inventory_internal.c" \
    "llvm_active_function_routine_by_name"
for term in \
    "llvm_mir_routine_signature_metadata_complete" \
    "llvm_mir_routine_signature_metadata_complete_for" \
    "LLVM_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME" \
    "LLVM_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES" \
    "LLVM_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES" \
    "llvm_mir_routine_has_signature(routine)" \
    "llvm_mir_routine_return_type_name(routine)" \
    "llvm_mir_routine_param_type_name(routine, i)"; do
    require_term "src/codegen/llvm_mir_signature.c" "$term"
done
if grep -Fq "routine == NULL || !llvm_active_has_mir(ctx)" \
        "$ROOT_DIR/src/codegen/llvm_mir_signature.c"; then
    fail "LLVM MIR signature owner must require metadata for any MIR routine, not only active-MIR builds"
fi
require_term "src/codegen/llvm_mir_signature.h" \
    "llvm_mir_routine_signature_metadata_complete"
require_term "src/codegen/llvm_mir_signature.h" \
    "llvm_mir_routine_signature_metadata_complete_for"
for term in \
    "llvm_mir_routine_signature_metadata_complete(ctx" \
    "MIR-only LLVM path missing user-call signature metadata"; do
    require_term "src/codegen/llvm_expr_call_dispatch.c" "$term"
done
for term in \
    "llvm_mir_routine_signature_metadata_complete_for(ctx" \
    "LLVM_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES" \
    "llvm_mir_or_ast_function_is_generic(routine, decl)" \
    "allow_ast_compat = routine == NULL" \
    "&& (decl_is_generic || decl_is_extern)" \
    "MIR-only LLVM path missing boundary call signature metadata"; do
    require_term "src/codegen/llvm_expr_boundary_projection_helpers.c" "$term"
done
if grep -Fq "routine_has_signature" \
        "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"; then
    fail "LLVM boundary call lowering must use explicit MIR signature facts, not routine_has_signature fallback naming"
fi
if grep -RIn "use_ast_signature" "$ROOT_DIR/src/codegen" >/dev/null 2>&1; then
    grep -RIn "use_ast_signature" "$ROOT_DIR/src/codegen" >&2 || true
    fail "backend AST compatibility gates must use the single allow_ast_compat name"
fi
for term in \
    "llvm_mir_routine_signature_metadata_complete_for(ctx" \
    "LLVM_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME" \
    "llvm_mir_or_ast_function_is_generic(routine, decl)" \
    "MIR-only LLVM path missing array return inference signature metadata"; do
    require_term "src/codegen/llvm_stmt_array_type_infer.c" "$term"
done
for term in \
    "llvm_mir_routine_signature_metadata_complete(ctx" \
    "llvm_mir_or_ast_function_is_generic(routine, decl)" \
    "MIR-only LLVM path missing callable let signature metadata" \
    "MIR-only LLVM path missing callable call-return signature metadata"; do
    require_term "src/codegen/llvm_stmt_let_callable.c" "$term"
done
for term in \
    "llvm_mir_routine_signature_metadata_complete_for(ctx" \
    "LLVM_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME" \
    "LLVM_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES" \
    "llvm_mir_or_ast_function_is_generic(routine, decl)" \
    "generic_func || extern_func"; do
    require_term "src/codegen/llvm_stmt_let_helpers.c" "$term"
done
for term in \
    "llvm_mir_routine_signature_metadata_complete_for(ctx" \
    "LLVM_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME" \
    "llvm_mir_or_ast_function_is_generic(routine, decl)" \
    "MIR-only LLVM path missing declared call return signature metadata"; do
    require_term "src/codegen/llvm_stmt_type_infer_helpers.c" "$term"
done
for term in \
    "llvm_mir_routine_signature_metadata_complete_for(ctx" \
    "LLVM_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES" \
    "llvm_mir_or_ast_function_is_generic(callee_routine, callee_decl)" \
    "allow_ast_compat = callee_decl != NULL" \
    "callee_is_generic_func" \
    "|| callee_is_extern_func" \
    "MIR-only LLVM path missing spawn signature metadata"; do
    require_term "src/codegen/llvm_expr_spawn_call_helpers.c" "$term"
done
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "llvm_mir_or_ast_function_is_generic("
require_term "src/codegen/llvm_mir_match_condition.c" \
    "llvm_mir_or_ast_function_is_generic(routine, decl)"
for term in \
    "llvm_mir_routine_signature_metadata_complete_for(ctx" \
    "LLVM_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME" \
    "MIR-only LLVM path missing match subject signature metadata"; do
    require_term "src/codegen/llvm_mir_match_condition.c" "$term"
done
for rel in \
    "src/codegen/llvm_expr_call_dispatch.c" \
    "src/codegen/llvm_expr_boundary_projection_helpers.c" \
    "src/codegen/llvm_stmt_array_type_infer.c" \
    "src/codegen/llvm_stmt_let_callable.c" \
    "src/codegen/llvm_stmt_let_helpers.c" \
    "src/codegen/llvm_stmt_type_infer_helpers.c" \
    "src/codegen/llvm_mir_match_condition.c"; do
    if grep -Fq "if (!llvm_mir_routine_has_signature" \
        "$ROOT_DIR/$rel"; then
        fail "$rel must consume llvm_mir_signature owner for signature metadata checks"
    fi
done
if grep -Fq "ast_func_generic_params(func)" \
    "$ROOT_DIR/src/codegen/llvm_backend_forward_declare.c"; then
    fail "LLVM forward policy must consume declaration-level generic metadata"
fi
if grep -Fq "param == NULL || param->type == NULL" \
    "$ROOT_DIR/src/codegen/llvm_backend_forward_declare.c"; then
    fail "LLVM forward policy must not require AST param->type before MIR routine param_type_name"
fi
require_term "src/codegen/transpiler_intent_emit.c" \
    "transpiler_collect_mir_intent_bindings"
require_term "src/codegen/transpiler_intent_emit.c" \
    "MIR-only C path has incomplete ordered intent binding metadata"
require_term "src/codegen/transpiler_intent_emit.c" \
    "mir_requires_routine = transpiler_active_has_mir(ctx) && decl_step_count > 0"
require_term "src/codegen/transpiler_intent_emit.c" \
    "mir_only_intent = mir_routine != NULL"
require_term "src/codegen/transpiler_intent_emit.c" \
    "IntentBindingMetadataView binding_metadata"
require_term "src/codegen/transpiler_intent_emit.c" \
    "&binding_metadata"
if grep -Fq "transpiler_collect_mir_intent_values(" \
    "$ROOT_DIR/src/codegen/transpiler_intent_emit.c"; then
    fail "C intent emitter prologue path must consume ordered binding metadata, not separate value metadata"
fi
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "transpiler_collect_mir_intent_bindings"
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "MIR-only C path missing ordered intent binding metadata for call target"
require_term "src/codegen/transpiler_intent_emit.c" \
    "MIR-only C path missing intent dispatch participant metadata"
require_term "src/codegen/transpiler_intent_emit.c" \
    "emit_intent_step_bind_bound_zone_with_metadata"
require_term "src/codegen/transpiler_intent_emit.c" \
    "emit_intent_step_rebind_bound_zone_aliases_with_metadata"
require_term "src/codegen/transpiler_intent_emit.c" \
    "emit_intent_step_sync_effective_zone_with_metadata"
require_term "src/codegen/transpiler_intent_emit.c" \
    "emit_intent_step_restore_bound_zone_aliases_with_metadata"
require_each_following_term "src/codegen/transpiler_intent_emit.c" \
    "emit_intent_step_bind_bound_zone_with_metadata(" \
    "ctx->backend_error" 8
require_each_following_term "src/codegen/transpiler_intent_emit.c" \
    "emit_intent_step_rebind_bound_zone_aliases_with_metadata(" \
    "ctx->backend_error" 7
require_each_following_term "src/codegen/transpiler_intent_emit.c" \
    "emit_intent_step_sync_effective_zone_with_metadata(" \
    "ctx->backend_error" 6
require_each_following_term "src/codegen/transpiler_intent_emit.c" \
    "emit_intent_step_restore_bound_zone_aliases_with_metadata(" \
    "ctx->backend_error" 7
require_each_following_term "src/codegen/transpiler_intent_cleanup_emit.c" \
    "emit_intent_step_bind_bound_zone_with_metadata(" \
    "ctx->backend_error" 6
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "transpiler_require_type_name_c_type_copy(ctx"
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "intent_type_name_is_subject_participant"
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "intent_type_name_uses_pointer_self"
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "const IntentBindingMetadataView *bindings_view"
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "MIR-backed C intent prologue has incomplete ordered binding metadata"
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "MIR-backed C intent prologue has invalid ordered binding metadata"
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "MIR-backed C intent prologue missing routine"
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "intent_binding_metadata_view_has_supported_row"
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "intent_binding_metadata_view_row_is_kind"
require_term "src/codegen/transpiler_intent_prologue_emit.c" \
    "mir_routine == NULL &&"
for term in \
    "const char **participant_aliases" \
    "const char **participant_types" \
    "const char **value_aliases" \
    "const char **value_types" \
    "const char **binding_kinds"; do
    if grep -Fq "$term" "$ROOT_DIR/src/codegen/transpiler_intent_prologue_emit.c"; then
        fail "C intent prologue must consume IntentBindingMetadataView instead of old parallel metadata parameter: $term"
    fi
done
require_term "src/codegen/transpiler_intent_prologue_emit.h" \
    "const IntentBindingMetadataView *bindings"
require_term "src/codegen/llvm_intent.c" \
    "llvm_collect_mir_intent_bindings"
require_term "src/codegen/llvm_intent.c" \
    "mir_requires_routine = llvm_active_has_mir(ctx) && decl_step_count > 0"
require_term "src/codegen/llvm_intent.c" \
    "mir_only_intent = mir_routine != NULL"
require_term "src/codegen/llvm_intent.c" \
    "MIR-only LLVM path has incomplete ordered intent binding metadata for entry setup"
require_term "src/codegen/llvm_intent.c" \
    "MIR-only LLVM path has invalid ordered intent binding metadata for entry setup"
require_term "src/codegen/llvm_intent.c" \
    "MIR-only LLVM path missing intent dispatch participant metadata"
require_term "src/codegen/llvm_intent_setup.c" \
    "pergyra_type_to_llvm(ctx, type_name)"
require_term "src/codegen/llvm_intent_setup.c" \
    "MIR-only LLVM path has incomplete ordered intent entry binding metadata"
require_term "src/codegen/llvm_intent_setup.c" \
    "MIR-only LLVM path has invalid ordered intent entry binding metadata"
require_term "src/codegen/llvm_intent_setup.c" \
    "LLVM intent entry binding %zu requires alias and type metadata; silent i8ptr fallback is not allowed"
require_term "src/codegen/llvm_intent_setup.c" \
    "const IntentBindingMetadataView *bindings_view"
require_term "src/codegen/llvm_intent.c" \
    "llvm_emit_intent_entry_bindings(ctx, node, fn, &binding_metadata"
if grep -A8 -F "void        llvm_emit_intent_entry_bindings" \
        "$ROOT_DIR/src/codegen/llvm_intent_internal.h" |
        grep -Fq "const char **binding_kinds"; then
    fail "LLVM intent entry setup must receive binding metadata as one view, not parallel parameters"
fi
if grep -Fq "LLVMTypeRef pt = ctx->type_i8ptr" \
    "$ROOT_DIR/src/codegen/llvm_intent_setup.c"; then
    fail "LLVM intent entry binding setup must not seed missing binding metadata with i8ptr"
fi
if grep -Fq 'alias != NULL ? alias : "param"' \
    "$ROOT_DIR/src/codegen/llvm_intent_setup.c"; then
    fail "LLVM intent entry binding setup must not synthesize parameter aliases"
fi
for rel in \
    "src/codegen/llvm_intent.c" \
    "src/codegen/llvm_intent_setup.c"; do
    if grep -Fq "llvm_collect_mir_intent_values(" "$ROOT_DIR/$rel" \
        || grep -Fq "llvm_collect_mir_intent_participants(" "$ROOT_DIR/$rel"; then
        fail "$rel must consume ordered binding rows, not separate participant/value collectors"
    fi
    if grep -Fq "participant_count + mir_value_count" "$ROOT_DIR/$rel"; then
        fail "$rel must not compare ordered binding rows against legacy participant/value counts"
    fi
done
require_term "src/codegen/llvm_intent_zone.c" \
    "MIR-only LLVM path missing intent participant metadata for zone-slot binding"
require_term "src/codegen/llvm_intent_zone.c" \
    "MIR-only LLVM path missing intent participant binding for zone rebind"
require_term "src/codegen/llvm_intent_zone.c" \
    "MIR-only LLVM path missing intent participant binding for zone restore"
require_term "src/codegen/llvm_intent_zone_bind.c" \
    "MIR-only LLVM path missing intent participant binding for zone transfer"
require_term "src/codegen/llvm_intent_zone_bind.c" \
    "MIR-only LLVM path missing intent participant binding for zone materialization"
require_each_following_term "src/codegen/llvm_intent.c" \
    "llvm_emit_intent_step_bind_bound_zone(" \
    "ctx->has_error" 6
require_each_following_term "src/codegen/llvm_intent.c" \
    "llvm_emit_intent_step_rebind_bound_zone_aliases(" \
    "ctx->has_error" 5
require_each_following_term "src/codegen/llvm_intent.c" \
    "llvm_emit_intent_step_sync_effective_zone(" \
    "ctx->has_error" 5
require_each_following_term "src/codegen/llvm_intent.c" \
    "llvm_emit_intent_step_restore_bound_zone_aliases(" \
    "ctx->has_error" 6
require_each_following_term "src/codegen/llvm_intent_cleanup.c" \
    "llvm_emit_intent_step_bind_bound_zone(" \
    "ctx->has_error" 6
require_term "src/codegen/llvm_intent_forward.c" \
    "llvm_collect_mir_intent_bindings"
require_term "src/codegen/llvm_intent_forward.c" \
    "MIR-only LLVM path has incomplete ordered intent binding metadata for forward declaration"
require_term "src/codegen/llvm_intent_forward.c" \
    "MIR-only LLVM path has invalid ordered intent binding metadata for forward declaration"
require_term "src/codegen/llvm_intent_forward.c" \
    "requires binding type metadata for parameter"
if grep -Fq "llvm_collect_mir_intent_values(" \
    "$ROOT_DIR/src/codegen/llvm_intent_forward.c" \
    || grep -Fq "llvm_collect_mir_intent_participants(" \
        "$ROOT_DIR/src/codegen/llvm_intent_forward.c"; then
    fail "LLVM intent forward declaration must consume ordered binding rows, not separate participant/value collectors"
fi
if grep -Fq "participant_count + mir_value_count" \
    "$ROOT_DIR/src/codegen/llvm_intent_forward.c"; then
    fail "LLVM intent forward declaration must not compare ordered binding rows against legacy participant/value counts"
fi
if grep -Fq "pt = ctx->type_i32;" \
    "$ROOT_DIR/src/codegen/llvm_intent_forward.c"; then
    fail "LLVM intent forward declaration must not hide missing binding type metadata with i32"
fi
require_term "src/codegen/llvm_mir_emit.c" \
    "llvm_collect_mir_intent_bindings"
require_term "src/codegen/llvm_mir_emit.c" \
    "pergyra_type_to_llvm(ctx, type_name)"
require_term "src/codegen/llvm_mir_emit.c" \
    "MIR-only LLVM path has incomplete ordered intent binding metadata"
require_term "src/codegen/llvm_mir_emit.c" \
    "MIR-only LLVM path has invalid ordered intent binding metadata"
require_term "src/codegen/llvm_mir_emit.c" \
    "MIR-only LLVM path missing intent participant type metadata"
require_term "src/codegen/llvm_mir_emit.c" \
    "MIR-only LLVM path missing intent parameter metadata"
require_term "src/codegen/llvm_mir_param_emit.c" \
    "llvm_collect_mir_intent_bindings"
require_term "src/codegen/llvm_mir_param_emit.c" \
    "pergyra_type_to_llvm(ctx, type_name)"
require_term "src/codegen/llvm_mir_param_emit.c" \
    "MIR-only LLVM path has incomplete ordered intent binding metadata"
require_term "src/codegen/llvm_mir_param_emit.c" \
    "MIR-only LLVM path missing intent parameter type metadata"
require_term "src/codegen/llvm_mir_param_emit.c" \
    "llvm_register_typed_var_abi_binding(ctx, alias, alloca"
for rel in \
    "src/codegen/llvm_mir_emit.c" \
    "src/codegen/llvm_mir_param_emit.c"; do
    if grep -Eq 'ast_intent_(involves_subject_type|involves_alias|value_type|value_alias)\(' "$ROOT_DIR/$rel"; then
        fail "$rel must not reopen intent binding AST alias/type in MIR-backed lowering"
    fi
    if grep -Fq "llvm_collect_mir_intent_values(" "$ROOT_DIR/$rel" \
        || grep -Fq "llvm_collect_mir_intent_participants(" "$ROOT_DIR/$rel"; then
        fail "$rel must consume ordered IntentBinding rows, not separate participant/value collectors"
    fi
    if grep -Fq "participant_count + mir_value_count" "$ROOT_DIR/$rel"; then
        fail "$rel must not compare ordered binding rows against legacy participant/value counts"
    fi
done
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "intent_param_type_name"
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "MIR-only C path missing intent routine for call target"
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "MIR-only C path has incomplete ordered intent binding metadata for call target"
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "MIR-only C path has invalid ordered intent binding metadata for call target"
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "C backend: MIR-backed intent call '%s' expects %zu argument(s), got %zu"
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "mir_requires_routine = transpiler_active_has_mir(ctx)"
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "mir_only_intent = intent_routine != NULL"
require_term "src/codegen/transpiler_expr_call_user_emit.c" \
    "if (mir_only_intent)"
if grep -Fq "transpiler_collect_mir_intent_values(" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c" \
    || grep -Fq "transpiler_collect_mir_intent_participants(" \
        "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"; then
    fail "C intent call target must consume ordered binding rows, not separate participant/value collectors"
fi
if grep -Fq "participant_count + value_meta_count" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"; then
    fail "C intent call target must not compare ordered binding rows against legacy participant/value counts"
fi
require_term "src/codegen/transpiler_call_subject_arg_policy.c" \
    "intent_param_type_name != NULL"
require_term "src/codegen/transpiler_call_subject_arg_policy.c" \
    "intent_type_name_uses_pointer_self(ctx, intent_param_type_name)"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "llvm_collect_mir_intent_bindings"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "MIR-only LLVM path missing ordered intent binding metadata for call target"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "MIR-only LLVM path has incomplete ordered intent binding metadata for call target"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "MIR-only LLVM path has invalid ordered intent binding metadata for call target"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "LLVM MIR-backed intent call '%s' expects %zu argument(s), got %zu"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "LLVM intent forward declaration requires binding type metadata; silent i8ptr fallback is not allowed"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "mir_requires_routine = llvm_active_has_mir(ctx) && intent_step_count > 0"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "mir_only_intent = intent_routine != NULL"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "pergyra_type_to_llvm(ctx, type_name)"
require_term "src/codegen/llvm_expr_call_intent_policy.h" \
    "llvm_non_mir_intent_call_binding_at"
require_term "src/codegen/llvm_expr_call_intent_policy.c" \
    "llvm_non_mir_intent_call_binding_at"
require_term "src/codegen/llvm_expr_call_dispatch.c" \
    "llvm_non_mir_intent_call_binding_at"
if grep -R -Fq "llvm_intent_call_binding_at(" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_intent_policy.h" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_intent_policy.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"; then
    fail "LLVM AST intent binding compatibility helper must be explicitly non-MIR named"
fi
if grep -Fq "LLVMTypeRef pt = ctx->type_i8ptr" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"; then
    fail "LLVM intent call-target forward declaration must not seed missing binding metadata with i8ptr"
fi
if grep -Fq "llvm_collect_mir_intent_values(" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c" \
    || grep -Fq "llvm_collect_mir_intent_participants(" \
        "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"; then
    fail "LLVM intent call target must consume ordered binding rows, not separate participant/value collectors"
fi
if grep -Fq "participant_count + mir_value_count" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"; then
    fail "LLVM intent call target must not compare ordered binding rows against legacy participant/value counts"
fi
require_term "src/codegen/llvm_intent_forward.c" \
    "mir_requires_routine = llvm_active_has_mir(ctx) && intent_step_count > 0"
require_term "src/codegen/llvm_intent_forward.c" \
    "mir_only_intent = mir_routine != NULL"
require_term "src/codegen/llvm_intent_forward.c" \
    "MIR-only LLVM path missing intent routine for forward declaration"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "mir_requires_routine = transpiler_active_has_mir(ctx) && intent_step_count > 0"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "mir_only_intent = mir_routine != NULL"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "MIR-only C path missing intent routine for forward declaration"
require_term "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "MIR-only C path missing intent routine for early forward eligibility"
require_term "src/codegen/llvm_mir_local_emit.c" \
    "MIR-only LLVM path missing local type metadata"
require_term "src/compiler/mir.h" \
    "callable_param_type_names"
require_term "src/compiler/mir_source_local_types.c" \
    "mir_source_local_type_append_callable"
require_term "src/compiler/mir_source_local_types.c" \
    "mir_source_local_callable_type_from_initializer"
require_term "src/compiler/mir_program_inventory.c" \
    "mir_routine_source_local_type_fact"
require_term "src/codegen/llvm_mir_local_emit.c" \
    "llvm_mir_local_source_fact("
require_term "src/codegen/llvm_mir_local_emit.c" \
    "llvm_mir_local_type_from_source_fact("
require_term "src/codegen/llvm_registry_aux.c" \
    "llvm_register_callable_signature_names"
require_term "src/codegen/llvm_expr_scalar_core.c" \
    "entry->param_type_names"
require_term "src/codegen/transpiler_type_declarator.c" \
    "pergyra_func_pointer_declarator_from_type_names_in_ctx"
require_term "src/codegen/transpiler_mir_func_ssa_locals_emit.c" \
    "source_local_fact->is_callable"
require_term "src/codegen/transpiler_mir_func_ssa_locals_emit.c" \
    "pergyra_func_pointer_declarator_from_type_names_in_ctx("
require_term "src/codegen/llvm_mir_local_emit.c" \
    "strcmp(base_name, name) != 0"
require_term "src/codegen/llvm_mir_local_emit.c" \
    "local_type = llvm_mir_local_type_from_source_fact(routine, ctx, name);"
if grep -Fq "llvm_mir_type_from_ast(ctx, type_expr)" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"; then
    fail "LLVM MIR source-local alloca typing must consume MIR source-local facts, not type_expr AST"
fi
if grep -Fq "llvm_register_typed_var_binding(ctx, owned_base" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"; then
    fail "LLVM MIR source-local scope binding must not re-register type_expr AST bindings"
fi
if ! awk '
    /llvm_mir_local_type_from_instruction_fact\(/ { in_fn = 1 }
    in_fn && /mir_instruction_uses_source_local_decl_emit\(inst\)/ { source = NR }
    in_fn && /llvm_mir_type_from_abi_layout\(ctx, inst->type_layout\)/ { layout = NR }
    in_fn && /^}/ {
        if (source > 0 && layout > 0 && source < layout) ok = 1
        in_fn = 0
    }
    END { exit ok ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"; then
    fail "LLVM MIR source-local instruction facts must require source-local type metadata before ABI layout fallback"
fi
require_term "src/codegen/llvm_mir_block_emit.c" \
    "llvm_mir_local_expected_type_name(routine, inst, NULL)"
if grep -Fq "ast_identifier_name(inst->expr1)" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"; then
    fail "LLVM MIR expected-type resolution must not reopen AST assignment target names"
fi
require_term "src/codegen/llvm_mir_block_emit.c" \
    "llvm_mir_local_expected_type_name(routine, inst"
require_each_following_term "src/codegen/llvm_mir_local_emit.c" \
    "inst->arg0 != NULL" \
    "llvm_mir_local_type_from_source_fact(" \
    6
require_term "src/codegen/llvm_mir_local_emit.c" \
    "routine, ctx, inst->result_name);"
require_term "src/codegen/llvm_mir_local_emit.c" \
    "llvm_mir_find_result_instruction"
require_term "src/codegen/llvm_mir_local_emit.c" \
    "inst->phi_incomings[i].value_name"
require_term "src/codegen/llvm_stmt_type_infer.c" \
    "channel receive requires registered Channel<T> metadata"
require_term "src/codegen/llvm_stmt_type_infer_call.c" \
    "call result requires registered function or expected type metadata"
require_term "src/codegen/llvm_stmt_type_infer_call.c" \
    "llvm_mir_slice_fact_array_type_from_slice_type(ctx, slice_ty)"
if awk '
    /strcmp\(callee, "SliceCopy"\)/ { in_slice = 1 }
    in_slice && /ctx->slice_type_/ { bad = 1 }
    in_slice && /SliceCopy requires concrete Slice<T> operand/ { in_slice = 0 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"; then
    fail "LLVM SliceCopy type inference must consume the slice fact owner instead of repeating slice->array ABI mapping"
fi
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
if ! awk '
    /llvm_stmt_infer_scalar_math_return_type\(/ { in_fn = 1 }
    in_fn && /active_mir && \(ty0 == NULL \|\| ty1 == NULL\)/ { guard = NR }
    in_fn && /return ctx->type_i32;/ { fallback = NR }
    in_fn && /^}/ {
        if (guard > 0 && fallback > 0 && guard < fallback) ok = 1
        in_fn = 0
    }
    END { exit ok ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"; then
    fail "LLVM MIR Min/Max type inference must fail closed before non-MIR i32 compatibility default"
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
    "transpiler_active_host_decl_header(ctx, host_name)"
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
    "llvm_hosted_shared_field_view_initializer(view, i)"
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_class_constructor_field_type_name_at("
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_hosted_class_field_view_from_decl("
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_mir_decl_field_type_name(field_meta)"
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "const char *expected_type"
require_term "src/compiler/mir_decl.h" \
    "MIRDeclFieldClaim"
require_term "src/compiler/mir_decl_header_fields.c" \
    "mir_decl_header_set_class_field_claims"
require_term "src/compiler/mir_decl_header_access.c" \
    "mir_decl_header_field_claim_count"
require_term "src/compiler/mir_decl_header_access.c" \
    "mir_decl_field_claim_inner_type_name"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "emit_one_field_slot_claim_meta"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "mir_decl_header_field_claim_count(header)"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_mir_decl_field_type_name(field)"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "ensure_type_specializations_from_type_name_to("
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_require_type_name_c_type_copy(ctx"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "MIR-only C path missing class field type-name metadata"
require_term "src/codegen/transpiler_class_constructor_emit.c" \
    "mir_decl_header_field_claim_count("
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_emit_field_slot_claims_from_header"
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "mir_decl_header_field_claim("
if grep -Fq "llvm_class_constructor_field_type_at" \
        "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    fail "LLVM class constructor field arguments must consume field type names, not AST field type nodes"
fi
if grep -Fq "ASTNode *field_type = llvm_class_constructor_field" \
        "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    fail "LLVM class constructor field argument lowering must not recover AST field type nodes"
fi
if grep -Eq 'llvm_hosted_shared_field_view_source_ast\(|ast_party_shared_initializer\(' \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    fail "LLVM constructor shared-field defaults must consume MIR-owned shared-field initializer metadata"
fi
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
    "emit_zone_projection_sync_loop_from_mir_refresh_view(ctx"
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
require_term "src/codegen/transpiler_zone_specialization_emit.c" \
    "transpiler_mir_decl_method_routine(ctx, method_meta)"
require_term "src/codegen/transpiler_zone_specialization_emit.c" \
    "ensure_collection_specializations_from_mir_routine_to(ctx, ctx->out"
if grep -Fq "transpiler_mir_decl_method_body_decl(ctx, method_meta)" \
    "$ROOT_DIR/src/codegen/transpiler_zone_specialization_emit.c"; then
    fail "C zone method specialization scan must consume linked MIRRoutine facts, not recover method source AST"
fi
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
require_term "src/codegen/llvm_channel_target.c" \
    "llvm_channel_current_host_field_type_name"
require_term "src/codegen/llvm_channel_target.c" \
    "llvm_hosted_class_field_view_from_decl("
require_term "src/codegen/llvm_channel_target.c" \
    "llvm_hosted_field_view_missing_mir_metadata("
require_term "src/codegen/llvm_channel_target.c" \
    "llvm_hosted_field_view_find_index("
require_term "src/codegen/llvm_channel_target.c" \
    "llvm_hosted_field_view_type_name("
require_term "src/codegen/llvm_channel_target.c" \
    "llvm_channel_field_inner_type_from_name"
require_term "src/codegen/llvm_channel_target.c" \
    "llvm_constructed_arg_name_copy(field_type_name, 0"
if grep -Fq "ast_generic_param_constraint(" \
    "$ROOT_DIR/src/codegen/llvm_channel_target.c"; then
    fail "LLVM channel current-host field target resolution must consume MIR field type-name metadata, not AST generic args"
fi
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_class_field_view_from_decl("
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_field_view_missing_mir_metadata("
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_field_view_find_index("
require_term "src/codegen/llvm_domain_lookup.c" \
    "llvm_hosted_field_view_type("
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
    "transpiler_hosted_shared_field_view_initializer("
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_hosted_shared_field_view_type("
if grep -Eq 'transpiler_hosted_shared_field_view_source_ast\(|ast_party_shared_initializer\(' \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"; then
    fail "C constructor shared-field defaults must consume MIR-owned shared-field initializer metadata"
fi
require_term "src/codegen/transpiler_projection.c" \
    "transpiler_domain_slot_view_is_projection_slot"
require_term "src/codegen/transpiler_domain_constructor_emit.c" \
    "transpiler_domain_slot_view_is_projection_slot_in_zone_refresh_view("
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
for term in \
    "transpiler_current_function_has_local_binding" \
    "transpiler_find_current_mir_routine" \
    "transpiler_mir_routine_has_param_name" \
    "mir_routine_param_count(routine)" \
    "transpiler_mir_routine_kind(routine) == MIR_SCOPE_METHOD" \
    "transpiler_mir_routine_owner_name(routine)" \
    "transpiler_mir_routine_has_source_local_binding("; do
    require_term "src/codegen/transpiler_mir_ssa_names.c" "$term"
done
require_term "src/codegen/transpiler_mir_local_binding.c" \
    "transpiler_mir_routine_source_local_type_name(routine, base_name)"
if grep -Fq "return transpiler_has_explicit_local_binding(ctx->current_func_decl" \
        "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"; then
    fail "C MIR SSA implicit-field local detection must split MIR parameter facts from AST body-local compatibility"
fi
if awk '
    /transpiler_current_function_has_local_binding\(TranspilerCtx \*ctx,/ { in_fn = 1 }
    in_fn && /transpiler_zone_shared_view_has_field\(TranspilerCtx \*ctx,/ { in_fn = 0 }
    in_fn && /transpiler_has_explicit_body_local_binding\(/ { bad = 1 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"; then
    fail "C MIR SSA implicit-field local detection must consume MIR source-local facts, not AST body-local scans"
fi
if grep -Fq "block->source_local_defs" \
        "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"; then
    fail "C MIR SSA implicit-field local detection must not treat source-local defs as lexical locals"
fi
if grep -Fq "ast_zone_layer_slots" \
    "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"; then
    fail "C MIR SSA implicit zone layer-field recovery must consume TranspilerHostedZoneLayerSlotView"
fi
if grep -Fq "pgy_host_shared_fields_compat_view_from_decl" \
    "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"; then
    fail "C MIR SSA implicit zone shared-field recovery must consume TranspilerHostedSharedFieldView"
fi
if [[ -e "$ROOT_DIR/src/codegen/llvm_domain_decl_parts_helpers.c" ||
      -e "$ROOT_DIR/src/codegen/llvm_domain_decl_parts_helpers.h" ]]; then
    fail "LLVM domain decl parts AST-array helper must stay retired"
fi
if [[ -e "$ROOT_DIR/src/codegen/llvm_domain_projection_target.c" ||
      -e "$ROOT_DIR/src/codegen/llvm_domain_projection_target_helpers.h" ]]; then
    fail "LLVM domain projection-target AST-array helper must stay retired"
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
    "transpiler_domain_slot_view_is_projection_slot_in_zone_refresh_view("
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
    "LLVMHostedZoneStateView state_view"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_zone_state_view_from_decl("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_zone_state_view_missing_mir_metadata("
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
if grep -Fq "ast_zone_states" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c"; then
    fail "LLVM zone struct type registration must consume LLVMHostedZoneStateView"
fi
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "LLVMHostedZoneLayerSlotView layer_view"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "LLVMHostedZoneStateView state_view"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_zone_layer_slot_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_zone_state_view_from_decl(ctx, decl_name, stmt)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_zone_state_view_missing_mir_metadata(&state_view)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_zone_layer_slot_view_name(&layer_view, j)"
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_hosted_zone_state_view_name(&state_view, j)"
if grep -Fq "ast_zone_layer_slots" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register_fields.c"; then
    fail "LLVM zone struct field registration must consume LLVMHostedZoneLayerSlotView"
fi
if grep -Fq "ast_zone_states" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register_fields.c"; then
    fail "LLVM zone struct field registration must consume LLVMHostedZoneStateView"
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
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "TranspilerHostedZoneStateView state_view"
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "transpiler_hosted_zone_state_view_from_decl("
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "transpiler_hosted_zone_state_view_missing_mir_metadata("
if grep -Fq "ast_zone_states(zone_decl, &state_count)" \
    "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.c"; then
    fail "C embedded world frontier member counting must consume MIR zone-state metadata, not reopen AST_ZONE_STATE inventory"
fi
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
    "LLVMHostedZoneStateView state_view"
require_term "src/codegen/llvm_domain_world_frontier.c" \
    "llvm_hosted_zone_state_view_from_decl("
require_term "src/codegen/llvm_domain_world_frontier.c" \
    "llvm_hosted_zone_state_view_rows_complete("
require_term "src/codegen/llvm_domain_world_frontier.c" \
    "pgy_domain_world_embedded_frontier_count_from_zone_types("
if grep -Fq "ast_zone_states(zone_decl, &state_count)" \
    "$ROOT_DIR/src/codegen/llvm_domain_world_frontier.c"; then
    fail "LLVM embedded world frontier member counting must consume MIR zone-state metadata, not reopen AST_ZONE_STATE inventory"
fi
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
    "llvm_domain_slot_view_is_projection_slot_in_zone_refresh_view("
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
    "zone_name = mir_decl_header_name(zone_header)"
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
    "llvm_domain_slot_view_is_projection_slot_in_zone_refresh_view" \
    "llvm_count_domain_projection_slots_in_zone_refresh_view"; do
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
require_term "src/compiler/mir_decl_header_fields.c" \
    "meta->is_binding_like = ast_domain_slot_is_binding(slot)"
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_count_domain_projection_slots_in_zone_refresh_view("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_hosted_zone_refresh_view_from_decl("
require_term "src/codegen/llvm_domain_struct_register.c" \
    "llvm_domain_slot_view_is_projection_slot_in_zone_refresh_view("
require_term "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_domain_add_projection_state_fields_from_zone_refresh_view("
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
    "transpiler_callable_decl_exists_local(ctx, fn_name)"
if grep -Fq "find_function_decl(ctx, fn_name)" \
        "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"; then
    fail "C role operator alias collision checks must consume callable existence lookup"
fi
if grep -Fq "find_callable_decl(ctx, fn_name)" \
        "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"; then
    fail "C role operator alias collision checks must not recover callable source declarations"
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
shared_field_source_accessor_hits="$(
    grep -RIn "hosted_shared_field_view_source_ast" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' || true
)"
if [[ -n "$shared_field_source_accessor_hits" ]]; then
    fail "shared-field source AST accessors must stay retired:
$shared_field_source_accessor_hits"
fi
for rel in \
    "src/codegen/llvm_inventory_zone_refresh_view.c" \
    "src/codegen/transpiler_decl_zone_refresh_view.c"; do
    require_term "$rel" "zone_refresh_view_mapped_source_field"
    require_term "$rel" "zone_refresh_view_mentions_source_field"
done
zone_refresh_compat_hits="$(
    grep -RIn "ast_compat_refreshes" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' || true
)"
if [[ -n "$zone_refresh_compat_hits" ]]; then
    fail "zone refresh compatibility arrays must stay retired from codegen:
$zone_refresh_compat_hits"
fi
zone_refresh_ast_hits="$(
    grep -RInE "ast_(relation|effect|zone)_refreshes\(|ast_zone_refresh_" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' || true
)"
if [[ -n "$zone_refresh_ast_hits" ]]; then
    fail "zone refresh AST inventory/accessors must stay retired from codegen:
$zone_refresh_ast_hits"
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
    "llvm_is_host_decl_type" \
    "pgy_host_decl_compat_is_type(decl_type)" \
    "pgy_host_decl_compat_types(&host_type_count)" \
    "host_types[i]" \
    "pgy_host_decl_compat_name(node)" \
    "return llvm_is_host_decl_type(decl->type)" \
    "llvm_decl_node_name(decl)"; do
    require_term "src/codegen/llvm_inventory_decl_lookup.c" "$term"
done
if grep -RIn "mir_find_decl_header(ctx->mir, name)" \
    "$ROOT_DIR/src/codegen"; then
    fail "codegen declaration-header lookup must be typed; do not reintroduce name-only ctx->mir lookup wrappers"
fi

for term in \
    "LLVMMIRDeclMethodRequirement" \
    "LLVM_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES" \
    "llvm_find_host_method_metadata_in_context" \
    "llvm_hosted_method_view" \
    "llvm_hosted_method_view_metadata" \
    "llvm_mir_decl_method_metadata_complete_for" \
    "llvm_mir_decl_method_name" \
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
if grep -RIn "llvm_hosted_method_view_source_ast" \
        "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h'; then
    fail "LLVM hosted method source accessor must stay retired; use MIRDeclMethod metadata first"
fi
if grep -RIn "llvm_mir_decl_method_source_ast" \
        "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h'; then
    fail "LLVM MIR method source alias must stay retired; use MIR source accessors directly at the compatibility boundary"
fi
if grep -RIn "mir_decl_method_source_ast(" \
        "$ROOT_DIR/src/codegen"/llvm_*.c \
        "$ROOT_DIR/src/codegen"/llvm_*.h; then
    fail "LLVM backend method body compatibility must stay retired; consume MIRDeclMethod metadata/routines"
fi
if grep -RIn "llvm_mir_decl_method_body_decl" \
        "$ROOT_DIR/src/codegen"/llvm_*.c \
        "$ROOT_DIR/src/codegen"/llvm_*.h; then
    fail "LLVM method body AST compatibility accessor must stay retired"
fi
require_term "src/codegen/llvm_inventory_host_methods.h" "ast_compat_count"
require_term "src/codegen/llvm_inventory_host_methods.c" \
    "view->count != view->ast_compat_count"
for term in \
    "mir_decl_header_nominal_kind_or" \
    "mir_decl_header_uses_pointer_self" \
    "mir_decl_method_name" \
    "mir_decl_method_param_count" \
    "mir_decl_method_param" \
    "mir_decl_method_return_type" \
    "mir_decl_method_is_action_like" \
    "mir_decl_method_routine_index"; do
    require_term "src/compiler/mir_decl_headers.h" "$term"
    require_term "src/compiler/mir_decl_header_access.c" "$term"
done
require_term "src/compiler/mir_decl.h" "NominalDeclKind nominal_kind"
require_term "src/compiler/mir_decl_headers.c" \
    "header.nominal_kind = ast_class_nominal_kind(decl)"
require_term "src/compiler/mir_decl_header_shape_validate.c" \
    "nominal pointer-self metadata drift"
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
    "mir_decl_field_owner_name" \
    "mir_decl_field_name" \
    "mir_decl_field_type" \
    "mir_decl_field_initializer" \
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
require_term "src/compiler/mir_decl_header_fields.c" \
    "meta->is_tobject_like = ast_domain_slot_is_tobject(slot)"
for term in \
    "MIR declaration header[%zu] '%s' has %zu hosted field(s) without MIRDeclField metadata" \
    "MIR declaration header[%zu] '%s' field metadata count %zu does not match declaration field count %zu" \
    "MIR declaration header[%zu] field[%zu] has owner metadata drift" \
    "MIR declaration header[%zu] field[%zu] has incomplete field metadata"; do
    require_term "src/compiler/mir_decl_header_validate.c" "$term"
done
if grep -RIn "mir_decl_method_source_ast\|mir_decl_field_source_ast" \
    "$ROOT_DIR/src/compiler" --include='*.c' --include='*.h'; then
    fail "MIR declaration method/field source AST accessors must stay retired"
fi
if grep -RIn "mir_decl_header_source_decl" \
    "$ROOT_DIR/src/compiler" "$ROOT_DIR/src/codegen" \
    --include='*.c' --include='*.h'; then
    fail "MIR declaration header source_decl accessor must stay retired"
fi
if grep -RIn "mir_decl_header_ast_shape" \
    "$ROOT_DIR/src/compiler" --include='*.c' --include='*.h'; then
    fail "MIR declaration header validation must not reopen source AST shape"
fi
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
    "transpiler_active_host_decl_header(ctx, host_name)" \
    "mir_decl_header_field_count(header)" \
    "mir_decl_header_field(header, i)" \
    "MIR_DECL_FIELD_SHARED" \
    "return transpiler_decl_header_shared_field(view->decl_header, index)" \
    "pgy_host_shared_fields_compat_view_from_decl(" \
    "mir_decl_field_name(field)" \
    "mir_decl_field_type(field)" \
    "mir_decl_field_initializer(field)"; do
    require_term "src/codegen/transpiler_decl_field_view.c" "$term"
done
for term in \
    "TranspilerHostedSharedFieldView" \
    "transpiler_hosted_shared_field_view_from_decl(" \
    "transpiler_hosted_shared_field_view_missing_mir_metadata(" \
    "transpiler_hosted_shared_field_view_metadata(" \
    "transpiler_hosted_shared_field_view_name(" \
    "transpiler_hosted_shared_field_view_initializer(" \
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
    "llvm_hosted_shared_field_view_name" \
    "llvm_hosted_shared_field_view_initializer" \
    "llvm_hosted_shared_field_view_type" \
    "pgy_host_shared_fields_compat_view_from_decl(decl)" \
    "MIR_DECL_FIELD_SHARED" \
    "return llvm_decl_header_shared_field(view->decl_header, index)" \
    "mir_decl_field_name(field)" \
    "mir_decl_field_initializer(field)"; do
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
if grep -RIn "mir_decl_method_source_ast(" \
        "$ROOT_DIR/src/codegen"/transpiler_*.c \
        "$ROOT_DIR/src/codegen"/transpiler_*.h; then
    fail "C backend method body compatibility must not expose method source AST"
fi
if grep -Fq "mir_decl_method_source_ast(" \
        "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"; then
    fail "C hosted method view must not expose method source AST provenance"
fi
if grep -RInE 'decl_header->source_ast|method->source_ast' \
        "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h'; then
    fail "backend source/provenance compatibility must use MIR source declaration accessors"
fi
require_term "src/codegen/host_decl_compat.c" \
    "ast_role_impl_method_total_count"
require_term "src/codegen/host_decl_compat.c" \
    "view.count = (size_t)-1"
require_term "src/codegen/host_decl_compat.c" \
    "case AST_ROLE_DECL"
require_term "src/codegen/llvm_inventory_host_methods.c" \
    "pgy_host_method_compat_view_from_decl(decl, llvm_active_has_mir(ctx))"
# MIR-only: the non-MIR LLVM host-method AST lookup fallback
# (llvm_find_host_method_decl_in_context) is retired -- it fails closed (returns
# NULL) and no longer reads compat methods, looks up active-inventory decls, or
# probes ctx->current_host_decl. The compat-method accessor is removed.
if grep -RInE 'method_view(\.|->)ast_compat_methods\[[^]]+\]' \
        "$ROOT_DIR/src/codegen"/llvm_*.c \
        "$ROOT_DIR/src/codegen"/llvm_*.h \
        | grep -v "src/codegen/llvm_inventory_host_methods.c"; then
    fail "LLVM consumers must not index hosted method compatibility arrays directly"
fi
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
for term in \
    "llvm_find_host_method_metadata_in_context(ctx, base" \
    "llvm_mir_decl_method_routine(ctx, method_meta)" \
    "specialized.owner_name = class_name" \
    "llvm_emit_func_from_mir(&specialized, ctx)"; do
    require_term "src/codegen/llvm_member_call_specialize.c" "$term"
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
    "llvm_mir_routine_kind" \
    "llvm_mir_routine_name" \
    "llvm_mir_routine_owner_name" \
    "llvm_mir_routine_owner_ast_type" \
    "llvm_mir_routine_has_signature" \
    "llvm_mir_routine_generic_param_count" \
    "llvm_mir_routine_param_count" \
    "llvm_mir_routine_param" \
    "llvm_mir_routine_param_type_name" \
    "llvm_mir_routine_return_type" \
    "llvm_mir_routine_return_type_name" \
    "llvm_mir_routine_within_zone" \
    "llvm_active_decl_header_inventory" \
    "llvm_decl_header_inventory_get" \
    "llvm_active_nominal_inventory" \
    "llvm_active_domain_inventory"; do
    require_term "src/codegen/llvm_inventory_internal.h" "$term"
done
for term in \
    "llvm_mir_routine_kind(const MIRRoutine *routine)" \
    "llvm_mir_routine_name(const MIRRoutine *routine)" \
    "llvm_mir_routine_owner_name(const MIRRoutine *routine)" \
    "llvm_mir_routine_owner_ast_type(const MIRRoutine *routine)" \
    "llvm_mir_routine_has_signature(const MIRRoutine *routine)" \
    "llvm_mir_routine_generic_param_count(const MIRRoutine *routine)" \
    "llvm_mir_routine_param_count(const MIRRoutine *routine)" \
    "llvm_mir_routine_param(const MIRRoutine *routine" \
    "llvm_mir_routine_param_type_name(const MIRRoutine *routine" \
    "llvm_mir_routine_return_type(const MIRRoutine *routine)" \
    "llvm_mir_routine_return_type_name(const MIRRoutine *routine)" \
    "llvm_mir_routine_within_zone(const MIRRoutine *routine)" \
    "llvm_active_decl_header_inventory(" \
    "llvm_decl_header_inventory_get(" \
    "return mir_routine_kind(routine)" \
    "return mir_routine_name(routine)" \
    "return mir_routine_owner_name(routine)" \
    "return mir_routine_owner_ast_type(routine)"; do
    require_term "src/codegen/llvm_inventory_internal.c" "$term"
done
if grep -RIn "llvm_mir_routine_source_ast(const MIRRoutine \*routine)" \
        "$ROOT_DIR/src/codegen/llvm_inventory_internal.c" \
        "$ROOT_DIR/src/codegen/llvm_inventory_internal.h"; then
    fail "LLVM routine source provenance must not be hidden behind a thin llvm_mir_routine_source_ast alias"
fi
if grep -RIn "llvm_mir_routine_source_ast_of_type" \
        "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h'; then
    fail "LLVM routine source provenance must use compiler-owned mir_routine_source_decl_of_type"
fi
for term in \
    "llvm_active_function_routine_by_name(const LLVMGenCtx *ctx," \
    "strcmp(routine_name, target) == 0" \
    "strncmp(routine_name, target, name_len) == 0"; do
    require_term "src/codegen/llvm_inventory_internal.c" "$term"
done
if grep -R -Fq "llvm_active_function_routine_for_source_ast" \
        "$ROOT_DIR/src/codegen"; then
    fail "LLVM function routine lookup must use MIR routine names, not source AST identity"
fi
if grep -Fq "ast_declaration_name((ASTNode *)func_decl)" \
        "$ROOT_DIR/src/codegen/llvm_inventory_internal.c"; then
    fail "LLVM function routine lookup must not derive the lookup key from a source AST node"
fi
if grep -Fq "llvm_mir_routine_source_ast(routine) == func_decl" \
        "$ROOT_DIR/src/codegen/llvm_inventory_internal.c"; then
    fail "LLVM function routine lookup must use MIR routine names, not source AST pointer identity"
fi
if ! awk '
    /llvm_active_function_routine_by_name/ { in_fn = 1 }
    in_fn && /strcmp\(routine_name, target\) == 0/ { exact = NR }
    in_fn && /strncmp\(routine_name, target, name_len\) == 0/ { prefix = NR }
    in_fn && /^}/ {
        if (exact > 0 && prefix > 0 && exact < prefix) ok = 1
        in_fn = 0
    }
    END { exit ok ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_inventory_internal.c"; then
    fail "LLVM function routine lookup must check exact routine names before prefix-specialization names"
fi
for term in \
    "mir_routine_kind(const MIRRoutine *routine)" \
    "mir_routine_name(const MIRRoutine *routine)" \
    "mir_routine_owner_name(const MIRRoutine *routine)" \
    "mir_routine_owner_ast_type(const MIRRoutine *routine)" \
    "mir_routine_source_decl(const MIRRoutine *routine)" \
    "mir_routine_source_decl_of_type(const MIRRoutine *routine" \
    "mir_routine_has_signature(const MIRRoutine *routine)" \
    "mir_routine_generic_param_count(const MIRRoutine *routine)" \
    "mir_routine_param_count(const MIRRoutine *routine)" \
    "mir_routine_param(const MIRRoutine *routine" \
    "mir_routine_param_type_name(const MIRRoutine *routine" \
    "mir_routine_return_type_name(const MIRRoutine *routine)" \
    "mir_routine_return_type(const MIRRoutine *routine)" \
    "mir_routine_within_zone(const MIRRoutine *routine)" \
    "mir_decl_header_inventory_from_program(" \
    "mir_decl_header_inventory_get("; do
    require_term "src/compiler/mir.h" "$term"
    require_term "src/compiler/mir_program_inventory.c" "$term"
done
for term in \
    "MIRDeclHeaderInventory" \
    "has_signature" \
    "size_t             generic_param_count" \
    "FuncParam        **params" \
    "char             **param_type_names" \
    "size_t             param_count" \
    "ASTNode           *return_type" \
    "char              *return_type_name"; do
    require_term "src/compiler/mir.h" "$term"
done
for term in \
    "MIRDeclGenericParam" \
    "char         *bound_type_name" \
    "char         *default_arg_type_name" \
    "size_t       generic_param_count" \
    "MIRDeclGenericParam *generic_metadata" \
    "size_t       generic_metadata_count"; do
    require_term "src/compiler/mir.h" "$term"
done
for term in \
    "mir_decl_header_set_generics" \
    "ast_declaration_generic_params(decl)" \
    "meta->bound_type_name = mir_capture_type_name(constraint, NULL)" \
    "meta->default_arg_type_name =" \
    "constraint != NULL && meta->bound_type_name == NULL" \
    "default_type != NULL" \
    "header->generic_metadata_count = count"; do
    require_term "src/compiler/mir_decl_header_generic_metadata.c" "$term"
done
require_term "src/compiler/mir_decl_headers.c" \
    "mir_decl_header_free_generics(&header)"
for term in \
    "mir_decl_header_generic_param_count" \
    "mir_decl_header_generic_param(" \
    "mir_decl_generic_param_name" \
    "mir_decl_generic_param_constraint_type_name" \
    "mir_decl_generic_param_default_type_name"; do
    require_term "src/compiler/mir_decl_headers.h" "$term"
    require_term "src/compiler/mir_decl_header_access.c" "$term"
done
for term in \
    "generic metadata count" \
    "generic[%zu] has incomplete metadata"; do
    require_term "src/compiler/mir_decl_header_validate.c" "$term"
done
if grep -RInE 'mir_decl_generic_param_(default_type|constraint)\(' \
    "$ROOT_DIR/src/compiler/mir_decl_headers.h" \
    "$ROOT_DIR/src/compiler/mir_decl_header_access.c" \
    "$ROOT_DIR/src/codegen" >/dev/null; then
    fail "MIR generic metadata must expose type-name facts, not AST generic default/constraint accessors"
fi
for term in \
    "llvm_generic_default_name_from_header" \
    "mir_decl_generic_param_default_type_name(param)" \
    "mir_decl_generic_param_constraint_type_name(param)" \
    "pergyra_type_to_llvm(ctx, default_type_name)"; do
    require_term "src/codegen/llvm_backend_type_map_generics.c" "$term"
done
for term in \
    "routine.has_signature = true" \
    "routine.generic_param_count = ast_generic_param_count" \
    "mir_record_decl_header(mir, hir->abilities[i])" \
    "ast_func_params(routine.ast, &routine.param_count)" \
    "routine.return_type = ast_func_return_type(routine.ast)" \
    "routine.within_zone = ast_func_within_zone(routine.ast)" \
    "mir_routine_signature_metadata_capture(mir, &routine)"; do
    require_term "src/compiler/mir.c" "$term"
done
require_term "src/compiler/mir_decl_headers.c" \
    "case AST_ABILITY_DECL"
require_term "src/compiler/mir_decl_headers.c" \
    "case AST_ABILITY_DECL"
for term in \
    "transpiler_mir_routine_kind" \
    "transpiler_mir_routine_name" \
    "transpiler_mir_routine_owner_name" \
    "transpiler_mir_routine_owner_ast_type" \
    "transpiler_mir_routine_has_signature" \
    "transpiler_mir_routine_generic_param_count" \
    "transpiler_mir_routine_param_count" \
    "transpiler_mir_routine_param" \
    "transpiler_mir_routine_param_type_name" \
    "transpiler_mir_routine_return_type" \
    "transpiler_mir_routine_return_type_name" \
    "transpiler_mir_routine_within_zone"; do
    require_term "src/codegen/transpiler_inventory_view.h" "$term"
    require_term "src/codegen/transpiler_inventory_view.c" "$term"
done
for term in \
    "transpiler_mir_type_name_supported" \
    "transpiler_mir_routine_signature_metadata_complete_for" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES" \
    "transpiler_mir_routine_signature_supported" \
    "MIR-only C path missing function signature eligibility metadata" \
    "mir_routine_return_type_name(routine)" \
    "mir_routine_param_type_name(routine, i)"; do
    require_term "src/codegen/transpiler_mir_signature.c" "$term"
done
if grep -Fq "routine == NULL || !transpiler_active_has_mir(ctx)" \
        "$ROOT_DIR/src/codegen/transpiler_mir_signature.c"; then
    fail "C MIR signature owner must require metadata for any MIR routine, not only active-MIR builds"
fi
require_term "src/codegen/transpiler_mir_signature.h" \
    "transpiler_mir_routine_signature_supported"
require_term "src/codegen/transpiler_mir_signature.h" \
    "transpiler_mir_routine_signature_metadata_complete_for"
require_term "src/codegen/transpiler_mir_emission_contract.c" \
    "transpiler_mir_routine_signature_supported((TranspilerCtx *)ctx"
for term in \
    "transpiler_mir_routine_signature_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES" \
    "transpiler_mir_or_ast_function_is_generic(callee_routine" \
    "param_type_name != NULL || param->type != NULL" \
    "MIR-only C path missing user-call signature metadata"; do
    require_term "src/codegen/transpiler_expr_call_user_emit.c" "$term"
done
for term in \
    "transpiler_mir_routine_signature_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES" \
    "transpiler_mir_or_ast_function_is_generic(callee_routine, decl)" \
    "if (param_type_name != NULL)" \
    "\"spawn wrapper MIR argument\"" \
    "allow_ast_compat = decl != NULL" \
    "callee_is_generic_func" \
    "|| callee_is_extern_func" \
    "MIR-only C path missing spawn signature metadata"; do
    require_term "src/codegen/transpiler_spawn_channel_emit.c" "$term"
done
require_term "src/codegen/llvm_expr_spawn_worker_boundary.c" \
    "if (param_type_name == NULL)"
if grep -Fq "ctx == NULL || param == NULL || param->type == NULL" \
        "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c"; then
    fail "LLVM spawn worker-boundary must consume MIR param type names before AST param type compatibility"
fi
if grep -Fq "transpiler_func_has_generic_params(decl)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c" \
    || grep -Fq "transpiler_func_has_generic_params(decl)" \
        "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"; then
    fail "C user/spawn call paths must consume MIR/AST generic owner"
fi
for term in \
    "transpiler_mir_routine_signature_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME" \
    "transpiler_mir_or_ast_function_is_generic(routine" \
    "MIR-only C path missing function inference signature metadata"; do
    require_term "src/codegen/transpiler_expr_call_type_infer.c" "$term"
done
for term in \
    "transpiler_mir_routine_signature_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME" \
    "transpiler_mir_or_ast_function_is_generic(routine, decl)" \
    "MIR-only C path missing spawn return signature metadata"; do
    require_term "src/codegen/transpiler_future_type_query.c" "$term"
done
for term in \
    "transpiler_mir_routine_signature_metadata_complete_for(ctx" \
    "transpiler_mir_or_ast_function_is_generic(routine, decl)" \
    "MIR-only C path missing callable let return signature metadata"; do
    require_term "src/codegen/transpiler_let_emit.c" "$term"
done
require_term "src/codegen/transpiler_nominal.c" \
    "transpiler_mir_or_ast_function_is_generic(routine"
for rel in \
    "src/codegen/transpiler_expr_call_type_infer.c" \
    "src/codegen/transpiler_future_type_query.c" \
    "src/codegen/transpiler_let_emit.c" \
    "src/codegen/transpiler_nominal.c"; do
    if grep -Fq "bool generic_call = transpiler_func_has_generic_params" \
        "$ROOT_DIR/$rel"; then
        fail "$rel must consume MIR/AST generic owner for call generic checks"
    fi
done
for rel in \
    "src/codegen/transpiler_expr_call_user_emit.c" \
    "src/codegen/transpiler_spawn_channel_emit.c" \
    "src/codegen/transpiler_expr_call_type_infer.c" \
    "src/codegen/transpiler_future_type_query.c" \
    "src/codegen/transpiler_let_emit.c"; do
    if grep -Fq "if (!transpiler_mir_routine_has_signature" \
        "$ROOT_DIR/$rel"; then
        fail "$rel must consume transpiler_mir_signature owner for signature metadata checks"
    fi
done
for term in \
    "transpiler_mir_routine_signature_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES" \
    "MIR-only C path missing function body routine" \
    "MIR-only C path missing function body signature metadata" \
    "transpiler_mir_routine_param_count(mir_routine)" \
    "transpiler_mir_routine_param(mir_routine" \
    "transpiler_mir_routine_param_type_name(mir_routine" \
    "transpiler_mir_routine_return_type(mir_routine)" \
    "transpiler_mir_routine_return_type_name(mir_routine)" \
    "transpiler_register_mir_source_local_bindings(ctx, mir_routine)"; do
    require_term "src/codegen/transpiler_mir_func_emit.c" "$term"
done
require_each_following_term "src/codegen/transpiler_mir_func_emit.c" \
    "if (transpiler_active_has_mir(ctx))" \
    "transpiler_register_mir_source_local_bindings(ctx, mir_routine)" \
    4
for term in \
    "MIR-only C path missing class-field slot registration metadata" \
    "transpiler_hosted_field_view_missing_mir_metadata(&fields_view)" \
    "transpiler_hosted_field_view_metadata(&fields_view, i)" \
    "transpiler_hosted_field_view_name(&fields_view, i)" \
    "transpiler_mir_decl_field_type_name(field_meta)" \
    "mir_decl_header_field_claim_count(header)" \
    "mir_decl_field_claim_token_name(claim)"; do
    require_term "src/codegen/transpiler_mir_self_field_slots.c" "$term"
done
require_term "src/codegen/transpiler_mir_func_emit.c" \
    "transpiler_mir_register_class_field_slots(ctx, resolved_host_decl)"
if grep -Fq "if (!transpiler_mir_routine_has_signature" \
    "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"; then
    fail "C MIR function body emission must consume transpiler_mir_signature owner"
fi
if grep -Fq "fields_view.ast_compat_fields" \
    "$ROOT_DIR/src/codegen/transpiler_mir_self_field_slots.c"; then
    fail "C class-field slot registration must consume hosted field metadata accessors, not ast_compat_fields directly"
fi
for term in \
    "llvm_hosted_field_view_missing_mir_metadata(&fields_view)" \
    "llvm_hosted_field_view_metadata(field_view, field_index)" \
    "llvm_mir_decl_field_type_name(field_meta)" \
    "llvm_render_type_name_in_ctx(ctx, field_type)" \
    "llvm_constructed_arg_name_copy(type_name, 0" \
    "mir_decl_header_field_claim_count(header)" \
    "mir_decl_field_claim_token_name(claim)" \
    "MIR-only LLVM path missing class-field slot registration metadata"; do
    require_term "src/codegen/llvm_mir_param_emit.c" "$term"
done
if grep -Fq "fields_view.ast_compat_fields" \
    "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"; then
    fail "LLVM class-field slot registration must consume hosted field metadata accessors, not ast_compat_fields directly"
fi
if grep -RInE '[A-Za-z_]*view(\.|->)ast_compat_fields\[[^]]+\]' \
        "$ROOT_DIR/src/codegen"/llvm_*.c \
        "$ROOT_DIR/src/codegen"/llvm_*.h \
        | grep -v "src/codegen/llvm_inventory_field_view.c"; then
    fail "LLVM consumers must not index hosted field compatibility arrays directly"
fi
if grep -RIn "ast_compat_fields" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h'; then
    fail "hosted field views must not expose AST compatibility field arrays; field shape is owned by MIR declaration metadata"
fi
# MIR-only closure lock (docs/125 'Dedicated declaration IR'): the non-MIR
# AST-compat declaration fallbacks have been retired across all three families
# (field/method/slot). ast_compat_decl / ast_compat_methods / ast_compat_slots
# must not reappear -- declaration field/method/slot shape is owned solely by MIR
# metadata, and accessors fail closed when MIR metadata is absent instead of
# reading the AST.
if grep -RInE "ast_compat_decl|ast_compat_methods|ast_compat_slots" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h'; then
    fail "ast_compat_{decl,methods,slots} are retired; hosted declaration views must not reopen an AST compatibility fallback (MIR metadata is the single owner)"
fi
# Non-MIR codegen path lock: the entire AST-based (non-MIR) codegen fallback has
# been retired. A new `!*_active_has_mir(ctx)` branch is how a non-MIR path would
# be reintroduced -- forbid it everywhere except the two intentional fail-closed
# MIR-enforcement guards (emit-program entry + role-method routine guard), which
# *refuse* non-MIR rather than generate from AST. Positive `active_has_mir(ctx)`
# checks (MIR-path gating) are unaffected. This is the structural backstop the
# manual 45-site sweep needed: a nullable ctx->mir lets these branches hide
# anywhere, so a grep gate -- not human review -- keeps the retirement permanent.
non_mir_branch_hits="$(
    grep -RInE '!(transpiler|llvm)_active_has_mir\(ctx\)' \
        "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h' |
        grep -vE 'src/codegen/(transpiler\.c|transpiler_domain_role_methods_emit\.c):' || true
)"
if [[ -n "$non_mir_branch_hits" ]]; then
    fail "non-MIR codegen path is retired; '!*_active_has_mir(ctx)' must not introduce an AST fallback (only the allowlisted fail-closed MIR-enforcement guards may negate it):
$non_mir_branch_hits"
fi
for rel in \
    "src/codegen/transpiler_mir_func_emit.c" \
    "src/codegen/transpiler_mir_block_emit.c" \
    "src/codegen/transpiler_mir_emission_mapping_contract.c"; do
    if grep -Eq 'routine_has_signature|ast_func_param_count\((node|func_decl)\)|ast_func_param\((node|func_decl)' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must seed and emit MIR parameters from MIR routine signature facts"
    fi
done
if grep -Fq "ast_func_return_type(node)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"; then
    fail "C MIR function body emission must render return type from MIR routine signature facts"
fi
if grep -R -n -F "transpiler_register_explicit_local_bindings_in_block" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' >/dev/null; then
    fail "C MIR local binding compatibility shim must keep an explicit AST-compat name"
fi
for term in \
    "transpiler_find_mir_function(ctx, node)" \
    "transpiler_mir_routine_signature_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES" \
    "allow_ast_compat = mir_routine == NULL" \
    "&& (generic_func || extern_func)" \
    "MIR-only C path missing function forward routine" \
    "MIR-only C path missing function forward signature metadata" \
    "transpiler_mir_routine_param_count(mir_routine)" \
    "transpiler_mir_routine_param(mir_routine" \
    "transpiler_mir_routine_param_type_name(mir_routine" \
    "transpiler_mir_routine_return_type(mir_routine)" \
    "return_type_name != NULL" \
    "ensure_type_specializations_from_type_name_to("; do
    require_term "src/codegen/transpiler_func_forward_emit.c" "$term"
done
if grep -Fq "if (!transpiler_mir_routine_has_signature" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_emit.c"; then
    fail "C function forward emission must consume transpiler_mir_signature owner"
fi
if grep -Fq "mir_routine != NULL && transpiler_active_has_mir(ctx)" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_emit.c"; then
    fail "C function forward emission must use MIR signature facts whenever a routine exists"
fi
for term in \
    "transpiler_mir_routine_signature_metadata_complete_for(ctx" \
    "MIR-only C path missing function SSA local routine" \
    "MIR-only C path missing function SSA local signature metadata" \
    "transpiler_mir_ssa_local_routine_has_param_name" \
    "transpiler_mir_ssa_local_routine_has_source_def" \
    "transpiler_mir_ssa_local_routine_has_destructure_binding" \
    "transpiler_mir_ssa_local_register_base_type_fact" \
    "has_param_fact" \
    "has_source_local_fact"; do
    require_term "src/codegen/transpiler_mir_func_ssa_locals_emit.c" "$term"
done
for term in \
    "transpiler_mir_routine_param_count(routine)" \
    "transpiler_mir_routine_param(routine" \
    "transpiler_mir_destructure_binding_type_name" \
    "mir_instruction_destructure_binding_index(inst" \
    "inst->kind != MIR_INST_DESTRUCTURE" \
    "transpiler_mir_ssa_local_entry_has_source_def" \
    "transpiler_mir_ssa_local_routine_has_source_def" \
    "transpiler_mir_ssa_local_routine_has_param_name" \
    "transpiler_mir_register_base_local_view_fact" \
    "transpiler_mir_view_constructor_call_from_source" \
    "transpiler_mir_ssa_local_register_base_type_fact" \
    "register_view_like_var(ctx, base_name, type_name, source," \
    "register_slot_var(ctx, base_name, inner_buf, is_secure, false)" \
    "block->source_local_defs[i]"; do
    require_term "src/codegen/transpiler_mir_ssa_local_facts.c" "$term"
done
ssa_local_payload_reads="$({ grep -F "mir_instruction_source_payload(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_ssa_local_facts.c" || true; } | wc -l | tr -d ' ')"
if [ "$ssa_local_payload_reads" != "0" ]; then
    fail "C MIR SSA local facts must consume MIR destructure binding facts, not source payload (expected 0, got $ssa_local_payload_reads)"
fi
if grep -Fq "transpiler_find_local_event_handler_type_ast(" \
        "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"; then
    fail "C MIR SSA local declarations must consume MIR callable source-local facts, not EventHandler AST fallback"
fi
if grep -Fq "transpiler_find_local_event_handler_type_ast(" \
        "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"; then
    fail "C parallel capture declarations must consume MIR callable source-local facts, not EventHandler AST fallback"
fi
require_term "src/codegen/transpiler_mir_func_ssa_locals_emit.c" \
    "source_local_fact != NULL && source_local_fact->is_callable"
require_term "src/codegen/transpiler_mir_func_ssa_locals_emit.c" \
    "pergyra_func_pointer_declarator_from_type_names_in_ctx"
require_term "src/codegen/transpiler_parallel_capture.c" \
    "transpiler_current_local_callable_capture"
require_term "src/codegen/transpiler_parallel_capture.c" \
    "mir_routine_source_local_type_fact(routine, name)"
require_term "src/codegen/transpiler_async_parallel_emit.c" \
    "TranspilerParallelCallableCapture capture_typed_callables"
require_term "src/codegen/transpiler_async_parallel_emit.c" \
    "pergyra_func_pointer_declarator_from_type_names_in_ctx"
if grep -Fq "if (!transpiler_mir_routine_has_signature" \
        "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"; then
    fail "C MIR SSA local declarations must use transpiler_mir_signature for missing-signature diagnostics"
fi
for term in \
    "transpiler_mir_register_with_slot_claim_fact" \
    "transpiler_register_mir_with_slot_claim_facts" \
    "inst->abi_type_name" \
    "mir_instruction_is_with_slot_claim(inst)" \
    "lookup_slot_type_copy(ctx, alias, inner_buf, sizeof(inner_buf))" \
    "register_typed_var(ctx, alias, type_name)" \
    "register_slot_var(ctx, alias, inner_buf, is_secure, false)"; do
    require_term "src/codegen/transpiler_mir_resource_op_emit.c" "$term"
done
require_term "src/codegen/transpiler_mir_func_emit.c" \
    "transpiler_register_mir_with_slot_claim_facts(ctx, mir_routine)"
if grep -Eq 'ast_func_body\(node\)|ast_block_statement_count\(body\)' \
        "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"; then
    fail "C MIR SSA local declarations must consume MIR source-local facts instead of rescanning function body AST"
fi
if grep -Eq 'ast_func_param_count\(node\)|ast_func_param\(node' \
        "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"; then
    fail "C MIR SSA local declarations must consume MIR routine parameter facts instead of function AST params"
fi
require_term "src/codegen/transpiler_mir_func_ssa_locals_emit.c" \
    "transpiler_mir_routine_signature_metadata_complete_for(ctx"
for term in \
    "transpiler_find_mir_function(ctx, callee_decl)" \
    "transpiler_mir_or_ast_function_is_generic(callee_routine" \
    "transpiler_mir_routine_signature_metadata_complete_for(" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME" \
    "MIR-only C path missing local parameter signature metadata" \
    "MIR-only C path missing local parameter type-name metadata" \
    "transpiler_mir_routine_param_type_name(routine, i)" \
    "transpiler_mir_routine_return_type_name(callee_routine)" \
    "MIR-only C path missing function call routine metadata" \
    "MIR-only C path missing function call return signature metadata" \
    "MIR-only C path missing function call return type-name metadata" \
    "if (callee_decl != NULL && callee_decl->type == AST_FUNC_DECL" \
    "&& transpiler_active_has_mir(ctx)" \
    "ast_func_return_type(callee_decl)"; do
    require_term "src/codegen/transpiler_mir_local_type_lookup.c" "$term"
done
if grep -Fq "transpiler_func_has_generic_params(callee_decl)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"; then
    fail "C MIR local type lookup must consume MIR/AST generic owner for function-call generic checks"
fi
if ! awk '
    /transpiler_find_mir_function\(const TranspilerCtx \*ctx,/ { in_fn = 1 }
    in_fn && /strcmp\(routine_name, target\) == 0/ { exact = NR }
    in_fn && /strncmp\(routine_name, target, name_len\) == 0/ { prefix = NR }
    in_fn && /^}/ {
        if (exact > 0 && prefix > 0 && exact < prefix) ok = 1
        in_fn = 0
    }
    END { exit ok ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent_collect.c"; then
    fail "C function routine lookup must check exact routine names before prefix-specialization names"
fi
require_term "src/codegen/transpiler_mir_local_type_lookup.c" \
    "transpiler_mir_routine_source_local_type_name("
require_term "src/codegen/transpiler_mir_preserved_let_emit.c" \
    "transpiler_mir_routine_source_local_type_name("
require_term "src/codegen/llvm_mir_local_emit.c" \
    "mir_routine_source_local_type_name(routine,"
require_term "src/codegen/llvm_stmt_let_collections.c" \
    "mir_routine_source_local_type_name(ctx->current_mir_routine, name)"
require_term "src/codegen/llvm_stmt_source_local_fallback.c" \
    "const MIRRoutine *routine = ctx->current_mir_routine"
require_term "src/codegen/llvm_stmt_source_local_fallback.c" \
    "routine = llvm_active_function_routine_by_name("
require_term "src/codegen/llvm_stmt_source_local_fallback.c" \
    "routine == NULL && llvm_active_has_mir(ctx)"
require_term "src/codegen/llvm_stmt_source_local_fallback.c" \
    "mir_routine_source_local_type_name(routine,"
require_term "src/codegen/llvm_stmt_source_local_fallback.c" \
    "llvm_stmt_source_local_type(LLVMGenCtx *ctx, const char *name)"
require_term "src/codegen/llvm_stmt_type_infer.c" \
    "llvm_stmt_source_local_type(ctx, name)"
require_term "src/codegen/llvm_stmt_source_local_fallback.c" \
    "llvm_stmt_non_mir_source_local_let_init(LLVMGenCtx *ctx, const char *name)"
if ! grep -A8 -F "llvm_stmt_non_mir_source_local_let_init(LLVMGenCtx *ctx, const char *name)" \
        "$ROOT_DIR/src/codegen/llvm_stmt_source_local_fallback.c" |
        grep -Fq "llvm_active_has_mir(ctx)"; then
    fail "LLVM initializer source-local recovery must be non-MIR only"
fi
if ! awk '
    /case AST_IDENTIFIER:/ { in_case = 1 }
    in_case && /llvm_stmt_source_local_type\(ctx, name\)/ { source = NR }
    in_case && /llvm_stmt_non_mir_source_local_let_init\(ctx, name\)/ { recovery = NR }
    in_case && /case AST_ASSIGNMENT:/ {
        if (source > 0 && recovery > source)
            ok = 1
        in_case = 0
    }
    END { exit ok ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"; then
    fail "LLVM identifier type inference must consult MIR source-local type facts before non-MIR initializer recovery"
fi
if grep -RFn "llvm_stmt_source_local_let_init" "$ROOT_DIR/src/codegen" >/dev/null; then
    fail "LLVM source-local initializer recovery reintroduced the old MIR-ambiguous name"
fi
require_term "src/codegen/llvm_stmt_source_local_fallback.c" \
    "legacy non-MIR callers fall back"
require_term "src/codegen/llvm_mir_emit.c" \
    "llvm_mir_preregister_source_local_classes(ctx, routine)"
require_term "src/codegen/llvm_mir_emit.c" \
    "routine->source_local_type_count"
if grep -Fq "llvm_mir_preregister_let_var_classes" \
        "$ROOT_DIR/src/codegen/llvm_mir_emit.c"; then
    fail "LLVM MIR eager var-class registration must consume MIR source-local facts, not AST let walks"
fi
if grep -B3 -F "llvm_mir_preregister_source_local_classes(ctx, routine)" \
        "$ROOT_DIR/src/codegen/llvm_mir_emit.c" |
        grep -Fq "ast_func_body"; then
    fail "LLVM MIR eager var-class registration reintroduced function-body AST walking"
fi
if grep -A18 -F "mir_routine_source_local_type_name(const MIRRoutine *routine," \
        "$ROOT_DIR/src/compiler/mir_program_inventory.c" |
        grep -Fq "mir_routine_source_local_walk"; then
    fail "MIR routine source-local type lookup must be fact-only, not AST fallback"
fi
if grep -Fq "mir_source_local_type_name_in_ast(ASTNode *body" \
        "$ROOT_DIR/src/compiler/mir_program_inventory.c"; then
    fail "MIR program inventory must not own source-local AST compatibility lookup"
fi
require_term "src/compiler/mir.h" "MIRSourceLocalType"
require_term "src/compiler/mir.h" "source_local_types"
require_term "src/compiler/mir.h" \
    "mir_routine_source_local_type_count"
require_term "src/compiler/mir_program_inventory.c" \
    "mir_routine_source_local_type_name_at"
require_term "src/codegen/transpiler_inventory_view.c" \
    "transpiler_mir_routine_source_local_type_name_at"
for term in \
    "ensure_type_specializations_from_type_name_to" \
    "transpiler_ensure_generic_class_specialization_from_type_name(" \
    "transpiler_mir_routine_return_type_name(routine)" \
    "transpiler_mir_routine_param_type_name(routine, i)" \
    "transpiler_mir_routine_source_local_type_name_at(routine, i)" \
    "ensure_collection_specializations_from_mir_routine_to"; do
    require_term "src/codegen/transpiler_specialization_type_name_scan.c" "$term"
done
require_term "src/codegen/transpiler_specialization_registry.h" \
    "transpiler_ensure_generic_class_specialization_from_type_name("
for rel in \
    "src/codegen/transpiler_call_constructor_result_emit.c" \
    "src/codegen/transpiler_expr_call_member_emit.c"; do
    require_term "$rel" \
        "transpiler_ensure_generic_class_specialization_from_type_name("
done
require_term "src/codegen/transpiler_decl_method_view.c" \
    "generic_start = strchr(host_type_name, '<')"
require_term "Makefile" \
    "\$(CODEGEN_DIR)/transpiler_specialization_type_name_scan.c"
if grep -Fq "mir_source_local_type_name_in_ast" \
        "$ROOT_DIR/src/compiler/mir.h"; then
    fail "MIR public inventory header must not expose source-local AST compatibility lookup"
fi
require_term "src/compiler/mir_source_local_types.h" \
    "mir_source_local_type_name_in_ast"
require_term "Makefile" \
    "\$(COMPILER_DIR)/mir_source_local_expr_types.c"
require_term "Makefile" \
    "\$(COMPILER_DIR)/mir_source_local_type_shape.c"
require_term "Makefile" \
    "\$(COMPILER_DIR)/mir_source_local_expr_binding_facts.c"
require_term "src/compiler/mir_source_local_types.c" \
    "mir_source_local_type_capture_node"
require_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "mir_source_local_builtin_call_type_name"
require_term "src/compiler/mir_source_local_expr_types.c" \
    "case AST_ARRAY_LITERAL"
require_term "src/compiler/mir_source_local_expr_types.c" \
    "mir_source_local_type_scratch_format(scratch, \"Array\""
require_term "src/compiler/mir_source_local_expr_binding_facts.c" \
    "mir_source_local_decl_header_is_constructor_type"
require_term "src/compiler/mir_source_local_expr_binding_facts.c" \
    "case AST_ZONE_DECL"
require_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "mir_source_local_decl_call_type_name(program, callee_name)"
require_term "src/compiler/mir_source_local_expr_binding_facts.c" \
    "header->ast_type == AST_INTENT_DECL"
require_term "src/compiler/mir_source_local_expr_binding_facts.c" \
    "return \"Bool\""
require_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "mir_source_local_call_return_type_name"
require_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "mir_source_local_generic_actual_type_name"
require_term "src/compiler/mir_source_local_expr_types.c" \
    "case AST_SPAWN_EXPR"
require_term "src/compiler/mir_source_local_expr_types.c" \
    "case AST_AWAIT_EXPR"
require_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "mir_source_local_extern_return_type_name"
require_term "src/tests/mir/test_mir_lowering_part_c_3.cases.h" \
    "MIR captures generic spawn and await source-local types"
require_term "src/tests/mir/test_mir_lowering_part_c_3.cases.h" \
    "MIR captures extern call source-local types"
require_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "mir_source_local_read_call_type_name"
require_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "mir_source_local_view_call_type_name"
require_term "src/compiler/mir_source_local_expr_binding_facts.c" \
    "mir_source_local_routine_owner_name"
if ! awk '
    /transpiler_mir_ssa_local_find_versioned_type_name\(/ { in_fn = 1 }
    in_fn && /transpiler_mir_routine_source_local_type_name\(/ && source == 0 { source_call = NR }
    in_fn && source_call > 0 && /routine, base_name\)/ && source == 0 {
        source = source_call
        source_call = 0
    }
    in_fn && /transpiler_infer_local_type_name_from_expr\(/ && infer == 0 { infer = NR }
    in_fn && /^}/ {
        if (source > 0 && infer > 0 && source < infer) ok = 1
        in_fn = 0
    }
    END { exit ok ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_mir_ssa_local_facts.c"; then
    fail "C MIR SSA local declarations must consume MIR source-local type facts before initializer AST inference"
fi
require_term "src/compiler/mir_source_local_expr_binding_facts.c" \
    "routine->hir_routine->owner_name"
require_term "src/compiler/mir_source_local_expr_binding_facts.c" \
    "mir_source_local_owner_method_return_type_name"
require_each_following_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "mir_source_local_owner_method_return_type_name(program," \
    "routine, callee_name" \
    5
require_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "mir_source_local_builtin_returns_first_arg_type"
for term in \
    "strcmp(callee_name, \"Clamp\") == 0" \
    "strcmp(callee_name, \"DeviceRead\") == 0" \
    "strcmp(callee_name, \"Max\") == 0" \
    "strcmp(callee_name, \"Min\") == 0" \
    "strcmp(callee_name, \"Read\") == 0" \
    "strcmp(callee_name, \"ViewRead\") == 0" \
    "strcmp(callee_name, \"ViewWrite\") == 0"; do
    require_term "src/compiler/mir_source_local_expr_call_facts.c" "$term"
done
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR captures builtin call return types for source locals"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR captures for-loop variable source-local types"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR captures slice source-local types"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR captures owner method call return types for source locals"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR captures self method call return types for source locals"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR captures callable return source-local facts"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR captures select receive source-local types"
require_term "src/compiler/mir_source_local_types.c" \
    "case AST_SELECT_STMT:"
require_term "src/compiler/mir_source_local_type_shape.c" \
    "mir_source_local_unwrap_channel_type"
require_term "src/compiler/mir_source_local_type_shape.c" \
    "mir_source_local_tuple_element_type"
require_term "src/compiler/mir_source_local_expr_types.c" \
    "case AST_CHANNEL_RECV:"
require_term "src/compiler/mir_source_local_expr_types.c" \
    "mir_source_local_for_loop_variable_type_name"
require_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "mir_source_local_call_return_type_name"
require_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "strcmp(member_name, \"Slice\") == 0"
require_term "src/compiler/mir_source_local_expr_call_facts.c" \
    "strcmp(callee_name, \"SliceCopy\") == 0"
for owner in \
    "src/compiler/mir_source_local_type_shape.c" \
    "src/compiler/mir_source_local_expr_binding_facts.c" \
    "src/compiler/mir_source_local_expr_call_facts.c" \
    "src/compiler/mir_source_local_expr_types.c"; do
    owner_lines="$(wc -l < "$ROOT_DIR/$owner")"
    if (( owner_lines >= 600 )); then
        fail "$owner is ${owner_lines} LOC; source-local type owners must stay below 600"
    fi
done
require_term "src/compiler/mir_source_local_types.c" \
    "mir_source_local_type_name_in_ast(ASTNode *body"
require_term "src/compiler/mir.c" \
    "mir_routine_source_local_type_names_capture(mir, &routine)"
require_term "src/codegen/transpiler_mir_ssa_local_facts.c" \
    "transpiler_mir_routine_source_local_type_name(routine, base_name)"
require_term "src/codegen/transpiler_mir_ssa_local_facts.c" \
    "transpiler_infer_local_type_name_from_expr("
require_term "src/codegen/llvm_internal.h" \
    "const MIRRoutine *current_mir_routine"
require_term "src/codegen/llvm_mir_emit.c" \
    "ctx->current_mir_routine = routine"
require_term "src/codegen/llvm_mir_emit.c" \
    "ctx->current_mir_routine = saved_mir_routine"
require_term "src/codegen/llvm_stmt_source_local_fallback.c" \
    "const MIRRoutine *routine = ctx->current_mir_routine"
require_term "src/compiler/mir_program_validate.c" \
    "without source-local type inventory"
require_term "src/compiler/mir_program_validate.c" \
    "source-local type fact[%zu] is incomplete"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR validator rejects missing source-local type inventory"
require_term "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "MIR validator rejects invalid source-local type fact"
if grep -Fq "transpiler_find_active_function_routine_for_call" \
        "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"; then
    fail "C MIR local type lookup reintroduced owner-local routine scan"
fi
if grep -Fq "if (!transpiler_mir_routine_has_signature" \
        "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"; then
    fail "C MIR local type lookup must use transpiler_mir_signature for missing-signature diagnostics"
fi
if grep -Fq "callee_return_type->type != AST_EVENT_HANDLER_TYPE" \
        "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"; then
    fail "C MIR local type lookup must let transpiler_mir_signature own return type-name completeness checks"
fi
for term in \
    "transpiler_mir_assignment_target_is_local" \
    "transpiler_mir_routine_has_source_local_binding(" \
    "mir_routine_param_count(routine)" \
    "transpiler_emit_mir_assignment_def_inst("; do
    require_term "src/codegen/transpiler_mir_assignment_emit.c" "$term"
done
if grep -Fq "block->source_local_defs" \
        "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.c"; then
    fail "C MIR assignment target classification must consume MIR source-local type facts, not block source-local def arrays"
fi
require_term "src/codegen/transpiler_mir_block_emit.c" \
    "buf, func_decl, mir_routine, block"
require_term "src/codegen/transpiler_mir_block_emit.c" \
    "transpiler_mir_routine_has_source_local_binding("
require_term "src/codegen/transpiler_mir_block_emit.c" \
    "transpiler_mir_routine_source_local_type_name("
require_term "src/codegen/transpiler_mir_block_emit.c" \
    "DEF '%s' is missing source-local type metadata"
if ! grep -B3 -F "return transpiler_has_explicit_local_binding(func_decl, target_name);" \
        "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.c" |
        grep -Fq "if (routine != NULL)"; then
    fail "C MIR assignment target classification must keep AST local-binding scan behind non-MIR fallback"
fi
if grep -Fq "transpiler_has_local_binding_in_block" \
        "$ROOT_DIR/src/codegen/transpiler_mir_local_binding.h"; then
    fail "C MIR local-binding AST block scan must stay private to its non-MIR compatibility owner"
fi
for term in \
    "mir_routine_param_count(routine)" \
    "mir_routine_param(routine, p)"; do
    require_term "src/codegen/transpiler_mir_emission_mapping_contract.c" "$term"
done
for term in \
    "transpiler_collect_mir_intent_bindings(" \
    "IntentBindingMetadataView binding_metadata" \
    "transpiler_seed_intent_aliases_for_mapping" \
    "transpiler_seed_aliases_from_mir_metadata" \
    "if (alias == NULL)" \
    "ssa_map, &binding_metadata"; do
    require_term "src/codegen/transpiler_mir_emission_mapping_contract.c" "$term"
done
require_term "src/codegen/transpiler_mir_inventory_intent_collect.h" \
    "IntentBindingMetadataView *bindings_out"
require_term "src/codegen/transpiler_mir_inventory_intent_alias_collect.c" \
    "bindings_out->kinds"
require_term "src/codegen/transpiler_mir_inventory_intent_alias_collect.c" \
    "bindings_out->aliases"
require_term "src/codegen/transpiler_mir_inventory_intent_alias_collect.c" \
    "bindings_out->types"
require_term "src/codegen/transpiler_mir_inventory_intent_alias_collect.c" \
    "bindings_out->owns_storage = true"
for rel in \
    "src/codegen/transpiler_expr_call_user_emit.c" \
    "src/codegen/transpiler_intent_emit.c" \
    "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "src/codegen/transpiler_mir_emission_mapping_contract.c"; do
    require_term "$rel" "IntentBindingMetadataView binding_metadata"
    if grep -Fq "&binding_kinds, &binding_aliases" "$ROOT_DIR/$rel"; then
        fail "$rel must collect MIR intent bindings through IntentBindingMetadataView"
    fi
done
for rel in \
    "src/codegen/llvm_expr_call_dispatch.c" \
    "src/codegen/llvm_intent.c" \
    "src/codegen/llvm_intent_forward.c" \
    "src/codegen/llvm_intent_setup.c" \
    "src/codegen/llvm_mir_emit.c" \
    "src/codegen/llvm_mir_param_emit.c" \
    "src/codegen/transpiler_expr_call_user_emit.c" \
    "src/codegen/transpiler_intent_emit.c" \
    "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "src/codegen/transpiler_mir_emission_mapping_contract.c"; do
    if grep -Fq "binding_kinds[i] == NULL" "$ROOT_DIR/$rel" \
        || grep -Fq "binding_aliases[i] == NULL" "$ROOT_DIR/$rel" \
        || grep -Fq "binding_types[i] == NULL" "$ROOT_DIR/$rel"; then
        fail "$rel must validate ordered binding rows through IntentBindingMetadataView predicates"
    fi
    if grep -Fq -- "->kinds" "$ROOT_DIR/$rel" \
        || grep -Fq -- "->aliases" "$ROOT_DIR/$rel" \
        || grep -Fq -- "->types" "$ROOT_DIR/$rel" \
        || grep -Fq ".kinds" "$ROOT_DIR/$rel" \
        || grep -Fq ".aliases" "$ROOT_DIR/$rel" \
        || grep -Fq ".types" "$ROOT_DIR/$rel"; then
        fail "$rel must read ordered binding rows through IntentBindingMetadataView accessors"
    fi
    if grep -Fq "strcmp(binding_kinds[i], \"participant\") != 0" "$ROOT_DIR/$rel" \
        || grep -Fq "strcmp(binding_kinds[i], \"value\") != 0" "$ROOT_DIR/$rel"; then
        fail "$rel must validate ordered binding kind through IntentBindingMetadataView predicates"
    fi
done
if grep -Fq "const char ***kinds_out" \
        "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent_collect.h" \
    || grep -Fq "const char ***kinds_out" \
        "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent_alias_collect.c"; then
    fail "C MIR intent binding collector must return one metadata view, not three parallel out-params"
fi
if grep -Fq "transpiler_collect_mir_intent_values(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c" \
    || grep -Fq "transpiler_collect_mir_intent_participants(" \
        "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"; then
    fail "C MIR mapping precheck must consume ordered binding rows, not separate participant/value collectors"
fi
if grep -Fq "mir_binding_count != mir_participant_count + mir_value_count" \
    "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"; then
    fail "C MIR mapping precheck must not compare ordered binding rows against legacy participant/value counts"
fi
if grep -Eq 'ast_intent_decl_(involves|values)\(' "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"; then
    fail "C MIR mapping precheck must not reopen intent participant/value AST arrays"
fi
if grep -Eq 'ast_intent_(involves|value)_alias\(' "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"; then
    fail "C MIR mapping precheck must seed intent aliases from MIR carrier rows"
fi
for rel in \
    "src/codegen/llvm_decl_routines.c" \
    "src/codegen/llvm_intent.c" \
    "src/codegen/llvm_intent_forward.c"; do
    require_term "$rel" "llvm_routine_inventory_get(inventory, i)"
    if grep -Eq '(inventory|routine_inventory)->routines\[[^]]+\]' "$ROOT_DIR/$rel"; then
        fail "$rel must consume LLVM MIR routine inventory through llvm_routine_inventory_get"
    fi
done
for term in \
    "llvm_mir_routine_kind(routine)" \
    "llvm_mir_routine_name(routine)" \
    "llvm_mir_routine_owner_name(routine)" \
    "llvm_mir_routine_owner_ast_type(routine)" \
    "llvm_mir_routine_signature_metadata_complete(ctx" \
    "MIR-only LLVM path missing function body signature metadata" \
    "llvm_mir_routine_param_count(routine)" \
    "llvm_mir_routine_param(routine" \
    "llvm_mir_routine_param_type_name(routine" \
    "llvm_mir_routine_return_type(routine)" \
    "llvm_mir_routine_return_type_name(routine)" \
    "llvm_mir_routine_within_zone(routine)"; do
    require_term "src/codegen/llvm_mir_emit.c" "$term"
done
if grep -Fq "!is_intent && llvm_active_has_mir(ctx)" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"; then
    fail "LLVM MIR body emission must consume llvm_mir_signature owner"
fi
for rel in \
    "src/codegen/llvm_mir_emit.c" \
    "src/codegen/llvm_mir_param_emit.c"; do
    if grep -Eq 'ast_func_(return_type|param_count|param)\(' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must not reopen source-AST function signature fallback"
    fi
done
for rel in \
    "src/codegen/llvm_decl_routines.c" \
    "src/codegen/llvm_intent_flow.c" \
    "src/codegen/llvm_inventory_internal.c" \
    "src/codegen/llvm_mir_contract.c" \
    "src/codegen/llvm_mir_emit.c" \
    "src/codegen/llvm_mir_param_emit.c"; do
    if grep -Eq 'routine->(kind|name|owner_name|owner_ast_type)' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must consume MIRRoutine metadata through llvm_mir_routine_* accessors"
    fi
done
for rel in \
    "src/codegen/transpiler_inventory_view.c" \
    "src/codegen/transpiler_mir_emission_contract.c" \
    "src/codegen/transpiler_mir_emission_mapping_contract.c" \
    "src/codegen/transpiler_mir_func_emit.c" \
    "src/codegen/transpiler_mir_inventory_intent_collect.c" \
    "src/codegen/transpiler_mir_local_type_lookup.c" \
    "src/codegen/transpiler_mir_resource_op_emit.c"; do
    if grep -Eq 'routine->(kind|name|owner_name|owner_ast_type)|mir_routine->(kind|name|owner_name|owner_ast_type)' \
        "$ROOT_DIR/$rel"; then
        fail "$rel must consume MIRRoutine metadata through transpiler_mir_routine_* accessors"
    fi
done
if grep -RInE --include='*.c' --include='*.h' \
    '(routine|mir_routine)->(kind|name|owner_name|owner_ast_type)' \
    "$ROOT_DIR/src/codegen" >/dev/null 2>&1; then
    grep -RInE --include='*.c' --include='*.h' \
        '(routine|mir_routine)->(kind|name|owner_name|owner_ast_type)' \
        "$ROOT_DIR/src/codegen" >&2 || true
    fail "C/LLVM codegen must consume MIRRoutine metadata through inventory accessors"
fi
require_term "src/codegen/llvm_boundary_slot_param.h" \
    "llvm_boundary_slot_inner_name_from_type_name"
require_term "src/codegen/llvm_boundary_slot_param.c" \
    "llvm_boundary_slot_inner_name_from_type_name"
require_term "src/codegen/llvm_mir_emit.c" \
    "llvm_boundary_slot_inner_name_from_type_name(ctx"
require_term "src/codegen/llvm_mir_emit.c" \
    "llvm_mir_routine_param_type_name(routine, i)"
require_term "src/codegen/llvm_mir_emit.c" \
    "slot_inner = param_type_name != NULL"
if grep -Fq "slot_inner != NULL && p != NULL && p->type != NULL" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"; then
    fail "LLVM MIR function type construction must not require AST param->type before routine slot type-name metadata"
fi
for term in \
    "llvm_forward_declare_func_from_mir" \
    "llvm_forward_declare_func_with_signature" \
    "allow_ast_compat = routine == NULL" \
    "&& (generic_func || extern_func)" \
    "llvm_function_emitted_param_count(ctx, node, routine," \
    "llvm_mir_routine_signature_metadata_complete(" \
    "MIR-only LLVM path missing function forward routine" \
    "MIR-only LLVM path missing function forward signature metadata" \
    "llvm_mir_routine_param_count(routine)" \
    "llvm_mir_routine_param(routine" \
    "llvm_mir_routine_param_type_name(routine" \
    "llvm_mir_routine_return_type(routine)" \
    "llvm_mir_routine_return_type_name(routine)"; do
    require_term "src/codegen/llvm_decl.c" "$term"
done
if grep -Fq "routine != NULL && llvm_active_has_mir(ctx)" \
    "$ROOT_DIR/src/codegen/llvm_decl.c"; then
    fail "LLVM function declaration emission must use MIR signature facts whenever a routine exists"
fi
require_term "src/codegen/llvm_boundary_slot_param.h" \
    "llvm_boundary_slot_inner_name_from_type_name"
require_term "src/codegen/llvm_boundary_slot_param.c" \
    "llvm_boundary_slot_inner_name_from_type_name"
require_term "src/codegen/llvm_decl.c" \
    "llvm_boundary_slot_inner_name_from_type_name(ctx"
require_term "src/codegen/llvm_decl_routines.c" \
    "llvm_forward_declare_func_from_mir(routine, NULL, ctx)"
require_term "src/codegen/llvm_decl_routines.c" \
    "llvm_mir_routine_kind(routine) != MIR_SCOPE_FUNCTION"
require_term "src/codegen/llvm_decl_routines.c" \
    "MIR-only LLVM path has unnamed function routine inventory row"
if grep -Fq "llvm_forward_declare_func_from_mir(routine, func_decl, ctx)" \
    "$ROOT_DIR/src/codegen/llvm_decl_routines.c"; then
    fail "LLVM function routine forward inventory must use routine-only MIR signatures for non-generic routines"
fi
require_term "src/codegen/llvm_mir_signature.c" \
    "MIR-only LLVM path missing function signature metadata"
for term in \
    "llvm_mir_routine_signature_metadata_complete_for(ctx" \
    "LLVM_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES" \
    "MIR-only LLVM path missing function parameter routine" \
    "MIR-only LLVM path missing function parameter signature metadata" \
    "llvm_mir_routine_param_count(routine)" \
    "llvm_mir_routine_param(routine" \
    "llvm_mir_routine_param_type_name(routine"; do
    require_term "src/codegen/llvm_mir_param_emit.c" "$term"
done
if grep -Fq "if (!llvm_mir_routine_has_signature" \
    "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"; then
    fail "LLVM MIR parameter emission must consume llvm_mir_signature owner"
fi
for term in \
    "llvm_boundary_slot_inner_name_from_type_name(ctx" \
    "llvm_register_typed_var_abi_binding(ctx, p->name, alloca" \
    "llvm_register_var_class(ctx, p->name, param_type_name)"; do
    require_term "src/codegen/llvm_mir_param_emit.c" "$term"
done
for term in \
    "llvm_registry_required_arg_name" \
    "llvm_constructed_arg_name_copy(type_name, arg_index"; do
    require_term "src/codegen/llvm_backend_type_registry.c" "$term"
done
if grep -Fq "ast_type_generic_args(" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c" \
    || grep -Fq "ast_generic_param_constraint(" \
        "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"; then
    fail "LLVM typed-var registry must consume rendered type-name args, not AST generic args"
fi
for term in \
    "llvm_mir_try_emit_await_local_def" \
    "llvm_mir_find_await_resource_op" \
    "llvm_mir_find_await_resource_op(mir_block, resource_name)" \
    "strcmp(candidate->name, \"AwaitLocal\")" \
    "strcmp(candidate->name, \"AwaitRemote\")" \
    "init = inst->expr0" \
    "type_ann = inst->expr1" \
    "operand->type == AST_SPAWN_EXPR" \
    "inner = llvm_infer_spawn_future_inner(ctx, operand)" \
    "resource_name = operand->type == AST_SPAWN_EXPR ? \"spawn\" : future_name" \
    "llvm_mir_async_fact_future_inner_from_source_local" \
    "llvm_await_task_handle(ctx, init, task, inner, is_remote)" \
    "LLVM MIR await def requires matching AwaitLocal/AwaitRemote resource fact"; do
    require_term "src/codegen/llvm_mir_await_emit.c" "$term"
done
for term in \
    "llvm_mir_async_fact_future_inner_from_source_local" \
    "mir_routine_source_local_type_name(routine, future_name)" \
    "llvm_constructed_arg_name_copy(type_name, 0, inner_out"; do
    require_term "src/codegen/llvm_mir_async_fact.c" "$term"
done
if grep -Fq "mir_instruction_source_payload" \
    "$ROOT_DIR/src/codegen/llvm_mir_await_emit.c"; then
    fail "LLVM await DEF emission must consume MIR expr/type facts, not source payload statements"
fi
grep -Fq "LLVM let binding '%s' initializer did not produce a value" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c" || \
    fail "LLVM let lowering must fail closed when an initializer returns no value"
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
    "PGY_PROJECTION_TARGET_LLVM"
require_term "src/codegen/llvm_inventory_internal.h" \
    "llvm_active_has_mir"
require_term "src/codegen/llvm_inventory_internal.c" \
    "llvm_active_has_mir(const LLVMGenCtx *ctx)"
require_term "src/compiler/verified_projection_plan.c" \
    "mir_program_recorded_inventory_uses_intent_observability_surface(mir)"
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
if grep -RIn "llvm_active_synthetic_executable_func" \
    "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h'; then
    fail "LLVM top-level executable wrapper must consume MIR program facts, not source declaration payloads"
fi
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
    "llvm_lookup_generic_template_entry(LLVMGenCtx *ctx, const char *name)" \
    "llvm_register_generic_template_entry(LLVMGenCtx *ctx," \
    "const MIRRoutine *routine)" \
    "ctx->generic_templates[ctx->generic_template_count].routine = routine" \
    "llvm_register_generic_template_decl(LLVMGenCtx *ctx, ASTNode *func_decl)" \
    "llvm_register_generic_template_routine(LLVMGenCtx *ctx," \
    "llvm_mir_routine_kind(routine) != MIR_SCOPE_FUNCTION" \
    "llvm_lookup_generic_template(LLVMGenCtx *ctx, const char *name)"; do
    require_term "src/codegen/llvm_backend_generic.c" "$term"
done
require_term "src/codegen/llvm_backend_generic.h" \
    "llvm_lookup_generic_template_entry("
require_term "src/codegen/llvm_internal.h" \
    "const MIRRoutine *routine"
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
if grep -RIn "mir_find_function_decl" \
    "$ROOT_DIR/src/compiler" "$ROOT_DIR/src/codegen" \
    --include='*.c' --include='*.h'; then
    fail "synthetic executable lookup must not expose an AST-returning MIR function query"
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
    "llvm_decl_function_routine_has_body_storage(" \
    "routine->block_count > 0 && routine->blocks != NULL" \
    "llvm_register_generic_template_routine(ctx, routine)" \
    "llvm_emit_func_from_mir(routine, ctx)" \
    "MIR-only LLVM path missing routine for function" \
    "MIR-only LLVM path has invalid function routine inventory row"; do
    require_term "src/codegen/llvm_decl_routines.c" "$term"
done
for forbidden in \
    "llvm_decl_require_function_source_decl(" \
    "mir_routine_source_decl_of_type(" \
    "llvm_register_generic_template_decl(ctx,"; do
    if grep -Fq "$forbidden" \
        "$ROOT_DIR/src/codegen/llvm_decl_routines.c"; then
        fail "LLVM function routine inventory owner must not recover generic/source declarations through $forbidden"
    fi
done
if awk '
    /llvm_validate_function_routine_bodies_from_inventory\(/ { in_fn = 1 }
    in_fn && /llvm_decl_require_function_source_decl\(/ { found = 1 }
    in_fn && /^}/ { in_fn = 0 }
    END { exit found ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_decl_routines.c"; then
    fail "LLVM function routine body validation must consume MIR routine facts, not source declaration recovery"
fi
if awk '
    /llvm_emit_function_routines_from_inventory\(/ { in_fn = 1 }
    in_fn && /llvm_decl_require_function_source_decl\(/ { found = 1 }
    in_fn && /^}/ { in_fn = 0 }
    END { exit found ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_decl_routines.c"; then
    fail "LLVM function routine emit inventory must consume MIR routine facts, not source declaration recovery"
fi
if grep -Fq "instruction_count > 0" \
    "$ROOT_DIR/src/codegen/llvm_decl_routines.c"; then
    fail "LLVM function routine validation must not use instruction_count as the body-existence proxy; empty Void bodies are valid"
fi
require_term "src/codegen/llvm_decl_routines.c" \
    "llvm_mir_or_ast_function_is_generic(routine, NULL)"
for term in \
    "const LLVMGenericTemplate *generic_template =" \
    "llvm_lookup_generic_template_entry(ctx, callee_name)" \
    "generic_template->routine" \
    "mir_decl_header_generic_param_count(generic_header)" \
    "llvm_mir_routine_signature_metadata_complete(ctx," \
    "specialized = *generic_routine" \
    "specialized.name = mangled" \
    "llvm_emit_func_from_mir(&specialized, ctx)"; do
    require_term "src/codegen/llvm_expr_spawn_generic.c" "$term"
done
for term in \
    "routine_kind = mir_routine_kind(routine)" \
    "is_intent = (routine_kind == MIR_SCOPE_INTENT)" \
    "llvm_mir_routine_signature_metadata_complete(ctx," \
    "ctx->current_mir_routine = routine"; do
    require_term "src/codegen/llvm_mir_emit.c" "$term"
done
if grep -Fq "mir_routine_source_decl_of_type(" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"; then
    fail "LLVM MIR body emitter must not recover source declarations; MIRRoutine metadata is the body source of truth"
fi
require_term "src/codegen/llvm_decl.c" '#include "llvm_decl_authority.h"'
for term in \
    "llvm_decl_emit_zone_authority_check(LLVMGenCtx *ctx)" \
    "pgy_zone_authority_check_export" \
    "llvm_mir_routine_owner_ast_type(ctx->current_mir_routine)" \
    "llvm_find_decl_header_in_context_of_type(" \
    "mir_decl_header_zone_authority_count(zone_header)" \
    "mir_decl_zone_authority_subject_slot_name(authority)" \
    "llvm_scope_lookup_snapshot(ctx, \"self\", &self_var)" \
    "llvm_set_mir_inventory_missing(ctx"; do
    require_term "src/codegen/llvm_decl_authority.c" "$term"
done
for term in \
    "transpiler_active_decl_header_of_type(" \
    "mir_decl_header_zone_authority_count(" \
    "mir_decl_zone_authority_subject_slot_name(authority)" \
    "PGY_ZONE_AUTHORITY_CHECK(self"; do
    require_term "src/codegen/transpiler_mir_func_emit.c" "$term"
done
if grep -Fq "llvm_scope_lookup(ctx, \"self\")" \
        "$ROOT_DIR/src/codegen/llvm_decl_authority.c"; then
    fail "LLVM zone authority checks must snapshot implicit self scope metadata"
fi
if grep -R "data\.zone_decl\.\(authorities\|authority_count\)" \
    "$ROOT_DIR/src/codegen/llvm_decl.c" \
    "$ROOT_DIR/src/codegen/llvm_decl_authority.c" >/dev/null; then
    fail "LLVM zone authority checks must use MIR declaration authority metadata"
fi
if grep -R "ast_zone_authorities\|ast_zone_authority_subject_slot_name" \
    "$ROOT_DIR/src/codegen/llvm_decl_authority.c" \
    "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c" >/dev/null; then
    fail "zone authority codegen must consume MIR declaration authority metadata instead of AST authority accessors"
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
        fail "$rel must not reopen MIRRoutine internals for source AST provenance"
    fi
done
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
    "llvm_forward_declare_intent_from_mir_routine(ctx, routine)" \
    "llvm_mir_routine_kind(routine) != MIR_SCOPE_INTENT" \
    "llvm_collect_mir_intent_bindings(" \
    "MIR-only LLVM path has invalid intent routine inventory row"; do
    require_term "src/codegen/llvm_intent_forward.c" "$term"
done
if grep -Fq "llvm_require_mir_intent_source_decl(ctx, routine, &intent_decl)" \
    "$ROOT_DIR/src/codegen/llvm_intent_forward.c" \
    || grep -Fq "llvm_forward_declare_intent(intent_decl, ctx)" \
        "$ROOT_DIR/src/codegen/llvm_intent_forward.c"; then
    fail "LLVM intent forward routine inventory must use MIR binding metadata directly, not source declaration recovery"
fi
for term in \
    "llvm_emit_intent_routines_from_inventory(" \
    "llvm_active_inventory(ctx, AST_INTENT_DECL" \
    "llvm_find_mir_intent_routine(ctx, node)" \
    "llvm_emit_intent_decl(intent_decl, ctx)" \
    "MIR-only LLVM path has invalid intent routine inventory row"; do
    require_term "src/codegen/llvm_intent.c" "$term"
done
require_term "src/codegen/llvm_intent.c" \
    "MIR-only LLVM path missing intent declaration inventory row for routine"
if grep -Fq "llvm_require_mir_intent_source_decl(" \
    "$ROOT_DIR/src/codegen/llvm_intent.c" \
    "$ROOT_DIR/src/codegen/llvm_intent_flow.c" \
    "$ROOT_DIR/src/codegen/llvm_intent_internal.h"; then
    fail "LLVM intent body emission must start from active declaration inventory, not recover routine source declarations"
fi
if grep -Fq "mir_routine_source_decl_of_type(" \
    "$ROOT_DIR/src/codegen/llvm_intent_flow.c"; then
    fail "LLVM intent flow must not recover AST intent declarations from MIRRoutine payload"
fi
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
    "transpiler_active_routine_count" \
    "transpiler_active_host_decl_header" \
    "transpiler_active_externs" \
    "transpiler_active_executables" \
    "transpiler_active_has_mir" \
    "transpiler_active_mir_identity" \
    "transpiler_active_has_main_function" \
    "transpiler_active_has_top_level_exec" \
    "transpiler_active_uses_thread_pool" \
    "transpiler_active_can_emit_intent_cleanup_from_mir"; do
    require_term "src/codegen/transpiler_inventory_view.h" "$term"
done
require_term "src/codegen/transpiler_inventory_view.c" \
    "transpiler_active_has_mir(const TranspilerCtx *ctx)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "transpiler_active_mir_identity(const TranspilerCtx *ctx)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "transpiler_active_host_decl_header(const TranspilerCtx *ctx, const char *name)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "pgy_host_decl_compat_types(&host_type_count)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "mir_routine_inventory_from_program(mir, &mir_inventory)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "mir_program_has_main_function(ctx->mir)"
require_term "src/codegen/transpiler_inventory_view.c" \
    "mir_program_has_top_level_exec(ctx->mir)"
if grep -RIn "transpiler_active_synthetic_executable_func" \
    "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h'; then
    fail "C top-level executable wrapper must consume MIR program facts, not source declaration payloads"
fi
require_term "src/codegen/transpiler_entry.c" \
    "PGY_PROJECTION_TARGET_C"
require_term "src/codegen/transpiler.c" \
    "transpiler_active_has_mir(ctx)"
if grep -Fq "ctx->mir" "$ROOT_DIR/src/codegen/transpiler.c"; then
    fail "C program emitter must use active MIR view helpers, not direct ctx->mir probes"
fi
require_term "src/codegen/transpiler_inventory_view.c" \
    "pgy_mir_program_uses_thread_pool(ctx->mir)"
if grep -RIn "transpiler_mir_routine_source_ast(const MIRRoutine \*routine)" \
        "$ROOT_DIR/src/codegen/transpiler_inventory_view.c" \
        "$ROOT_DIR/src/codegen/transpiler_inventory_view.h"; then
    fail "C routine source provenance must not be hidden behind a thin transpiler_mir_routine_source_ast alias"
fi
if grep -RIn "transpiler_mir_routine_source_ast_of_type" \
        "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h'; then
    fail "C routine source provenance compatibility aliases must stay retired"
fi
if grep -RIn "mir_routine_source_decl_of_type(" \
        "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h'; then
    fail "C backend must not recover routine source declarations in codegen"
fi
for rel in \
    "src/codegen/transpiler_inventory_view.c" \
    "src/codegen/transpiler_mir_emission_contract.c"; do
    if grep -Fq "routine->ast" "$ROOT_DIR/$rel"; then
        fail "$rel must not reopen MIRRoutine internals for source AST provenance"
    fi
done
if grep -Fq "transpiler_mir_routine_source_ast_of_type(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.c"; then
    fail "C MIR emission contract must validate routine compatibility through MIR kind/name/signature facts, not source_ast"
fi

for term in \
    "transpiler_active_inventory(ctx, AST_ABILITY_DECL, &abilities, &ability_count)" \
    "transpiler_active_inventory(ctx, AST_CLASS_DECL, &types, &type_count)" \
    "transpiler_active_inventory(ctx, AST_FUNC_DECL, &functions, &function_count)" \
    "transpiler_active_inventory(ctx, AST_INTENT_DECL, &intents, &intent_count)" \
    "transpiler_active_inventory(ctx, AST_ROLE_DECL, &roles, &role_count)" \
    "transpiler_active_inventory(ctx, AST_PARTY_DECL, &parties, &party_count)" \
    "transpiler_active_inventory(ctx, AST_ROSTER_DECL, &rosters, &roster_count)" \
    "transpiler_is_synthetic_executable_func(functions[i])" \
    "transpiler_active_decl_header_of_type(" \
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
require_term "src/codegen/transpiler_host_self_policy.c" \
    "mir_decl_header_uses_pointer_self(header)"
require_term "src/codegen/transpiler_host_self_policy.c" \
    "if (transpiler_active_has_mir(ctx))"
require_term "src/codegen/transpiler_projection.c" \
    "mir_decl_header_nominal_kind_or("
require_term "src/codegen/transpiler_projection.c" \
    "if (transpiler_active_has_mir(ctx))"
require_term "src/codegen/transpiler_intent_context.c" \
    "mir_decl_header_nominal_kind_or("
require_term "src/codegen/transpiler_intent_context.c" \
    "if (transpiler_active_has_mir(ctx))"
require_term "src/codegen/llvm_domain_lookup.c" \
    "mir_decl_header_uses_pointer_self(mir_decl)"
require_term "src/codegen/llvm_domain_lookup.c" \
    "if (llvm_active_has_mir(ctx))"
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
require_term "src/codegen/llvm_expr_identifier_slot_helpers.c" \
    "inner = llvm_lookup_slot_inner(ctx, source_name)"
if grep -Fq "ast_func_param_count(current_decl)" \
        "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"; then
    fail "LLVM slot identifier resolution must consume slot registry metadata instead of rescanning current function parameters"
fi
require_term "src/codegen/llvm_expr_call_variable.c" \
    "callable_entry = llvm_lookup_callable_entry(ctx, callee_name)"
if grep -Fq "ast_func_param_count(current_decl)" \
        "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"; then
    fail "LLVM callable variable calls must consume callable registry metadata instead of rescanning current function parameters"
fi
if grep -Fq "llvm_find_local_let_type_in_block" \
        "$ROOT_DIR/src/codegen/llvm_expr_common.c" \
        || grep -Fq "llvm_infer_local_let_type_in_block" \
        "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_nominal.c"; then
    fail "LLVM nominal type inference must consume active registries instead of rescanning the current function body"
fi
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
require_term "src/codegen/transpiler_expr_call_type_infer.c" \
    "ASTNode *decl = find_callable_decl(ctx, name)"
if grep -Eq 'find_(intent|function)_decl\(ctx, name\)' \
        "$ROOT_DIR/src/codegen/transpiler_expr_call_type_infer.c"; then
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
    "llvm_build_domain_projection_value_from_zone_refresh_view("
require_term "src/codegen/llvm_domain_projection_sync_body_helpers.c" \
    "&refresh_view, i, source_ptr)"
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
require_term "src/codegen/transpiler_projection.h" \
    "emit_projection_literal_by_name("
require_term "src/codegen/transpiler_projection_emit.c" \
    "resolve_projection_source_path_by_name(TranspilerCtx *ctx"
require_term "src/codegen/transpiler_projection_emit.c" \
    "projection_class_field_view_by_name("
require_term "src/codegen/transpiler_projection_emit.c" \
    "mir_decl_header_nominal_kind_or("
require_term "src/codegen/transpiler_projection_emit.c" \
    "emit_projection_literal_by_name("
if grep -RIn "compat_decl" \
        "$ROOT_DIR/src/codegen/transpiler_projection_emit.c"; then
    fail "C projection field views must not keep AST compatibility declaration parameters"
fi
if grep -RInE "resolve_projection_source_path_rec\\(|emit_projection_literal\\(TranspilerCtx \\*ctx" \
        "$ROOT_DIR/src/codegen/transpiler_projection_emit.c" \
        "$ROOT_DIR/src/codegen/transpiler_projection.h"; then
    fail "C projection literal/path loading must expose by-name owners, not AST source-decl wrappers"
fi
require_term "src/codegen/llvm_expr_projection_path_helpers.h" \
    "llvm_load_projection_path_value_by_name("
require_term "src/codegen/llvm_expr_projection_path_helpers.c" \
    "llvm_resolve_projection_source_path_by_name(LLVMGenCtx *ctx"
require_term "src/codegen/llvm_expr_projection_path_helpers.c" \
    "llvm_projection_field_view_by_name("
require_term "src/codegen/llvm_expr_projection_path_helpers.c" \
    "mir_decl_header_nominal_kind_or("
require_term "src/codegen/llvm_expr_projection_path_helpers.c" \
    "llvm_load_projection_path_value_by_name("
if grep -RIn "compat_decl" \
        "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"; then
    fail "LLVM projection field views must not keep AST compatibility declaration parameters"
fi
if grep -RIn "llvm_load_projection_path_value(LLVMGenCtx *ctx" \
        "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.h"; then
    fail "LLVM projection path loading must expose the by-name owner, not the AST source-decl wrapper"
fi
require_term "src/codegen/llvm_domain_projection_value_helpers.h" \
    "llvm_load_domain_projection_path_value_by_name("
require_term "src/codegen/llvm_domain_projection_value_helpers.c" \
    "llvm_resolve_domain_projection_source_path_by_name("
require_term "src/codegen/llvm_domain_projection_value_helpers.c" \
    "llvm_domain_projection_field_view_by_name("
require_term "src/codegen/llvm_domain_projection_value_helpers.c" \
    "llvm_domain_projection_type_is_vessel("
for rel in \
    "src/codegen/llvm_expr_host_spawn_literal_helpers.c" \
    "src/codegen/llvm_expr_member_access.c"; do
    require_term "$rel" "llvm_load_projection_path_value_by_name("
    if grep -Fq "llvm_find_projection_nominal_decl(ctx, source_class_name)" \
            "$ROOT_DIR/$rel"; then
        fail "$rel must consume projection type/header facts instead of recovering source declarations"
    fi
done
if grep -Fq "llvm_find_projection_nominal_decl(ctx, source_type_name)" \
        "$ROOT_DIR/src/codegen/llvm_domain_projection_sync_body_helpers.c"; then
    fail "LLVM domain projection sync must pass source type names instead of recovering source declarations"
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
    "transpiler_domain_constructor_decl_exists_local("
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
if grep -RInE 'transpiler_find_decl_in_inventory_local\(ctx, AST_(CLASS|ZONE|WORLD|RELATION|EFFECT|PARTY|ROLE|ROSTER|ENUM|ABILITY|FUNC)_DECL' \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' >/dev/null 2>&1; then
    grep -RInE 'transpiler_find_decl_in_inventory_local\(ctx, AST_(CLASS|ZONE|WORLD|RELATION|EFFECT|PARTY|ROLE|ROSTER|ENUM|ABILITY|FUNC)_DECL' \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' >&2 || true
    fail "C backend type-specific declaration recovery must prefer transpiler_find_named_decl_local"
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
    "transpiler_find_named_decl_local(ctx, AST_ZONE_DECL, zone_type)"
if grep -Fq "return find_zone_decl(ctx, zone_type)" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C world-zone projection resolution must consume typed declaration lookup instead of direct AST lookup"
fi
require_term "src/codegen/transpiler_world_select_event_emit.c" \
    "transpiler_ctx, AST_ZONE_DECL, zone_name)"
if grep -Fq "return find_zone_decl((TranspilerCtx *)ctx, zone_name)" \
    "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.c"; then
    fail "C world frontier lookup must consume active inventory instead of direct AST lookup"
fi
require_term "src/codegen/transpiler_mir_ssa_names.c" \
    "transpiler_active_decl_header_of_type("
if awk '
    /transpiler_mir_ssa_base_name_is_host_field/ { in_fn = 1 }
    in_fn && /transpiler_find_named_decl_local\(ctx, AST_ZONE_DECL, host_name\)/ { saw_ast = 1 }
    in_fn && /transpiler_active_has_mir\(ctx\)/ { saw_active = 1 }
    in_fn && /^}/ {
        if (saw_ast && !saw_active) bad = 1
        in_fn = 0
    }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"; then
    fail "C MIR SSA zone recovery must use declaration headers before AST zone lookup"
fi
if grep -Fq "find_zone_decl(ctx, host_name)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"; then
    fail "C MIR SSA host recovery must consume active inventory for zone lookup"
fi
require_term "src/codegen/transpiler_func_forward_policy.c" \
    "transpiler_active_decl_header_of_type("
if grep -Fq "transpiler_find_named_decl_local(ctx, AST_WORLD_DECL" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"; then
    fail "C function forward policy world existence checks must consume declaration headers in MIR-active paths"
fi
if grep -Fq "find_world_decl(ctx, name)" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"; then
    fail "C function forward policy must consume typed declaration lookup for world lookup"
fi
require_term "src/codegen/transpiler_projection_sync.c" \
    "transpiler_find_named_decl_local(ctx, AST_EFFECT_DECL,"
for rel in \
    "src/codegen/transpiler_projection_sync.c" \
    "src/codegen/transpiler_expr_call_member_emit.c"; do
    if grep -Fq "find_zone_decl(ctx, zone_type_name)" "$ROOT_DIR/$rel"; then
        fail "$rel must consume typed declaration lookup for world-zone projection/action context lookup"
    fi
done
require_term "src/codegen/transpiler_projection_sync.c" \
    "AST_EFFECT_DECL"
if grep -Fq "find_effect_decl(ctx, effect_name)" \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.c"; then
    fail "C world action effect sync must consume typed declaration lookup for effect lookup"
fi
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "effect_decl = transpiler_find_named_decl_local("
require_term "src/codegen/transpiler_overlay_zone_bind.c" \
    "ctx, AST_EFFECT_DECL, effect_type_name)"
if grep -Fq "find_effect_decl(ctx, ast_zone_layer_slot_layer_type(layer_slot))" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_bind.c"; then
    fail "C zone effect bind must consume typed declaration lookup for effect lookup"
fi
require_term "src/codegen/transpiler_overlay_zone_relation_bind.c" \
    "relation_decl = transpiler_find_named_decl_local("
require_term "src/codegen/transpiler_overlay_zone_relation_bind.c" \
    "ctx, AST_RELATION_DECL, relation_type_name)"
if grep -Fq "find_relation_decl(ctx," \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_relation_bind.c"; then
    fail "C zone relation bind must consume typed declaration lookup for relation lookup"
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
require_term "src/codegen/transpiler_expr_call_type_infer.c" \
    "transpiler_has_known_nominal_type(ctx, name)"
if grep -Fq "find_subject_host_decl(ctx, name)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_type_infer.c"; then
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
require_term "src/codegen/transpiler_host_field_identifier.h" \
    "transpiler_identifier_is_stale_host_field_snapshot"
require_term "src/codegen/transpiler_host_field_identifier.c" \
    "transpiler_current_function_has_self_receiver"
require_term "src/codegen/transpiler_host_field_identifier.c" \
    "transpiler_emit_current_host_field_identifier"
for term in \
    "transpiler_mir_routine_has_param_name" \
    "transpiler_mir_routine_has_source_local_binding" \
    "if (transpiler_active_has_mir(ctx))"; do
    require_term "src/codegen/transpiler_host_field_identifier.c" "$term"
done
require_term "src/codegen/transpiler_mir_local_binding.c" \
    "transpiler_mir_routine_source_local_type_name(routine, base_name)"
if grep -Fq "return transpiler_has_explicit_local_binding(ctx->current_func_decl" \
    "$ROOT_DIR/src/codegen/transpiler_host_field_identifier.c"; then
    fail "C host-field lexical shadowing must split MIR parameter facts from AST body-local compatibility"
fi
if awk '
    /transpiler_identifier_is_current_true_local\(TranspilerCtx \*ctx,/ { in_fn = 1 }
    in_fn && /transpiler_current_host_has_field\(TranspilerCtx \*ctx,/ { in_fn = 0 }
    in_fn && /transpiler_has_explicit_body_local_binding\(/ { bad = 1 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_host_field_identifier.c"; then
    fail "C host-field lexical shadowing must consume MIR source-local facts, not AST body-local scans"
fi
require_each_following_term "src/codegen/transpiler_host_field_identifier.c" \
    "transpiler_emit_current_host_field_identifier(TranspilerCtx *ctx" \
    "transpiler_current_world_has_field(ctx, id_name)" \
    20
require_each_following_term "src/codegen/transpiler_expr_dispatch_emit.c" \
    "if (transpiler_current_function_has_self_receiver(ctx)" \
    "transpiler_is_implicit_field(ctx, id_name)" \
    3
require_term "src/codegen/transpiler_expr_dispatch_emit.c" \
    "ident_is_stale_host_field_snapshot"
require_each_following_term "src/codegen/transpiler_expr_dispatch_emit.c" \
    "ident_is_stale_host_field_snapshot =" \
    "transpiler_identifier_is_stale_host_field_snapshot(ctx" \
    3
require_each_following_term "src/codegen/transpiler_expr_dispatch_emit.c" \
    "&& ident_is_stale_host_field_snapshot" \
    "transpiler_emit_current_host_field_identifier(ctx, id_name)" \
    20
if grep -Fq "host_decl->type == AST_PARTY_DECL" \
    "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"; then
    fail "C self-member dispatch must consume host pointer-self policy instead of repeating domain host chains"
fi
# MIR-only: the non-MIR C host-method AST lookups (current_host_method_decl,
# find_nominal_host_method_decl) are retired -- they fail closed (return NULL) and
# no longer build a method view, read MIR method names, or cache nominal method
# decls. Their host-DECL lookup siblings still consume the compat owner table.
for term in \
    "transpiler_active_mir_identity(ctx)" \
    "pgy_host_decl_compat_is_type(owner_ast_type)" \
    "owner_ast_type, owner_name" \
    "pgy_host_decl_compat_nominal_lookup_types(&host_lookup_type_count)" \
    "host_lookup_types[i]" \
    "AST_ROLE_DECL"; do
    require_term "src/codegen/transpiler_decl_host_lookup.c" "$term"
done
if grep -Fq "transpiler_mir_decl_method_body_decl" \
        "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c"; then
    fail "C host-method AST lookup must not recover MIR routine source declarations"
fi
if grep -Fq "transpiler_hosted_method_view_source_ast(&method_view, i)" \
    "$ROOT_DIR/src/codegen/transpiler_decl_host_lookup.c"; then
    fail "C host-method lookup must match method names through MIRDeclMethod metadata before compatibility AST recovery"
fi
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
# MIR-only: the non-MIR AST host-method lookup (find_nominal_host_method_decl)
# is retired; these subject/receiver resolvers consume MIR method metadata only.
for rel in \
    "src/codegen/transpiler_intent_context.c" \
    "src/codegen/transpiler_domain_receiver_query.c"; do
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
    "emit_role_method_impl(name, method_meta, mir_method, NULL, ctx)"; do
    require_term "src/codegen/transpiler_domain_nominal_emit.c" "$term"
done
for term in \
    "transpiler_mir_decl_method_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES" \
    "MIR-only C path missing included role method return type-name metadata" \
    "MIR-only C path missing included role method parameter type-name metadata"; do
    require_term "src/codegen/transpiler_domain_role_include_emit.c" "$term"
done
for term in \
    "const MIRDeclMethod *method_meta" \
    "const MIRRoutine *mir_method" \
    "transpiler_mir_decl_method_name(method_meta)" \
    "emit_func_decl_from_mir_named(NULL, mir_method, emitted_name" \
    "MIR-only C path missing declaration metadata for role method"; do
    require_term "src/codegen/transpiler_domain_role_methods_emit.c" "$term"
done
if grep -Fq "transpiler_mir_decl_method_body_decl(ctx, method_meta)" \
        "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"; then
    fail "C role method body emission must pass linked MIRRoutine directly instead of recovering source AST"
fi
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
    "transpiler_party_slot_method_ability_tag" \
    "render_mir_ability_ref_vtable_tag_in_ctx"; do
    require_term "src/codegen/transpiler_role_ability_helpers.h" "$term"
    require_term "src/codegen/transpiler_role_ability.c" "$term"
done
require_term "src/compiler/mir_decl.h" \
    "MIRAbilityRef"
require_term "src/compiler/mir_decl_header_fields.c" \
    "mir_ability_ref_capture"
require_term "src/compiler/mir_decl_headers.h" \
    "mir_decl_field_required_ability_ref"
require_term "src/compiler/mir_decl_headers.c" \
    "methods = ast_ability_methods(decl, &method_count)"
require_term "src/tests/mir/test_mir_lowering_part_d.cases.h" \
    "MIR declaration headers preserve ability method metadata"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "TranspilerHostedRoleSlotView"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_role_slot_view_from_decl"
require_term "src/codegen/transpiler_decl_lookup.h" \
    "transpiler_hosted_role_slot_view_required_ability_ref"
if grep -RInE 'required_ability_type_names|mir_decl_field_required_ability_type_name|transpiler_hosted_role_slot_view_required_ability_type_name|llvm_hosted_role_slot_view_required_ability_type_name' \
    "$ROOT_DIR/src/compiler" \
    "$ROOT_DIR/src/codegen" >/dev/null; then
    fail "role-slot ability metadata must have one MIR SoT: MIRAbilityRef, not legacy type-name aliases"
fi
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "transpiler_hosted_role_slot_view_from_decl(ctx, name, node)"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "transpiler_hosted_role_slot_view_missing_mir_metadata(&role_view)"
require_term "src/codegen/transpiler_domain_nominal_emit.c" \
    "transpiler_hosted_role_slot_view_required_ability_ref("
require_term "src/codegen/transpiler_role_ability.c" \
    "transpiler_hosted_role_slot_view_from_decl(ctx, party_name, party_decl)"
require_term "src/codegen/transpiler_role_ability.c" \
    "transpiler_hosted_role_slot_view_missing_mir_metadata(&role_view)"
require_term "src/codegen/transpiler_role_ability.c" \
    "transpiler_hosted_role_slot_view_required_ability_ref("
require_term "src/codegen/transpiler_role_ability.c" \
    "transpiler_render_mir_ability_formal_fallback"
require_term "src/codegen/transpiler_role_ability.c" \
    "mir_decl_generic_param_default_type_name(formal)"
require_term "src/codegen/transpiler_role_ability.c" \
    "mir_decl_generic_param_constraint_type_name(formal)"
require_term "src/codegen/transpiler_role_ability.c" \
    "transpiler_active_decl_header_of_type("
require_term "src/codegen/transpiler_role_ability.c" \
    "ctx, AST_ABILITY_DECL, base_name"
require_term "src/codegen/transpiler_role_ability.c" \
    "mir_decl_header_generic_param_count(ability_header)"
require_term "src/codegen/transpiler_role_ability.c" \
    "ability_decl = !mir_active"
require_term "src/codegen/transpiler_role_ability.c" \
    "MIR-only C path missing ability declaration header for ability tag"
require_term "src/codegen/transpiler_role_ability.c" \
    "MIR-only C path missing generic ability argument metadata"
require_term "src/codegen/transpiler_role_ability.c" \
    "transpiler_ability_header_has_method("
require_term "src/codegen/transpiler_role_ability.c" \
    "transpiler_active_decl_header_of_type("
if awk '
    /render_ability_ref_parts_vtable_tag_in_ctx\(TranspilerCtx \*ctx,/ { in_fn = 1 }
    in_fn && /if \(rendered == NULL\)/ { in_missing = 1 }
    in_missing && /return render_ability_type_name_vtable_tag\(base_name\);/ { bad = 1 }
    in_missing && /if \(mir_active\)/ { protected = 1 }
    in_missing && protected && /return NULL;/ { in_missing = 0; protected = 0 }
    in_fn && /^}/ { in_fn = 0; in_missing = 0; protected = 0 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_role_ability.c"; then
    fail "C MIR-active ability tag rendering must fail closed on missing generic/default metadata instead of base-name fallback"
fi
if awk '
    /transpiler_party_slot_method_ability_tag\(TranspilerCtx \*ctx,/ { in_fn = 1 }
    in_fn && /ast_ability_method_(count|method)\(/ { bad = 1 }
    in_fn && /^}/ { in_fn = 0 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_role_ability.c"; then
    fail "C party slot method dispatch must consume ability MIRDeclMethod rows in MIR-active paths"
fi
if awk '
    /transpiler_party_slot_method_ability_tag\(TranspilerCtx \*ctx,/ { in_fn = 1 }
    in_fn && /fallback_tag/ { bad = 1 }
    in_fn && /^}/ { in_fn = 0 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_role_ability.c"; then
    fail "C party slot method dispatch must fail closed instead of falling back to the first ability tag"
fi
require_term "src/codegen/transpiler_role_ability.c" \
    "has no required ability that provides method"
c_ability_decl_body="$(
    awk '
        /emit_ability_decl\(ASTNode \*node, TranspilerCtx \*ctx\)/ { in_body = 1 }
        in_body { print }
    ' "$ROOT_DIR/src/codegen/transpiler_domain_ability_emit.c"
)"
for term in \
    "TranspilerAbilityMethodView methods" \
    "transpiler_ability_method_view_from_decl(ctx, name)" \
    "transpiler_require_ability_method_view_rows(ctx, &methods, name)" \
    "transpiler_ability_method_view_metadata(&methods, i)" \
    "transpiler_mir_decl_method_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES" \
    "transpiler_mir_decl_method_return_type_name(method_meta)" \
    "transpiler_mir_decl_method_param_type_name(method_meta, j)" \
    "transpiler_require_type_name_c_type_copy("; do
    grep -Fq "$term" <<<"$c_ability_decl_body" ||
        fail "C ability typedef emission must consume ability MIRDeclMethod metadata: missing $term"
done
if grep -Eq 'ast_ability_method_(count|method)\(' <<<"$c_ability_decl_body"; then
    fail "C ability typedef emission must not reopen AST ability method arrays"
fi
if grep -RInE 'ast_compat_ability|ast_compat_count|transpiler_ability_method_view_compat_method|ast_ability_method_(count|method)\(' \
    "$ROOT_DIR/src/codegen/transpiler_domain_ability_emit.c" >/dev/null; then
    fail "C ability typedef emission must not keep AST compatibility state"
fi
require_term "src/codegen/transpiler_domain_ability_emit.c" \
    "transpiler_active_decl_header_of_type("
require_term "src/codegen/transpiler_domain_ability_emit.c" \
    "ctx, AST_ABILITY_DECL, ability_name"
require_term "src/codegen/transpiler_domain_ability_emit.c" \
    "mir_decl_header_method_count(view.decl_header)"
require_term "src/codegen/transpiler_domain_ability_emit.c" \
    "mir_decl_header_generic_param_count(methods.decl_header)"
for term in \
    "transpiler_emit_mir_ability_ref_vtable_decl(" \
    "mir_decl_header_method_count(ability_header)" \
    "mir_decl_header_method(ability_header, i)" \
    "transpiler_mir_decl_method_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES" \
    "transpiler_mir_decl_method_return_type_name(method_meta)" \
    "transpiler_mir_decl_method_param_type_name(method_meta, j)" \
    "mir_decl_generic_param_default_type_name(formal)" \
    "mir_decl_generic_param_constraint_type_name(formal)"; do
    require_term "src/codegen/transpiler_domain_role_ability_mir_emit.c" "$term"
done
if grep -Eq 'ast_ability_method_(count|method)\(|ast_func_param_count\(|ast_func_param\(|ast_func_return_type\(' \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_mir_emit.c"; then
    fail "C generic ability vtable specialization must consume MIRDeclMethod metadata, not AST ability method arrays"
fi
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "llvm_render_mir_ability_formal_fallback"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "mir_decl_generic_param_default_type_name(formal)"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "mir_decl_generic_param_constraint_type_name(formal)"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "llvm_find_decl_header_in_context_of_type("
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "ctx, AST_ABILITY_DECL, base_name"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "mir_decl_header_generic_param_count(ability_header)"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "ability_decl = !mir_active"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "MIR-only LLVM path missing ability declaration header for ability tag"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "MIR-only LLVM path missing generic ability argument metadata"
if awk '
    /llvm_render_mir_ability_ref_vtable_tag\(LLVMGenCtx \*ctx,/ { in_fn = 1 }
    in_fn && /if \(arg == NULL\)/ { in_missing = 1 }
    in_missing && /return llvm_keep_ability_tag\(ctx, base_name\);/ { bad = 1 }
    in_missing && /if \(mir_active\)/ { protected = 1 }
    in_missing && protected && /return NULL;/ { in_missing = 0; protected = 0 }
    in_fn && /^}/ { in_fn = 0; in_missing = 0; protected = 0 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_domain_role_lookup.c"; then
    fail "LLVM MIR-active ability tag rendering must fail closed on missing generic/default metadata instead of base-name fallback"
fi
if grep -RIn "transpiler_hosted_role_slot_view_required_ability_type_name(" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_role_ability.c" >/dev/null; then
    fail "C party role-slot ability consumers must read MIRAbilityRef, not compatibility type-name strings"
fi
if grep -RInE 'mir_decl_generic_param_(default_type|constraint)\(formal\)' \
    "$ROOT_DIR/src/codegen/transpiler_role_ability.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_lookup.c" >/dev/null; then
    fail "MIR ability generic fallbacks must consume MIR type-name metadata, not AST generic defaults/constraints"
fi
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
for term in \
    "mir_decl_header_enum_variant_count" \
    "mir_decl_header_enum_variant(" \
    "mir_decl_enum_variant_name" \
    "mir_decl_enum_variant_param_count" \
    "mir_decl_enum_variant_param_type_name"; do
    require_term "src/compiler/mir_decl_headers.h" "$term"
    require_term "src/compiler/mir_decl_header_access.c" "$term"
done
if grep -RInE 'mir_decl_header_variant(_count)?\(|mir_decl_variant_(name|param_count|param_type_name)\(' \
        "$ROOT_DIR/src" >/dev/null 2>&1; then
    grep -RInE 'mir_decl_header_variant(_count)?\(|mir_decl_variant_(name|param_count|param_type_name)\(' \
        "$ROOT_DIR/src" >&2 || true
    fail "MIR enum variant metadata must use the single enum-specific accessor family"
fi
require_term "src/compiler/mir_decl_header_variants.c" \
    "meta[i].param_type_names[p] ="
require_term "src/compiler/mir_decl_header_variants.c" \
    "mir_capture_type_name(pt, NULL)"
require_term "src/compiler/mir_lifecycle.c" \
    "variant->param_type_names[p]"
require_term "src/compiler/mir_decl_header_validate.c" \
    "enum variant metadata count"
require_term "src/compiler/mir_decl_header_validate.c" \
    "enum variant[%zu] payload[%zu] has no type metadata"
for rel in \
    "src/codegen/transpiler_enum.c" \
    "src/codegen/transpiler_enum_decl_emit.c" \
    "src/codegen/transpiler_match_bindings.c"; do
    require_term "$rel" "transpiler_active_decl_header_of_type("
    require_term "$rel" "mir_decl_header_enum_variant_count("
    require_term "$rel" "mir_decl_enum_variant_param_count("
    require_term "$rel" "if (enum_header == NULL)"
done
for rel in \
    "src/codegen/transpiler_enum_decl_emit.c" \
    "src/codegen/transpiler_match_bindings.c"; do
    require_term "$rel" "mir_decl_enum_variant_param_type_name("
    require_term "$rel" "transpiler_require_type_name_c_type_copy(ctx"
done
if grep -Fq "ASTNode ***binding_types_out" \
        "$ROOT_DIR/src/codegen/transpiler_match_bindings.h"; then
    fail "C enum match destructor must expose payload type names, not AST type nodes"
fi
for rel in \
    "src/codegen/llvm_register.c" \
    "src/codegen/llvm_expr_identifier_slot_helpers.c"; do
    require_term "$rel" "llvm_find_decl_header_in_context_of_type("
    require_term "$rel" "mir_decl_header_enum_variant_count("
    require_term "$rel" "mir_decl_enum_variant_param_count("
    require_term "$rel" "if (enum_header == NULL)"
done
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "llvm_find_decl_header_in_context_of_type("
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "mir_decl_header_enum_variant("
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "mir_decl_enum_variant_param_count("
require_term "src/codegen/llvm_expr_constructor_calls.c" \
    "if (enum_header == NULL)"
require_term "src/codegen/llvm_register.c" \
    "mir_decl_enum_variant_param_type_name("
if grep -Fq "ast_enum_name(stmt)" "$ROOT_DIR/src/codegen/llvm_expr_common.c"; then
    fail "LLVM enum declaration lookup must consume llvm_decl_node_name"
fi
if grep -Fq "llvm_active_inventory(ctx, AST_ENUM_DECL" \
        "$ROOT_DIR/src/codegen/llvm_expr_common.c"; then
    fail "LLVM enum lookup must not rescan active inventory after owner lookup"
fi
if grep -RInF "llvm_find_enum_decl(" "$ROOT_DIR/src/codegen" >/dev/null 2>&1; then
    grep -RInF "llvm_find_enum_decl(" "$ROOT_DIR/src/codegen" >&2 || true
    fail "LLVM enum constructors must consume MIR enum headers instead of recovering enum source declarations"
fi
require_term "src/codegen/llvm_intent_effect.c" "llvm_decl_node_name(zone_decl)"
if grep -Fq "ast_zone_name(zone)" "$ROOT_DIR/src/codegen/llvm_intent_effect.c"; then
    fail "LLVM intent effect zone lookup must consume llvm_decl_node_name"
fi
require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_decl_node_name(effect_decl)"
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
    "return transpiler_find_named_decl_local(ctx, AST_CLASS_DECL, name);"
require_term "src/codegen/transpiler_decl_lookup.c" \
    "ASTNode *decl = transpiler_find_projection_nominal_decl_local("
if grep -A4 -F "transpiler_find_projection_nominal_decl_local(TranspilerCtx *ctx" \
        "$ROOT_DIR/src/codegen/transpiler_decl_lookup.c" \
        | grep -Fq "transpiler_find_decl_in_inventory_local(ctx, AST_CLASS_DECL"; then
    fail "C projection nominal lookup must prefer typed MIR declaration headers"
fi
if ! awk '
    /is_subject_type_name\(TranspilerCtx \*ctx/ { in_fn = 1 }
    in_fn && /transpiler_active_decl_header_of_type\(/ { saw_header = NR }
    in_fn && /find_subject_host_decl\(ctx, type_name\)/ { saw_ast = NR }
    in_fn && /^}/ {
        if (saw_header > 0 && saw_ast > saw_header) ok = 1
        in_fn = 0
    }
    END { exit ok ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_projection.c"; then
    fail "C subject type classification must consume MIR declaration headers before AST nominal fallback"
fi
require_term "src/codegen/transpiler_projection_emit.c" \
    "resolve_projection_source_path_by_name(TranspilerCtx *ctx"
require_term "src/codegen/transpiler_projection_emit.c" \
    "projection_class_field_view_by_name("
require_term "src/codegen/transpiler_projection_emit.c" \
    "mir_decl_header_nominal_kind_or("
require_term "src/codegen/transpiler_projection_emit.c" \
    "emit_projection_literal_by_name("
require_term "src/codegen/transpiler_projection_field_path.c" \
    "transpiler_find_projection_nominal_decl_local("
require_term "src/codegen/transpiler_projection_method_invalidation.c" \
    "transpiler_find_projection_nominal_decl_local("
require_term "src/codegen/transpiler_expr_projection_builtin.c" \
    "transpiler_projection_type_is_struct_like(ctx, target_name)"
require_term "src/codegen/transpiler_expr_projection_builtin.c" \
    "emit_projection_literal_by_name("
if grep -Fq "target_decl = transpiler_find_projection_nominal_decl_local(ctx, target_name)" \
        "$ROOT_DIR/src/codegen/transpiler_expr_projection_builtin.c"; then
    fail "C ToTObject lowering must consume projection type/header facts instead of recovering target source declarations"
fi
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
    "emit_projection_literal_by_zone_refresh_metadata("
if grep -Fq "target_decl = transpiler_find_projection_nominal_decl_local(" \
        "$ROOT_DIR/src/codegen/transpiler_domain_provenance_emit.c"; then
    fail "C domain provenance projection refresh must consume projection type/header facts instead of recovering target source declarations"
fi
for rel in \
    "src/codegen/transpiler_expr_dispatch_emit.c"; do
    require_term "$rel" "emit_projection_literal_by_name("
done
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
    "TranspilerMIRDeclMethodRequirement" \
    "TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES" \
    "transpiler_mir_decl_method_metadata_complete_for" \
    "transpiler_mir_decl_method_param_count" \
    "transpiler_mir_decl_method_param" \
    "transpiler_mir_decl_method_param_type_name" \
    "transpiler_mir_decl_method_return_type" \
    "transpiler_mir_decl_method_return_type_name" \
    "transpiler_mir_decl_method_is_action_like" \
    "transpiler_mir_decl_method_projection_write_count" \
    "transpiler_mir_decl_method_projection_write_root_name" \
    "transpiler_mir_decl_method_projection_write_member_name" \
    "transpiler_mir_decl_method_projection_call_count" \
    "transpiler_mir_decl_method_projection_call_receiver_name" \
    "transpiler_mir_decl_method_projection_call_method_name"; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
done
for term in \
    "projection_write_root_names" \
    "projection_write_member_names" \
    "projection_write_count" \
    "projection_call_receiver_names" \
    "projection_call_method_names" \
    "projection_call_count"; do
    require_term "src/compiler/mir_decl.h" "$term"
done
require_term "Makefile" \
    '$(COMPILER_DIR)/mir_decl_method_projection.c'
require_term "Makefile" \
    '$(BUILD_DIR)/compiler/mir_decl_method_projection.o'
require_term "src/compiler/mir_decl_headers.c" \
    "mir_decl_method_projection_metadata_capture"
require_term "src/compiler/mir_decl_method_projection.h" \
    "mir_decl_method_projection_metadata_clear"
require_term "src/compiler/mir_decl_method_projection.c" \
    "mir_decl_method_projection_append_write"
require_term "src/compiler/mir_decl_method_projection.c" \
    "mir_decl_method_projection_append_call"
if grep -Fq "mir_decl_method_projection_append_write" \
        "$ROOT_DIR/src/compiler/mir_decl_headers.c"; then
    fail "MIR declaration headers must not own method projection fact capture"
fi
for term in \
    "mir_decl_method_projection_write_count" \
    "mir_decl_method_projection_write_root_name" \
    "mir_decl_method_projection_write_member_name" \
    "mir_decl_method_projection_call_count" \
    "mir_decl_method_projection_call_receiver_name" \
    "mir_decl_method_projection_call_method_name"; do
    require_term "src/compiler/mir_decl_headers.h" "$term"
    require_term "src/compiler/mir_decl_header_access.c" "$term"
    require_term "src/codegen/transpiler_decl_method_view.c" "$term"
done
for term in \
    "method_projection_write_field_name(" \
    "transpiler_mir_decl_method_projection_write_count(method_meta)" \
    "transpiler_mir_decl_method_projection_call_count(method_meta)" \
    "append_overlay_method_projection_invalidations_from_metadata" \
    "MIR-only C path missing projection invalidation method metadata"; do
    require_term "src/codegen/transpiler_projection_method_invalidation.c" "$term"
done
require_term "src/codegen/transpiler_projection_field_path.c" \
    "method_projection_write_field_name(TranspilerCtx *ctx"
if grep -Fq "transpiler_mir_decl_method_body_decl(ctx, method_meta)" \
        "$ROOT_DIR/src/codegen/transpiler_projection_method_invalidation.c"; then
    fail "C projection method invalidation must consume MIRDeclMethod projection facts instead of recovering method source declarations"
fi
for term in \
    "emit_hosted_method_forward_decl_from_metadata" \
    "method_meta == NULL" \
    "transpiler_mir_decl_method_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES" \
    "transpiler_mir_decl_method_param_count(method_meta)" \
    "transpiler_mir_decl_method_param_type_name(method_meta, j)" \
    "transpiler_mir_decl_method_return_type(method_meta)" \
    "transpiler_mir_decl_method_return_type_name(method_meta)" \
    "transpiler_mir_decl_method_param(method_meta, j)" \
    "transpiler_active_has_mir(ctx)" \
    "MIR-only C path missing hosted method forward metadata"; do
    require_term "src/codegen/transpiler_func_forward_metadata.c" "$term"
done
require_each_following_term "src/codegen/transpiler_func_forward_metadata.c" \
    "if (method_meta == NULL) {" \
    "transpiler_active_has_mir(ctx)" \
    4
if grep -Fq "host_name == NULL || method == NULL || buf == NULL || ctx == NULL" \
        "$ROOT_DIR/src/codegen/transpiler_func_forward_metadata.c"; then
    fail "C hosted method forward declarations must not require source AST when MIRDeclMethod metadata exists"
fi
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "ensure_collection_specializations_from_mir_routine_to(ctx, ctx->out"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_mir_decl_method_routine(ctx, method_meta)"
if grep -Fq "transpiler_mir_decl_method_body_decl(ctx, method_meta)" \
    "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c"; then
    fail "C class method specialization scan must consume linked MIRRoutine facts, not recover method source AST"
fi
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "emit_func_decl_from_mir_named(NULL, mir_method, emitted_name"
if grep -Fq "transpiler_hosted_method_view_source_ast(&method_view, i)" \
    "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c"; then
    fail "C class specialization scan must use linked MIRRoutine provenance instead of method source AST back-pointers"
fi
for rel in \
    "src/codegen/transpiler_class_decl_emit.c" \
    "src/codegen/transpiler_enum_decl_emit.c"; do
    require_term "$rel" "transpiler_hosted_method_view_from_decl(ctx"
    require_term "$rel" "transpiler_mir_decl_method_routine(ctx, method_meta)"
    require_term "$rel" "method_meta == NULL"
    require_term "$rel" "MIR-only C path missing hosted method forward metadata row"
    require_term "$rel" "emit_hosted_method_forward_decl_from_metadata"
done
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "ensure_collection_specializations_from_mir_routine_to(ctx, ctx->out"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "emit_func_decl_from_mir_named(NULL, mir_method, emitted_name"
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "emit_func_decl_from_mir_named(NULL, mir_method, emitted_name"
if grep -Fq "transpiler_mir_decl_method_body_decl(ctx, method_meta)" \
        "$ROOT_DIR/src/codegen/transpiler_enum_decl_emit.c"; then
    fail "C enum hosted-method bodies must pass linked MIRRoutine directly instead of recovering source AST"
fi
if grep -Fq "transpiler_hosted_method_view_source_ast(&method_view, i)" \
    "$ROOT_DIR/src/codegen/transpiler_enum_decl_emit.c"; then
    fail "C enum hosted-method bodies must use linked MIRRoutine provenance instead of method source AST back-pointers"
fi
for rel in \
    "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "src/codegen/transpiler_domain_nominal_emit.c" \
    "src/codegen/transpiler_roster_decl_emit.c" \
    "src/codegen/transpiler_relation_effect_emit.c" \
    "src/codegen/transpiler_world_select_event_emit.c" \
    "src/codegen/transpiler_zone_methods_emit.c"; do
    require_term "$rel" "MIR-only C path missing hosted method forward metadata row"
done
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_hosted_method_view_from_decl(ctx, base_class_name"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "transpiler_mir_decl_method_routine(ctx, method_meta)"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "emit_func_decl_from_mir_named(NULL, mir_method, emitted_name"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "method_meta == NULL"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "emit_hosted_method_forward_decl_from_metadata"
if grep -Fq "transpiler_mir_decl_method_body_decl(ctx, method_meta)" \
        "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"; then
    fail "C generic class hosted-method bodies must pass linked MIRRoutine directly instead of recovering source AST"
fi
if grep -Fq "transpiler_hosted_method_view_source_ast(&method_view, i)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"; then
    fail "C generic class hosted-method bodies must use linked MIRRoutine provenance instead of method source AST back-pointers"
fi
if grep -RInE 'transpiler_(hosted_method_view_source_ast|mir_decl_method_source_ast)\(' \
    "$ROOT_DIR/src/codegen/transpiler_decl_lookup.h" \
    "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"; then
    fail "C method provenance aliases must stay retired; use MIRDeclMethod metadata or the compiler source_ast accessor at the compatibility boundary"
fi
if grep -Eq 'class_decl->data\.class_decl\.methods\[[^]]+\]' \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"; then
    fail "generic class specialization must consume TranspilerHostedMethodView, not index AST method arrays"
fi
for term in \
    "TranspilerHostedMethodView" \
    "TranspilerHostedFieldView" \
    "ast_compat_count" \
    "transpiler_hosted_method_view(" \
    "transpiler_hosted_method_view_metadata(" \
    "transpiler_find_host_method_metadata_in_context(" \
    "transpiler_mir_decl_method_name(" \
    "transpiler_mir_decl_method_is_async(" \
    "transpiler_mir_decl_method_is_action_like(" \
    "transpiler_mir_decl_method_within_zone(" \
    "transpiler_mir_decl_method_causes_effect(" \
    "transpiler_mir_decl_method_routine(" \
    "transpiler_hosted_method_view_from_decl(" \
    "transpiler_hosted_method_view_missing_mir_metadata(" \
    "transpiler_hosted_class_field_view_from_decl(" \
    "transpiler_hosted_field_view_metadata(" \
    "transpiler_hosted_field_view_name(" \
    "transpiler_hosted_field_view_type(" \
    "transpiler_hosted_field_view_missing_mir_metadata("; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
done
for term in \
    "transpiler_generic_class_spec_base_decl(" \
    "transpiler_decl_name_local(base_decl)" \
    "transpiler_active_host_decl_header(ctx, base_name)"; do
    require_term "src/codegen/transpiler_decl_method_view.c" "$term"
done
if grep -Fq "transpiler_generic_class_spec_base_decl(ctx," \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"; then
    fail "C member-call emitter must not manually peel generic specializations; consume transpiler_find_host_method_metadata_in_context"
fi
# MIR-only: the C compat-method accessor and all its call sites (class/zone
# specialization emit, host-method lookup) are retired -- method shape is owned
# by MIR metadata and these paths fail closed.
if grep -RInE 'method_view(\.|->)ast_compat_methods\[[^]]+\]' \
        "$ROOT_DIR/src/codegen"/transpiler_*.c \
        "$ROOT_DIR/src/codegen"/transpiler_*.h \
        | grep -v "src/codegen/transpiler_decl_method_view.c"; then
    fail "C consumers must not index hosted method compatibility arrays directly"
fi
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
    "view->requires_mir_metadata"; do
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
    require_term "$rel" "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
    require_term "$rel" "emit_hosted_method_forward_decl_from_metadata"
done
if grep -Fq "transpiler_hosted_method_view_source_ast(&method_view, i)" \
    "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.c"; then
    fail "C world hosted-method forward declarations must consume MIRDeclMethod metadata without source AST back-pointers"
fi
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
    "emit_hosted_method_forward_decl_from_metadata"
if grep -Fq "transpiler_hosted_method_view_source_ast(method_view, i)" \
    "$ROOT_DIR/src/codegen/transpiler_zone_methods_emit.c"; then
    fail "C zone hosted-method forward declarations must consume MIRDeclMethod metadata without source AST back-pointers"
fi
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
require_term "src/codegen/transpiler_operator.c" \
    "MIR-only C path missing role operator method metadata for role"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "mir_decl_header_role_impl_method("
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path missing role operator method metadata for role"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_find_role_operator_method_metadata("
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
if grep -Fq "transpiler_mir_decl_method_body_decl(ctx, method_meta)" \
        "$ROOT_DIR/src/codegen/transpiler_hosted_method_body_emit.c"; then
    fail "hosted method body emission must pass linked MIRRoutine directly instead of recovering source AST"
fi
if grep -Eq 'emit_hosted_methods_from_mir_or_error_local\([^)]*ASTNode \*\*methods|emit_hosted_methods_from_mir_or_error_local\([^)]*size_t method_count' \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.h"; then
    fail "hosted method body emission must accept TranspilerHostedMethodView, not AST method arrays"
fi
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_hosted_method_view_from_decl(ctx, name"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "ensure_collection_specializations_from_mir_routine_to(ctx, ctx->out"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for class"
require_term "src/codegen/transpiler_class_decl_emit.c" \
    "MIR-only C path missing method body metadata row for class"
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_hosted_method_view_from_decl(ctx, ename"
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(&method_view)"
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "emit_func_decl_from_mir_named(NULL, mir_method, emitted_name"
if grep -Fq "transpiler_mir_decl_method_body_decl(ctx, method_meta)" \
        "$ROOT_DIR/src/codegen/transpiler_enum_decl_emit.c"; then
    fail "C enum hosted-method bodies must pass linked MIRRoutine directly instead of recovering source AST"
fi
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_set_mir_inventory_missing("
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for enum"
require_term "src/codegen/transpiler_enum_decl_emit.c" \
    "MIR-only C path missing method body metadata row for enum"
require_term "src/codegen/transpiler_generic_class_specialization_emit.c" \
    "MIR-only C path missing method body metadata row for generic class"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(method_view)"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_require_hosted_method_view_rows("
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "MIR-only C path has invalid method declaration metadata row for"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "MIR-only C path missing method body metadata row for"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "MIR-only C path missing method name metadata for"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "emit_func_decl_from_mir_named(NULL, mir_method, emitted_name"
require_term "src/codegen/transpiler_hosted_method_body_emit.c" \
    "transpiler_set_mir_inventory_missing("
transpiler_method_row_hits="$(
    grep -RIn "transpiler_hosted_method_view_missing_mir_method_row(" \
        "$ROOT_DIR/src/codegen" \
        | grep -v "src/codegen/transpiler_decl_lookup.h" \
        | grep -v "src/codegen/transpiler_decl_method_view.c" || true
)"
if [[ -n "$transpiler_method_row_hits" ]]; then
    fail "C hosted-method emission must let TranspilerHostedMethodView own row validation:
$transpiler_method_row_hits"
fi
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
require_term "src/codegen/transpiler_mir_emit_state.c" \
    "MIR-only C path attempted AST hosted method body emission"
require_term "src/codegen/transpiler_mir_emit_state.c" \
    "transpiler_active_has_mir(ctx)"
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
if grep -RIn "llvm_emit_func_decl" "$ROOT_DIR/src/codegen"; then
    fail "LLVM function bodies must emit through MIR routines, not the removed AST function-body emitter"
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
    "llvm_find_decl_header_in_context_of_type(ctx, host_types[i], name)" \
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
    "llvm_mir_decl_method_param(method_meta, pk)" \
    "bool allow_ast_compat = false" \
    "allow_ast_compat ? ast_func_param_count(method_decl) : 0" \
    "allow_ast_compat ? ast_func_param(method_decl, pk) : NULL"; do
    require_term "src/codegen/llvm_member_call_support.c" "$term"
done
for term in \
    "llvm_find_host_method_metadata_in_context(ctx," \
    "llvm_member_call_adjust_pointer_self_arg("; do
    require_term "src/codegen/llvm_member_call_emit.c" "$term"
done
if grep -Fq "llvm_mir_decl_method_source_ast(method_meta)" \
    "$ROOT_DIR/src/codegen/llvm_member_call_emit.c"; then
    fail "LLVM member-call emit must not recover method AST back-pointers from MIR metadata"
fi
if grep -Eq 'ast_func_param_count\(method_decl\)|ast_func_param\(method_decl' \
    "$ROOT_DIR/src/codegen/llvm_member_call_emit.c"; then
    fail "LLVM member-call emit must consume MIRDeclMethod metadata through llvm_member_call_adjust_pointer_self_arg"
fi
for term in \
    "llvm_find_host_method_metadata_in_context(ctx," \
    "llvm_mir_decl_method_is_async(method_meta)" \
    "llvm_mir_decl_method_within_zone(method_meta)" \
    "llvm_mir_decl_method_causes_effect(method_meta)" \
    "llvm_mir_decl_method_is_action_like(method_meta)" \
    "llvm_active_has_mir(ctx)" \
    "MIR-only LLVM path missing zone action method metadata" \
    "MIR-only LLVM path missing zone action within-zone metadata" \
    "if (method_meta == NULL)"; do
    require_term "src/codegen/llvm_stmt_zone_action.c" "$term"
done
require_each_following_term "src/codegen/llvm_stmt_zone_action.c" \
    "if (method_meta == NULL) {" \
    "llvm_active_has_mir(ctx)" \
    6
if grep -Eq 'ast_func_(within_zone|causes_effect|is_action)\(method_decl\)|llvm_find_host_method_decl_in_context\(ctx,' \
        "$ROOT_DIR/src/codegen/llvm_stmt_zone_action.c"; then
    fail "LLVM zone action effect sync must not recover method AST contracts"
fi
for term in \
    "const MIRDeclMethod *method_meta" \
    "llvm_mir_decl_method_is_async(method_meta)" \
    "llvm_mir_decl_method_within_zone(method_meta)" \
    "llvm_mir_decl_method_causes_effect(method_meta)" \
    "llvm_mir_decl_method_is_action_like(method_meta)" \
    "llvm_active_has_mir(ctx)" \
    "MIR-only LLVM path missing world effect sync method metadata" \
    "MIR-only LLVM path missing world effect sync within-zone metadata" \
    "if (method_meta == NULL)"; do
    require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" "$term"
done
require_each_following_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "if (method_meta == NULL) {" \
    "llvm_active_has_mir(ctx)" \
    6
if grep -Eq 'ast_func_(within_zone|causes_effect|is_action)\(method_decl\)' \
        "$ROOT_DIR/src/codegen/llvm_expr_call_methods_world_effect_sync.c"; then
    fail "LLVM world effect sync must not recover method AST contracts"
fi
for term in \
    "llvm_find_host_method_metadata_in_context(ctx, host_name, callee_name)" \
    "llvm_mir_decl_method_param_count(method_meta)" \
    "llvm_mir_decl_method_param(method_meta" \
    "llvm_set_mir_inventory_missing(ctx" \
    "MIR-only LLVM path missing hosted self-call method metadata" \
    "llvm_callable_decl_exists(ctx, callee_name)" \
    "llvm_hosted_self_logical_param("; do
    require_term "src/codegen/llvm_expr_call_hosted.c" "$term"
done
if grep -Fq "llvm_find_callable_decl(ctx, callee_name)" \
        "$ROOT_DIR/src/codegen/llvm_expr_call_hosted.c"; then
    fail "LLVM hosted self-call callable guard must not recover callable source declarations"
fi
if grep -Fq "llvm_mir_decl_method_source_ast(method_meta)" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_hosted.c"; then
    fail "LLVM hosted self-call emit must not recover method AST back-pointers from MIR metadata"
fi
require_term "src/codegen/llvm_member_call_emit.c" \
    "obj_node, method_meta)"
for term in \
    "llvm_set_mir_inventory_missing(ctx" \
    "MIR-only LLVM path missing member-call method metadata"; do
    require_term "src/codegen/llvm_member_call_emit.c" "$term"
done
for term in \
    "transpiler_find_host_method_metadata_in_context(ctx," \
    "transpiler_mir_decl_method_is_async(method_meta)" \
    "transpiler_mir_decl_method_within_zone(method_meta)" \
    "transpiler_mir_decl_method_causes_effect(method_meta)" \
    "transpiler_mir_decl_method_is_action_like(method_meta)" \
    "transpiler_active_has_mir(ctx)" \
    "MIR-only C path missing zone action method metadata" \
    "MIR-only C path missing world effect sync method metadata" \
    "MIR-only C path missing zone action within-zone metadata" \
    "MIR-only C path missing world effect sync within-zone metadata" \
    "if (method_meta == NULL)"; do
    require_term "src/codegen/transpiler_projection_sync.c" "$term"
done
require_each_following_term "src/codegen/transpiler_projection_sync.c" \
    "if (method_meta == NULL) {" \
    "transpiler_active_has_mir(ctx)" \
    6
if grep -Eq 'ast_func_(within_zone|causes_effect|is_action)\(method_decl\)|transpiler_find_subject_host_method_decl\(ctx,' \
        "$ROOT_DIR/src/codegen/transpiler_projection_sync.c"; then
    fail "C zone/world effect sync must not recover method AST contracts"
fi
require_term "src/codegen/transpiler_expr_call_member_emit.c" \
    "ctx, obj, method_meta)"
for term in \
    "transpiler_mir_decl_method_metadata_complete_for(ctx" \
    "TRANSPILER_MIR_DECL_METHOD_REQUIRE_PARAM_TYPE_NAMES" \
    "TRANSPILER_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME" \
    "transpiler_mir_decl_method_param_count(" \
    "transpiler_mir_decl_method_param(" \
    "transpiler_mir_decl_method_param_type_name(" \
    "transpiler_mir_decl_method_return_type(" \
    "transpiler_mir_decl_method_return_type_name(" \
    "transpiler_set_mir_inventory_missing(ctx" \
    "MIR-only C path missing member-call method metadata"; do
    require_term "src/codegen/transpiler_expr_call_member_emit.c" "$term"
done
if grep -Fq "transpiler_mir_decl_method_source_ast(method_meta)" \
        "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"; then
    fail "C member-call emit must not eagerly recover method AST back-pointers from MIR metadata"
fi
if grep -Fq "AST_EVENT_HANDLER_TYPE" \
        "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"; then
    fail "C member-call emission must let TranspilerHostedMethodView own type-name completeness checks"
fi
# MIR-only: the non-MIR AST method_decl branches in C member-call emission are
# retired (method shape owned by MIR metadata); only the fail-closed MIR path
# remains.
for rel in \
    "src/codegen/transpiler_expr_call_type_infer.c" \
    "src/codegen/transpiler_mir_local_type_lookup.c" \
    "src/codegen/transpiler_nominal.c"; do
    require_term "$rel" "transpiler_find_host_method_metadata_in_context("
    require_term "$rel" "transpiler_mir_decl_method_metadata_complete_for("
    require_term "$rel" \
        "TRANSPILER_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME"
    require_term "$rel" "transpiler_mir_decl_method_return_type("
    require_term "$rel" "transpiler_mir_decl_method_return_type_name("
done
# MIR-only: the non-MIR AST host-method return-type fallback in
# type_infer / mir_local_type_lookup / nominal is retired -- these consumers now
# rely solely on MIR method metadata (asserted above) and fail closed when it is
# absent, so the old !transpiler_active_has_mir / current_host_method_decl /
# find_nominal_host_method_decl assertions are removed.
for term in \
    "transpiler_mir_routine_signature_metadata_complete_for(" \
    "TRANSPILER_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME" \
    "MIR-only C path missing nominal function-call signature metadata" \
    "MIR-only C path missing nominal function-call return type-name metadata"; do
    require_term "src/codegen/transpiler_nominal.c" "$term"
done
if grep -Fq "if (!transpiler_mir_routine_has_signature" \
        "$ROOT_DIR/src/codegen/transpiler_nominal.c"; then
    fail "C nominal function-call type inference must use transpiler_mir_signature for missing-signature diagnostics"
fi
require_term "src/codegen/llvm_stmt_let_helpers.c" \
    "llvm_find_host_method_metadata_in_context("
require_term "src/codegen/llvm_stmt_let_helpers.c" \
    "llvm_mir_decl_method_metadata_complete_for("
require_term "src/codegen/llvm_stmt_let_helpers.c" \
    "LLVM_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME"
require_term "src/codegen/llvm_stmt_let_helpers.c" \
    "llvm_mir_decl_method_return_type("
require_term "src/codegen/llvm_stmt_let_helpers.c" \
    "llvm_mir_decl_method_return_type_name("
require_term "src/codegen/llvm_stmt_let_helpers.c" \
    "method_return_type == NULL && method_meta == NULL"
require_term "src/codegen/llvm_stmt_let_helpers.c" \
    "llvm_active_has_mir(ctx)"
require_term "src/codegen/llvm_stmt_let_helpers.c" \
    "MIR-only LLVM path missing let method return metadata"
require_term "src/codegen/llvm_stmt_type_infer_call.c" \
    "llvm_find_host_method_metadata_in_context("
require_term "src/codegen/llvm_stmt_type_infer_call.c" \
    "llvm_mir_decl_method_metadata_complete_for("
require_term "src/codegen/llvm_stmt_type_infer_call.c" \
    "LLVM_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME"
require_term "src/codegen/llvm_stmt_type_infer_call.c" \
    "llvm_mir_decl_method_return_type("
require_term "src/codegen/llvm_stmt_type_infer_call.c" \
    "llvm_mir_decl_method_return_type_name("
require_term "src/codegen/llvm_stmt_type_infer_call.c" \
    "ret_ty == NULL && method_meta == NULL"
require_term "src/codegen/llvm_stmt_type_infer_call.c" \
    "llvm_active_has_mir(ctx)"
require_term "src/codegen/llvm_stmt_type_infer_call.c" \
    "MIR-only LLVM path missing method return metadata"
require_each_following_term "src/codegen/llvm_stmt_type_infer_call.c" \
    "llvm_stmt_infer_builtin_return_type(ctx, callee)" \
    "llvm_current_host_class_name(ctx)" \
    24
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
    "llvm_mir_decl_method_metadata_complete_for(ctx" \
    "LLVM_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES" \
    "llvm_mir_decl_method_param_count(method_meta)" \
    "llvm_mir_decl_method_param_type_name(method_meta, k)" \
    "llvm_mir_decl_method_return_type(method_meta)" \
    "llvm_mir_decl_method_return_type_name(method_meta)" \
    "llvm_mir_decl_method_is_action_like(method_meta)" \
    "pergyra_type_to_llvm(ctx, param_type_name)" \
    "pergyra_type_to_llvm(ctx, return_type_name)" \
    "llvm_hosted_method_view_from_decl(ctx, enum_name, stmt)" \
    "llvm_hosted_method_view_from_decl(ctx, cls_name, stmt)" \
    "llvm_hosted_method_view_missing_mir_metadata(&enum_method_view)" \
    "llvm_hosted_method_view_missing_mir_metadata(&class_method_view)" \
    "llvm_hosted_method_view_metadata(&enum_method_view, j)" \
    "llvm_hosted_method_view_metadata(&class_method_view, j)" \
    "llvm_require_hosted_method_view_rows(" \
    "llvm_set_mir_inventory_missing(ctx" \
    "MIR-only LLVM path missing enum method declaration metadata" \
    "MIR-only LLVM path missing class method declaration metadata" \
    "MIR-only LLVM path has invalid method declaration metadata row for enum" \
    "MIR-only LLVM path has invalid method declaration metadata row for class" \
    "LLVM payload enum method self type requires registered enum metadata"; do
    require_term "src/codegen/llvm_register.c" "$term"
done
if grep -Fq "llvm_mir_decl_method_source_ast(method_meta)" \
    "$ROOT_DIR/src/codegen/llvm_register.c"; then
    fail "LLVM nominal method registry must consume MIRDeclMethod metadata without source AST back-pointers"
fi
if grep -Fq "return_type->type != AST_EVENT_HANDLER_TYPE" \
        "$ROOT_DIR/src/codegen/llvm_register.c" \
    || grep -Fq "p->type->type != AST_EVENT_HANDLER_TYPE" \
        "$ROOT_DIR/src/codegen/llvm_register.c"; then
    fail "LLVM nominal method registry must use LLVMHostedMethodView completeness owner, not local type-name drift checks"
fi
for term in \
    "llvm_mir_decl_method_metadata_complete_for(ctx" \
    "LLVM_MIR_DECL_METHOD_REQUIRE_PARAM_TYPE_NAMES"; do
    require_term "src/codegen/llvm_expr_call_hosted.c" "$term"
    require_term "src/codegen/llvm_member_call_support.c" "$term"
done
if grep -Fq "AST_EVENT_HANDLER_TYPE" \
        "$ROOT_DIR/src/codegen/llvm_expr_call_hosted.c" \
        "$ROOT_DIR/src/codegen/llvm_member_call_support.c"; then
    fail "LLVM hosted/member-call emission must let LLVMHostedMethodView own type-name completeness checks"
fi
if grep -Fq "LLVMTypeRef self_type = ctx->type_i32" \
    "$ROOT_DIR/src/codegen/llvm_register.c"; then
    fail "LLVM enum method self type must distinguish no-payload ABI from payload enum metadata"
fi
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
if grep -Fq "bool allow_ast_compat" <<<"$domain_method_forward_body"; then
    fail "LLVM domain method forward declarations must not keep AST compatibility fallback state"
fi
if grep -Fq "llvm_hosted_method_view_source_ast(methods, j)" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c"; then
    fail "LLVM domain method forward declarations must consume MIRDeclMethod metadata without source AST back-pointers"
fi
for term in \
    "bool allow_ast_compat"; do
    require_term "src/codegen/llvm_domain_forward_internal.h" "$term"
done
for term in \
    "llvm_mir_decl_method_metadata_complete_for(ctx" \
    "LLVM_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES" \
    "llvm_hosted_method_view_missing_mir_metadata(methods)" \
    "MIR-only LLVM path missing method forward metadata for domain" \
    "MIR-only LLVM path missing method forward metadata row for domain" \
    "MIR-only LLVM path missing method forward name metadata for domain" \
    "llvm_hosted_method_view_metadata(methods, j)" \
    "llvm_require_hosted_method_view_rows(ctx, methods" \
    "MIR-only LLVM path has invalid method forward metadata row for domain" \
    "llvm_domain_method_param_count_metadata_first" \
    "llvm_domain_method_param_metadata_first" \
    "llvm_domain_method_param_type_name_metadata_first" \
    "llvm_domain_method_return_type_metadata_first" \
    "llvm_domain_method_return_type_name_metadata_first" \
    "pergyra_type_to_llvm(ctx, type_name)" \
    "pergyra_type_to_llvm(ctx, return_type_name)"; do
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
if grep -Fq "bool allow_ast_compat" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"; then
    fail "LLVM role method forward declarations must not keep AST compatibility fallback state"
fi
if grep -Fq "llvm_hosted_method_view_source_ast(methods, j)" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"; then
    fail "LLVM role method forward declarations must consume MIRDeclMethod metadata without source AST back-pointers"
fi
for term in \
    "llvm_mir_decl_method_metadata_complete_for(ctx" \
    "LLVM_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES" \
    "llvm_hosted_method_view_missing_mir_metadata(methods)" \
    "MIR-only LLVM path missing method forward metadata for role" \
    "MIR-only LLVM path missing method forward metadata row for role" \
    "llvm_hosted_method_view_metadata(methods, j)" \
    "llvm_require_hosted_method_view_rows(ctx, methods" \
    "MIR-only LLVM path has invalid method forward metadata row for role" \
    "llvm_domain_method_param_count_metadata_first" \
    "llvm_domain_method_param_metadata_first" \
    "llvm_domain_method_param_type_name_metadata_first" \
    "llvm_domain_method_return_type_metadata_first" \
    "llvm_domain_method_return_type_name_metadata_first" \
    "pergyra_type_to_llvm(ctx, param_type_name)" \
    "pergyra_type_to_llvm(ctx, return_type_name)"; do
    grep -Fq "$term" <<<"$role_method_forward_body" ||
        fail "LLVM role method forward declarations must be MIRDeclMethod metadata-first: missing $term"
done
if grep -Eq 'method->data\.func_decl\.(param_count|return_type)' \
    <<<"$role_method_forward_body"; then
    fail "LLVM role method forward declarations must not read AST method param_count/return_type directly"
fi
if grep -Fq "llvm_hosted_method_view_missing_mir_method_row(methods, j)" \
    <<<"$domain_method_forward_body$role_method_forward_body"; then
    fail "LLVM domain/role method forward declarations must let LLVMHostedMethodView own row validation"
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
if grep -Fq "llvm_mir_decl_method_source_ast(method_meta)" \
    <<<"$role_operator_body"; then
    fail "LLVM role operator forward declarations must consume MIRDeclMethod metadata without source AST back-pointers"
fi
for term in \
    "llvm_domain_method_param_count_metadata_first" \
    "llvm_domain_method_param_metadata_first" \
    "llvm_domain_method_return_type_name_metadata_first"; do
    grep -Fq "$term" <<<"$ability_vtable_body" ||
        fail "LLVM ability_vtable_body must route method signature reads through MIR method accessors: missing $term"
done
for term in \
    "llvm_domain_method_param_count_metadata_first" \
    "llvm_domain_method_param_metadata_first" \
    "llvm_domain_method_return_type_metadata_first"; do
    grep -Fq "$term" <<<"$role_operator_body" ||
        fail "LLVM role_operator_body must route method signature reads through the shared method accessors: missing $term"
done
for body_name in ability_vtable_body role_operator_body; do
    body="${!body_name}"
    if grep -Eq 'method->data\.func_decl\.(param_count|return_type)' <<<"$body"; then
        fail "LLVM ${body_name} must not read AST method param_count/return_type directly"
    fi
done
for term in \
    "LLVMAbilityMethodView methods" \
    "llvm_ability_method_view_from_decl(ctx, ab_name)" \
    "llvm_require_ability_method_view_rows(ctx, &methods, ab_name)" \
    "llvm_ability_method_view_metadata(&methods, j)" \
    "method_meta, NULL, false" \
    "method_meta, NULL, k, false" \
    "llvm_mir_decl_method_metadata_complete_for(ctx" \
    "LLVM_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES" \
    "llvm_domain_method_param_type_name_metadata_first" \
    "llvm_domain_method_return_type_name_metadata_first" \
    "pergyra_type_to_llvm(ctx, param_type_name)" \
    "pergyra_type_to_llvm(ctx, return_type_name)"; do
    grep -Fq "$term" <<<"$ability_vtable_body" ||
        fail "LLVM ability vtable must consume ability MIRDeclMethod metadata: missing $term"
done
if grep -Eq 'ast_ability_method_(count|method)\(' <<<"$ability_vtable_body"; then
    fail "LLVM ability vtable emission must not reopen AST ability method arrays"
fi
if grep -RInE 'ast_compat_ability|ast_compat_count|llvm_ability_method_view_compat_method|ast_ability_method_(count|method)\(' \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_ability.c" >/dev/null; then
    fail "LLVM ability vtable emission must not keep AST compatibility state"
fi
require_term "src/codegen/llvm_domain_forward_ability.c" \
    "llvm_find_decl_header_in_context_of_type("
require_term "src/codegen/llvm_domain_forward_ability.c" \
    "ctx, AST_ABILITY_DECL, ability_name"
require_term "src/codegen/llvm_domain_forward_ability.c" \
    "mir_decl_header_method_count(view.decl_header)"
for term in \
    "method_meta, method, false" \
    "method_meta, method, pj, false"; do
    grep -Fq "$term" <<<"$role_operator_body" ||
        fail "LLVM role operator forward declaration must not open AST compatibility fallback: missing $term"
done
for term in \
    "llvm_mir_decl_method_metadata_complete_for(ctx" \
    "LLVM_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES" \
    "llvm_domain_method_param_type_name_metadata_first" \
    "llvm_domain_method_return_type_name_metadata_first" \
    "pergyra_type_to_llvm(ctx, rhs_type_name)" \
    "pergyra_type_to_llvm(ctx, return_type_name)"; do
    grep -Fq "$term" <<<"$role_operator_body" ||
        fail "LLVM role operator forward declaration must consume MIRDeclMethod type-name facts: missing $term"
done
if grep -Fq "AST_EVENT_HANDLER_TYPE" <<<"$role_operator_body"; then
    fail "LLVM role operator forward declaration must let LLVMHostedMethodView own type-name completeness checks"
fi
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
    "llvm_require_hosted_method_view_rows(ctx, &method_view"
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
    "llvm_require_hosted_method_view_rows(ctx, &method_view"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path has invalid method declaration metadata row for role"
require_term "src/codegen/llvm_domain_method_emit.c" \
    "MIR-only LLVM path missing method body metadata row for domain"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path missing method body metadata row for role"
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
    "llvm_emit_func_from_mir(mir_method, ctx)"
if grep -Fq "llvm_hosted_method_view_missing_mir_method_row(&method_view, j)" \
    "$ROOT_DIR/src/codegen/llvm_domain_method_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"; then
    fail "LLVM domain/role method body emission must let LLVMHostedMethodView own row validation"
fi
if awk '
    /llvm_emit_domain_role_method_bodies/ { in_role_method_body = 1 }
    in_role_method_body { print }
    index($0, "for (size_t ii = 0; ii < ast_role_impl_count") { exit }
' \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c" \
    | grep -Fq "MIR-only LLVM path missing registered function for role method"; then
    fail "role method body emission must let llvm_emit_func_from_mir own registered-function validation"
fi
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path missing vtable function for role method"
require_term "src/codegen/transpiler_domain_role_include_emit.c" \
    "ctx != NULL && ctx->backend_error != NULL"
require_term "src/codegen/transpiler_domain_role_include_emit.c" \
    "mir_decl_header_role_include_count(owner_role_header)"
require_term "src/codegen/transpiler_operator.c" \
    "find_role_operator_method_metadata_in_header"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "llvm_find_role_operator_method_metadata_in_header"
require_term "src/codegen/transpiler_domain_role_include_emit.c" \
    "mir_decl_role_include_name(include_meta)"
require_term "src/codegen/transpiler_operator.c" \
    "mir_decl_header_role_include_count(role_header)"
require_term "src/codegen/llvm_domain_role_lookup.c" \
    "mir_decl_header_role_include_count(role_header)"
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
    "mir_decl_header_role_impl("
require_term "src/codegen/llvm_domain_role_emit.c" \
    "mir_decl_role_impl_ability_ref(impl_meta)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "llvm_render_mir_ability_ref_vtable_tag(ctx, mir_ref)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "MIR-only LLVM path missing role vtable ability-ref metadata"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "ensure_mir_ability_ref_vtable_decl(ability_ref_meta, ctx)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "render_mir_ability_ref_vtable_tag_in_ctx("
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "mir_decl_role_impl_method_count(role_impl_meta)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "mir_decl_header_role_impl_method("
require_term "src/codegen/transpiler_domain_role_include_emit.c" \
    "mir_decl_header_role_impl("
require_term "src/codegen/transpiler_domain_role_include_emit.c" \
    "emit_role_vtable_instance(owner_role_name,"
require_term "src/compiler/mir_decl.h" \
    "MIRDeclRoleImpl"
require_term "src/compiler/mir_decl.h" \
    "MIRDeclRoleInclude"
require_term "src/compiler/mir_decl.h" \
    "role_include_metadata"
require_term "src/compiler/mir_decl.h" \
    "method_start_index"
require_term "src/compiler/mir_decl_header_access.c" \
    "mir_decl_header_role_impl_method"
require_term "src/compiler/mir_decl_header_access.c" \
    "mir_decl_header_role_include_count"
require_term "src/compiler/mir_decl_header_access.c" \
    "mir_decl_role_include_name"
require_term "src/compiler/mir_decl_headers.c" \
    "mir_decl_header_set_role_impls"
require_term "src/compiler/mir_decl_headers.c" \
    "mir_decl_header_set_role_includes"
require_term "src/compiler/mir_decl_header_role_validate.c" \
    "mir_validate_decl_role_impl_metadata"
require_term "src/compiler/mir_decl_header_role_validate.c" \
    "mir_validate_decl_role_include_metadata"
require_term "src/compiler/mir_decl_header_role_validate.c" \
    "method span metadata drift"
require_term "src/compiler/mir_decl_header_role_validate.c" \
    "role include metadata count"
require_term "src/tests/mir/test_mir_lowering_part_d.cases.h" \
    "MIR declaration headers preserve role include metadata"
require_term "src/tests/mir/test_mir_lowering_part_d.cases.h" \
    "MIR validator rejects role include metadata drift"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "ast_impl_ability_method(impl, j)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "mir_decl_role_impl_method_count(impl_meta)"
require_term "src/codegen/llvm_domain_role_emit.c" \
    "mir_decl_header_role_impl_method("
if grep -Fq "mir_ability_ref_capture(&mir_ref, ability_ref)" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"; then
    fail "LLVM role impl vtable emission must consume MIR role-impl ability-ref metadata, not recapture AST ability refs"
fi
if grep -Fq "transpiler_find_host_method_metadata_in_context(" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"; then
    fail "C role impl vtable emission must consume MIR role-impl method spans, not name-lookup AST method rows"
fi
if grep -Fq "llvm_find_host_method_metadata_in_context(" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"; then
    fail "LLVM role impl vtable emission must consume MIR role-impl method spans, not name-lookup AST method rows"
fi
if awk '
    /find_role_operator_method_metadata_in_header/ { in_fn = 1 }
    in_fn && /^const MIRDeclMethod \*/ { exit }
    in_fn { print }
' "$ROOT_DIR/src/codegen/transpiler_operator.c" |
    grep -Eq 'ast_role_include_count|ast_role_include\(|ast_include_role_name'; then
    fail "C role operator metadata recursion must consume MIR role include metadata"
fi
if awk '
    /llvm_find_role_operator_method_metadata_in_header/ { in_fn = 1 }
    in_fn && /^const MIRDeclMethod \*/ { exit }
    in_fn { print }
' "$ROOT_DIR/src/codegen/llvm_domain_role_lookup.c" |
    grep -Eq 'ast_role_include_count|ast_role_include\(|ast_include_role_name'; then
    fail "LLVM role operator metadata recursion must consume MIR role include metadata"
fi
require_term "src/codegen/llvm_domain_role_helpers.h" \
    "llvm_party_slot_first_ability_tag"
require_term "src/codegen/llvm_domain_role_helpers.h" \
    "llvm_render_mir_ability_ref_vtable_tag"
require_term "src/codegen/llvm_domain_role_helpers.h" \
    "llvm_lookup_role_vtable_global"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "LLVMHostedRoleSlotView"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_role_slot_view_from_decl"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_role_slot_view_is_dynamic"
require_term "src/codegen/llvm_inventory_decl_lookup.h" \
    "llvm_hosted_role_slot_view_required_ability_ref"
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
    "llvm_hosted_role_slot_view_required_ability_ref("
require_term "src/codegen/llvm_stmt.c" \
    "llvm_party_slot_first_ability_tag("
require_term "src/codegen/llvm_stmt.c" \
    "llvm_lookup_role_vtable_global("
if grep -RIn "llvm_party_slot_first_ability_name(" \
    "$ROOT_DIR/src/codegen" >/dev/null; then
    fail "LLVM role-slot ability lookup must expose MIRAbilityRef-derived tags, not compatibility ability names"
fi
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
require_term "src/compiler/mir_decl_headers.c" \
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
    "$ROOT_DIR/src/codegen/transpiler_domain_ability_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_mir_emit.c" >/dev/null; then
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
    fail "LLVM bind lowering must consume llvm_party_slot_first_ability_tag instead of scanning party role slots"
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
require_term "src/codegen/transpiler_operator.c" \
    "transpiler_hosted_method_view_missing_mir_metadata(&view)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "find_role_operator_method_metadata(ctx, role, op, 0)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "transpiler_mir_decl_method_metadata_complete_for(ctx"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "transpiler_mir_decl_method_param_type_name(method_meta, j)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "transpiler_mir_decl_method_return_type_name(method_meta)"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "transpiler_require_type_name_c_type_copy(ctx"
require_term "src/codegen/transpiler_domain_role_methods_emit.c" \
    "!transpiler_active_has_mir(ctx)"
if grep -Fq "AST_EVENT_HANDLER_TYPE" \
        "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"; then
    fail "C role operator emission must let TranspilerHostedMethodView own type-name completeness checks"
fi
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
              "$rel" == "src/codegen/llvm_inventory_host_methods.h" ]]; then
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
    "view->count != view->ast_compat_count"

for term in \
    "MIRDeclMethod" \
    "method_metadata" \
    "method_metadata_count" \
    "mir_decl_header_set_methods" \
    "mir_link_decl_method_routines" \
    "params" \
    "param_count" \
    "param_type_names" \
    "return_type" \
    "return_type_name" \
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
for term in \
    "mir_decl_method_metadata_capture_type_names" \
    "mir_capture_type_name(param->type, NULL)" \
    "mir_capture_type_name(meta->return_type, NULL)"; do
    require_term "src/compiler/mir_decl_headers.c" "$term"
done
for rel in \
    "src/compiler/mir_decl_headers.h" \
    "src/compiler/mir_decl_header_access.c"; do
    require_term "$rel" "mir_decl_method_param_type_name"
    require_term "$rel" "mir_decl_method_return_type_name"
done
for term in \
    "method->param_type_names" \
    "method->return_type_name"; do
    require_term "src/compiler/mir_lifecycle.c" "$term"
done
for rel in \
    "src/codegen/transpiler_decl_lookup.h" \
    "src/codegen/transpiler_decl_method_view.c" \
    "src/codegen/llvm_inventory_host_methods.h" \
    "src/codegen/llvm_inventory_host_methods.c"; do
    require_term "$rel" "mir_decl_method_param_type_name"
    require_term "$rel" "mir_decl_method_return_type_name"
done
for term in \
    "mir_capture_type_name(ASTNode *type_node, const char *type_name)" \
    "return mir_render_type_name(type_node)" \
    "return pergyra_strdup(type_name)"; do
    require_term "src/compiler/mir_type_helpers.c" "$term"
done
for rel in \
    "src/compiler/mir_decl_headers.c" \
    "src/compiler/mir_signature_metadata.c" \
    "src/compiler/mir_source_local_types.c"; do
    require_term "$rel" "mir_capture_type_name("
    if grep -Fq "mir_render_type_name(" "$ROOT_DIR/$rel"; then
        fail "$rel must capture stored MIR type-name facts through mir_capture_type_name"
    fi
done
if grep -Fq "meta->type_name = type_name" \
        "$ROOT_DIR/src/compiler/mir_decl_headers.c"; then
    fail "MIRDeclField type_name must be a MIR-owned capture, not a borrowed AST string"
fi
require_term "src/compiler/mir_decl_header_validate.c" \
    "mir_decl_field_expected_type_name"
require_term "src/compiler/mir_decl_header_validate.c" \
    "field[%zu] type metadata drift"
for term in \
    "mir_validate_decl_header_metadata" \
    "declaration method count" \
    "duplicates declaration header" \
    "method metadata count" \
    "type-name storage" \
    "routine index" \
    "routine link metadata drift"; do
    require_term "src/compiler/mir_decl_header_validate.c" "$term"
done
for term in \
    "mir_validate_decl_header_shape_metadata" \
    "declaration name metadata" \
    "pointer-self ABI metadata drift"; do
    require_term "src/compiler/mir_decl_header_shape_validate.c" "$term"
done
if awk '
    /header->method_metadata_count != header->method_count/ { in_method_count = 1 }
    in_method_count && /AST_ROLE_DECL/ { found = 1 }
    in_method_count && /return false;/ { in_method_count = 0 }
    END { exit found ? 0 : 1 }
' "$ROOT_DIR/src/compiler/mir_decl_header_validate.c"; then
    fail "MIR declaration header validation must not keep role method-count exceptions"
fi
require_term "src/compiler/mir_program_validate.c" \
    "mir_validate_decl_header_metadata(mir, error_message)"
require_term "src/tests/mir/test_mir_lowering_part_h_2.cases.h" \
    "MIR validator rejects hosted method signature metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_h_2.cases.h" \
    "MIR validator rejects hosted method routine link metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_h_2.cases.h" \
    "MIR method routine linker requires owner metadata"
require_term "src/tests/mir/test_mir_lowering_part_g.cases.h" \
    "MIR validator rejects declaration header name metadata drift"
require_term "src/tests/mir/test_mir_lowering_part_g.cases.h" \
    "MIR declaration headers preserve pointer-self ABI shape"
require_term "src/tests/mir/test_mir_lowering_part_g.cases.h" \
    "MIR validator rejects pointer-self ABI metadata drift"
require_term_any \
    "MIR validator rejects duplicate declaration header names" \
    "src/tests/mir/test_mir_lowering_part_c.cases.h" \
    "src/tests/mir/test_mir_lowering_part_d.cases.h" \
    "src/tests/mir/test_mir_lowering_part_d_2.cases.h"

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
    "LLVM MIR parameter self-field slot registration consumes" \
    "MIRDeclFieldClaim" \
    "MIRDeclZoneRefresh" \
    "Dedicated declaration IR" \
    "Closed (MIR-only decision"; do
    require_term "docs/125_source_of_truth_spine.md" "$term"
done
for term in \
    "MIRDeclZoneRefresh" \
    "zone_refresh_metadata" \
    "zone_refresh_metadata_count"; do
    require_term "src/compiler/mir_decl.h" "$term"
done
for term in \
    "MIRDeclZoneState" \
    "zone_state_metadata" \
    "zone_state_metadata_count"; do
    require_term "src/compiler/mir_decl.h" "$term"
done
for term in \
    "mir_decl_header_set_refreshes" \
    "ast_relation_refreshes(decl, &refresh_count)" \
    "ast_effect_refreshes(decl, &refresh_count)" \
    "ast_zone_refreshes(decl, &refresh_count)" \
    "ast_zone_refresh_mapped_target_field" \
    "ast_zone_refresh_mapped_source_field"; do
    require_term "src/compiler/mir_decl_header_refresh.c" "$term"
done
for term in \
    "mir_decl_header_zone_refresh_count" \
    "mir_decl_zone_refresh_mapped_target_field" \
    "mir_decl_zone_refresh_mapped_source_field"; do
    require_term "src/compiler/mir_decl_header_zone_access.c" "$term"
    require_term "src/compiler/mir_decl_headers.h" "$term"
done
for term in \
    "mir_decl_header_set_zone_states" \
    "ast_zone_states(decl, &state_count)" \
    "ast_zone_state_layer_slot_name" \
    "ast_zone_state_left_or_target_slot_name" \
    "ast_zone_state_right_slot_name"; do
    require_term "src/compiler/mir_decl_header_zone_state.c" "$term"
done
for term in \
    "mir_decl_header_zone_state_count" \
    "mir_decl_zone_state_layer_slot_name" \
    "mir_decl_zone_state_left_or_target_slot_name" \
    "mir_decl_zone_state_right_slot_name"; do
    require_term "src/compiler/mir_decl_header_zone_access.c" "$term"
    require_term "src/compiler/mir_decl_headers.h" "$term"
done
for term in \
    "zone state metadata count" \
    "zone state[%zu] has incomplete state metadata"; do
    require_term "src/compiler/mir_decl_header_zone_state_validate.c" "$term"
done
for term in \
    "TranspilerHostedZoneStateView" \
    "transpiler_hosted_zone_state_view_from_decl" \
    "transpiler_hosted_zone_state_view_metadata" \
    "transpiler_hosted_zone_state_view_name"; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
    require_term "src/codegen/transpiler_decl_slot_view.c" "$term"
done
for term in "ast_zone_states" "ast_zone_state_" "ast_compat_states"; do
    if grep -Fq "$term" "$ROOT_DIR/src/codegen/transpiler_decl_slot_view.c"; then
        fail "C hosted zone-state view must be MIR-only and not keep AST zone-state compatibility payloads"
    fi
done
for term in \
    "transpiler_zone_has_state(" \
    "TranspilerHostedZoneStateView state_view" \
    "transpiler_hosted_zone_state_view_missing_mir_metadata" \
    "transpiler_hosted_zone_state_view_name"; do
    require_term "src/codegen/transpiler_projection.c" "$term"
done
require_term "src/codegen/transpiler_projection.h" \
    "bool transpiler_zone_has_state"
for term in \
    "transpiler_zone_has_state(ctx, zone_decl, state_name)"; do
    require_term "src/codegen/transpiler_expr_domain_query_builtin.c" "$term"
done
if grep -R "transpiler_find_zone_state_decl" "$ROOT_DIR/src/codegen" >/dev/null; then
    fail "C zone-state query lookup must not return AST_ZONE_STATE payloads"
fi
for term in \
    "transpiler_hosted_zone_state_view_rows_complete"; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
    require_term "src/codegen/transpiler_decl_slot_view.c" "$term"
done
for term in \
    "TranspilerHostedZoneStateView state_view" \
    "transpiler_hosted_zone_state_view_missing_mir_metadata" \
    "transpiler_hosted_zone_state_view_name"; do
    require_term "src/codegen/transpiler_zone_struct_emit.c" "$term"
done
if grep -Fq "ast_zone_states(node, &state_count)" \
    "$ROOT_DIR/src/codegen/transpiler_zone_struct_emit.c"; then
    fail "C zone struct emission must consume MIR zone state metadata, not reopen AST_ZONE_STATE inventory"
fi
for term in \
    "TranspilerHostedZoneStateView state_view" \
    "transpiler_hosted_zone_state_view_from_decl(ctx, name, node)" \
    "transpiler_hosted_zone_state_view_missing_mir_metadata" \
    "transpiler_hosted_zone_state_view_name(&state_view" \
    "transpiler_hosted_zone_state_view_layer_slot_name(&state_view" \
    "transpiler_hosted_zone_state_view_left_or_target_slot_name" \
    "transpiler_hosted_zone_state_view_right_slot_name" \
    "transpiler_emit_zone_frontier_change_checks(ctx," \
    "&state_view, &layer_view"; do
    require_term "src/codegen/transpiler_zone_decl_emit.c" "$term"
done
if grep -Fq "ast_zone_states(node, &state_count)" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.c"; then
    fail "C zone sync/frontier emission must consume MIR zone state metadata, not reopen AST_ZONE_STATE inventory"
fi
for term in \
    "const TranspilerHostedZoneStateView *state_view" \
    "transpiler_hosted_zone_state_view_name(state_view, i)"; do
    require_term "src/codegen/transpiler_zone_frontier_emit.c" "$term"
    require_term "src/codegen/transpiler_zone_frontier_emit.h" \
        "const TranspilerHostedZoneStateView *state_view"
done
if grep -Fq "ast_zone_state_name(" \
    "$ROOT_DIR/src/codegen/transpiler_zone_frontier_emit.c"; then
    fail "C zone frontier guard emission must consume MIR zone state metadata, not AST zone-state accessors"
fi
for file in \
    "src/codegen/transpiler_block_intent_helpers.c" \
    "src/codegen/transpiler_projection_sync.c"; do
    for term in \
        "TranspilerHostedZoneStateView state_view" \
        "transpiler_hosted_zone_state_view_from_decl" \
        "transpiler_hosted_zone_state_view_rows_complete" \
        "transpiler_hosted_zone_state_view_name(&state_view" \
        "transpiler_hosted_zone_state_view_layer_slot_name"; do
        require_term "$file" "$term"
    done
    if grep -Fq "ast_zone_states(zone_decl, &state_count)" "$ROOT_DIR/$file"; then
        fail "$file must consume MIR zone state metadata, not reopen AST_ZONE_STATE inventory"
    fi
    if grep -Fq "ast_zone_state_" "$ROOT_DIR/$file"; then
        fail "$file must consume TranspilerHostedZoneStateView, not AST zone-state accessors"
    fi
done
for term in \
    "LLVMHostedZoneStateView" \
    "llvm_hosted_zone_state_view_from_decl" \
    "llvm_hosted_zone_state_view_metadata" \
    "llvm_hosted_zone_state_view_name" \
    "llvm_hosted_zone_state_view_rows_complete" \
    "llvm_hosted_zone_state_view_find_name" \
    "llvm_hosted_zone_state_view_find_effect_state" \
    "llvm_hosted_zone_state_view_find_relation_state"; do
    require_term "src/codegen/llvm_inventory_decl_lookup.h" "$term"
    require_term "src/codegen/llvm_inventory_zone_state_view.c" "$term"
done
for term in "ast_zone_states" "ast_zone_state_" "ast_compat_states"; do
    if grep -Fq "$term" "$ROOT_DIR/src/codegen/llvm_inventory_zone_state_view.c"; then
        fail "LLVM hosted zone-state view must be MIR-only and not keep AST zone-state compatibility payloads"
    fi
done
for term in \
    "llvm_zone_has_state(" \
    "LLVMHostedZoneStateView state_view" \
    "llvm_hosted_zone_state_view_missing_mir_metadata" \
    "llvm_hosted_zone_state_view_name"; do
    require_term "src/codegen/llvm_domain_lookup.c" "$term"
done
require_term "src/codegen/llvm_internal_api.h" \
    "bool llvm_zone_has_state"
for term in \
    "llvm_zone_has_state(ctx, zone_decl, state_name)" \
    "llvm_zone_has_state(ctx, zone_decl, detail_name)"; do
    require_term "src/codegen/llvm_expr_domain_query_calls.c" "$term"
done
if grep -R "llvm_find_zone_state_decl" "$ROOT_DIR/src/codegen" >/dev/null; then
    fail "LLVM zone-state query lookup must not return AST_ZONE_STATE payloads"
fi
for term in \
    "LLVMHostedZoneStateView state_view" \
    "llvm_hosted_zone_state_view_from_decl(ctx, zone_name, stmt)" \
    "llvm_hosted_zone_state_view_missing_mir_metadata" \
    "llvm_hosted_zone_state_view_name(&state_view" \
    "llvm_hosted_zone_state_view_layer_slot_name(state_view" \
    "llvm_hosted_zone_state_view_left_or_target_slot_name" \
    "llvm_hosted_zone_state_view_right_slot_name"; do
    require_term "src/codegen/llvm_domain_zone_frontier_state.c" "$term"
done
if grep -Fq "ast_zone_states(stmt, &state_count)" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_frontier_state.c"; then
    fail "LLVM zone frontier previous/reset/change tracking must consume MIR zone state metadata, not reopen AST_ZONE_STATE inventory"
fi
if grep -Fq "ast_zone_state_name(" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_frontier_state.c"; then
    fail "LLVM zone frontier previous/reset/change tracking must consume LLVMHostedZoneStateView, not AST zone-state accessors"
fi
for term in \
    "LLVMHostedZoneStateView state_view" \
    "llvm_hosted_zone_state_view_from_decl(ctx, decl_name, stmt)" \
    "llvm_hosted_zone_state_view_rows_complete" \
    "llvm_hosted_zone_state_view_find_effect_state" \
    "llvm_hosted_zone_state_view_find_name"; do
    require_term "src/codegen/llvm_domain_zone_sync.c" "$term"
done
for term in \
    "LLVMHostedZoneStateView state_view" \
    "llvm_hosted_zone_state_view_from_decl(ctx, zone_name, stmt)" \
    "llvm_hosted_zone_state_view_rows_complete" \
    "llvm_hosted_zone_state_view_find_effect_state" \
    "llvm_hosted_zone_state_view_find_name"; do
    require_term "src/codegen/llvm_domain_zone_sync_clauses.c" "$term"
done
for term in \
    "LLVMHostedZoneStateView state_view" \
    "llvm_hosted_zone_state_view_from_decl(ctx, zone_name, stmt)" \
    "llvm_hosted_zone_state_view_rows_complete" \
    "llvm_hosted_zone_state_view_find_relation_state" \
    "llvm_hosted_zone_state_view_find_name"; do
    require_term "src/codegen/llvm_domain_zone_sync_relations.c" "$term"
done
for file in \
    "src/codegen/llvm_domain_zone_sync.c" \
    "src/codegen/llvm_domain_zone_sync_clauses.c" \
    "src/codegen/llvm_domain_zone_sync_relations.c"; do
    if grep -Fq "ast_zone_states(stmt, &state_count)" "$ROOT_DIR/$file"; then
        fail "$file must consume MIR zone state metadata, not reopen AST_ZONE_STATE inventory"
    fi
    if grep -Fq "ast_zone_state_" "$ROOT_DIR/$file"; then
        fail "$file must consume LLVMHostedZoneStateView, not AST zone-state accessors"
    fi
done
for file in \
    "src/codegen/llvm_intent_effect.c" \
    "src/codegen/llvm_expr_call_methods_world_effect_sync.c"; do
    for term in \
        "LLVMHostedZoneStateView state_view" \
        "llvm_hosted_zone_state_view_from_decl" \
        "llvm_hosted_zone_state_view_rows_complete" \
        "llvm_hosted_zone_state_view_name(&state_view" \
        "llvm_hosted_zone_state_view_layer_slot_name(&state_view"; do
        require_term "$file" "$term"
    done
    if grep -Fq "ast_zone_states(zone_decl, &state_count)" "$ROOT_DIR/$file"; then
        fail "$file must consume MIR zone state metadata, not reopen AST_ZONE_STATE inventory"
    fi
    if grep -Fq "ast_zone_state_" "$ROOT_DIR/$file"; then
        fail "$file must consume LLVMHostedZoneStateView, not AST zone-state accessors"
    fi
done
for term in \
    "TranspilerHostedZoneRefreshView" \
    "transpiler_hosted_zone_refresh_view_from_decl" \
    "transpiler_hosted_zone_refresh_view_metadata" \
    "transpiler_hosted_zone_refresh_view_object_slot_name" \
    "transpiler_hosted_zone_refresh_view_source_slot_name"; do
    require_term "src/codegen/transpiler_decl_lookup.h" "$term"
    require_term "src/codegen/transpiler_decl_zone_refresh_view.c" "$term"
done
for term in \
    "ast_relation_refreshes(" \
    "ast_effect_refreshes(" \
    "ast_zone_refreshes(" \
    "ast_zone_refresh_object_slot_name" \
    "ast_zone_refresh_source_slot_name" \
    "ast_zone_refresh_field_map_count" \
    "ast_zone_refresh_mapped_target_field" \
    "ast_zone_refresh_mapped_source_field"; do
    if grep -Fq "$term" "$ROOT_DIR/src/codegen/transpiler_decl_zone_refresh_view.c"; then
        fail "C hosted zone refresh view must consume MIRDeclZoneRefresh rows, not AST refresh compatibility term '$term'"
    fi
done
for term in \
    "emit_projection_literal_by_zone_refresh_metadata" \
    "mir_decl_zone_refresh_mapped_target_field" \
    "mir_decl_zone_refresh_mapped_source_field"; do
    require_term "src/codegen/transpiler_projection_emit.c" "$term"
done
for term in \
    "transpiler_domain_slot_view_is_projection_slot_in_zone_refresh_view" \
    "transpiler_hosted_zone_refresh_view_object_slot_name"; do
    require_term "src/codegen/transpiler_projection.c" "$term"
    require_term "src/codegen/transpiler_projection.h" \
        "transpiler_domain_slot_view_is_projection_slot_in_zone_refresh_view"
done
for term in \
    "emit_zone_projection_sync_loop_from_mir_refresh_view" \
    "transpiler_hosted_zone_refresh_view_metadata" \
    "emit_projection_literal_by_zone_refresh_metadata"; do
    require_term "src/codegen/transpiler_domain_provenance_emit.c" "$term"
done
for term in \
    "TranspilerHostedZoneRefreshView refresh_view" \
    "transpiler_hosted_zone_refresh_view_missing_mir_metadata" \
    "emit_zone_projection_sync_loop_from_mir_refresh_view(ctx"; do
    require_term "src/codegen/transpiler_zone_decl_emit.c" "$term"
done
if grep -Fq "ast_zone_refreshes(node, &refresh_count)" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.c"; then
    fail "C zone declaration emission must consume MIR zone refresh metadata, not reopen AST_ZONE_REFRESH inventory"
fi
for term in \
    "TranspilerHostedZoneRefreshView refresh_view" \
    "transpiler_hosted_zone_refresh_view_missing_mir_metadata" \
    "transpiler_domain_slot_view_is_projection_slot_in_zone_refresh_view"; do
    require_term "src/codegen/transpiler_domain_constructor_emit.c" "$term"
done
if grep -Fq "ast_relation_refreshes(decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"; then
    fail "C relation constructor projection-dirty initialization must consume MIR refresh metadata"
fi
if grep -Fq "ast_effect_refreshes(decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"; then
    fail "C effect constructor projection-dirty initialization must consume MIR refresh metadata"
fi
if grep -Fq "ast_zone_refreshes(zone_decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"; then
    fail "C zone constructor projection-dirty initialization must consume MIR zone refresh metadata"
fi
for term in \
    "TranspilerHostedZoneRefreshView refresh_view" \
    "transpiler_hosted_zone_refresh_view_missing_mir_metadata" \
    "emit_zone_projection_sync_loop_from_mir_refresh_view(ctx"; do
    require_term "src/codegen/transpiler_relation_effect_emit.c" "$term"
done
if grep -Fq "ast_relation_refreshes(node, &refresh_count)" \
    "$ROOT_DIR/src/codegen/transpiler_relation_effect_emit.c"; then
    fail "C relation declaration sync must consume MIR refresh metadata"
fi
if grep -Fq "ast_effect_refreshes(node, &refresh_count)" \
    "$ROOT_DIR/src/codegen/transpiler_relation_effect_emit.c"; then
    fail "C effect declaration sync must consume MIR refresh metadata"
fi
for term in \
    "CurrentOverlayRefreshView" \
    "TranspilerHostedZoneRefreshView zone_refresh_view" \
    "transpiler_hosted_zone_refresh_view_missing_mir_metadata" \
    "transpiler_hosted_zone_refresh_view_mapped_source_field"; do
    require_term "src/codegen/transpiler_overlay_projection.c" "$term"
done
if grep -Fq "ast_zone_refreshes(decl, refresh_count_out)" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_projection.c"; then
    fail "C overlay projection invalidation must consume MIR zone refresh metadata"
fi
if grep -Fq "ast_relation_refreshes(decl, &view.count)" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_projection.c"; then
    fail "C overlay relation projection invalidation must consume MIR refresh metadata"
fi
if grep -Fq "ast_effect_refreshes(decl, &view.count)" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_projection.c"; then
    fail "C overlay effect projection invalidation must consume MIR refresh metadata"
fi
for term in \
    "TranspilerHostedZoneRefreshView effect_refresh_view" \
    "transpiler_hosted_zone_refresh_view_missing_mir_metadata" \
    "transpiler_hosted_zone_refresh_view_object_slot_name" \
    "transpiler_hosted_zone_refresh_view_source_slot_name"; do
    require_term "src/codegen/transpiler_overlay_zone_bind.c" "$term"
done
if grep -Fq "ast_effect_refreshes(effect_decl, &effect_refresh_count)" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_bind.c"; then
    fail "C zone effect bind invalidation must consume MIR refresh metadata"
fi
for term in \
    "TranspilerHostedZoneRefreshView relation_refresh_view" \
    "transpiler_hosted_zone_refresh_view_missing_mir_metadata" \
    "transpiler_hosted_zone_refresh_view_object_slot_name" \
    "transpiler_hosted_zone_refresh_view_source_slot_name"; do
    require_term "src/codegen/transpiler_overlay_zone_relation_bind.c" "$term"
done
if grep -Fq "ast_relation_refreshes(relation_decl, &relation_refresh_count)" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_zone_relation_bind.c"; then
    fail "C zone relation bind invalidation must consume MIR refresh metadata"
fi
for term in \
    "TranspilerHostedZoneRefreshView effect_refresh_view" \
    "transpiler_hosted_zone_refresh_view_missing_mir_metadata" \
    "transpiler_hosted_zone_refresh_view_object_slot_name" \
    "transpiler_hosted_zone_refresh_view_source_slot_name"; do
    require_term "src/codegen/transpiler_projection_sync.c" "$term"
done
if grep -Fq "ast_effect_refreshes(effect_decl, &effect_refresh_count)" \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.c"; then
    fail "C world embedded effect sync must consume MIR refresh metadata"
fi
for term in \
    "LLVMHostedZoneRefreshView" \
    "llvm_hosted_zone_refresh_view_from_decl" \
    "llvm_hosted_zone_refresh_view_metadata" \
    "llvm_hosted_zone_refresh_view_object_slot_name" \
    "llvm_hosted_zone_refresh_view_source_slot_name"; do
    require_term "src/codegen/llvm_inventory_decl_lookup.h" "$term"
    require_term "src/codegen/llvm_inventory_zone_refresh_view.c" "$term"
done
for term in \
    "ast_relation_refreshes(" \
    "ast_effect_refreshes(" \
    "ast_zone_refreshes(" \
    "ast_zone_refresh_object_slot_name" \
    "ast_zone_refresh_source_slot_name" \
    "ast_zone_refresh_field_map_count" \
    "ast_zone_refresh_mapped_target_field" \
    "ast_zone_refresh_mapped_source_field"; do
    if grep -Fq "$term" "$ROOT_DIR/src/codegen/llvm_inventory_zone_refresh_view.c"; then
        fail "LLVM hosted zone refresh view must consume MIRDeclZoneRefresh rows, not AST refresh compatibility term '$term'"
    fi
done
for term in \
    "llvm_count_domain_projection_slots_in_zone_refresh_view" \
    "llvm_domain_slot_view_is_projection_slot_in_zone_refresh_view"; do
    require_term "src/codegen/llvm_domain_projection_count.c" "$term"
    require_term "src/codegen/llvm_domain_projection_count_helpers.h" "$term"
done
for term in \
    "llvm_build_domain_projection_value_from_zone_refresh_metadata" \
    "mir_decl_zone_refresh_mapped_target_field" \
    "mir_decl_zone_refresh_mapped_source_field"; do
    require_term "src/codegen/llvm_domain_projection_value_helpers.c" "$term"
done
require_term "src/codegen/llvm_domain_projection_value_helpers.h" \
    "llvm_build_domain_projection_value_from_zone_refresh_metadata"
for term in \
    "LLVMHostedZoneRefreshView refresh_view" \
    "llvm_hosted_zone_refresh_view_missing_mir_metadata" \
    "llvm_build_domain_projection_value_from_zone_refresh_view"; do
    require_term "src/codegen/llvm_domain_projection_sync_body_helpers.c" "$term"
done
for term in \
    "llvm_domain_add_projection_state_fields_from_zone_refresh_view" \
    "llvm_domain_slot_view_is_projection_slot_in_zone_refresh_view"; do
    require_term "src/codegen/llvm_domain_struct_fields.c" "$term"
done
require_term "src/codegen/llvm_domain_struct_fields.h" \
    "llvm_domain_add_projection_state_fields_from_zone_refresh_view"
for term in \
    "LLVMHostedZoneRefreshView refresh_view" \
    "llvm_hosted_zone_refresh_view_missing_mir_metadata" \
    "llvm_domain_add_projection_state_fields_from_zone_refresh_view"; do
    require_term "src/codegen/llvm_domain_struct_register_fields.c" "$term"
done
for term in \
    "llvm_count_domain_projection_slots_in_zone_refresh_view" \
    "llvm_domain_slot_view_is_projection_slot_in_zone_refresh_view"; do
    require_term "src/codegen/llvm_domain_struct_register.c" "$term"
done
for term in \
    "LLVMHostedZoneRefreshView refresh_view" \
    "llvm_hosted_zone_refresh_view_missing_mir_metadata" \
    "llvm_domain_slot_view_is_projection_slot_in_zone_refresh_view"; do
    require_term "src/codegen/llvm_expr_constructor_calls.c" "$term"
done
if grep -Fq "ast_relation_refreshes(relation_decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    fail "LLVM relation constructor projection-dirty initialization must consume MIR refresh metadata"
fi
if grep -Fq "ast_effect_refreshes(effect_decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    fail "LLVM effect constructor projection-dirty initialization must consume MIR refresh metadata"
fi
if grep -Fq "ast_zone_refreshes(zone_decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    fail "LLVM zone constructor projection-dirty initialization must consume MIR zone refresh metadata"
fi
for term in \
    "LLVMHostedZoneRefreshView effect_refresh_view" \
    "LLVMHostedZoneRefreshView relation_refresh_view" \
    "llvm_hosted_zone_refresh_view_missing_mir_metadata" \
    "llvm_hosted_zone_refresh_view_object_slot_name" \
    "llvm_hosted_zone_refresh_view_source_slot_name"; do
    require_term "src/codegen/llvm_domain_zone_bind_lowering.c" "$term"
done
if grep -Fq "ast_effect_refreshes(effect_decl, &effect_refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_bind_lowering.c"; then
    fail "LLVM zone effect bind invalidation must consume MIR refresh metadata"
fi
if grep -Fq "ast_relation_refreshes(relation_decl, &relation_refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_bind_lowering.c"; then
    fail "LLVM zone relation bind invalidation must consume MIR refresh metadata"
fi
for term in \
    "LLVMHostedZoneRefreshView refresh_view" \
    "llvm_hosted_zone_refresh_view_missing_mir_metadata" \
    "llvm_hosted_zone_refresh_view_object_slot_name" \
    "llvm_hosted_zone_refresh_view_source_slot_name"; do
    require_term "src/codegen/llvm_stmt_zone_action.c" "$term"
done
if grep -Fq "ast_effect_refreshes(effect_decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_zone_action.c"; then
    fail "LLVM zone action effect invalidation must consume MIR refresh metadata"
fi
for term in \
    "LLVMHostedZoneRefreshView refresh_view" \
    "llvm_hosted_zone_refresh_view_missing_mir_metadata" \
    "llvm_hosted_zone_refresh_view_object_slot_name" \
    "llvm_hosted_zone_refresh_view_source_slot_name"; do
    require_term "src/codegen/llvm_expr_call_methods_world_effect_sync.c" "$term"
done
if grep -Fq "ast_effect_refreshes(effect_decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_world_effect_sync.c"; then
    fail "LLVM world effect sync invalidation must consume MIR refresh metadata"
fi
for term in \
    "LLVMHostedZoneRefreshView refresh_view" \
    "llvm_hosted_zone_refresh_view_missing_mir_metadata" \
    "llvm_hosted_zone_refresh_view_object_slot_name" \
    "llvm_hosted_zone_refresh_view_source_slot_name"; do
    require_term "src/codegen/llvm_expr_call_projection_sync.c" "$term"
done
if grep -Fq "ast_zone_refreshes(host_decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_projection_sync.c"; then
    fail "LLVM zone subject projection sync must consume MIR zone refresh metadata"
fi
for term in \
    "LLVMHostedZoneRefreshView refresh_view" \
    "LLVMHostedZoneRefreshView embedded_refresh_view" \
    "llvm_emit_projection_invalidations_for_zone_refresh_view" \
    "llvm_hosted_zone_refresh_view_mentions_source_field" \
    "llvm_hosted_zone_refresh_view_missing_mir_metadata"; do
    require_term "src/codegen/llvm_expr_assignment_projection.c" "$term"
done
if grep -Fq "ast_zone_refreshes(host_decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_projection.c"; then
    fail "LLVM zone assignment projection invalidation must consume MIR zone refresh metadata"
fi
if grep -Fq "ast_zone_refreshes(zone_decl, &zone_refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_projection.c"; then
    fail "LLVM world-embedded zone assignment invalidation must consume MIR zone refresh metadata"
fi
if grep -Fq "ast_relation_refreshes(host_decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_projection.c"; then
    fail "LLVM relation assignment projection invalidation must consume MIR refresh metadata"
fi
if grep -Fq "ast_effect_refreshes(host_decl, &refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_projection.c"; then
    fail "LLVM effect assignment projection invalidation must consume MIR refresh metadata"
fi
if grep -Fq "llvm_emit_projection_invalidations_for_host" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_projection.c"; then
    fail "LLVM assignment projection invalidation must not keep the AST-array host refresh helper"
fi
if grep -Fq "ast_zone_refreshes(stmt, &refresh_count)" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register_fields.c"; then
    fail "LLVM zone struct field registration must consume MIR zone refresh metadata"
fi
for term in \
    "zone refresh metadata count" \
    "domain refresh metadata on a non-domain declaration" \
    "zone refresh[%zu] field-map[%zu] has incomplete metadata"; do
    require_term "src/compiler/mir_decl_header_validate.c" "$term"
done
require_term "src/tests/mir/test_mir_lowering_part_d.cases.h" \
    "MIR declaration headers preserve zone refresh field maps"
require_term "src/tests/mir/test_mir_lowering_part_d_2.cases.h" \
    "MIR declaration headers preserve relation and effect refresh metadata"
require_term "src/tests/mir/test_mir_lowering_part_h_2.cases.h" \
    "MIR validator rejects zone refresh metadata drift"

require_term "TODO.md" "declaration-side MIR-only debt"

# Row 607 (SoT docs/125): intent VALUE shape is owned solely by the MIR
# IntentBinding carrier. Codegen must not reopen ast_intent_value_type(...) or
# ast_intent_value_alias(...); a value binding without a MIR routine fails
# closed. This keeps the row's intent-value residue from being reintroduced.
while IFS= read -r f; do
    if grep -Eq 'ast_intent_value_(type|alias)\(' "$f"; then
        fail "Row 607: codegen must consume MIR IntentBinding carrier, not ast_intent_value_*: ${f#$ROOT_DIR/}"
    fi
done < <(find "$ROOT_DIR/src/codegen" -type f -name '*.c')

# F2 (docs/144) Phase 2+: semantic consumers migrated to the pre-semantic
# PgyDeclField field-shape model must not reopen ast_class_fields(...). This
# allowlist grows as each consumer migrates; Phase 5 forbids the call everywhere
# except the model owner (decl_field_model.c) and declaration validation.
for f in \
    "src/semantic/type_checker_resolution_graph_decl.c" \
    "src/semantic/type_checker_resolution_stage_nominal.c" \
    "src/semantic/type_checker_assignment.c" \
    "src/semantic/type_checker_projection_path.c" \
    "src/semantic/type_checker_helpers_resources.c" \
    "src/semantic/type_checker_intent_role_fields.c"; do
    if grep -Eq 'ast_class_fields[[:space:]]*\(' "$ROOT_DIR/$f"; then
        fail "F2: migrated semantic consumer must consume PgyDeclField, not ast_class_fields(): $f"
    fi
done

# F2: the ONLY remaining ast_class_fields READER allowed in src/semantic is the
# generic-shell AST *writer* in type_checker_class_decl.c (it mutates field->type
# during parse-completion; the model is read-only over finalized AST). Every
# other class-field shape read must go through the PgyDeclField model. Enforce
# that the semantic-wide reader count stays at exactly that one writer site.
sem_acf=$(grep -rE 'ast_class_fields[[:space:]]*\(' "$ROOT_DIR/src/semantic" --include='*.c' | wc -l | tr -d ' ')
if [ "$sem_acf" != "1" ]; then
    fail "F2: src/semantic ast_class_fields call count is $sem_acf (expected exactly 1 — the class_decl generic-shell AST writer). New readers must consume PgyDeclField."
fi

require_term "src/compiler/mir_stmt_population_source.c" \
    "inst.abi_type_name ="
require_term "src/codegen/llvm_stmt_destructure.c" \
    "llvm_destructure_claim_abi_name"
require_term "src/codegen/transpiler_mir_destructure_emit.c" \
    "claim_abi_type_name = inst->abi_type_name"
if grep -Fq "transpiler_let_slot_inner_from_call_type_arg(ctx, init)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"; then
    fail "C MIR destructure claim emission must consume MIR ABI facts, not AST generic args"
fi
if grep -Fq "ast_call_generic_arg(init, 0)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"; then
    fail "C MIR destructure claim emission must not re-read claim generic args from AST"
fi

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

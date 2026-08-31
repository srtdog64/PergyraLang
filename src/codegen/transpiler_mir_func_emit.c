#include "transpiler_mir_func_emit.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "codegen_slot_type_policy.h"
#include "transpiler_defer_emit.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_cfg_control_emit.h"
#include "transpiler_mir_emit_state.h"
#include "transpiler_mir_func_param_bindings.h"
#include "transpiler_mir_local_binding.h"
#include "transpiler_mir_local_type_lookup.h"
#include "transpiler_mir_func_ssa_locals_emit.h"
#include "transpiler_mir_pending_uses.h"
#include "transpiler_mir_pin_emit.h"
#include "transpiler_mir_resource_hook_emit.h"
#include "transpiler_mir_resource_op_emit.h"
#include "transpiler_mir_self_field_slots.h"
#include "transpiler_mir_signature.h"
#include "transpiler_mir_ssa_contract.h"
#include "transpiler_mir_ssa_entry.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_mir_terminator_emit.h"
#include "transpiler_specialization_registry.h"
#include "transpiler_type_require.h"
#include "transpiler_symbols.h"
#include "transpiler_type_declarator.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"
#include "transpiler_mir_block_emit.h"

static const char *
resolve_generic_class_self_type_name(const TranspilerCtx *ctx,
                                     const char *owner_name)
{
    if (ctx != NULL && owner_name != NULL
        && ctx->active_generic_class_base_name != NULL
        && ctx->active_generic_class_spec_name != NULL
        && strcmp(owner_name, ctx->active_generic_class_base_name) == 0)
        return ctx->active_generic_class_spec_name;
    return owner_name;
}

void
emit_func_decl_from_mir_named(ASTNode *node, const MIRRoutine *mir_routine,
                              const char *emitted_name, CodeBuf *buf,
                              TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL
        ? emitted_name
        : (transpiler_mir_routine_name(mir_routine) != NULL
            ? transpiler_mir_routine_name(mir_routine)
            : (node != NULL ? ast_declaration_name(node) : NULL));
    TranspilerMirEmitState saved_emit_state;
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    bool is_method = mir_routine != NULL
        && transpiler_mir_routine_kind(mir_routine) == MIR_SCOPE_METHOD;
    const char *owner_name = is_method
        ? transpiler_mir_routine_owner_name(mir_routine)
        : NULL;
    ASTNodeType owner_ast_type = is_method
        ? transpiler_mir_routine_owner_ast_type(mir_routine)
        : AST_PROGRAM;
    const char *owner_role_subject_name = NULL;
    char owner_role_subject_c_type_buf[256];
    const char *owner_role_subject_c_type = NULL;
    bool owner_is_zone = false;
    bool owner_is_role = owner_ast_type == AST_ROLE_DECL;
    bool owner_is_relation = owner_ast_type == AST_RELATION_DECL;
    bool owner_is_effect = owner_ast_type == AST_EFFECT_DECL;
    bool owner_is_world = owner_ast_type == AST_WORLD_DECL;
    bool pointer_self = false;
    ASTNode *resolved_host_decl = NULL;
    ASTNode *return_type = NULL;
    size_t func_param_count = 0;
    bool mir_active = transpiler_active_has_mir(ctx);
    uint32_t region_scope_id = 0;
    const ASTNode *region_source_decl = NULL;

    if (ctx != NULL && transpiler_active_has_mir(ctx) && mir_routine == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing function body routine for '%s'",
            name != NULL ? name : "<function>");
        codebuf_destroy(params_sig);
        return;
    }

    if (mir_active) {
        if (!transpiler_mir_routine_signature_supported_strict(ctx,
                mir_routine)) {
            codebuf_destroy(params_sig);
            return;
        }
    } else if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
                   mir_routine,
                   node,
                   TRANSPILER_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES,
                   "MIR-only C path missing function body signature metadata for '%s'",
                   "MIR-only C path missing function body return type-name metadata for '%s'",
                   "MIR-only C path missing function body parameter type-name metadata for '%s'")) {
        codebuf_destroy(params_sig);
        return;
    }

    if (is_method && owner_name == NULL) {
        transpiler_set_mir_topology_invalid(
            ctx,
            "MIR-only transpiler missing owner metadata for method '%s'",
            name != NULL ? name : "<method>");
        codebuf_destroy(params_sig);
        return;
    }

    if (owner_name != NULL) {
        owner_is_zone = owner_ast_type == AST_ZONE_DECL;
        if (owner_is_role) {
            owner_role_subject_name =
                transpiler_role_subject_name_local(ctx, owner_name);
            if (owner_role_subject_name != NULL)
                if (transpiler_require_type_name_c_type_copy(ctx,
                        owner_role_subject_name,
                        "MIR role owner subject",
                        owner_role_subject_c_type_buf,
                        sizeof(owner_role_subject_c_type_buf))) {
                    owner_role_subject_c_type = owner_role_subject_c_type_buf;
                }
        }
    }

    if (owner_is_role) {
        if (owner_role_subject_name != NULL) {
            resolved_host_decl = transpiler_find_host_decl_from_owner_local(
                ctx, owner_name, owner_ast_type);
        }
    } else if (owner_name != NULL) {
        resolved_host_decl = transpiler_find_host_decl_from_owner_local(
            ctx, owner_name, owner_ast_type);
    }

    if (owner_name != NULL && !owner_is_role && resolved_host_decl == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only transpiler missing declaration inventory for host '%s' of method '%s'",
            owner_name,
            name != NULL ? name : "<method>");
        codebuf_destroy(params_sig);
        return;
    }
    transpiler_capture_mir_emit_state_local(ctx, &saved_emit_state);
    /* Region scope is function-local evidence. On-demand helper emission may
     * occur while its caller owns an active scope, but the nested function must
     * establish its own scope or emit no region cleanup at all. The snapshot is
     * restored on every existing exit path; inheriting caller state is forbidden. */
    ctx->region_scope_id = 0;
    ctx->region_scope_active = false;
    ctx->out = buf;
    ctx->active_mir_routine = mir_routine;
    transpiler_bind_function_emit_host_local(ctx,
        resolved_host_decl,
        node != NULL ? node : (mir_routine != NULL ? mir_routine->ast : NULL));

    if (mir_routine != NULL) {
        ensure_collection_specializations_from_mir_routine_to(ctx, ctx->decls,
            mir_routine);
    } else if (node != NULL) {
        ensure_collection_specializations_from_stmt_to(ctx, ctx->decls, node);
    }

    return_type = transpiler_mir_routine_return_type(mir_routine);
    const char *return_type_name =
        transpiler_mir_routine_return_type_name(mir_routine);
    const MIRCallableSig *return_callable_sig =
        transpiler_mir_routine_return_callable_sig(mir_routine);
    if (return_type_name != NULL) {
        transpiler_set_current_return_type_local(ctx, return_type_name);
    } else if (return_type != NULL) {
        char *rendered = render_type_name_in_ctx(ctx, return_type);
        transpiler_set_current_return_type_local(ctx, rendered);
        free(rendered);
    } else if (return_callable_sig == NULL) {
        transpiler_set_current_return_type_local(ctx, "Void");
    }
    ctx->current_return_callable_type =
        return_type != NULL && return_type->type == AST_EVENT_HANDLER_TYPE
            ? return_type
            : NULL;

    if (owner_name != NULL) {
        if (owner_is_role) {
            codebuf_write(params_sig, "void *_raw_self");
        } else {
        pointer_self = owner_is_zone || owner_is_relation
            || owner_is_effect || owner_is_world
            || is_pointer_self_host_type_name(ctx, owner_name);
        codebuf_write(params_sig, "%s%s",
            resolve_generic_class_self_type_name(ctx, owner_name),
            pointer_self ? " *self" : " self");
        }
    }

    transpiler_mut_ref_params_reset(ctx);
    func_param_count = transpiler_mir_routine_param_count(mir_routine);
    for (size_t i = 0; i < func_param_count; i++) {
        FuncParam *p = transpiler_mir_routine_param(mir_routine, i);
        char pt_buf[256];
        const char *pt = NULL;
        const char *type_name =
            transpiler_mir_routine_param_type_name(mir_routine, i);
        char *owned_type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        bool event_handler_param = false;
        MIRParamCarriage carriage =
            transpiler_mir_routine_param_carriage(mir_routine, i);
        bool pass_indirect = transpiler_mir_routine_param_passes_indirect(mir_routine, i);
        /* Row 607: prefer the MIR-owned callable signature; the AST
           EventHandler node is only the fallback carrier. */
        const MIRCallableSig *param_callable =
            transpiler_mir_routine_param_callable_sig(mir_routine, i);
        if (p == NULL || p->name == NULL)
            continue;
        if (is_method && p != NULL && p->name != NULL
            && strcmp(p->name, "self") == 0 && p->type == NULL) {
            continue;
        }
        event_handler_param =
            param_callable != NULL
            || (!mir_active
                && p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE);
        if (!event_handler_param && type_name != NULL) {
            if (transpiler_require_type_name_c_type_copy(ctx,
                    type_name, "MIR function parameter",
                    pt_buf, sizeof(pt_buf))) {
                pt = pt_buf;
            }
        } else if (!mir_active && !event_handler_param && p->type != NULL) {
            if (pergyra_ast_type_to_c_copy_in_ctx(
                    ctx, p->type, pt_buf, sizeof(pt_buf)))
                pt = pt_buf;
        } else if (p->name != NULL
                 && strcmp(p->name, "self") == 0
                 && mir_routine != NULL
                 && transpiler_mir_routine_owner_name(mir_routine) != NULL) {
            pt = transpiler_mir_routine_owner_name(mir_routine);
            owned_type_name = pergyra_strdup(
                transpiler_mir_routine_owner_name(mir_routine));
            type_name = owned_type_name;
        }
        if (!mir_active && ctx != NULL && ctx->generic_binding_count > 0
            && !event_handler_param && p->type != NULL) {
            char pt_ast_buf[256];
            if (pergyra_ast_type_to_c_copy_in_ctx(ctx, p->type,
                    pt_ast_buf, sizeof(pt_ast_buf))
                && pt_ast_buf[0] != '\0'
                && strcmp(pt_ast_buf, "Unknown") != 0) {
                pergyra_str_copy(pt_buf, sizeof(pt_buf), pt_ast_buf);
                pt = pt_buf;
            }
        }
        if (!event_handler_param && pt == NULL) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "cannot determine parameter type for MIR-emitted function '%s' at argument %llu",
                name != NULL ? name : "<function>",
                (unsigned long long) i);
            codebuf_destroy(params_sig);
            free(header_decl);
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
            return;
        }
        if (params_sig->len > 0)
            codebuf_write(params_sig, ", ");
        if (!mir_active && type_name == NULL && p->type != NULL) {
            owned_type_name = render_type_name_in_ctx(ctx, p->type);
            type_name = owned_type_name;
        }
        boundary_slot = transpiler_mir_routine_param_is_boundary_resource(
            mir_routine, i);
        secure_slot = transpiler_mir_routine_param_is_secure_slot(
            mir_routine, i);
        if (carriage == MIR_PARAM_CARRIAGE_VALUE_RESULT) {
            if (transpiler_host_type_owns_embedded_zone_resource(
                    ctx, type_name)) {
                codebuf_write(params_sig, "%s *%s", pt, p->name);
            } else {
                codebuf_write(params_sig, "%s *%s__mutref", pt, p->name);
                transpiler_register_mut_ref_param(ctx, p->name, pt);
            }
        } else if (boundary_slot) {
            char inner_buf[128];
            const char *inner = inner_buf;
            if (!slot_inner_type_name_copy(type_name, inner_buf,
                    sizeof(inner_buf))) {
                transpiler_set_backend_error_with_hints(
                    ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "cannot determine slot payload type for MIR-emitted function '%s' parameter '%s'",
                    name != NULL ? name : "<function>",
                    p->name != NULL ? p->name : "<param>");
                codebuf_destroy(params_sig);
                free(header_decl);
                free(decl);
                free(owned_type_name);
                transpiler_restore_mir_emit_state_from_snapshot_local(ctx,
                    &saved_emit_state);
                return;
            }
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (event_handler_param) {
            /* Row 607: emit the function-pointer declarator from the MIR
               callable signature; fall back to the retained AST node only
               for the non-MIR / nested-unrenderable edge. */
            if (param_callable != NULL) {
                decl = pergyra_func_pointer_declarator_from_type_names_in_ctx(
                    ctx, param_callable->return_type_name,
                    param_callable->param_count,
                    param_callable->param_type_names, p->name);
            } else {
                decl = pergyra_ast_typed_declarator_in_ctx(ctx, p->type,
                    p->name);
            }
            if (decl == NULL) {
                codebuf_destroy(params_sig);
                free(header_decl);
                free(owned_type_name);
                transpiler_restore_mir_emit_state_from_snapshot_local(ctx,
                    &saved_emit_state);
                return;
            }
            codebuf_write(params_sig, "%s", decl);
        } else if (p->name != NULL && strcmp(p->name, "self") == 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else if (p->name != NULL && strcmp(p->name, "self") != 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else if (pass_indirect) {
            codebuf_write(params_sig, "const %s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(owned_type_name);
    }

    if (return_type_name != NULL) {
        char return_c_type[256];
        if (!transpiler_require_type_name_c_type_copy(ctx,
                return_type_name,
                "MIR function return",
                return_c_type,
                sizeof(return_c_type))) {
            codebuf_destroy(params_sig);
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx,
                &saved_emit_state);
            return;
        }
        header_decl = pergyra_strdup_printf("%s %s(%s)",
            return_c_type,
            name != NULL ? name : "value",
            params_sig != NULL ? params_sig->data : "void");
    } else if (return_callable_sig != NULL) {
        header_decl = pergyra_func_signature_declarator_from_callable_sig_in_ctx(
            ctx, return_callable_sig, name,
            params_sig != NULL ? params_sig->data : "void");
    } else {
        header_decl = pergyra_func_signature_declarator_in_ctx(ctx,
            return_type, name, params_sig != NULL ? params_sig->data : "void");
    }
    if (header_decl == NULL) {
        codebuf_destroy(params_sig);
        transpiler_restore_mir_emit_state_from_snapshot_local(ctx,
            &saved_emit_state);
        return;
    }
    codebuf_write(ctx->out, "\n%s\n{\n", header_decl);
    free(header_decl);
    header_decl = NULL;
    codebuf_destroy(params_sig);
    params_sig = NULL;

    ctx->indent++;
    transpiler_emit_mut_ref_copyins(ctx);
    if (owner_name != NULL) {
        if (owner_is_role) {
            if (owner_role_subject_name != NULL) {
                register_typed_var(ctx, "self", owner_role_subject_name);
                write_indent(ctx);
                if (is_pointer_self_host_type_name(ctx, owner_role_subject_name)) {
                    codebuf_write(ctx->out, "%s *self = (%s *)_raw_self;\n",
                        owner_role_subject_c_type != NULL ? owner_role_subject_c_type
                                                          : owner_role_subject_name,
                        owner_role_subject_c_type != NULL ? owner_role_subject_c_type
                                                          : owner_role_subject_name);
                } else {
                    codebuf_write(ctx->out, "%s self = *(%s *)_raw_self;\n",
                        owner_role_subject_c_type != NULL ? owner_role_subject_c_type
                                                          : owner_role_subject_name,
                        owner_role_subject_c_type != NULL ? owner_role_subject_c_type
                                                          : owner_role_subject_name);
                }
                write_indent(ctx);
                codebuf_write(ctx->out, "(void)self;\n");
            } else {
                write_indent(ctx);
                codebuf_write(ctx->out, "(void)_raw_self;\n");
            }
        } else {
            register_typed_var(ctx, "self", owner_name);
        }
        if (!owner_is_role
            && (owner_is_zone || owner_is_relation || owner_is_effect || owner_is_world)) {
            if (owner_is_zone) {
                const MIRDeclHeader *zone_header =
                    transpiler_active_decl_header_of_type(
                        ctx, AST_ZONE_DECL, owner_name);
                if (zone_header == NULL) {
                    transpiler_set_mir_inventory_missing(ctx,
                        "C zone authority check requires MIR declaration header for zone '%s'",
                        owner_name != NULL ? owner_name : "(anonymous)");
                } else if (mir_decl_header_zone_authority_count(
                               zone_header) > 0) {
                    const MIRDeclZoneAuthority *authority =
                        mir_decl_header_zone_authority(zone_header, 0);
                    const char *auth_slot =
                        mir_decl_zone_authority_subject_slot_name(authority);
                    if (auth_slot != NULL) {
                        char *participant_expr =
                            transpiler_scratch_fmt(ctx, "&self->%s", auth_slot);
                        write_indent(ctx);
                        codebuf_write(ctx->out,
                            "PGY_ZONE_AUTHORITY_CHECK(self, %s, \"%s\", \"%s\");\n",
                            participant_expr, owner_name, auth_slot);
                    }
                }
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", owner_name);
            if (owner_is_zone) {
                write_indent(ctx);
                codebuf_write(ctx->out,
                    "uint32_t __attribute__((unused)) __pgy_zone_gen = PGY_ZONE_GENERATION_LOAD(self);\n");
            }
        }
    }
    if (!transpiler_register_mir_func_param_bindings(
            ctx, mir_routine, name, is_method, mir_active)) {
        transpiler_restore_mir_emit_state_from_snapshot_local(ctx,
            &saved_emit_state);
        return;
    }
    if (is_method && resolved_host_decl != NULL)
        transpiler_mir_register_class_field_slots(ctx, resolved_host_decl);
    transpiler_register_mir_with_slot_claim_facts(ctx, mir_routine);
    if (transpiler_active_has_mir(ctx)) {
        if (!transpiler_register_mir_source_local_bindings(ctx, mir_routine)) {
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx,
                &saved_emit_state);
            return;
        }
    } else if (node != NULL) {
        transpiler_register_ast_compat_local_bindings_in_block(ctx, node,
            ast_func_body(node));
    }

    if (!transpiler_emit_mir_func_ssa_local_decls(ctx, node, mir_routine, name)) {
        transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
        return;
    }

    region_source_decl = node != NULL
        ? node
        : (mir_routine != NULL ? mir_routine_source_decl(mir_routine) : NULL);
    if (region_source_decl != NULL)
        (void)transpiler_region_scope_for_function_id(
            ctx, ast_node_stable_id(region_source_decl), &region_scope_id);
    if (region_scope_id == 0 && mir_routine != NULL)
        (void)transpiler_region_scope_for_function_id(
            ctx, mir_routine->source_syntax_id, &region_scope_id);
    if (region_scope_id != 0)
        transpiler_region_scope_begin(ctx, region_scope_id);

    for (size_t i = 0; i < mir_routine->block_count; i++) {
        const MIRBasicBlock *block = &mir_routine->blocks[i];
        if (!block->is_reachable || block->is_cleanup)
            continue;
    }

    TranspilerSSANameMap match_alias_map = {0};
    void *saved_match_alias_map = ctx->match_binding_alias_map;
    ctx->match_binding_alias_map = &match_alias_map;

    transpiler_defer_scope_push(ctx);

    write_indent(ctx);
    codebuf_write(ctx->out, "/* emitted-from-mir */\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n", name, mir_routine->entry_block);

    for (size_t i = 0; i < mir_routine->block_count; i++) {
        const MIRBasicBlock *block = &mir_routine->blocks[i];
        TranspilerSSANameMap block_ssa_map = {0};
        bool block_emitted;
        bool terminator_emitted = false;
        char block_reason[512];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        transpiler_emit_mir_block_mapping_comment(ctx->out, ctx->indent,
                                                 name,
                                                 mir_routine,
                                                 block);
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n", name, block->id);
        write_indent(ctx);
        codebuf_write(ctx->out, ";\n");
        block_emitted = transpiler_emit_mir_block_statements(ctx->out, node, mir_routine,
                                                           block, ctx, &block_ssa_map,
                                                           block_reason,
                                                           sizeof(block_reason));
        if (!block_emitted) {
            transpiler_ssa_map_clear(&block_ssa_map);
            transpiler_set_mir_topology_invalid(
                ctx,
                "MIR block emission failed in function '%s' at block %llu: %s",
                name != NULL ? name : "<function>",
                (unsigned long long) block->id,
                block_reason[0] != '\0' ? block_reason : "unknown reason");
            transpiler_defer_scope_pop(ctx);
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
            return;
        }

        /* Ensure function parameters are in SSA map for expression resolution in branches/returns */
        for (size_t p = 0; p < func_param_count; p++) {
            FuncParam *param = transpiler_mir_routine_param(mir_routine, p);
            if (param != NULL && param->name != NULL) {
                if (transpiler_resolve_ssa_name(&block_ssa_map, param->name) == NULL) {
                    transpiler_ssa_name_map_set(&block_ssa_map, param->name, param->name);
                }
            }
        }
        if (owner_name != NULL
            && transpiler_resolve_ssa_name(&block_ssa_map, "self") == NULL) {
            transpiler_ssa_name_map_set(&block_ssa_map, "self", "self");
        }
        const void *saved_terminator_ssa_map = ctx->active_ssa_map;
        ctx->active_ssa_map = &block_ssa_map;
        if (!transpiler_emit_mir_explicit_terminator(
                mir_routine, block, i, name, ctx, &block_ssa_map,
                &terminator_emitted, block_reason, sizeof(block_reason))) {
            ctx->active_ssa_map = saved_terminator_ssa_map;
            transpiler_ssa_map_clear(&block_ssa_map);
            transpiler_defer_scope_pop(ctx);
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
            return;
        }
        if (!terminator_emitted) {
            if (!transpiler_emit_mir_fallthrough_terminator(
                    mir_routine, block, i, name, ctx,
                    block_reason, sizeof(block_reason))) {
                ctx->active_ssa_map = saved_terminator_ssa_map;
                transpiler_ssa_map_clear(&block_ssa_map);
                transpiler_defer_scope_pop(ctx);
                transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
                return;
            }
        }
        ctx->active_ssa_map = saved_terminator_ssa_map;
    }

    /* Emit cleanup blocks if present (for intent compensation) */
    if (mir_routine->has_cleanup_block
        && node != NULL
        && node->type == AST_INTENT_DECL) {
        const char *cleanup_handle = node != NULL && node->type == AST_INTENT_DECL
            ? "__intent_handle"
            : "0";
        write_indent(ctx);
        codebuf_write(ctx->out, "/* cleanup-emitted-from-mir */\n");
        for (size_t i = 0; i < mir_routine->block_count; i++) {
            const MIRBasicBlock *block = &mir_routine->blocks[i];
            if (!block->is_cleanup || !block->is_reachable)
                continue;
            write_indent(ctx);
            codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n", name, block->id);
            write_indent(ctx);
            codebuf_write(ctx->out, ";\n");
            for (size_t j = 0; j < block->instruction_count; j++) {
                const MIRInstruction *inst = &block->instructions[j];
                if (inst->kind == MIR_INST_CLEANUP_EDGE) {
                    if (!transpiler_emit_mir_resource_hook(ctx, ctx->out, ctx->indent,
                                                           block, inst,
                                                           cleanup_handle, true)) {
            transpiler_defer_scope_pop(ctx);
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
                        return;
                    }
                } else if (inst->kind == MIR_INST_RETURN) {
                    if (inst->expr0 != NULL) {
                        char *ret_expr = emit_expression(inst->expr0, ctx);
                        if (ret_expr == NULL) {
                            if (ctx->backend_error == NULL) {
                                transpiler_set_backend_error_with_hints(ctx,
                                    PGY_CODE_C_TYPE_UNSUPPORTED,
                                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                                    PGY_FIX_INSPECT_MIR_INVENTORY,
                                    "MIR cleanup block return in function '%s' could not lower return value",
                                    name != NULL ? name : "<function>");
                            }
                            transpiler_defer_scope_pop(ctx);
                            transpiler_restore_mir_emit_state_from_snapshot_local(
                                ctx, &saved_emit_state);
                            return;
                        }
                        transpiler_emit_mut_ref_writebacks(ctx);
                        if (!transpiler_emit_mir_embedded_zone_local_cleanups(
                                ctx, ctx->out, ctx->indent)) {
                            free(ret_expr);
                            transpiler_defer_scope_pop(ctx);
                            transpiler_restore_mir_emit_state_from_snapshot_local(
                                ctx, &saved_emit_state);
                            return;
                        }
                        transpiler_region_scope_destroy(ctx);
                        write_indent(ctx);
                        codebuf_write(ctx->out, "return %s;\n", ret_expr);
                        free(ret_expr);
                    } else {
                        transpiler_emit_mut_ref_writebacks(ctx);
                        if (!transpiler_emit_mir_embedded_zone_local_cleanups(
                                ctx, ctx->out, ctx->indent)) {
                            transpiler_defer_scope_pop(ctx);
                            transpiler_restore_mir_emit_state_from_snapshot_local(
                                ctx, &saved_emit_state);
                            return;
                        }
                        transpiler_region_scope_destroy(ctx);
                        write_indent(ctx);
                        codebuf_write(ctx->out, "return;\n");

                    }
                }
            }
        }
    }

    transpiler_defer_scope_pop(ctx);
    ctx->match_binding_alias_map = saved_match_alias_map;
    transpiler_emit_mut_ref_writebacks(ctx);
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");
    transpiler_region_scope_end(ctx);
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
}

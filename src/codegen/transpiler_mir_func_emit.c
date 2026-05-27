#include "transpiler_mir_func_emit.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_defer_emit.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_mir_cfg_control_emit.h"
#include "transpiler_mir_emit_state.h"
#include "transpiler_mir_local_binding.h"
#include "transpiler_mir_local_type_lookup.h"
#include "transpiler_mir_func_ssa_locals_emit.h"
#include "transpiler_mir_pending_uses.h"
#include "transpiler_mir_pin_emit.h"
#include "transpiler_mir_resource_hook_emit.h"
#include "transpiler_mir_ssa_contract.h"
#include "transpiler_mir_ssa_entry.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_mir_terminator_emit.h"
#include "transpiler_specialization_registry.h"
#include "transpiler_symbols.h"
#include "transpiler_type_declarator.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"
#include "transpiler_mir_block_emit.h"

void
emit_func_decl_from_mir_named(ASTNode *node, const MIRRoutine *mir_routine,
                              const char *emitted_name, CodeBuf *buf,
                              TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL ? emitted_name : ast_declaration_name(node);
    TranspilerMirEmitState saved_emit_state;
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    bool is_method = mir_routine != NULL
        && mir_routine->kind == MIR_SCOPE_METHOD;
    const char *owner_name = is_method ? mir_routine->owner_name : NULL;
    ASTNodeType owner_ast_type = is_method ? mir_routine->owner_ast_type : AST_PROGRAM;
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
                if (pergyra_type_to_c_copy(owner_role_subject_name,
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
    ctx->out = buf;
    transpiler_bind_function_emit_host_local(ctx, resolved_host_decl, node);

    ensure_collection_specializations_from_stmt_to(ctx, ctx->decls, node);

    ASTNode *return_type = ast_func_return_type(node);
    if (return_type != NULL) {
        char *rendered = render_type_name_in_ctx(ctx, return_type);
        transpiler_set_current_return_type_local(ctx, rendered);
        free(rendered);
    } else {
        transpiler_set_current_return_type_local(ctx, "Void");
    }

    if (owner_name != NULL) {
        if (owner_is_role) {
            codebuf_write(params_sig, "void *_raw_self");
        } else {
        pointer_self = owner_is_zone || owner_is_relation
            || owner_is_effect || owner_is_world
            || is_pointer_self_host_type_name(ctx, owner_name);
        codebuf_write(params_sig, "%s%s", owner_name, pointer_self ? " *self" : " self");
        }
    }

    size_t func_param_count = ast_func_param_count(node);
    for (size_t i = 0; i < func_param_count; i++) {
        FuncParam *p = ast_func_param(node, i);
        char pt_buf[256];
        const char *pt = NULL;
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p == NULL || p->name == NULL)
            continue;
        if (is_method && p != NULL && p->name != NULL
            && strcmp(p->name, "self") == 0 && p->type == NULL) {
            continue;
        }
        if (p->type != NULL) {
            if (pergyra_ast_type_to_c_copy(p->type, pt_buf, sizeof(pt_buf)))
                pt = pt_buf;
        } else if (p->name != NULL
                 && strcmp(p->name, "self") == 0
                 && mir_routine != NULL
                 && mir_routine->owner_name != NULL) {
            pt = mir_routine->owner_name;
            type_name = pergyra_strdup(mir_routine->owner_name);
        }
        if (pt == NULL) {
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
        if (p->type != NULL && type_name == NULL)
            type_name = render_type_name_in_ctx(ctx, p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
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
                free(type_name);
                transpiler_restore_mir_emit_state_from_snapshot_local(ctx,
                    &saved_emit_state);
                return;
            }
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE) {
            decl = pergyra_ast_typed_declarator(p->type, p->name);
            codebuf_write(params_sig, "%s", decl);
        } else if (p->name != NULL && strcmp(p->name, "self") == 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else if (p->name != NULL && strcmp(p->name, "self") != 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(type_name);
    }

    header_decl = pergyra_func_signature_declarator(return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(ctx->out, "\n%s\n{\n", header_decl);
    free(header_decl);
    header_decl = NULL;
    codebuf_destroy(params_sig);
    params_sig = NULL;

    ctx->indent++;
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
                ASTNode *zone_decl = (resolved_host_decl != NULL
                    && resolved_host_decl->type == AST_ZONE_DECL)
                    ? resolved_host_decl
                    : NULL;
                size_t authority_count = 0;
                ASTNode **authorities = ast_zone_authorities(zone_decl,
                    &authority_count);
                if (authority_count > 0
                    && authorities != NULL
                    && authorities[0] != NULL) {
                    const char *auth_slot =
                        ast_zone_authority_subject_slot_name(authorities[0]);
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
    for (size_t i = 0; i < func_param_count; i++) {
        FuncParam *p = ast_func_param(node, i);
        char *type_name = NULL;
        if (p == NULL || p->name == NULL)
            continue;
        if (is_method && strcmp(p->name, "self") == 0 && p->type == NULL)
            continue;
        if (p->type != NULL)
            type_name = render_type_name_in_ctx(ctx, p->type);
        if (p->type == NULL
            && strcmp(p->name, "self") == 0
            && mir_routine != NULL
            && mir_routine->owner_name != NULL) {
            free(type_name);
            type_name = pergyra_strdup(mir_routine->owner_name);
        }
        if (type_name == NULL)
            continue;
        if (type_name != NULL) {
            bool boundary_slot = (strncmp(type_name, "Slot<", 5) == 0
                               || strncmp(type_name, "SecureSlot<", 11) == 0)
                && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
            register_typed_var(ctx, p->name, type_name);
            if (p->name != NULL && strcmp(p->name, "self") != 0
                && is_pointer_self_host_type_name(ctx, type_name)) {
                TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                if (entry != NULL)
                    entry->is_subject_ref = true;
            }
            if (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0) {
                char inner_buf[128];
                bool secure_slot = strncmp(type_name, "SecureSlot<", 11) == 0;
                if (!slot_inner_type_name_copy(type_name, inner_buf,
                        sizeof(inner_buf))) {
                    transpiler_set_backend_error_with_hints(
                        ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "cannot register slot parameter metadata for MIR-emitted function '%s' parameter '%s'",
                        name != NULL ? name : "<function>",
                        p->name != NULL ? p->name : "<param>");
                    free(type_name);
                    transpiler_restore_mir_emit_state_from_snapshot_local(ctx,
                        &saved_emit_state);
                    return;
                }
                register_slot_var(ctx, p->name, inner_buf, secure_slot,
                    boundary_slot);
            }
            free(type_name);
        }
    }
    transpiler_register_explicit_local_bindings_in_block(ctx, node,
        ast_func_body(node));

    if (!transpiler_emit_mir_func_ssa_local_decls(ctx, node, mir_routine, name)) {
        transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
        return;
    }

    for (size_t i = 0; i < mir_routine->block_count; i++) {
        const MIRBasicBlock *block = &mir_routine->blocks[i];
        if (!block->is_reachable || block->is_cleanup)
            continue;
    }

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
            FuncParam *param = ast_func_param(node, p);
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
        if (!transpiler_emit_mir_explicit_terminator(
                node, mir_routine, block, i, name, ctx, &block_ssa_map,
                &terminator_emitted, block_reason, sizeof(block_reason))) {
            transpiler_ssa_map_clear(&block_ssa_map);
            transpiler_defer_scope_pop(ctx);
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
            return;
        }
        if (!terminator_emitted) {
            if (!transpiler_emit_mir_fallthrough_terminator(
                    mir_routine, block, i, name, ctx,
                    block_reason, sizeof(block_reason))) {
                transpiler_ssa_map_clear(&block_ssa_map);
                transpiler_defer_scope_pop(ctx);
                transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
                return;
            }
        }
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
                    if (!transpiler_emit_mir_resource_hook(ctx, ctx->out, ctx->indent, inst,
                                                           cleanup_handle, true)) {
            transpiler_defer_scope_pop(ctx);
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
                        return;
                    }
                } else if (inst->kind == MIR_INST_RETURN) {
                    if (inst->expr0 != NULL) {
                        char *ret_expr = emit_expression(inst->expr0, ctx);
                        write_indent(ctx);
                        codebuf_write(ctx->out, "return %s;\n", ret_expr);
                        free(ret_expr);
                    } else {
                        write_indent(ctx);
                        codebuf_write(ctx->out, "return;\n");

                    }
                }
            }
        }
    }

    transpiler_defer_scope_pop(ctx);
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
}

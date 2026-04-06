/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>

#include "transpiler.h"
#include "../common/string_compat.h"
#include "../semantic/type_checker.h"

#include "transpiler_helpers.inc"

/* Forward declarations for emitters defined later */
void emit_ability_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_role_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_party_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_systemic_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_relation_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_effect_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_zone_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_world_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_actor_decl(ASTNode *node, TranspilerCtx *ctx);
static void emit_type_alias_decl(ASTNode *node, TranspilerCtx *ctx);
static bool
select_case_parts(ASTNode *case_node, ASTNode **channel_out,
                  const char **bind_name_out, ASTNode **body_out)
{
    if (case_node == NULL || case_node->type != AST_BLOCK
        || case_node->data.block.count == 0)
        return false;

    ASTNode *first = case_node->data.block.statements[0];
    ASTNode *body = case_node->data.block.count >= 2
        ? case_node->data.block.statements[1] : NULL;

    if (first->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = NULL;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    if (first->type == AST_ASSIGNMENT
        && first->data.assignment.target != NULL
        && first->data.assignment.target->type == AST_IDENTIFIER
        && first->data.assignment.value != NULL
        && first->data.assignment.value->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.assignment.value->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = first->data.assignment.target->data.identifier.name;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    return false;
}

void emit_select_stmt(ASTNode *node, TranspilerCtx *ctx);
static const ASTNode *transpiler_decl_node_for_routine(const MIRRoutine *routine);
#define TRANSPILE_SSA_MAP_CAPACITY 256
#define TRANSPILE_SSA_NAME_BUCKETS 1024

typedef struct
{
    bool in_use;
    const char *base_name;
    const char *versioned_name;
} TranspilerSSANameBucket;

typedef struct
{
    TranspilerSSANameBucket buckets[TRANSPILE_SSA_NAME_BUCKETS];
} TranspilerSSANameMap;

static const MIRRoutine *transpiler_find_mir_function(const TranspilerCtx *ctx,
                                                      const ASTNode *func_decl);
static const MIRRoutine *transpiler_find_mir_intent(const TranspilerCtx *ctx,
                                                    const ASTNode *intent_decl);
static const HIRBasicBlock *transpiler_find_hir_block_for_mir(const MIRRoutine *mir_routine,
                                                              size_t block_id);
static bool transpiler_parse_versioned_name(const char *versioned,
                                            char *base,
                                            size_t base_size,
                                            size_t *version_out);
static char *transpiler_make_c_ssa_name(const char *versioned_name);
static const char *transpiler_find_local_type_name(const ASTNode *func_decl,
                                                   const char *base_name);
static const char *transpiler_infer_local_type_name_from_expr(const ASTNode *func_decl,
                                                              ASTNode *expr);
static bool transpiler_emit_mir_block_with_ssa_map(TranspilerSSANameMap *ssa_map,
                                                  const MIRBasicBlock *block);
static char *emit_expression_with_ssa_map(ASTNode *node,
                                          TranspilerCtx *ctx,
                                          const TranspilerSSANameMap *ssa_map);
static bool transpiler_collect_ssa_name_entries(const char **versioned_values,
                                               size_t value_count,
                                               const char **base_names,
                                               const char **versioned_names,
                                               size_t max_entries,
                                               size_t *map_count_out);
static void transpiler_emit_mir_block_mapping_comment(CodeBuf *out,
                                                      int indent,
                                                      const char *routine_name,
                                                      const MIRRoutine *routine,
                                                      const MIRBasicBlock *block);
static void transpiler_free_ssa_name_entries(const char **base_names,
                                            size_t entry_count);
static bool transpiler_rebuild_ssa_map(TranspilerSSANameMap *ssa_map,
                                      const char **base_names,
                                      const char **versioned_names,
                                      size_t map_count);
static const char *transpiler_resolve_ssa_name(const TranspilerSSANameMap *ssa_map,
                                              const char *base_name);
static bool transpiler_ssa_name_map_set(TranspilerSSANameMap *ssa_map,
                                        const char *base_name,
                                        const char *versioned_name);
static bool transpiler_validate_mir_emission_contract(const MIRRoutine *routine,
                                                     const ASTNode *decl,
                                                     bool require_cleanup,
                                                     bool require_cleanup_blocks,
                                                     char *reason,
                                                     size_t reason_cap);
static bool transpiler_has_mapping_for_all_emitted_blocks(const MIRRoutine *routine,
                                                        const ASTNode *func_decl,
                                                        bool require_non_cleanup,
                                                        char *reason,
                                                        size_t reason_cap);
static bool transpiler_expr_identifiers_mapped(const ASTNode *expr,
                                              const TranspilerSSANameMap *ssa_map,
                                              const char *routine_name,
                                              char *reason,
                                              size_t reason_cap);
static bool transpiler_emit_mir_phi_copies(CodeBuf *buf,
                                           int indent,
                                           const MIRBasicBlock *pred_block,
                                           const MIRBasicBlock *target_block);
static bool transpiler_emit_mir_block_statements(CodeBuf *buf,
                                                 const ASTNode *func_decl,
                                                 const MIRRoutine *mir_routine,
                                                 const MIRBasicBlock *block,
                                                 TranspilerCtx *ctx,
                                                 TranspilerSSANameMap *out_ssa_map);
static bool transpiler_can_emit_function_from_mir(const TranspilerCtx *ctx,
                                                  const ASTNode *func_decl,
                                                  const MIRRoutine **mir_routine_out);
static bool transpiler_can_emit_function_from_mir_with_reason(const TranspilerCtx *ctx,
                                                             const ASTNode *func_decl,
                                                             const MIRRoutine **mir_routine_out,
                                                             char *reason,
                                                             size_t reason_cap);
static bool transpiler_can_emit_intent_cleanup_from_mir(const TranspilerCtx *ctx,
                                                        const ASTNode *intent_decl,
                                                        const MIRRoutine **mir_routine_out);
static bool transpiler_can_emit_intent_cleanup_from_mir_with_reason(const TranspilerCtx *ctx,
                                                                  const ASTNode *intent_decl,
                                                                  const MIRRoutine **mir_routine_out,
                                                                  char *reason,
                                                                  size_t reason_cap);
static void transpiler_emit_mir_resource_hook(CodeBuf *out,
                                              int indent,
                                              const MIRInstruction *inst,
                                              const char *handle_expr,
                                              bool cleanup_hook);
static void emit_func_decl_from_mir_named(ASTNode *node,
                                          const MIRRoutine *mir_routine,
                                          const char *emitted_name,
                                          CodeBuf *buf,
                                          TranspilerCtx *ctx);

/* Forward declarations for generic class monomorphization */
static bool class_has_generic_params(ASTNode *node);
static const char *ensure_generic_class_specialization(
    TranspilerCtx *ctx, ASTNode *class_decl, ASTNode *ann);

/* -----------------------------------------------------------------
 * Let declaration emitter
 * ----------------------------------------------------------------- */

void
emit_let_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode    *init = node->data.let_decl.initializer;
    ASTNode    *ann  = node->data.let_decl.type;
    ASTNode    *resolved_ann = resolve_type_alias_target(ctx, ann);
    char       *ann_type_name = ann != NULL ? render_type_name(ann) : NULL;
    ASTNode    *callable_type = NULL;
    ASTNode    *callable_decl = NULL;

    /* Generic class monomorphization trigger:
     * If the annotation is a user-defined generic class (e.g. Node<Int>),
     * monomorphize the class and replace ann_type_name with the
     * specialized name (e.g. Node_Int). */
    const char *generic_class_spec_name = NULL;
    if (ann != NULL && ann->type == AST_TYPE
        && ann->data.type.generic_args != NULL
        && ann->data.type.generic_args->count > 0
        && ann->data.type.name != NULL) {
        ASTNode *gc_decl = find_class_decl(ctx, ann->data.type.name);
        if (gc_decl != NULL && class_has_generic_params(gc_decl)) {
            generic_class_spec_name =
                ensure_generic_class_specialization(ctx, gc_decl, ann);
            /* Replace ann_type_name with the specialized name */
            free(ann_type_name);
            ann_type_name = pergyra_strdup(generic_class_spec_name);
        }
    }

    if (ann != NULL && ann->type == AST_EVENT_HANDLER_TYPE) {
        callable_type = ann;
    } else if (init != NULL && init->type == AST_CALL
               && init->data.call.callee != NULL
               && init->data.call.callee->type == AST_IDENTIFIER
               && init->data.call.callee->data.identifier.name != NULL) {
        ASTNode *decl = find_function_decl(ctx, init->data.call.callee->data.identifier.name);
        if (decl != NULL && decl->type == AST_FUNC_DECL
            && decl->data.func_decl.return_type != NULL
            && decl->data.func_decl.return_type->type == AST_EVENT_HANDLER_TYPE) {
            callable_type = decl->data.func_decl.return_type;
        }
    } else if (init != NULL && init->type == AST_IDENTIFIER
               && init->data.identifier.name != NULL) {
        ASTNode *decl = find_function_decl(ctx, init->data.identifier.name);
        if (decl != NULL && decl->type == AST_FUNC_DECL) {
            callable_decl = decl;
        }
    }

    /* Detect ClaimSlot / ClaimSecureSlot */
    bool is_slot        = false;
    bool is_secure_slot = false;
    bool is_device_slot = false;
    const char *slot_inner = "Int"; /* default */

    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        if (strcmp(callee_name, "ClaimSlot") == 0) {
            is_slot   = true;
        } else if (strcmp(callee_name, "ClaimSecureSlot") == 0) {
            is_slot        = true;
            is_secure_slot = true;
        } else if (strcmp(callee_name, "ClaimDeviceSlot") == 0) {
            is_device_slot = true;
        }
    }

    if (is_slot) {
        /*
         * Resolve inner type from annotation.
         * If not annotated, default to Int.
         */
        if (ann != NULL) {
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                slot_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                slot_inner = slot_inner_type_name(ann->data.type.name);
            }
        }

        register_slot_var(ctx, name, slot_inner, is_secure_slot, false);

        write_indent(ctx);
        if (is_secure_slot) {
            codebuf_write(ctx->out,
                "PgyToken_%s %s_token;\n", slot_inner, name);
            write_indent(ctx);
            codebuf_write(ctx->out,
                "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
                slot_inner, name, slot_inner, name);
        } else {
            codebuf_write(ctx->out,
                "PgySlot_%s %s = pgy_claim_%s();\n",
                slot_inner, name, slot_inner);
        }
        if (ann_type_name != NULL)
            register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    if (is_device_slot) {
        if (ann != NULL) {
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                slot_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                slot_inner = slot_inner_type_name(ann->data.type.name);
            }
        }

        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyDeviceSlot_%s %s = pgy_claim_device_%s();\n",
            slot_inner, name, slot_inner);
        if (ann_type_name != NULL)
            register_typed_var(ctx, name, ann_type_name);
        else {
            char *device_type = strdup_fmt("DeviceSlot<%s>", slot_inner);
            register_typed_var(ctx, name, device_type);
            free(device_type);
        }
        free(ann_type_name);
        return;
    }

    if (ann != NULL && ann->type == AST_TYPE && ann->data.type.name != NULL) {
        const char *ann_name = ann->data.type.name;
        if (ann_name != NULL && ann_name[0] != '\0'
        && (strcmp(ann_name, "ReadView") == 0
            || strncmp(ann_name, "ReadView<", 9) == 0
            || strcmp(ann_name, "WriteView") == 0
            || strncmp(ann_name, "WriteView<", 10) == 0
            || strcmp(ann_name, "MoveToken") == 0
            || strncmp(ann_name, "MoveToken<", 10) == 0)
        && init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && init->data.call.arg_count >= 1
        && init->data.call.arguments[0] != NULL
        && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        const char *source_name = init->data.call.arguments[0]->data.identifier.name;
        bool is_view_decl = (strcmp(callee_name, "ViewRead") == 0
                          || strcmp(callee_name, "ViewWrite") == 0);
        bool is_move_decl = strcmp(callee_name, "Move") == 0;
        if (is_view_decl || is_move_decl) {
            const char *inner = "Int";
            bool source_secure = lookup_slot_is_secure(ctx, source_name);
            const char *ctype;
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                inner = ann->data.type.generic_args->params[0]->name;
            } else {
                inner = slot_inner_type_name(ann_name);
            }
            ctype = source_secure ? pergyra_type_to_c("SecureSlot<Int>") : pergyra_type_to_c("Slot<Int>");
            if (source_secure) {
                char secure_name[128];
                snprintf(secure_name, sizeof(secure_name), "SecureSlot<%s>", inner);
                ctype = pergyra_type_to_c(secure_name);
            } else {
                char slot_name_buf[128];
                snprintf(slot_name_buf, sizeof(slot_name_buf), "Slot<%s>", inner);
                ctype = pergyra_type_to_c(slot_name_buf);
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = %s;\n", ctype, name, source_name);
            if (source_secure) {
                write_indent(ctx);
                codebuf_write(ctx->out, "PgyToken_%s %s_token = %s_token;\n",
                    inner, name, source_name);
            }
            register_view_like_var(ctx, name,
                ann_type_name != NULL ? ann_type_name : ann_name,
                source_name, source_secure, is_move_decl);
            if (is_move_decl) {
                for (int i = 0; i < ctx->slot_var_count; i++) {
                    if (strcmp(ctx->slot_vars[i].name, source_name) == 0) {
                        ctx->slot_vars[i].released = true;
                        break;
                    }
                }
            }
            free(ann_type_name);
            return;
        }
    }
    }

    /* Slot sugar: let x: Slot<Int> = 42 → auto Claim + Write */
    if (!is_slot && ann != NULL && ann->type == AST_TYPE) {
        const char *ann_name = ann->data.type.name;
        bool is_slot_sugar = false;
        bool sugar_secure = false;
        if (ann_name != NULL) {
            if (strcmp(ann_name, "Slot") == 0 || strncmp(ann_name, "Slot<", 5) == 0) {
                is_slot_sugar = true;
            } else if (strcmp(ann_name, "SecureSlot") == 0 || strncmp(ann_name, "SecureSlot<", 11) == 0) {
                is_slot_sugar = true;
                sugar_secure = true;
            }
        }
        if (is_slot_sugar) {
            const char *sugar_inner = "Int";
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                sugar_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                sugar_inner = slot_inner_type_name(ann_name);
            }

            if (init != NULL && init->type == AST_IDENTIFIER) {
                TypedVarEntry *move_entry = lookup_typed_entry(ctx, init->data.identifier.name);
                if (move_entry != NULL && move_entry->is_move_token) {
                    write_indent(ctx);
                    if (sugar_secure) {
                        codebuf_write(ctx->out, "PgySecureSlot_%s %s = %s;\n",
                            sugar_inner, name, init->data.identifier.name);
                        write_indent(ctx);
                        codebuf_write(ctx->out, "PgyToken_%s %s_token = %s_token;\n",
                            sugar_inner, name, init->data.identifier.name);
                    } else {
                        codebuf_write(ctx->out, "PgySlot_%s %s = %s;\n",
                            sugar_inner, name, init->data.identifier.name);
                    }
                    register_slot_var(ctx, name, sugar_inner, sugar_secure, false);
                    if (ann_type_name != NULL)
                        register_typed_var(ctx, name, ann_type_name);
                    free(ann_type_name);
                    return;
                }
            }

            register_slot_var(ctx, name, sugar_inner, sugar_secure, false);

            write_indent(ctx);
            if (sugar_secure) {
                codebuf_write(ctx->out,
                    "PgyToken_%s %s_token;\n", sugar_inner, name);
                write_indent(ctx);
                codebuf_write(ctx->out,
                    "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
                    sugar_inner, name, sugar_inner, name);
            } else {
                codebuf_write(ctx->out,
                    "PgySlot_%s %s = pgy_claim_%s();\n",
                    sugar_inner, name, sugar_inner);
            }

            /* Auto Write the initializer value */
            if (init != NULL) {
                char *init_expr = emit_expression(init, ctx);
                write_indent(ctx);
                if (sugar_secure) {
                    codebuf_write(ctx->out,
                        "pgy_secure_write_%s(&%s, %s, &%s_token);\n",
                        sugar_inner, name, init_expr, name);
                } else {
                    codebuf_write(ctx->out,
                        "pgy_write_%s(&%s, %s);\n",
                        sugar_inner, name, init_expr);
                }
                free(init_expr);
            }

            if (ann_type_name != NULL)
                register_typed_var(ctx, name, ann_type_name);
            free(ann_type_name);
            return;
        }
    }

    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "BoxArray") == 0) {
        const char *inner = "Int";
        if (ann_type_name != NULL && strncmp(ann_type_name, "Box<Array<", 10) == 0) {
            inner = ann_type_name + 10;
            const char *close = strstr(inner, ">>");
            static char inner_buf[64];
            size_t len = close != NULL ? (size_t)(close - inner) : strlen(inner);
            if (len >= sizeof(inner_buf))
                len = sizeof(inner_buf) - 1;
            memcpy(inner_buf, inner, len);
            inner_buf[len] = '\0';
            inner = inner_buf;
        }

        char *capacity = (init->data.call.arg_count > 0)
                         ? emit_expression(init->data.call.arguments[0], ctx)
                         : pergyra_strdup("0");
        char *allocator = (init->data.call.arg_count > 1)
                          ? emit_expression(init->data.call.arguments[1], ctx)
                          : pergyra_strdup("NULL");
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyBoxArray_%s %s = pgy_box_array_new_%s(%s, %s);\n",
            inner, name, inner, capacity, allocator);
        register_typed_var(ctx, name,
            ann_type_name != NULL ? ann_type_name : "Box<Array<Int>>");
        free(capacity);
        free(allocator);
        free(ann_type_name);
        return;
    }

    /* Detect Box<T> - Type inference from Box(value) */
    bool is_box = false;
    const char *box_inner = "Int";
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        if (strcmp(callee_name, "Box") == 0) {
            is_box = true;
            /* Infer type from annotation or argument */
            if (ann != NULL && ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                box_inner = ann->data.type.generic_args->params[0]->name;
            } else if (init->data.call.arg_count > 0) {
                /* Infer from argument type */
                ASTNode *arg = init->data.call.arguments[0];
                if (arg->type == AST_NUMBER) box_inner = "Int";
                else if (arg->type == AST_STRING) box_inner = "String";
                else if (arg->type == AST_BOOLEAN) box_inner = "Bool";
            }
        }
        /* Rc<T> - Reference counted box */
        else if (strcmp(callee_name, "Rc") == 0) {
            is_box = true;
            if (ann != NULL && ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                box_inner = ann->data.type.generic_args->params[0]->name;
            } else if (init->data.call.arg_count > 0) {
                ASTNode *arg = init->data.call.arguments[0];
                if (arg->type == AST_NUMBER) box_inner = "Int";
                else if (arg->type == AST_STRING) box_inner = "String";
                else if (arg->type == AST_BOOLEAN) box_inner = "Bool";
            }
        }
    }

    if (is_box) {
        write_indent(ctx);
        codebuf_write(ctx->out, "PgyBox_%s %s = pgy_box_new_%s(", 
                      box_inner, name, box_inner);
        if (init->data.call.arg_count > 0) {
            char *arg = emit_expression(init->data.call.arguments[0], ctx);
            codebuf_write(ctx->out, "%s", arg);
            free(arg);
        }
        codebuf_write(ctx->out, ");\n");
        register_typed_var(ctx, name,
            ann_type_name != NULL ? ann_type_name : "Box<Int>");
        free(ann_type_name);
        return;
    }

    if (ann_type_name != NULL && strncmp(ann_type_name, "Channel<", 8) == 0) {
        const char *inner = slot_inner_type_name(ann_type_name);
        char *capacity = pergyra_strdup("16");

        if (init != NULL && init->type == AST_CALL
            && init->data.call.arg_count > 0) {
            free(capacity);
            capacity = emit_expression(init->data.call.arguments[0], ctx);
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "PgyChannel_%s %s;\n", inner, name);
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_channel_init_%s(&%s, %s);\n",
            inner, name, capacity);
        register_typed_var(ctx, name, ann_type_name);
        free(capacity);
        free(ann_type_name);
        return;
    }

    if (ann_type_name != NULL && strncmp(ann_type_name, "Option<", 7) == 0) {
        const char *inner = slot_inner_type_name(ann_type_name);
        if (init != NULL
            && init->type == AST_CALL
            && init->data.call.callee != NULL
            && init->data.call.callee->type == AST_IDENTIFIER) {
            const char *callee_name = init->data.call.callee->data.identifier.name;
            if (strcmp(callee_name, "Some") == 0 && init->data.call.arg_count == 1) {
                char *arg = emit_expression(init->data.call.arguments[0], ctx);
                write_indent(ctx);
                codebuf_write(ctx->out, "PgyOption_%s %s = Some_%s(%s);\n",
                    inner, name, inner, arg);
                register_typed_var(ctx, name, ann_type_name);
                free(arg);
                free(ann_type_name);
                return;
            }
            if (strcmp(callee_name, "None") == 0 && init->data.call.arg_count == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "PgyOption_%s %s = None_%s();\n",
                    inner, name, inner);
                register_typed_var(ctx, name, ann_type_name);
                free(ann_type_name);
                return;
            }
        }
    }

    if (resolved_ann != NULL
        && resolved_ann->type == AST_TYPE
        && resolved_ann->data.type.name != NULL
        && (strcmp(resolved_ann->data.type.name, "HashMap") == 0
            || strcmp(resolved_ann->data.type.name, "List") == 0
            || strcmp(resolved_ann->data.type.name, "Queue") == 0)
        && init != NULL
        && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        const char *type_name = resolved_ann->data.type.name;
        const char *inner = slot_inner_type_name(ann_type_name);
        if (strcmp(callee_name, "MapNew") == 0
            && strcmp(type_name, "HashMap") == 0
            && resolved_ann->data.type.generic_args != NULL
            && resolved_ann->data.type.generic_args->count == 2) {
            GenericParam *key_param = resolved_ann->data.type.generic_args->params[0];
            GenericParam *value_param = resolved_ann->data.type.generic_args->params[1];
            char *key = (key_param != NULL && key_param->constraint != NULL)
                ? render_type_name(key_param->constraint)
                : pergyra_strdup((key_param != NULL && key_param->name != NULL)
                    ? key_param->name : "Int");
            char *value = (value_param != NULL && value_param->constraint != NULL)
                ? render_type_name(value_param->constraint)
                : pergyra_strdup((value_param != NULL && value_param->name != NULL)
                    ? value_param->name : "Int");
            if (strcmp(key, "String") == 0 && value != NULL) {
                ensure_collection_specialization(ctx, "Map", value);
                write_indent(ctx);
                codebuf_write(ctx->out, "%s %s = pgy_map_new_%s();\n",
                    pergyra_type_to_c(ann_type_name), name,
                    collection_runtime_suffix(value));
                register_typed_var(ctx, name, ann_type_name);
                free(key);
                free(value);
                free(ann_type_name);
                return;
            }
            free(key);
            free(value);
        }
        if (strcmp(callee_name, "ListNew") == 0
            && strcmp(type_name, "List") == 0) {
            ensure_collection_specialization(ctx, "List", inner);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = pgy_list_new_%s();\n",
                pergyra_type_to_c(ann_type_name), name,
                collection_runtime_suffix(inner));
            register_typed_var(ctx, name, ann_type_name);
            free(ann_type_name);
            return;
        }
        if (strcmp(callee_name, "QueueNew") == 0
            && strcmp(type_name, "Queue") == 0) {
            ensure_collection_specialization(ctx, "Queue", inner);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = pgy_queue_new_%s();\n",
                pergyra_type_to_c(ann_type_name), name,
                collection_runtime_suffix(inner));
            register_typed_var(ctx, name, ann_type_name);
            free(ann_type_name);
            return;
        }
    }

    /* Array literal: let arr = [1, 2, 3] → PgyArray_Int arr = ({ ... }); */
    if (init != NULL && init->type == AST_ARRAY_LITERAL) {
        const char *array_type_name = ann_type_name != NULL
            ? ann_type_name
            : infer_expression_type_name(ctx, init);
        const char *array_c_type = pergyra_type_to_c(array_type_name);
        char *init_expr = emit_expression(init, ctx);
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = %s;\n", array_c_type, name, init_expr);
        free(init_expr);
        register_typed_var(ctx, name, array_type_name);
        free(ann_type_name);
        return;
    }

    /* Normal variable with type inference */
    const char *c_type = "int32_t"; /* fallback */
    if (ann != NULL) {
        c_type = pergyra_ast_type_to_c(ann);
    } else if (init != NULL) {
        /* Type inference from initializer */
        if (init->type == AST_NUMBER)  c_type = "int32_t";
        else if (init->type == AST_STRING)  c_type = "char*";
        else if (init->type == AST_BOOLEAN) c_type = "bool";
        else if (init->type == AST_SPAWN_EXPR) c_type = "PgyTaskHandle";
        else if (init->type == AST_CHANNEL_RECV) {
            c_type = pergyra_type_to_c(infer_expression_type_name(ctx, init));
        }
        else if (init->type == AST_CALL) {
            c_type = pergyra_type_to_c(infer_expression_type_name(ctx, init));
        }
        else if (init->type == AST_ARRAY_LITERAL)
            c_type = pergyra_type_to_c(infer_expression_type_name(ctx, init));
        else
            c_type = pergyra_type_to_c(infer_expression_type_name(ctx, init));
    }

    /* Struct/class constructor: let p: Point = Point(...)
     * Lower positional constructor args into field-order initialization.
     * Missing fields stay zero-initialized.
     * For generic classes: callee is "Node" but ann_type_name is "Node_Int",
     * so also match against the original class name via generic_class_spec_name. */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER
        && ann_type_name != NULL
        && (strcmp(init->data.call.callee->data.identifier.name, ann_type_name) == 0
            || (generic_class_spec_name != NULL
                && ann->data.type.name != NULL
                && strcmp(init->data.call.callee->data.identifier.name, ann->data.type.name) == 0))
        ) {
        ASTNode *class_decl = find_class_decl(ctx, ann_type_name);
        /* For generic classes, find_class_decl won't find "Node_Int" —
         * fall back to the original generic class declaration. */
        if (class_decl == NULL && generic_class_spec_name != NULL
            && ann->data.type.name != NULL)
            class_decl = find_class_decl(ctx, ann->data.type.name);
        write_indent(ctx);
        if (class_decl != NULL
            && class_decl->type == AST_CLASS_DECL
            && class_decl->data.class_decl.field_count > 0
            && init->data.call.arg_count > 0) {
            codebuf_write(ctx->out, "%s %s = { ", ann_type_name, name);
            for (size_t i = 0; i < init->data.call.arg_count; i++) {
                ClassField *field;
                char *arg_expr;
                if (i >= class_decl->data.class_decl.field_count)
                    break;
                field = class_decl->data.class_decl.fields[i];
                if (field == NULL || field->name == NULL)
                    continue;
                arg_expr = emit_expression(init->data.call.arguments[i], ctx);
                if (i > 0)
                    codebuf_write(ctx->out, ", ");
                codebuf_write(ctx->out, ".%s = %s", field->name, arg_expr);
                free(arg_expr);
            }
            codebuf_write(ctx->out, " };\n");
        } else {
            codebuf_write(ctx->out, "%s %s = {0};\n", ann_type_name, name);
        }
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    write_indent(ctx);
    if (callable_type != NULL || callable_decl != NULL) {
        char *decl = callable_type != NULL
            ? pergyra_ast_typed_declarator(callable_type, name)
            : pergyra_func_pointer_declarator_from_decl(callable_decl, name);
        if (init != NULL) {
            char *init_expr = emit_expression(init, ctx);
            codebuf_write(ctx->out, "%s = %s;\n", decl, init_expr);
            free(init_expr);
        } else {
            codebuf_write(ctx->out, "%s = 0;\n", decl);
        }
        free(decl);
    } else if (init != NULL) {
        char *init_expr = emit_expression(init, ctx);
        codebuf_write(ctx->out, "%s %s = %s;\n", c_type, name, init_expr);
        free(init_expr);
    } else {
        codebuf_write(ctx->out, "%s %s = 0;\n", c_type, name);
    }

    if (ann_type_name != NULL) {
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
    } else if (init != NULL && init->type == AST_CALL) {
        register_typed_var(ctx, name, infer_expression_type_name(ctx, init));
    } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        char *future_type = infer_spawn_return_type_name(ctx, init);
        char *wrapped = strdup_fmt("Future<%s>", future_type);
        register_typed_var(ctx, name, wrapped);
        free(future_type);
        free(wrapped);
    } else if (init != NULL && init->type == AST_CHANNEL_RECV) {
        const char *inner = infer_expression_type_name(ctx, init);
        register_typed_var(ctx, name, inner);
    } else if (init != NULL) {
        /* Fallback: infer from any initializer (string literals, binary exprs, etc.) */
        const char *inferred = infer_expression_type_name(ctx, init);
        if (inferred != NULL && strcmp(inferred, "Int") != 0) {
            /* Only register non-default (Int is the fallback, skip to avoid noise) */
            register_typed_var(ctx, name, inferred);
        }
    }
}

/* -----------------------------------------------------------------
 * Function declaration emitter
 * ----------------------------------------------------------------- */

static void
emit_func_forward_decl_named(ASTNode *node, const char *emitted_name,
                             CodeBuf *buf, TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    g_type_render_ctx = ctx;
    ensure_type_specializations_from_ast(ctx, node->data.func_decl.return_type);
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = "int32_t";
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p->type != NULL)
            ensure_type_specializations_from_ast(ctx, p->type);
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (i > 0)
            codebuf_write(params_sig, ", ");
        if (p->type != NULL)
            type_name = render_type_name(p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            const char *inner = slot_inner_type_name(type_name);
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE) {
            decl = pergyra_ast_typed_declarator(p->type, p->name);
            codebuf_write(params_sig, "%s", decl);
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
    header_decl = pergyra_func_signature_declarator(node->data.func_decl.return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(buf, "%s;\n", header_decl);
    free(header_decl);
    codebuf_destroy(params_sig);
    g_type_render_ctx = saved_render_ctx;
}

static void
emit_func_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    emit_func_forward_decl_named(node, node->data.func_decl.name, buf, ctx);
}

static const MIRRoutine *
transpiler_find_mir_function(const TranspilerCtx *ctx, const ASTNode *func_decl)
{
    if (ctx == NULL || ctx->mir == NULL || func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.name == NULL) {
        return NULL;
    }

    const char *target = func_decl->data.func_decl.name;
    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_FUNCTION)
            continue;
        if (routine->name == NULL)
            continue;
        /* Exact match */
        if (strcmp(routine->name, target) == 0)
            return routine;
        /* Specialized name: e.g. "Identity_Int" matches "Identity" */
        size_t name_len = strlen(target);
        if (strncmp(routine->name, target, name_len) == 0
            && (routine->name[name_len] == '_' || routine->name[name_len] == '\0'))
            return routine;
    }

    return NULL;
}

static const MIRRoutine *
transpiler_find_mir_intent(const TranspilerCtx *ctx, const ASTNode *intent_decl)
{
    if (ctx == NULL || ctx->mir == NULL || intent_decl == NULL
        || intent_decl->type != AST_INTENT_DECL
        || intent_decl->data.intent_decl.name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_INTENT
            || routine->name == NULL
            || strcmp(routine->name, intent_decl->data.intent_decl.name) != 0) {
            continue;
        }
        if (routine->hir_routine != NULL && routine->hir_routine->ast == intent_decl)
            return routine;
    }

    return NULL;
}

static const HIRBasicBlock *
transpiler_find_hir_block_for_mir(const MIRRoutine *mir_routine, size_t block_id)
{
    const MIRBasicBlock *mir_block;
    size_t hir_block_id;

    if (mir_routine == NULL || mir_routine->hir_routine == NULL
        || !mir_routine->hir_routine->has_cfg
        || block_id >= mir_routine->block_count) {
        return NULL;
    }
    mir_block = &mir_routine->blocks[block_id];
    hir_block_id = mir_block->source_hir_block_id;

    if (hir_block_id == SIZE_MAX || hir_block_id >= mir_routine->hir_routine->cfg.block_count)
        return NULL;
    return &mir_routine->hir_routine->cfg.blocks[hir_block_id];
}

static bool
transpiler_parse_versioned_name(const char *versioned, char *base, size_t base_size,
                                size_t *version_out)
{
    const char *dot;
    size_t len;
    if (versioned == NULL || base == NULL || base_size == 0 || version_out == NULL)
        return false;
    dot = strrchr(versioned, '.');
    if (dot == NULL)
        return false;
    len = (size_t)(dot - versioned);
    if (len + 1 > base_size)
        return false;
    memcpy(base, versioned, len);
    base[len] = '\0';
    *version_out = (size_t)strtoull(dot + 1, NULL, 10);
    return true;
}

static size_t
transpiler_ssa_name_bucket_index(const char *key)
{
    size_t hash = 5381u;
    const unsigned char *u = (const unsigned char *)key;
    while (u != NULL && *u != '\0') {
        hash = ((hash << 5) + hash) + (size_t)(*u);
        ++u;
    }
    return hash % TRANSPILE_SSA_NAME_BUCKETS;
}

static void
transpiler_ssa_map_clear(TranspilerSSANameMap *map)
{
    if (map == NULL)
        return;
    memset(map, 0, sizeof(*map));
}

static bool
transpiler_ssa_name_map_set(TranspilerSSANameMap *map,
                            const char *base_name,
                            const char *versioned_name)
{
    size_t idx;
    size_t attempts;

    if (map == NULL || base_name == NULL || versioned_name == NULL)
        return false;
    idx = transpiler_ssa_name_bucket_index(base_name);
    for (attempts = 0; attempts < TRANSPILE_SSA_NAME_BUCKETS; ++attempts) {
        TranspilerSSANameBucket *bucket = &map->buckets[idx];
        if (!bucket->in_use) {
            bucket->in_use = true;
            bucket->base_name = base_name;
            bucket->versioned_name = versioned_name;
            return true;
        }
        if (bucket->base_name != NULL && strcmp(bucket->base_name, base_name) == 0) {
            bucket->versioned_name = versioned_name;
            return true;
        }
        idx = (idx + 1) % TRANSPILE_SSA_NAME_BUCKETS;
    }
    return false;
}

static bool
transpiler_collect_ssa_name_entries(const char **versioned_values,
                                   size_t value_count,
                                   const char **base_names,
                                   const char **versioned_names,
                                   size_t max_entries,
                                   size_t *map_count_out)
{
    size_t map_count = 0;

    if (versioned_values == NULL
        || base_names == NULL
        || versioned_names == NULL
        || max_entries == 0) {
        return true;
    }
    for (size_t i = 0; i < value_count; i++) {
        const char *versioned = versioned_values[i];
        char base[128];
        size_t parsed_version = 0;
        bool replaced = false;

        if (versioned == NULL)
            continue;
        if (!transpiler_parse_versioned_name(versioned, base, sizeof(base), &parsed_version))
            continue;
        for (size_t j = 0; j < map_count; j++) {
            if (base_names[j] != NULL && strcmp(base_names[j], base) == 0) {
                versioned_names[j] = versioned;
                replaced = true;
                break;
            }
        }
        if (replaced)
            continue;
        if (map_count >= max_entries)
            return false;
        base_names[map_count] = pergyra_strdup(base);
        versioned_names[map_count] = versioned;
        map_count++;
    }
    if (map_count_out != NULL)
        *map_count_out = map_count;
    return true;
}

static void
transpiler_free_ssa_name_entries(const char **base_names, size_t entry_count)
{
    for (size_t i = 0; i < entry_count; i++) {
        free((void *)base_names[i]);
    }
}

static bool
transpiler_rebuild_ssa_map(TranspilerSSANameMap *ssa_map,
                          const char **base_names,
                          const char **versioned_names,
                          size_t map_count)
{
    if (ssa_map == NULL || base_names == NULL || versioned_names == NULL) {
        if (ssa_map != NULL)
            transpiler_ssa_map_clear(ssa_map);
        return false;
    }
    transpiler_ssa_map_clear(ssa_map);
    for (size_t i = 0; i < map_count; i++) {
        if (base_names[i] == NULL || versioned_names[i] == NULL)
            continue;
        if (!transpiler_ssa_name_map_set(ssa_map, base_names[i], versioned_names[i]))
            return false;
    }
    return true;
}

static void
transpiler_emit_mir_block_mapping_comment(CodeBuf *out,
                                          int indent,
                                          const char *routine_name,
                                          const MIRRoutine *routine,
                                          const MIRBasicBlock *block)
{
    const ASTNode *source_stmt = NULL;
    const HIRBasicBlock *hir_block = NULL;
    uint32_t line = 0;
    uint32_t column = 0;

    if (out == NULL || routine == NULL || block == NULL)
        return;

    if (routine->hir_routine != NULL && routine->hir_routine->has_cfg
        && block->source_hir_block_id != SIZE_MAX
        && block->source_hir_block_id < routine->hir_routine->cfg.block_count) {
        hir_block = &routine->hir_routine->cfg.blocks[block->source_hir_block_id];
        if (hir_block != NULL) {
            if (hir_block->statement_count > 0)
                source_stmt = hir_block->statements[0];
            else if (hir_block->terminator_condition != NULL)
                source_stmt = hir_block->terminator_condition;
            else if (hir_block->terminator_value != NULL)
                source_stmt = hir_block->terminator_value;
            if (source_stmt != NULL) {
                line = source_stmt->line;
                column = source_stmt->column;
            }
        }
    }

    if (source_stmt != NULL) {
        write_indent_to(out, indent);
        codebuf_write(out,
            "/* mir block=%zu hir=%zu (%s) src=%u:%u ast=%p */\n",
            block->id,
            block->source_hir_block_id,
            routine_name != NULL ? routine_name : "<routine>",
            line,
            column,
            (const void *)source_stmt);
    } else {
        write_indent_to(out, indent);
        codebuf_write(out,
            "/* mir block=%zu hir=%s (%s) */\n",
            block->id,
            block->source_hir_block_id == SIZE_MAX ? "<none>" : "mapped",
            routine_name != NULL ? routine_name : "<routine>");
    }
}

static const char *
transpiler_resolve_ssa_name(const TranspilerSSANameMap *ssa_map,
                            const char *base_name)
{
    size_t idx;
    size_t attempts;

    if (ssa_map == NULL || base_name == NULL)
        return NULL;
    idx = transpiler_ssa_name_bucket_index(base_name);
    for (attempts = 0; attempts < TRANSPILE_SSA_NAME_BUCKETS; ++attempts) {
        const TranspilerSSANameBucket *bucket = &ssa_map->buckets[idx];
        if (!bucket->in_use)
            return NULL;
        if (bucket->base_name != NULL && strcmp(bucket->base_name, base_name) == 0)
            return bucket->versioned_name;
        idx = (idx + 1) % TRANSPILE_SSA_NAME_BUCKETS;
    }
    return NULL;
}

static bool
transpiler_emit_mir_block_with_ssa_map(TranspilerSSANameMap *ssa_map,
                                       const MIRBasicBlock *block)
{
    const char *base_names[TRANSPILE_SSA_MAP_CAPACITY];
    const char *versioned_names[TRANSPILE_SSA_MAP_CAPACITY];
    size_t map_count = 0;

    transpiler_ssa_map_clear(ssa_map);
    if (block == NULL || block->ssa_entry_values == NULL || block->ssa_entry_value_count == 0)
        return true;
    if (!transpiler_collect_ssa_name_entries(block->ssa_entry_values, block->ssa_entry_value_count,
                                            base_names, versioned_names,
                                            TRANSPILE_SSA_MAP_CAPACITY,
                                            &map_count)) {
        transpiler_free_ssa_name_entries(base_names, map_count);
        return false;
    }
    if (!transpiler_rebuild_ssa_map(ssa_map, base_names, versioned_names, map_count))
        return false;
    transpiler_free_ssa_name_entries(base_names, map_count);
    return true;
}

static char *
transpiler_make_c_ssa_name(const char *versioned_name)
{
    if (versioned_name == NULL)
        return NULL;
    char base[128];
    size_t version = 0;
    if (!transpiler_parse_versioned_name(versioned_name, base, sizeof(base), &version))
        return pergyra_strdup(versioned_name);
    return strdup_fmt("_pgy_ssa_%s_%zu", base, version);
}

static const char *
transpiler_infer_local_type_name_from_expr(const ASTNode *func_decl, ASTNode *expr)
{
    if (expr == NULL)
        return NULL;
    switch (expr->type) {
        case AST_NUMBER:
            return "Int";
        case AST_STRING:
            return "String";
        case AST_BOOLEAN:
            return "Bool";
        case AST_IDENTIFIER:
            return transpiler_find_local_type_name(func_decl, expr->data.identifier.name);
        case AST_BINARY:
            switch (expr->data.binary.op.type) {
                case TOKEN_EQUAL:
                case TOKEN_NOT_EQUAL:
                case TOKEN_LESS:
                case TOKEN_GREATER:
                case TOKEN_LESS_EQUAL:
                case TOKEN_GREATER_EQUAL:
                case TOKEN_AND:
                case TOKEN_OR:
                    return "Bool";
                default:
                    return transpiler_infer_local_type_name_from_expr(func_decl, expr->data.binary.left);
            }
        case AST_UNARY:
            if (expr->data.unary.op.type == TOKEN_NOT)
                return "Bool";
            return transpiler_infer_local_type_name_from_expr(func_decl, expr->data.unary.operand);
        default:
            return NULL;
    }
}

static const char *
transpiler_find_local_type_name_in_block(const ASTNode *func_decl, ASTNode *body, const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            const char *found = transpiler_find_local_type_name_in_block(
                func_decl, body->data.block.statements[i], base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        if (body->data.let_decl.type != NULL) {
            static char *rendered = NULL;
            free(rendered);
            rendered = render_type_name(body->data.let_decl.type);
            return rendered;
        }
        return transpiler_infer_local_type_name_from_expr(func_decl, body->data.let_decl.initializer);
    }
    if (body->type == AST_IF_STMT) {
        const char *found = transpiler_find_local_type_name_in_block(func_decl, body->data.if_stmt.then_branch, base_name);
        if (found != NULL)
            return found;
        return transpiler_find_local_type_name_in_block(func_decl, body->data.if_stmt.else_branch, base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_local_type_name_in_block(func_decl, body->data.while_loop.body, base_name);
    return NULL;
}

static const char *
transpiler_find_local_type_name(const ASTNode *func_decl, const char *base_name)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || base_name == NULL)
        return NULL;
    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *p = func_decl->data.func_decl.params[i];
        if (p != NULL && p->name != NULL && strcmp(p->name, base_name) == 0 && p->type != NULL) {
            static char *rendered_param = NULL;
            free(rendered_param);
            rendered_param = render_type_name(p->type);
            return rendered_param;
        }
    }
    return transpiler_find_local_type_name_in_block(func_decl, func_decl->data.func_decl.body, base_name);
}

static bool
transpiler_mir_type_supported(const char *type_name)
{
    if (type_name == NULL)
        return false;
    return strcmp(type_name, "Int") == 0
           || strcmp(type_name, "Long") == 0
           || strcmp(type_name, "Float") == 0
           || strcmp(type_name, "Bool") == 0
           || strcmp(type_name, "String") == 0
           || strncmp(type_name, "Slot<", 5) == 0
           || strncmp(type_name, "SecureSlot<", 11) == 0
           || strncmp(type_name, "DeviceSlot<", 11) == 0;
}

static bool
transpiler_mir_function_signature_supported(const ASTNode *func_decl)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;

    if (func_decl->data.func_decl.return_type != NULL) {
        char *return_type = render_type_name(func_decl->data.func_decl.return_type);
        bool ok = transpiler_mir_type_supported(return_type);
        free(return_type);
        if (!ok)
            return false;
    }

    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *param = func_decl->data.func_decl.params[i];
        char *param_type = NULL;
        bool ok;
        if (param == NULL || param->type == NULL)
            continue;
        param_type = render_type_name(param->type);
        ok = transpiler_mir_type_supported(param_type);
        free(param_type);
        if (!ok)
            return false;
    }

    return true;
}

static char *
emit_expression_with_ssa_map(ASTNode *node, TranspilerCtx *ctx,
                             const TranspilerSSANameMap *ssa_map)
{
    if (node == NULL)
        return pergyra_strdup("0");
    if (node->type == AST_IDENTIFIER && node->data.identifier.name != NULL) {
        const char *versioned_name = transpiler_resolve_ssa_name(ssa_map,
                                                                node->data.identifier.name);
        if (versioned_name != NULL) {
            return transpiler_make_c_ssa_name(versioned_name);
        }
        return emit_expression(node, ctx);
    }
    if (node->type == AST_NUMBER || node->type == AST_STRING || node->type == AST_BOOLEAN)
        return emit_expression(node, ctx);
    if (node->type == AST_BINARY) {
        char *left = emit_expression_with_ssa_map(node->data.binary.left, ctx,
                                                  ssa_map);
        char *right = emit_expression_with_ssa_map(node->data.binary.right, ctx,
                                                  ssa_map);
        const char *lt = infer_expression_type_name(ctx, node->data.binary.left);
        const char *rt = infer_expression_type_name(ctx, node->data.binary.right);
        bool string_concat =
            node->data.binary.op.type == TOKEN_PLUS
            && (((lt != NULL && strcmp(lt, "String") == 0)
                 || (rt != NULL && strcmp(rt, "String") == 0))
                || (node->data.binary.left != NULL
                    && node->data.binary.left->type == AST_STRING)
                || (node->data.binary.right != NULL
                    && node->data.binary.right->type == AST_STRING));
        bool string_eq =
            (node->data.binary.op.type == TOKEN_EQUAL
             || node->data.binary.op.type == TOKEN_NOT_EQUAL)
            && (((lt != NULL && strcmp(lt, "String") == 0)
                 || (rt != NULL && strcmp(rt, "String") == 0))
                || (node->data.binary.left != NULL
                    && node->data.binary.left->type == AST_STRING)
                || (node->data.binary.right != NULL
                    && node->data.binary.right->type == AST_STRING));
        char *expr;
        if (string_concat) {
            expr = strdup_fmt("StringConcat(%s, %s)", left, right);
        } else if (string_eq) {
            if (node->data.binary.op.type == TOKEN_EQUAL)
                expr = strdup_fmt("pgy_string_equals(%s, %s)", left, right);
            else
                expr = strdup_fmt("(!pgy_string_equals(%s, %s))", left, right);
        } else {
            expr = strdup_fmt("(%s %s %s)", left,
                binary_op_to_c(node->data.binary.op.type), right);
        }
        free(left);
        free(right);
        return expr;
    }
    if (node->type == AST_UNARY) {
        char *operand = emit_expression_with_ssa_map(node->data.unary.operand, ctx,
                                                    ssa_map);
        char *expr = strdup_fmt("(%s%s)", token_type_to_string(node->data.unary.op.type), operand);
        free(operand);
        return expr;
    }
    return emit_expression(node, ctx);
}

static bool
transpiler_emit_mir_phi_copies(CodeBuf *buf, int indent,
                               const MIRBasicBlock *pred_block,
                               const MIRBasicBlock *target_block)
{
    if (buf == NULL || pred_block == NULL || target_block == NULL)
        return false;
    for (size_t i = 0; i < target_block->instruction_count; i++) {
        const MIRInstruction *inst = &target_block->instructions[i];
        if (inst->kind != MIR_INST_PHI || inst->result_name == NULL)
            continue;
        for (size_t j = 0; j < inst->phi_incoming_count; j++) {
            if (inst->phi_incomings[j].predecessor_block != pred_block->id
                || inst->phi_incomings[j].value_name == NULL) {
                continue;
            }
            char *lhs = transpiler_make_c_ssa_name(inst->result_name);
            char *rhs = transpiler_make_c_ssa_name(inst->phi_incomings[j].value_name);
            write_indent_to(buf, indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);
        }
    }
    return true;
}

static bool
transpiler_emit_mir_block_statements(CodeBuf *buf, const ASTNode *func_decl,
                                     const MIRRoutine *mir_routine,
                                     const MIRBasicBlock *block,
                                     TranspilerCtx *ctx,
                                     TranspilerSSANameMap *out_ssa_map)
{
    const HIRBasicBlock *hir_block = transpiler_find_hir_block_for_mir(mir_routine, block->id);
    TranspilerSSANameMap ssa_map;
    TranspilerSSANameMap *ssa_map_out = &ssa_map;

    if (buf == NULL || func_decl == NULL || mir_routine == NULL || block == NULL || ctx == NULL)
        return false;

    if (out_ssa_map != NULL)
        ssa_map_out = out_ssa_map;
    if (!transpiler_emit_mir_block_with_ssa_map(ssa_map_out, block)) {
        return false;
    }
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        char base[128];
        size_t version = 0;
        if (inst->kind != MIR_INST_PHI || inst->result_name == NULL)
            continue;
        if (!transpiler_parse_versioned_name(inst->result_name, base, sizeof(base), &version))
            continue;
        if (!transpiler_ssa_name_map_set(ssa_map_out, base, inst->result_name))
            return false;
    }

    if (hir_block != NULL) {
        size_t def_index = 0;
        for (size_t i = 0; i < hir_block->statement_count; i++) {
            ASTNode *stmt = hir_block->statements[i];
            if (stmt == NULL || stmt->type == AST_IF_STMT || stmt->type == AST_RETURN)
                continue;
            if (stmt->type == AST_LET_DECL && stmt->data.let_decl.name != NULL) {
                while (def_index < block->instruction_count
                       && block->instructions[def_index].kind == MIR_INST_PHI)
                    def_index++;
                const MIRInstruction *def_inst =
                    (def_index < block->instruction_count) ? &block->instructions[def_index] : NULL;
                if (def_inst == NULL || def_inst->kind != MIR_INST_DEF
                    || def_inst->arg0 == NULL || def_inst->result_name == NULL
                    || strcmp(def_inst->arg0, stmt->data.let_decl.name) != 0) {
                    /* Dead SSA defs may be removed by MIR DCE. In that case the
                     * original let initializer is semantically dead in the MIR
                     * subset and should not be emitted back into C. */
                    continue;
                }
                char *lhs = transpiler_make_c_ssa_name(def_inst->result_name);
                char *rhs = emit_expression_with_ssa_map(stmt->data.let_decl.initializer, ctx,
                                                        ssa_map_out);
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s = %s;\n", lhs, rhs);
                free(lhs);
                free(rhs);
                if (!transpiler_ssa_name_map_set(ssa_map_out, stmt->data.let_decl.name,
                                                def_inst->result_name)) {
                    return false;
                }
                def_index++;
                continue;
            }
            if (stmt->type == AST_ASSIGNMENT
                && stmt->data.assignment.target != NULL
                && stmt->data.assignment.target->type == AST_IDENTIFIER) {
                while (def_index < block->instruction_count
                       && block->instructions[def_index].kind == MIR_INST_PHI)
                    def_index++;
                const MIRInstruction *def_inst =
                    (def_index < block->instruction_count) ? &block->instructions[def_index] : NULL;
                const char *target_name = stmt->data.assignment.target->data.identifier.name;
                if (def_inst == NULL || def_inst->kind != MIR_INST_DEF
                    || def_inst->arg0 == NULL || def_inst->result_name == NULL
                    || strcmp(def_inst->arg0, target_name) != 0) {
                    /* Dead assignment versions are also removed by MIR DCE. */
                    continue;
                }
                char *lhs = transpiler_make_c_ssa_name(def_inst->result_name);
                char *rhs = emit_expression_with_ssa_map(stmt->data.assignment.value, ctx,
                                                        ssa_map_out);
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s = %s;\n", lhs, rhs);
                free(lhs);
                free(rhs);
                if (!transpiler_ssa_name_map_set(ssa_map_out, target_name, def_inst->result_name)) {
                    return false;
                }
                def_index++;
                continue;
            }
            emit_statement(stmt, ctx);
            continue;
        }
    }
    return true;
}

static bool
transpiler_can_emit_function_from_mir(const TranspilerCtx *ctx,
                                      const ASTNode *func_decl,
                                      const MIRRoutine **mir_routine_out)
{
    return transpiler_can_emit_function_from_mir_with_reason(
        ctx, func_decl, mir_routine_out, NULL, 0);
}

static bool
transpiler_can_emit_intent_cleanup_from_mir(const TranspilerCtx *ctx,
                                            const ASTNode *intent_decl,
                                            const MIRRoutine **mir_routine_out)
{
    return transpiler_can_emit_intent_cleanup_from_mir_with_reason(
        ctx, intent_decl, mir_routine_out, NULL, 0);
}

static bool
transpiler_expr_identifiers_mapped(const ASTNode *expr,
                                  const TranspilerSSANameMap *ssa_map,
                                  const char *routine_name,
                                  char *reason,
                                  size_t reason_cap)
{
    if (expr == NULL)
        return true;
    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL) {
        if (transpiler_resolve_ssa_name(ssa_map, expr->data.identifier.name) != NULL)
            return true;
        if (reason != NULL && reason_cap > 0) {
            snprintf(reason, reason_cap,
                     "MIR contract breach in %s at line %u: unresolved identifier `%s` (expected SSA-mapped local)",
                     routine_name != NULL ? routine_name : "<routine>",
                     expr->line,
                     expr->data.identifier.name);
        }
        return false;
    }

    switch (expr->type) {
        case AST_BINARY:
            if (!transpiler_expr_identifiers_mapped(expr->data.binary.left, ssa_map,
                                                   routine_name, reason, reason_cap))
                return false;
            return transpiler_expr_identifiers_mapped(expr->data.binary.right, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_UNARY:
            return transpiler_expr_identifiers_mapped(expr->data.unary.operand, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CALL:
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type != AST_IDENTIFIER) {
                if (!transpiler_expr_identifiers_mapped(expr->data.call.callee, ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            } else if (expr->data.call.callee != NULL
                       && expr->data.call.callee->type == AST_IDENTIFIER
                       && expr->data.call.callee->data.identifier.name != NULL) {
                const char *call_target = expr->data.call.callee->data.identifier.name;
                if (transpiler_resolve_ssa_name(ssa_map, call_target) != NULL
                    && !transpiler_expr_identifiers_mapped(expr->data.call.callee, ssa_map,
                                                          routine_name, reason, reason_cap))
                    return false;
            }
            for (size_t i = 0; i < expr->data.call.arg_count; i++) {
                if (!transpiler_expr_identifiers_mapped(expr->data.call.arguments[i], ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            }
            return true;
        case AST_MEMBER_ACCESS:
            return transpiler_expr_identifiers_mapped(expr->data.member.object, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_ARRAY_ACCESS:
            if (!transpiler_expr_identifiers_mapped(expr->data.array_access.array, ssa_map,
                                                   routine_name, reason, reason_cap))
                return false;
            return transpiler_expr_identifiers_mapped(expr->data.array_access.index, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_ARRAY_LITERAL: {
            const ASTNode **elements = expr->data.array_literal.elements;
            for (size_t i = 0; i < expr->data.array_literal.count; i++) {
                if (!transpiler_expr_identifiers_mapped(elements[i], ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            }
            return true;
        }
        case AST_ASSIGNMENT:
            return transpiler_expr_identifiers_mapped(expr->data.assignment.target, ssa_map,
                                                     routine_name, reason, reason_cap)
                && transpiler_expr_identifiers_mapped(expr->data.assignment.value, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_AWAIT_EXPR:
            return transpiler_expr_identifiers_mapped(expr->data.await_expr.expression, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CHANNEL_SEND:
            return transpiler_expr_identifiers_mapped(expr->data.channel_send.channel, ssa_map,
                                                     routine_name, reason, reason_cap)
                && transpiler_expr_identifiers_mapped(expr->data.channel_send.value, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CHANNEL_RECV:
            return transpiler_expr_identifiers_mapped(expr->data.channel_recv.channel, ssa_map,
                                                     routine_name, reason, reason_cap);
        default:
            return true;
    }
}

static bool
transpiler_has_mapping_for_all_emitted_blocks(const MIRRoutine *routine,
                                             const ASTNode *func_decl,
                                             bool require_non_cleanup,
                                             char *reason,
                                             size_t reason_cap)
{
    const char *routine_name = routine != NULL && routine->name != NULL ? routine->name : "<routine>";
    if (routine == NULL || routine->blocks == NULL)
        return false;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        TranspilerSSANameMap ssa_map;

        if (block == NULL || (require_non_cleanup && block->is_cleanup)
            || !block->is_reachable)
            continue;
        if (!transpiler_emit_mir_block_with_ssa_map(&ssa_map, block))
            return false;
        /* Add function parameters to SSA map - they're valid C identifiers, not SSA vars */
        if (func_decl != NULL && func_decl->type == AST_FUNC_DECL) {
            for (size_t p = 0; p < func_decl->data.func_decl.param_count; p++) {
                FuncParam *param = func_decl->data.func_decl.params[p];
                if (param != NULL && param->name != NULL) {
                    transpiler_ssa_name_map_set(&ssa_map, param->name, param->name);
                }
            }
        }
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if ((inst->kind == MIR_INST_BRANCH || inst->kind == MIR_INST_RETURN
                 || inst->kind == MIR_INST_RESOURCE_OP || inst->kind == MIR_INST_CLEANUP_EDGE)
                && inst->ast != NULL) {
                if (!transpiler_expr_identifiers_mapped(inst->ast, &ssa_map, routine_name,
                                                       reason, reason_cap))
                    return false;
            }
            if (inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI) {
                char base[128];
                size_t version = 0;
                const char *base_name = (inst->kind == MIR_INST_PHI
                                         ? inst->name : inst->arg0);
                if (base_name != NULL && base_name[0] != '\0'
                    && inst->result_name != NULL
                    && transpiler_parse_versioned_name(inst->result_name, base, sizeof(base), &version)) {
                    if (!transpiler_ssa_name_map_set(&ssa_map, base_name, inst->result_name))
                        return false;
                }
            }
        }
    }
    return true;
}

static bool
transpiler_validate_mir_emission_contract(const MIRRoutine *routine,
                                         const ASTNode *decl,
                                         bool require_cleanup,
                                         bool require_cleanup_blocks,
                                         char *reason,
                                         size_t reason_cap)
{
    const char *routine_name = "<routine>";
    const char *decl_name = NULL;

    if (decl != NULL) {
        if (decl->type == AST_FUNC_DECL && decl->data.func_decl.name != NULL)
            decl_name = decl->data.func_decl.name;
        if (decl->type == AST_INTENT_DECL && decl->data.intent_decl.name != NULL)
            decl_name = decl->data.intent_decl.name;
    }
    routine_name = decl_name != NULL ? decl_name
        : (routine != NULL && routine->name != NULL ? routine->name : "<routine>");

    if (routine == NULL || routine->blocks == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "MIR contract invalid for %s: no routine", routine_name);
        return false;
    }
    {
        char *topology_error = NULL;
        if (!mir_validate_emission_topology(routine,
                                           require_cleanup,
                                           !require_cleanup_blocks,
                                           &topology_error)) {
            if (reason != NULL && reason_cap > 0) {
                if (topology_error != NULL)
                    snprintf(reason, reason_cap, "%s", topology_error);
                else
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: topology validation failed",
                             routine_name);
            }
            free(topology_error);
            return false;
        }
        free(topology_error);
    }

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];

        if (block == NULL)
            return false;
        if (block->has_succ_true && block->succ_true >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %zu bad true successor",
                         routine_name, block->id);
            return false;
        }
        if (block->has_succ_false && block->succ_false >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %zu bad false successor",
                         routine_name, block->id);
            return false;
        }
        if (block->has_cleanup_succ && block->cleanup_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %zu bad cleanup successor",
                         routine_name, block->id);
            return false;
        }
        if (block->has_rollback_succ && block->rollback_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %zu bad rollback successor",
                         routine_name, block->id);
            return false;
        }
        if (block->has_invalidation_succ && block->invalidation_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %zu bad invalidation successor",
                         routine_name, block->id);
            return false;
        }

        for (size_t j = 0; j < block->instruction_count; j++) {
            MIRInstKind kind = block->instructions[j].kind;
            if (kind != MIR_INST_BRANCH && kind != MIR_INST_RETURN
                && kind != MIR_INST_RESOURCE_OP && kind != MIR_INST_CLEANUP_EDGE
                && kind != MIR_INST_PHI && kind != MIR_INST_DEF) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap, "MIR contract invalid for %s: unsupported instruction kind %d in block %zu",
                             routine_name, (int)kind, block->id);
                return false;
            }
            if ((kind == MIR_INST_BRANCH || kind == MIR_INST_RETURN)
                && require_cleanup_blocks && block->is_cleanup) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: cleanup block %zu has terminal instruction",
                             routine_name, block->id);
                return false;
            }
        }
    }
    if (!transpiler_has_mapping_for_all_emitted_blocks(routine, decl,
                                                       !require_cleanup_blocks,
                                                       reason,
                                                       reason_cap)) {
        return false;
    }
    return true;
}

static bool
transpiler_can_emit_function_from_mir_with_reason(const TranspilerCtx *ctx,
                                                 const ASTNode *func_decl,
                                                 const MIRRoutine **mir_routine_out,
                                                 char *reason,
                                                 size_t reason_cap)
{
    const MIRRoutine *routine = transpiler_find_mir_function(ctx, func_decl);
    if (mir_routine_out != NULL)
        *mir_routine_out = NULL;
    if (reason != NULL && reason_cap > 0)
        reason[0] = '\0';
    if (routine == NULL || func_decl == NULL || func_decl->type != AST_FUNC_DECL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function cannot lower to MIR: no matching MIR routine");
        return false;
    }
    if (routine->kind != MIR_SCOPE_FUNCTION) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has wrong MIR kind: %d", func_decl->data.func_decl.name, routine->kind);
        return false;
    }
    if (routine->hir_routine == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has no HIR routine in MIR", func_decl->data.func_decl.name);
        return false;
    }
    if (routine->hir_routine->is_hosted) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s is hosted", func_decl->data.func_decl.name);
        return false;
    }
    if (routine->hir_routine->is_action_like) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s is action-like", func_decl->data.func_decl.name);
        return false;
    }
    if (routine->has_cleanup_block) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has cleanup block", func_decl->data.func_decl.name);
        return false;
    }
    if (!transpiler_mir_function_signature_supported(func_decl)) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has unsupported MIR signature", func_decl->data.func_decl.name);
        return false;
    }
    if (!transpiler_validate_mir_emission_contract(routine,
                                                   func_decl,
                                                   false,
                                                   false,
                                                   reason,
                                                   reason_cap)) {
        return false;
    }
    if (!transpiler_has_mapping_for_all_emitted_blocks(routine, func_decl, true, reason, reason_cap)) {
        return false;
    }
    if (mir_routine_out != NULL)
        *mir_routine_out = routine;
    return true;
}

static bool
transpiler_can_emit_intent_cleanup_from_mir_with_reason(const TranspilerCtx *ctx,
                                                       const ASTNode *intent_decl,
                                                       const MIRRoutine **mir_routine_out,
                                                       char *reason,
                                                       size_t reason_cap)
{
    const MIRRoutine *routine = transpiler_find_mir_intent(ctx, intent_decl);
    if (mir_routine_out != NULL)
        *mir_routine_out = NULL;
    if (reason != NULL && reason_cap > 0)
        reason[0] = '\0';
    if (routine == NULL || intent_decl == NULL || intent_decl->type != AST_INTENT_DECL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "intent cannot lower to MIR: no matching MIR routine");
        return false;
    }
    if (routine->kind != MIR_SCOPE_INTENT
        || routine->hir_routine == NULL
        || !routine->has_cleanup_block) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "intent %s has no MIR cleanup section", intent_decl->data.intent_decl.name);
        return false;
    }
    if (!transpiler_validate_mir_emission_contract(routine,
                                                   intent_decl,
                                                   true,
                                                   true,
                                                   reason,
                                                   reason_cap)) {
        return false;
    }
    if (!transpiler_has_mapping_for_all_emitted_blocks(routine, intent_decl, false, reason, reason_cap))
        return false;
    if (mir_routine_out != NULL)
        *mir_routine_out = routine;
    return true;
}

static void
transpiler_emit_mir_resource_hook(CodeBuf *out,
                                  int indent,
                                  const MIRInstruction *inst,
                                  const char *handle_expr,
                                  bool cleanup_hook)
{
    const char *slot_anchor = "";
    const char *arg_name = "";
    const char *helper = cleanup_hook
        ? "pgy_mir_cleanup_op_export"
        : "pgy_mir_resource_op_export";

    if (out == NULL || inst == NULL)
        return;

    if (inst->name != NULL) {
        codebuf_write(out, "%*s%s(%s, ", indent * 4, "", helper,
            handle_expr != NULL ? handle_expr : "0");
        codebuf_write(out, "\"%s\", ", inst->name);
    } else {
        codebuf_write(out, "%*s%s(%s, \"unknown\", ", indent * 4, "", helper,
            handle_expr != NULL ? handle_expr : "0");
    }

    if (inst->rir_op != NULL && inst->rir_op->slot_anchor != NULL)
        slot_anchor = inst->rir_op->slot_anchor;
    else if (inst->arg0 != NULL)
        slot_anchor = inst->arg0;
    if (inst->arg1 != NULL)
        arg_name = inst->arg1;
    else if (inst->rir_op != NULL && inst->rir_op->arg0 != NULL)
        arg_name = inst->rir_op->arg0;

    codebuf_write(out, "\"%s\", \"%s\");\n",
        slot_anchor != NULL ? slot_anchor : "",
        arg_name != NULL ? arg_name : "");
}

static void
emit_func_decl_from_mir_named(ASTNode *node, const MIRRoutine *mir_routine,
                              const char *emitted_name, CodeBuf *buf,
                              TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    int saved_slot_count = ctx->slot_var_count;
    int saved_typed_count = ctx->typed_var_count;
    CodeBuf *saved_out = ctx->out;
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    char saved_return_type[128];
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;

    ctx->out = buf;
    g_type_render_ctx = ctx;
    snprintf(saved_return_type, sizeof(saved_return_type), "%s",
        ctx->current_return_type);
    if (node->data.func_decl.return_type != NULL) {
        char *rendered = render_type_name(node->data.func_decl.return_type);
        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
            "%s", rendered);
        free(rendered);
    } else {
        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type), "Void");
    }

    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = "int32_t";
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (i > 0)
            codebuf_write(params_sig, ", ");
        if (p->type != NULL)
            type_name = render_type_name(p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            const char *inner = slot_inner_type_name(type_name);
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE) {
            decl = pergyra_ast_typed_declarator(p->type, p->name);
            codebuf_write(params_sig, "%s", decl);
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

    header_decl = pergyra_func_signature_declarator(node->data.func_decl.return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(ctx->out, "\n%s\n{\n", header_decl);
    free(header_decl);
    codebuf_destroy(params_sig);

    ctx->indent++;
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        if (p == NULL || p->name == NULL || p->type == NULL)
            continue;
        char *type_name = render_type_name(p->type);
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
            if (strncmp(type_name, "Slot<", 5) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), false, boundary_slot);
            else if (strncmp(type_name, "SecureSlot<", 11) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), true, boundary_slot);
            free(type_name);
        }
    }

    for (size_t i = 0; i < mir_routine->block_count; i++) {
        const MIRBasicBlock *block = &mir_routine->blocks[i];
        if (!block->is_reachable || block->is_cleanup)
            continue;
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            const char *type_name = NULL;
            const char *c_type = "int32_t";
            char *c_name = NULL;
            if ((inst->kind != MIR_INST_DEF && inst->kind != MIR_INST_PHI)
                || inst->result_name == NULL) {
                continue;
            }
            const char *lookup_name =
                (inst->kind == MIR_INST_PHI)
                    ? inst->name
                    : (inst->arg0 != NULL ? inst->arg0 : inst->name);
            type_name = transpiler_find_local_type_name(node,
                lookup_name);
            if (type_name != NULL)
                c_type = pergyra_type_to_c(type_name);
            c_name = transpiler_make_c_ssa_name(inst->result_name);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = 0;\n", c_type, c_name);
            free(c_name);
        }
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "/* emitted-from-mir */\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n", name, mir_routine->entry_block);

    for (size_t i = 0; i < mir_routine->block_count; i++) {
        const MIRBasicBlock *block = &mir_routine->blocks[i];
        TranspilerSSANameMap block_ssa_map;
        bool block_emitted;
        bool terminator_emitted = false;
        if (block->is_cleanup || !block->is_reachable)
            continue;
        transpiler_emit_mir_block_mapping_comment(ctx->out, ctx->indent,
                                                 name,
                                                 mir_routine,
                                                 block);
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n", name, block->id);
        block_emitted = transpiler_emit_mir_block_statements(ctx->out, node, mir_routine,
                                                           block, ctx, &block_ssa_map);
        if (!block_emitted) {
            transpiler_ssa_map_clear(&block_ssa_map);
            write_indent(ctx);
            codebuf_write(ctx->out, "/* MIR block emission fallback unavailable */\n");
        }
        /* Ensure function parameters are in SSA map for expression resolution in branches/returns */
        for (size_t p = 0; p < node->data.func_decl.param_count; p++) {
            FuncParam *param = node->data.func_decl.params[p];
            if (param != NULL && param->name != NULL) {
                if (transpiler_resolve_ssa_name(&block_ssa_map, param->name) == NULL) {
                    transpiler_ssa_name_map_set(&block_ssa_map, param->name, param->name);
                }
            }
        }
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_RESOURCE_OP) {
                transpiler_emit_mir_resource_hook(ctx->out, ctx->indent, inst, "0", false);
            } else if (inst->kind == MIR_INST_BRANCH) {
                char *cond = emit_expression_with_ssa_map(inst->ast, ctx,
                    &block_ssa_map);
                if (block->has_succ_true)
                    transpiler_emit_mir_phi_copies(ctx->out, ctx->indent, block,
                        &mir_routine->blocks[block->succ_true]);
                write_indent(ctx);
                codebuf_write(ctx->out, "if (%s) {\n", cond != NULL ? cond : "false");
                write_indent_to(ctx->out, ctx->indent + 1);
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n", name, block->succ_true);
                write_indent(ctx);
                codebuf_write(ctx->out, "} else {\n");
                if (block->has_succ_false)
                    transpiler_emit_mir_phi_copies(ctx->out, ctx->indent + 1, block,
                        &mir_routine->blocks[block->succ_false]);
                write_indent_to(ctx->out, ctx->indent + 1);
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n", name, block->succ_false);
                write_indent(ctx);
                codebuf_write(ctx->out, "}\n");
                free(cond);
                terminator_emitted = true;
            } else if (inst->kind == MIR_INST_RETURN) {
                if (inst->ast != NULL) {
                    char *ret_expr = emit_expression_with_ssa_map(inst->ast, ctx,
                        &block_ssa_map);
                    write_indent(ctx);
                    codebuf_write(ctx->out, "return %s;\n", ret_expr);
                    free(ret_expr);
                } else {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "return;\n");
                }
                terminator_emitted = true;
            }
        }
        if (!terminator_emitted && block->has_succ_true) {
            transpiler_emit_mir_phi_copies(ctx->out, ctx->indent, block,
                &mir_routine->blocks[block->succ_true]);
            write_indent(ctx);
            codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n", name, block->succ_true);
        }
    }

    ctx->indent--;
    codebuf_write(ctx->out, "}\n");
    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
        "%s", saved_return_type);
    g_type_render_ctx = saved_render_ctx;
    ctx->out = saved_out;
}

void
emit_func_decl_named(ASTNode *node, const char *emitted_name,
                     CodeBuf *buf, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_routine = NULL;
    if (transpiler_can_emit_function_from_mir(ctx, node, &mir_routine)) {
        emit_func_decl_from_mir_named(node, mir_routine, emitted_name, buf, ctx);
        return;
    }
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    int saved_slot_count = ctx->slot_var_count;
    int saved_typed_count = ctx->typed_var_count;
    CodeBuf *saved_out = ctx->out;
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    char saved_return_type[128];
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    ctx->out = buf;
    g_type_render_ctx = ctx;
    snprintf(saved_return_type, sizeof(saved_return_type), "%s",
        ctx->current_return_type);
    if (node->data.func_decl.return_type != NULL) {
        {
            char *rendered = render_type_name(node->data.func_decl.return_type);
            snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                "%s", rendered);
            free(rendered);
        }
    } else {
        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type), "Void");
    }

    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = "int32_t";
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (i > 0) codebuf_write(params_sig, ", ");
        if (p->type != NULL)
            type_name = render_type_name(p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            const char *inner = slot_inner_type_name(type_name);
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE) {
            decl = pergyra_ast_typed_declarator(p->type, p->name);
            codebuf_write(params_sig, "%s", decl);
        } else if (p->name != NULL && strcmp(p->name, "self") != 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            /* Subject parameters are passed by pointer (reference semantics) */
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(type_name);
    }
    header_decl = pergyra_func_signature_declarator(node->data.func_decl.return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(ctx->out, "\n%s\n{\n", header_decl);
    free(header_decl);
    codebuf_destroy(params_sig);

    ctx->indent++;
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        if (p == NULL || p->name == NULL || p->type == NULL)
            continue;
        char *type_name = render_type_name(p->type);
        if (type_name != NULL) {
            bool boundary_slot = (strncmp(type_name, "Slot<", 5) == 0
                               || strncmp(type_name, "SecureSlot<", 11) == 0)
                && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
            register_typed_var(ctx, p->name, type_name);
            /* Mark pointer-self parameters (subject/relation/effect/zone/world) for -> access */
            if (p->name != NULL && strcmp(p->name, "self") != 0
                && is_pointer_self_host_type_name(ctx, type_name)) {
                TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                if (entry != NULL)
                    entry->is_subject_ref = true;
            }
            if (strncmp(type_name, "Slot<", 5) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), false, boundary_slot);
            else if (strncmp(type_name, "SecureSlot<", 11) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), true, boundary_slot);
            free(type_name);
        }
    }
    if (node->data.func_decl.body != NULL)
        emit_block(node->data.func_decl.body, ctx);
    ctx->indent--;
    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
        "%s", saved_return_type);

    codebuf_write(ctx->out, "}\n");
    g_type_render_ctx = saved_render_ctx;
    ctx->out = saved_out;
}

void
emit_func_decl(ASTNode *node, TranspilerCtx *ctx)
{
    emit_func_decl_named(node, node->data.func_decl.name, ctx->out, ctx);
}

void
emit_extern_block(ASTNode *node, TranspilerCtx *ctx)
{
    codebuf_write(ctx->out, "\n/* extern \"%s\" */\n",
                  node->data.extern_block.abi != NULL
                    ? node->data.extern_block.abi : "");

    for (size_t i = 0; i < node->data.extern_block.count; i++) {
        ASTNode *decl = node->data.extern_block.declarations[i];
        if (decl == NULL || decl->type != AST_FUNC_DECL)
            continue;

        const char *name = decl->data.func_decl.name;
        const char *ret_type = "void";
        if (decl->data.func_decl.return_type != NULL) {
            ret_type = pergyra_ast_type_to_c(decl->data.func_decl.return_type);
        }

        codebuf_write(ctx->out, "%s %s(", ret_type, name);

        for (size_t j = 0; j < decl->data.func_decl.param_count; j++) {
            FuncParam *p = decl->data.func_decl.params[j];
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            if (j > 0) {
                codebuf_write(ctx->out, ", ");
            }
            codebuf_write(ctx->out, "%s %s", pt, p->name);
        }

        codebuf_write(ctx->out, ");\n");
    }
}

/* -----------------------------------------------------------------
 * Class declaration emitter
 * ----------------------------------------------------------------- */

/* -----------------------------------------------------------------
 * Generic class monomorphization
 * ----------------------------------------------------------------- */

static bool
class_has_generic_params(ASTNode *node)
{
    return node != NULL
        && node->type == AST_CLASS_DECL
        && node->data.class_decl.generic_params != NULL
        && node->data.class_decl.generic_params->count > 0;
}

/* Ensure a monomorphized specialization of a generic class exists.
 * Returns the specialized name (e.g. "Node_Int") that should be used
 * as the C struct type name.  The struct + methods are emitted into
 * ctx->helpers on first invocation.
 *
 * `ann` is the AST_TYPE node for the annotation (e.g. Node<Int>).
 * We extract generic_args from it and match them to class_decl's
 * generic_params to build the bindings. */
static const char *
ensure_generic_class_specialization(TranspilerCtx *ctx,
                                     ASTNode *class_decl,
                                     ASTNode *ann)
{
    GenericParams *gp = class_decl->data.class_decl.generic_params;
    GenericParams *ga = ann->data.type.generic_args;
    if (gp == NULL || ga == NULL || gp->count != ga->count)
        return class_decl->data.class_decl.name;

    /* Build specialized name: ClassName_Arg1_Arg2 */
    CodeBuf *nbuf = codebuf_create();
    codebuf_write(nbuf, "%s", class_decl->data.class_decl.name);
    for (size_t i = 0; i < ga->count; i++) {
        codebuf_write(nbuf, "_");
        GenericParam *garg = ga->params[i];
        if (garg != NULL && garg->name != NULL)
            append_mangled_type_name(nbuf, garg->name);
        else if (garg != NULL && garg->constraint != NULL
                 && garg->constraint->type == AST_TYPE
                 && garg->constraint->data.type.name != NULL)
            append_mangled_type_name(nbuf, garg->constraint->data.type.name);
        else
            codebuf_write(nbuf, "Unknown");
    }

    /* Check if already emitted */
    for (int i = 0; i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name, nbuf->data) == 0) {
            const char *result = ctx->generic_class_specs[i].specialized_name;
            codebuf_destroy(nbuf);
            return result;
        }
    }

    /* Register new specialization */
    if (ctx->generic_class_spec_count >= MAX_GENERIC_CLASS_SPECIALIZATIONS) {
        codebuf_destroy(nbuf);
        return class_decl->data.class_decl.name;
    }

    GenericClassSpecEntry *entry = &ctx->generic_class_specs[ctx->generic_class_spec_count++];
    entry->class_decl = class_decl;
    snprintf(entry->specialized_name, sizeof(entry->specialized_name), "%s", nbuf->data);
    entry->emitted = true;
    const char *spec_name = entry->specialized_name;

    /* Build bindings: T -> Int, U -> String, etc. */
    int saved_binding_count = ctx->generic_binding_count;
    for (size_t i = 0; i < gp->count; i++) {
        if (ctx->generic_binding_count >= MAX_GENERIC_BINDINGS)
            break;
        GenericBindingEntry *b = &ctx->generic_bindings[ctx->generic_binding_count++];
        snprintf(b->name, sizeof(b->name), "%s",
                 gp->params[i] != NULL ? gp->params[i]->name : "T");
        /* The concrete type name comes from the annotation's generic_args.
         * ga->params[i] is a GenericParam whose 'name' field holds the
         * actual type name (e.g. "Int") when used as a type argument. */
        const char *concrete = "int32_t";
        if (ga->params[i] != NULL && ga->params[i]->name != NULL)
            snprintf(b->concrete_type, sizeof(b->concrete_type), "%s",
                     ga->params[i]->name);
        else if (ga->params[i] != NULL && ga->params[i]->constraint != NULL
                 && ga->params[i]->constraint->type == AST_TYPE)
            snprintf(b->concrete_type, sizeof(b->concrete_type), "%s",
                     ga->params[i]->constraint->data.type.name);
        else
            snprintf(b->concrete_type, sizeof(b->concrete_type), "%s", concrete);

        entry->bindings[i] = *b;
    }
    entry->binding_count = gp->count;

    /* Set g_type_render_ctx so pergyra_ast_type_to_c resolves T → Int */
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    g_type_render_ctx = ctx;

    /* Emit struct typedef into helpers buffer */
    codebuf_write(ctx->helpers, "\ntypedef struct %s\n{\n", spec_name);
    for (size_t i = 0; i < class_decl->data.class_decl.field_count; i++) {
        ClassField *f = class_decl->data.class_decl.fields[i];
        const char *ft = "int32_t";
        if (f->type != NULL)
            ft = pergyra_ast_type_to_c(f->type);
        codebuf_write(ctx->helpers, "    %s %s;\n", ft, f->name);
    }
    codebuf_write(ctx->helpers, "} %s;\n", spec_name);

    /* Container types */
    codebuf_write(ctx->helpers,
        "\nPGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n",
        spec_name, spec_name,
        spec_name, spec_name,
        spec_name, spec_name);

    /* Methods */
    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, spec_name);
        emit_hosted_method_forward_decl_named(spec_name, method, use_self_cell,
                                              ctx->helpers, ctx);
    }

    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, spec_name);
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type    = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        if (use_self_cell) {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s *self",
                          ret_type, spec_name, method_name, spec_name);
        } else {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s self",
                          ret_type, spec_name, method_name, spec_name);
        }

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->helpers, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->helpers, ")\n{\n");

        {
            int saved_slot_count = ctx->slot_var_count;
            int saved_typed_count = ctx->typed_var_count;
            const char *saved_class_name = ctx->current_class_name;
            CodeBuf *saved_out = ctx->out;

            ctx->current_class_name = spec_name;
            ctx->out = ctx->helpers;
            register_typed_var(ctx, "self", spec_name);

            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                char *tn;
                if (p == NULL || p->name == NULL
                    || strcmp(p->name, "self") == 0 || p->type == NULL)
                    continue;
                tn = render_type_name(p->type);
                if (tn != NULL) {
                    register_typed_var(ctx, p->name, tn);
                    free(tn);
                }
            }

            ctx->indent++;
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            ctx->indent--;

            ctx->out = saved_out;
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            ctx->current_class_name = saved_class_name;
        }

        codebuf_write(ctx->helpers, "}\n");
    }

    /* Restore bindings and render context */
    g_type_render_ctx = saved_render_ctx;
    ctx->generic_binding_count = saved_binding_count;
    codebuf_destroy(nbuf);

    return spec_name;
}

void
emit_class_decl(ASTNode *node, TranspilerCtx *ctx)
{
    /* Generic classes are emitted lazily when first used (monomorphized). */
    if (class_has_generic_params(node))
        return;

    const char *name = node->data.class_decl.name;

    codebuf_write(ctx->out, "\ntypedef struct %s\n{\n", name);

    /* Fields */
    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *f = node->data.class_decl.fields[i];
        const char *ft = "int32_t";
        if (f->type != NULL)
            ft = pergyra_ast_type_to_c(f->type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, f->name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Auto-generate Slot and Result container types for this struct.
     * This allows Slot<MyStruct>, Result<MyStruct> in user code. */
    codebuf_write(ctx->out,
        "\n/* Auto-generated container types for %s */\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n",
        name,
        name, name,
        name, name,
        name, name);

    /* Methods become free functions over a subject self-cell or class value. */
    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, name);
        emit_hosted_method_forward_decl_named(name, method, use_self_cell,
                                              ctx->out, ctx);
    }

    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, name);
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type    = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        if (use_self_cell) {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                          ret_type, name, method_name, name);
        } else {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s self",
                          ret_type, name, method_name, name);
        }

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            {
                char *ptn = (p->type != NULL) ? render_type_name(p->type) : NULL;
                bool subj_param = ptn != NULL && is_pointer_self_host_type_name(ctx, ptn);
                if (subj_param)
                    codebuf_write(ctx->out, ", %s *%s", pt, p->name);
                else
                    codebuf_write(ctx->out, ", %s %s", pt, p->name);
                free(ptn);
            }
        }
        codebuf_write(ctx->out, ")\n{\n");

        {
            int saved_slot_count = ctx->slot_var_count;
            int saved_typed_count = ctx->typed_var_count;
            const char *saved_class_name = ctx->current_class_name;

            ctx->current_class_name = name;
            register_typed_var(ctx, "self", name);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                char *type_name;

                if (p == NULL || p->name == NULL
                    || strcmp(p->name, "self") == 0
                    || p->type == NULL)
                    continue;

                type_name = render_type_name(p->type);
                if (type_name != NULL) {
                    register_typed_var(ctx, p->name, type_name);
                    if (is_pointer_self_host_type_name(ctx, type_name)) {
                        TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                        if (entry != NULL)
                            entry->is_subject_ref = true;
                    }
                    free(type_name);
                }
            }
            ctx->indent++;
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            ctx->indent--;
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            ctx->current_class_name = saved_class_name;
        }

        codebuf_write(ctx->out, "}\n");
    }
}

/* -----------------------------------------------------------------
 * with slot<T> as s { }
 * ----------------------------------------------------------------- */

void
emit_with_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias = node->data.with_stmt.alias;
    bool is_secure    = node->data.with_stmt.is_secure;
    int saved_slot_count = ctx->slot_var_count;
    int saved_typed_count = ctx->typed_var_count;

    const char *inner = "Int";
    if (node->data.with_stmt.slot_type != NULL)
        inner = node->data.with_stmt.slot_type->data.type.name;

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    /* Register alias in slot variable table */
    register_slot_var(ctx, alias, inner, is_secure, false);

    if (is_secure) {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyToken_%s %s_token;\n", inner, alias);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
            inner, alias, inner, alias);
    } else {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySlot_%s %s = pgy_claim_%s();\n",
            inner, alias, inner);
    }

    if (node->data.with_stmt.body != NULL)
        emit_block(node->data.with_stmt.body, ctx);

    /* Auto-release */
    write_indent(ctx);
    if (is_secure) {
        codebuf_write(ctx->out,
            "pgy_secure_release_%s(&%s, &%s_token);\n",
            inner, alias, alias);
    } else {
        codebuf_write(ctx->out,
            "pgy_release_%s(&%s);\n", inner, alias);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
}

/* -----------------------------------------------------------------
 * Parallel block
 * ----------------------------------------------------------------- */

void
emit_parallel_block(ASTNode *node, TranspilerCtx *ctx)
{
    size_t count = node->data.parallel.task_count;
    if (count == 0)
        return;

    unsigned int pid = ctx->parallel_id++;

    /* ---------------------------------------------------------------
     * 1) Generate a context struct that holds pointers to all local
     *    variables currently in scope (slots + non-duplicate typed vars).
     *    Wrapper functions access outer variables through this struct.
     * --------------------------------------------------------------- */
    int n_slots  = ctx->slot_var_count;
    int n_typed  = ctx->typed_var_count;

    /* Helper: check if a typed_var is already in slot_vars (avoid dupes) */
    #define IS_SLOT_DUP(name_str) ({ \
        bool _dup = false; \
        for (int _j = 0; _j < n_slots; _j++) { \
            if (strcmp(ctx->slot_vars[_j].name, (name_str)) == 0) \
                { _dup = true; break; } \
        } _dup; })

    /* Count non-duplicate typed vars */
    int n_unique_typed = 0;
    for (int i = 0; i < n_typed; i++) {
        if (!IS_SLOT_DUP(ctx->typed_vars[i].name))
            n_unique_typed++;
    }

    bool has_captures = (n_slots > 0 || n_unique_typed > 0);

    if (has_captures) {
        codebuf_write(ctx->helpers,
            "typedef struct {\n");
        for (int i = 0; i < n_slots; i++) {
            codebuf_write(ctx->helpers,
                "    PgySlot_%s *%s;\n",
                ctx->slot_vars[i].inner_type,
                ctx->slot_vars[i].name);
        }
        for (int i = 0; i < n_typed; i++) {
            if (IS_SLOT_DUP(ctx->typed_vars[i].name))
                continue;
            const char *c_type = pergyra_type_to_c(ctx->typed_vars[i].type_name);
            codebuf_write(ctx->helpers,
                "    %s *%s;\n", c_type, ctx->typed_vars[i].name);
        }
        codebuf_write(ctx->helpers,
            "} _pgy_par_ctx_%u;\n\n", pid);
    }

    /* ---------------------------------------------------------------
     * 2) Generate static wrapper functions for each task.
     *    Variable references inside the wrapper go through _pctx->.
     * --------------------------------------------------------------- */
    for (size_t i = 0; i < count; i++) {
        codebuf_write(ctx->helpers,
            "static void *_pgy_par_%zu_%u(void *_arg) {\n",
            i, pid);
        if (has_captures) {
            codebuf_write(ctx->helpers,
                "    _pgy_par_ctx_%u *_pctx = "
                "(_pgy_par_ctx_%u *)_arg;\n",
                pid, pid);
        } else {
            codebuf_write(ctx->helpers, "    (void)_arg;\n");
        }

        /* Redirect output to helpers and set parallel-capture mode */
        CodeBuf *saved = ctx->out;
        int saved_indent = ctx->indent;
        bool saved_in_pw = ctx->in_parallel_wrapper;
        int saved_slot_end  = ctx->par_capture_slot_end;
        int saved_typed_end = ctx->par_capture_typed_end;

        ctx->out = ctx->helpers;
        ctx->indent = 1;
        ctx->in_parallel_wrapper  = true;
        ctx->par_capture_slot_end  = n_slots;
        ctx->par_capture_typed_end = n_typed;

        emit_statement(node->data.parallel.tasks[i], ctx);

        ctx->out = saved;
        ctx->indent = saved_indent;
        ctx->in_parallel_wrapper  = saved_in_pw;
        ctx->par_capture_slot_end  = saved_slot_end;
        ctx->par_capture_typed_end = saved_typed_end;

        codebuf_write(ctx->helpers,
            "    return NULL;\n"
            "}\n\n");
    }

    /* ---------------------------------------------------------------
     * 3) Emit context initialization + spawn + await at call site.
     * --------------------------------------------------------------- */
    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    if (has_captures) {
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_par_ctx_%u _pctx%u = { ", pid, pid);
        bool first = true;
        for (int i = 0; i < n_slots; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", ctx->slot_vars[i].name);
            first = false;
        }
        for (int i = 0; i < n_typed; i++) {
            if (IS_SLOT_DUP(ctx->typed_vars[i].name))
                continue;
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", ctx->typed_vars[i].name);
            first = false;
        }
        codebuf_write(ctx->out, " };\n");
    }

    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        if (has_captures) {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_spawn(_pgy_par_%zu_%u, &_pctx%u);\n",
                i, i, pid, pid);
        } else {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_spawn(_pgy_par_%zu_%u, NULL);\n",
                i, i, pid);
        }
    }
    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_await(_ph_%zu);\n", i);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    #undef IS_SLOT_DUP
}

static void
emit_async_block(ASTNode *node, TranspilerCtx *ctx)
{
    unsigned int pid = ctx->parallel_id++;
    int n_slots  = ctx->slot_var_count;
    int n_typed  = ctx->typed_var_count;

    #define IS_SLOT_DUP(name_str) ({ \
        bool _dup = false; \
        for (int _j = 0; _j < n_slots; _j++) { \
            if (strcmp(ctx->slot_vars[_j].name, (name_str)) == 0) \
                { _dup = true; break; } \
        } _dup; })

    int n_unique_typed = 0;
    for (int i = 0; i < n_typed; i++) {
        if (!IS_SLOT_DUP(ctx->typed_vars[i].name))
            n_unique_typed++;
    }

    bool has_captures = (n_slots > 0 || n_unique_typed > 0);

    if (has_captures) {
        codebuf_write(ctx->helpers, "typedef struct {\n");
        for (int i = 0; i < n_slots; i++) {
            codebuf_write(ctx->helpers, "    PgySlot_%s *%s;\n",
                ctx->slot_vars[i].inner_type, ctx->slot_vars[i].name);
        }
        for (int i = 0; i < n_typed; i++) {
            if (IS_SLOT_DUP(ctx->typed_vars[i].name))
                continue;
            codebuf_write(ctx->helpers, "    %s *%s;\n",
                pergyra_type_to_c(ctx->typed_vars[i].type_name),
                ctx->typed_vars[i].name);
        }
        codebuf_write(ctx->helpers, "} _pgy_async_ctx_%u;\n\n", pid);
    }

    codebuf_write(ctx->helpers, "static void *_pgy_async_%u(void *_arg) {\n", pid);
    if (has_captures) {
        codebuf_write(ctx->helpers,
            "    _pgy_async_ctx_%u *_pctx = (_pgy_async_ctx_%u *)_arg;\n",
            pid, pid);
    } else {
        codebuf_write(ctx->helpers, "    (void)_arg;\n");
    }

    CodeBuf *saved = ctx->out;
    int saved_indent = ctx->indent;
    bool saved_in_pw = ctx->in_parallel_wrapper;
    int saved_slot_end  = ctx->par_capture_slot_end;
    int saved_typed_end = ctx->par_capture_typed_end;

    ctx->out = ctx->helpers;
    ctx->indent = 1;
    ctx->in_parallel_wrapper  = true;
    ctx->par_capture_slot_end  = n_slots;
    ctx->par_capture_typed_end = n_typed;

    for (size_t i = 0; i < node->data.async_block.statement_count; i++)
        emit_statement(node->data.async_block.statements[i], ctx);

    ctx->out = saved;
    ctx->indent = saved_indent;
    ctx->in_parallel_wrapper  = saved_in_pw;
    ctx->par_capture_slot_end  = saved_slot_end;
    ctx->par_capture_typed_end = saved_typed_end;

    codebuf_write(ctx->helpers, "    return NULL;\n}\n\n");

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (has_captures) {
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_async_ctx_%u _pctx%u = { ", pid, pid);
        bool first = true;
        for (int i = 0; i < n_slots; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", ctx->slot_vars[i].name);
            first = false;
        }
        for (int i = 0; i < n_typed; i++) {
            if (IS_SLOT_DUP(ctx->typed_vars[i].name))
                continue;
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", ctx->typed_vars[i].name);
            first = false;
        }
        codebuf_write(ctx->out, " };\n");
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "PgyTaskHandle _ah_%u = pgy_async_spawn(_pgy_async_%u, %s);\n",
        pid, pid, has_captures ? "&_pctx" : "NULL");
    write_indent(ctx);
    codebuf_write(ctx->out, "pgy_async_detach(_ah_%u);\n", pid);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    #undef IS_SLOT_DUP
}

/* -----------------------------------------------------------------
 * Control flow
 * ----------------------------------------------------------------- */

void
emit_if_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(node->data.if_stmt.condition, ctx);
    write_indent(ctx);
    codebuf_write(ctx->out, "if (%s)\n", cond);
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.if_stmt.then_branch != NULL)
        emit_block(node->data.if_stmt.then_branch, ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    if (node->data.if_stmt.else_branch != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_statement(node->data.if_stmt.else_branch, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_for_loop(ASTNode *node, TranspilerCtx *ctx)
{
    const char *var = node->data.for_loop.variable;

    /* for-in collection loop: for item in array { } */
    if (node->data.for_loop.iterable != NULL) {
        char *coll = emit_expression(node->data.for_loop.iterable, ctx);
        const char *coll_type = infer_expression_type_name(ctx,
            node->data.for_loop.iterable);
        const char *elem_type = "int32_t";
        if (coll_type != NULL
            && (strncmp(coll_type, "Array<", 6) == 0
                || strncmp(coll_type, "Slice<", 6) == 0
                || strncmp(coll_type, "List<", 5) == 0)) {
            elem_type = pergyra_type_to_c(slot_inner_type_name(coll_type));
        }

        int idx_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "for (size_t _pgy_idx_%d = 0; "
            "_pgy_idx_%d < %s.count; "
            "_pgy_idx_%d++)\n",
            idx_id, idx_id, coll, idx_id);
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = %s.data[_pgy_idx_%d];\n",
            elem_type, var, coll, idx_id);
        register_typed_var(ctx, var, slot_inner_type_name(coll_type));
        if (node->data.for_loop.body != NULL)
            emit_block(node->data.for_loop.body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        free(coll);
        return;
    }

    /* Range loop: for x in start..end { } */
    char *start = emit_expression(node->data.for_loop.range_start, ctx);
    char *end   = emit_expression(node->data.for_loop.range_end,   ctx);

    write_indent(ctx);
    codebuf_write(ctx->out,
        "for (int32_t %s = %s; %s < %s; %s++)\n",
        var, start, var, end, var);
    free(start);
    free(end);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.for_loop.body != NULL)
        emit_block(node->data.for_loop.body, ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

void
emit_while_loop(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(node->data.while_loop.condition, ctx);
    write_indent(ctx);
    codebuf_write(ctx->out, "while (%s)\n", cond);
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.while_loop.body != NULL)
        emit_block(node->data.while_loop.body, ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

/* Check if a match-case pattern is a destructor like Ok(x), Err(x),
 * or a tagged union variant like Circle(r), Rect(w, h) */
static bool
is_result_destructor(ASTNode *pat, const char **kind, const char **binding)
{
    if (pat == NULL || pat->type != AST_CALL)
        return false;
    if (pat->data.call.callee == NULL || pat->data.call.callee->type != AST_IDENTIFIER)
        return false;
    const char *name = pat->data.call.callee->data.identifier.name;
    if (strcmp(name, "Ok") != 0 && strcmp(name, "Err") != 0)
        return false;
    *kind = name;
    if (pat->data.call.arg_count > 0
        && pat->data.call.arguments[0] != NULL
        && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
        *binding = pat->data.call.arguments[0]->data.identifier.name;
    } else {
        *binding = NULL;
    }
    return true;
}

static bool
is_option_destructor(ASTNode *pat, const char **kind, const char **binding)
{
    *kind = NULL;
    *binding = NULL;

    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = pat->data.identifier.name;
        if (name != NULL && strcmp(name, "None") == 0) {
            *kind = "None";
            return true;
        }
        return false;
    }

    if (pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;

    if (strcmp(name, "None") == 0 && pat->data.call.arg_count == 0) {
        *kind = "None";
        return true;
    }
    if (strcmp(name, "Some") == 0 && pat->data.call.arg_count == 1) {
        *kind = "Some";
        if (pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }

    return false;
}

/* Check if pattern is a tagged union variant destructor: Circle(r), Rect(w, h), None */
static bool
is_enum_variant_destructor(ASTNode *pat, TranspilerCtx *ctx,
                           const char **variant_name_out,
                           const char **enum_name_out,
                           const char ***bindings_out,
                           size_t *binding_count_out)
{
    const char *name = NULL;
    size_t argc = 0;

    if (pat == NULL) return false;

    if (pat->type == AST_CALL
        && pat->data.call.callee != NULL
        && pat->data.call.callee->type == AST_IDENTIFIER) {
        name = pat->data.call.callee->data.identifier.name;
        argc = pat->data.call.arg_count;
    } else if (pat->type == AST_IDENTIFIER) {
        name = pat->data.identifier.name;
        argc = 0;
    } else {
        return false;
    }

    if (name == NULL) return false;

    /* Look up in HIR enum declarations */
    if (ctx->hir == NULL) return false;
    for (size_t i = 0; i < ctx->hir->type_count; i++) {
        ASTNode *stmt = ctx->hir->types[i];
        if (stmt == NULL || stmt->type != AST_ENUM_DECL)
            continue;
        /* Only tagged unions (at least one variant has data) */
        bool has_data = false;
        for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
            if (stmt->data.enum_decl.variant_param_counts != NULL
                && stmt->data.enum_decl.variant_param_counts[j] > 0) {
                has_data = true;
                break;
            }
        }
        if (!has_data) continue;

        for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
            if (strcmp(stmt->data.enum_decl.variants[j], name) == 0) {
                *variant_name_out = name;
                *enum_name_out = stmt->data.enum_decl.name;
                /* Collect bindings */
                static const char *bindings_buf[8];
                *binding_count_out = 0;
                for (size_t k = 0; k < argc && k < 8; k++) {
                    ASTNode *arg = pat->data.call.arguments[k];
                    if (arg != NULL && arg->type == AST_IDENTIFIER)
                        bindings_buf[k] = arg->data.identifier.name;
                    else
                        bindings_buf[k] = NULL;
                    (*binding_count_out)++;
                }
                *bindings_out = bindings_buf;
                return true;
            }
        }
    }
    return false;
}

void
emit_match_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    char *subj = emit_expression(node->data.match_stmt.subject, ctx);
    int tmp_id = ctx->tmp_counter++;
    const char *subject_type = infer_expression_type_name(ctx, node->data.match_stmt.subject);
    bool subject_is_option = subject_type != NULL && strncmp(subject_type, "Option<", 7) == 0;

    /* Detect if any case uses Ok()/Err() or tagged union destructuring */
    bool is_result_match = false;
    bool is_option_match = false;
    bool is_enum_match = false;
    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *pat = node->data.match_stmt.cases[i]->data.match_case.pattern;
        const char *k, *b;
        if (subject_is_option && is_option_destructor(pat, &k, &b)) {
            is_option_match = true;
            break;
        }
        if (is_result_destructor(pat, &k, &b)) {
            is_result_match = true;
            break;
        }
        const char *vn, *en; const char **bs; size_t bc;
        if (is_enum_variant_destructor(pat, ctx, &vn, &en, &bs, &bc)) {
            is_enum_match = true;
            break;
        }
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    if (is_result_match || is_option_match || is_enum_match) {
        /* Struct match: store subject as-is (tagged union/result) */
        write_indent(ctx);
        codebuf_write(ctx->out, "__typeof__(%s) __match_%d = %s;\n", subj, tmp_id, subj);
    } else {
        write_indent(ctx);
        codebuf_write(ctx->out, "int32_t __match_%d = %s;\n", tmp_id, subj);
    }
    free(subj);

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        const char *kind = NULL, *binding = NULL;
        bool option_case = subject_is_option
            && is_option_destructor(mc->data.match_case.pattern, &kind, &binding);

        write_indent(ctx);

        if (option_case) {
            const char *tag_val = (strcmp(kind, "Some") == 0)
                ? "PgyOptionSome" : "PgyOptionNone";
            if (i == 0)
                codebuf_write(ctx->out, "if (__match_%d.tag == %s",
                    tmp_id, tag_val);
            else
                codebuf_write(ctx->out, "else if (__match_%d.tag == %s",
                    tmp_id, tag_val);
        } else if (is_result_destructor(mc->data.match_case.pattern, &kind, &binding)) {
            /* case Ok(x): → if (__match_N.tag == PGY_RESULT_OK) { ... } */
            const char *tag_val = (strcmp(kind, "Ok") == 0)
                ? "PgyResultOk" : "PgyResultErr";
            if (i == 0)
                codebuf_write(ctx->out, "if (__match_%d.tag == %s",
                    tmp_id, tag_val);
            else
                codebuf_write(ctx->out, "else if (__match_%d.tag == %s",
                    tmp_id, tag_val);
        } else {
            /* Check for tagged union variant pattern */
            const char *vname = NULL, *ename = NULL;
            const char **bindings = NULL;
            size_t bind_count = 0;
            if (is_enum_variant_destructor(mc->data.match_case.pattern, ctx,
                                            &vname, &ename, &bindings, &bind_count)) {
                if (i == 0)
                    codebuf_write(ctx->out, "if (__match_%d.tag == %s_TAG_%s",
                        tmp_id, ename, vname);
                else
                    codebuf_write(ctx->out, "else if (__match_%d.tag == %s_TAG_%s",
                        tmp_id, ename, vname);
                /* Mark for binding emission below */
                kind = vname;
            } else {
                char *pat = emit_expression(mc->data.match_case.pattern, ctx);
                if (i == 0)
                    codebuf_write(ctx->out, "if (__match_%d == %s", tmp_id, pat);
                else
                    codebuf_write(ctx->out, "else if (__match_%d == %s", tmp_id, pat);
                free(pat);
            }
        }

        if (mc->data.match_case.guard != NULL) {
            char *guard = emit_expression(mc->data.match_case.guard, ctx);
            codebuf_write(ctx->out, " && %s", guard);
            free(guard);
        }
        codebuf_write(ctx->out, ")\n");

        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;

        /* Emit binding variable if destructuring */
        if (binding != NULL && kind != NULL) {
            write_indent(ctx);
            if (strcmp(kind, "Some") == 0)
                codebuf_write(ctx->out, "__typeof__(__match_%d.value) %s = __match_%d.value;\n",
                    tmp_id, binding, tmp_id);
            else if (strcmp(kind, "Ok") == 0)
                codebuf_write(ctx->out, "int32_t %s = __match_%d.ok;\n",
                    binding, tmp_id);
            else if (strcmp(kind, "Err") == 0)
                codebuf_write(ctx->out, "PgyError %s = __match_%d.err;\n",
                    binding, tmp_id);
        }
        /* Emit enum variant bindings: case Circle(r) → int32_t r = __match_N.Circle._0 */
        if (kind != NULL
            && strcmp(kind, "Some") != 0
            && strcmp(kind, "None") != 0
            && strcmp(kind, "Ok") != 0
            && strcmp(kind, "Err") != 0) {
            const char *vn2 = NULL, *en2 = NULL;
            const char **bs2 = NULL;
            size_t bc2 = 0;
            if (is_enum_variant_destructor(mc->data.match_case.pattern, ctx,
                                            &vn2, &en2, &bs2, &bc2)) {
                for (size_t b = 0; b < bc2; b++) {
                    if (bs2[b] != NULL) {
                        write_indent(ctx);
                        codebuf_write(ctx->out,
                            "int32_t %s = __match_%d.%s._%zu;\n",
                            bs2[b], tmp_id, vn2, b);
                    }
                }
            }
        }

        emit_block(mc->data.match_case.body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    if (node->data.match_stmt.default_body != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_block(node->data.match_stmt.default_body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

void
emit_return_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    write_indent(ctx);
    if (node->data.return_stmt.value != NULL) {
        if (ctx->current_return_type[0] != '\0'
            && strncmp(ctx->current_return_type, "Option<", 7) == 0) {
            const char *inner = slot_inner_type_name(ctx->current_return_type);
            ASTNode *value = node->data.return_stmt.value;
            if (value->type == AST_CALL
                && value->data.call.callee != NULL
                && value->data.call.callee->type == AST_IDENTIFIER) {
                const char *callee_name = value->data.call.callee->data.identifier.name;
                if (strcmp(callee_name, "Some") == 0 && value->data.call.arg_count == 1) {
                    char *arg = emit_expression(value->data.call.arguments[0], ctx);
                    codebuf_write(ctx->out, "return Some_%s(%s);\n", inner, arg);
                    free(arg);
                    return;
                }
                if (strcmp(callee_name, "None") == 0 && value->data.call.arg_count == 0) {
                    codebuf_write(ctx->out, "return None_%s();\n", inner);
                    return;
                }
            }
        }
        char *val = emit_expression(node->data.return_stmt.value, ctx);
        codebuf_write(ctx->out, "return %s;\n", val);
        free(val);
    } else {
        codebuf_write(ctx->out, "return;\n");
    }
}

/* -----------------------------------------------------------------
 * Statement dispatcher
 * ----------------------------------------------------------------- */

static void
emit_type_alias_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias_name;
    const char *target_c_type;

    if (node == NULL || node->type != AST_TYPE_ALIAS
        || node->data.type_alias.name == NULL
        || node->data.type_alias.target_type == NULL) {
        return;
    }

    alias_name = node->data.type_alias.name;
    ensure_type_specializations_from_ast(ctx, node->data.type_alias.target_type);
    target_c_type = pergyra_ast_type_to_c(node->data.type_alias.target_type);
    codebuf_write(ctx->out, "typedef %s %s;\n", target_c_type, alias_name);
}

void
emit_statement(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_LET_DECL:
        emit_let_decl(node, ctx);
        break;
    case AST_TYPE_ALIAS:
        emit_type_alias_decl(node, ctx);
        break;
    case AST_LET_DESTRUCTURE:
    {
        /* let (a, b, c) = expr;
         * → auto _tmp = expr;
         *   Type a = _tmp.data[0]; // Array
         *   Type b = _tmp.data[1]; */
        ASTNode *init = node->data.let_destructure.initializer;
        char *init_expr = emit_expression(init, ctx);
        const char *init_type = infer_expression_type_name(ctx, init);
        const char *c_init_type = pergyra_type_to_c(init_type);
        const char *elem_c_type = "int32_t";
        const char *inner = "Int";
        if (init_type != NULL
            && (strncmp(init_type, "Array<", 6) == 0
                || strncmp(init_type, "Slice<", 6) == 0)) {
            inner = slot_inner_type_name(init_type);
            elem_c_type = pergyra_type_to_c(inner);
        }
        int tmp_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s _pgy_destr_%d = %s;\n",
            c_init_type, tmp_id, init_expr);
        for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = _pgy_destr_%d.data[%zu];\n",
                elem_c_type, node->data.let_destructure.names[i], tmp_id, i);
            register_typed_var(ctx, node->data.let_destructure.names[i], inner);
        }
        free(init_expr);
        break;
    }
    case AST_FUNC_DECL:
        emit_func_decl(node, ctx);
        break;
    case AST_CLASS_DECL:
        emit_class_decl(node, ctx);
        break;
    case AST_EXTERN_BLOCK:
        emit_extern_block(node, ctx);
        break;
    case AST_IMPORT_DECL:
        /* Import is resolved at driver level (AST merging).
         * Nothing to emit — the imported declarations are
         * already present in the merged AST. */
        break;
    case AST_USE_DECL:
        /* use module; — standard library modules.
         * Runtime functions are always available via pgy_runtime.h.
         * Future: emit module-specific includes/initializers here. */
        codebuf_write(ctx->out, "/* use %s */\n",
            node->data.use_decl.module_name != NULL
                ? node->data.use_decl.module_name : "unknown");
        break;
    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } → emit body directly (no safety wrappers) */
        write_indent(ctx);
        codebuf_write(ctx->out, "/* unsafe */\n");
        if (node->data.unsafe_block.body != NULL)
            emit_block(node->data.unsafe_block.body, ctx);
        break;
    case AST_DEFER_STMT: {
        /* defer { body } → GCC __attribute__((cleanup)) pattern.
         * 1. Emit a static cleanup function into the helpers buffer.
         * 2. Declare a sentinel variable with cleanup attribute.
         * When the sentinel goes out of scope, GCC calls the cleanup. */
        int defer_id = ctx->defer_counter++;

        /* Generate cleanup function in helpers buffer */
        codebuf_write(ctx->helpers,
            "\nstatic void _pgy_defer_%d(int *_pgy_unused) {\n"
            "    (void)_pgy_unused;\n", defer_id);

        /* Save current output, redirect to helpers for the body */
        CodeBuf *saved_out = ctx->out;
        int saved_indent = ctx->indent;
        ctx->out = ctx->helpers;
        ctx->indent = 1;
        if (node->data.defer_stmt.body != NULL)
            emit_block(node->data.defer_stmt.body, ctx);
        ctx->out = saved_out;
        ctx->indent = saved_indent;

        codebuf_write(ctx->helpers, "}\n");

        /* Emit sentinel with cleanup attribute */
        write_indent(ctx);
        codebuf_write(ctx->out,
            "int _pgy_defer_sentinel_%d "
            "__attribute__((cleanup(_pgy_defer_%d))) = 0;\n",
            defer_id, defer_id);
        break;
    }
    case AST_BIND_STMT: {
        /* bind party.slot = Role;
         * → lookup party's typed_var to get PartyType,
         *   then emit PartyType_bind_slot(&party, NULL, &Role_Ability_vtable_instance)
         * For now: use the typed_var mapping to find the party type. */
        const char *pvar = node->data.bind_stmt.party_var;
        const char *slot = node->data.bind_stmt.slot_name;
        const char *role = node->data.bind_stmt.role_name;
        const char *party_type = NULL;
        for (int ti = 0; ti < ctx->typed_var_count; ti++) {
            if (strcmp(ctx->typed_vars[ti].name, pvar) == 0) {
                party_type = ctx->typed_vars[ti].type_name;
                break;
            }
        }
        if (party_type == NULL) party_type = "UnknownParty";

        /* Find the ability name by scanning the HIR for the party declaration.
         * The dyn role slot records the required ability. */
        const char *ability_name = slot; /* fallback: use slot name */
        if (ctx->hir != NULL) {
            for (size_t hi = 0; hi < ctx->hir->item_count; hi++) {
                ASTNode *it = ctx->hir->items[hi].ast;
                if (it == NULL || it->type != AST_PARTY_DECL)
                    continue;
                if (strcmp(it->data.party_decl.name, party_type) != 0)
                    continue;
                for (size_t ri = 0; ri < it->data.party_decl.role_count; ri++) {
                    ASTNode *rs = it->data.party_decl.role_slots[ri];
                    if (strcmp(rs->data.role_slot.slot_name, slot) == 0
                        && rs->data.role_slot.ability_count > 0
                        && rs->data.role_slot.required_abilities[0] != NULL) {
                        ability_name = rs->data.role_slot.required_abilities[0]->data.type.name;
                    }
                }
            }
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "%s_bind_%s(&%s, NULL, &%s_%s_vtable_instance);\n",
            party_type, slot, pvar,
            role, ability_name);
        break;
    }
    case AST_ABILITY_DECL:
        emit_ability_decl(node, ctx);
        break;
    case AST_ROLE_DECL:
        emit_role_decl(node, ctx);
        break;
    case AST_PARTY_DECL:
        emit_party_decl(node, ctx);
        break;
    case AST_SYSTEMIC_DECL:
        emit_systemic_decl(node, ctx);
        break;
    case AST_WORLD_DECL:
        emit_world_decl(node, ctx);
        break;
    case AST_RELATION_DECL:
        emit_relation_decl(node, ctx);
        break;
    case AST_EFFECT_DECL:
        emit_effect_decl(node, ctx);
        break;
    case AST_ZONE_DECL:
        emit_zone_decl(node, ctx);
        break;
    case AST_EVENT_DECL:
        emit_event_decl(node, ctx);
        break;
    case AST_EVENT_SUBSCRIBE:
        emit_event_subscribe(node, ctx);
        break;
    case AST_EVENT_UNSUBSCRIBE:
        emit_event_unsubscribe(node, ctx);
        break;
    case AST_IF_STMT:
        emit_if_stmt(node, ctx);
        break;
    case AST_FOR_LOOP:
        emit_for_loop(node, ctx);
        break;
    case AST_WHILE_LOOP:
        emit_while_loop(node, ctx);
        break;
    case AST_MATCH_STMT:
        emit_match_stmt(node, ctx);
        break;
    case AST_RETURN:
        emit_return_stmt(node, ctx);
        break;
    case AST_BREAK:
        write_indent(ctx);
        codebuf_write(ctx->out, "break;\n");
        break;
    case AST_ENUM_DECL: {
        const char *ename = node->data.enum_decl.name;

        /* Check if any variant has data → tagged union */
        bool has_data = false;
        for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
            if (node->data.enum_decl.variant_param_counts != NULL
                && node->data.enum_decl.variant_param_counts[i] > 0) {
                has_data = true;
                break;
            }
        }

        if (!has_data) {
            /* Simple enum: typedef enum { Color_Red=0, ... } Color; */
            codebuf_write(ctx->out, "typedef enum {\n");
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                codebuf_write(ctx->out, "    %s_%s = %zu",
                    ename, node->data.enum_decl.variants[i], i);
                if (i + 1 < node->data.enum_decl.variant_count)
                    codebuf_write(ctx->out, ",");
                codebuf_write(ctx->out, "\n");
            }
            codebuf_write(ctx->out, "} %s;\n\n", ename);
        } else {
            /* Tagged union:
             * typedef enum { Shape_TAG_Circle, Shape_TAG_Rect, Shape_TAG_None } Shape_Tag;
             * typedef struct {
             *     Shape_Tag tag;
             *     union {
             *         struct { int32_t _0; } Circle;
             *         struct { int32_t _0; int32_t _1; } Rect;
             *     };
             * } Shape; */

            /* Tag enum */
            codebuf_write(ctx->out, "typedef enum {\n");
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                codebuf_write(ctx->out, "    %s_TAG_%s = %zu",
                    ename, node->data.enum_decl.variants[i], i);
                if (i + 1 < node->data.enum_decl.variant_count)
                    codebuf_write(ctx->out, ",");
                codebuf_write(ctx->out, "\n");
            }
            codebuf_write(ctx->out, "} %s_Tag;\n\n", ename);

            /* Tagged union struct */
            codebuf_write(ctx->out, "typedef struct {\n");
            codebuf_write(ctx->out, "    %s_Tag tag;\n", ename);
            codebuf_write(ctx->out, "    union {\n");
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                size_t pc = (node->data.enum_decl.variant_param_counts != NULL)
                    ? node->data.enum_decl.variant_param_counts[i] : 0;
                if (pc == 0) continue;
                codebuf_write(ctx->out, "        struct { ");
                for (size_t p = 0; p < pc; p++) {
                    ASTNode *pt = node->data.enum_decl.variant_params[i][p];
                    const char *ctype = "int32_t";
                    if (pt != NULL && pt->type == AST_TYPE
                        && pt->data.type.name != NULL) {
                        ctype = pergyra_ast_type_to_c(pt);
                    }
                    codebuf_write(ctx->out, "%s _%zu; ", ctype, p);
                }
                codebuf_write(ctx->out, "} %s;\n",
                    node->data.enum_decl.variants[i]);
            }
            codebuf_write(ctx->out, "    };\n");
            codebuf_write(ctx->out, "} %s;\n\n", ename);

            /* Constructor functions:
             * static inline Shape Shape_Circle(int32_t _0) {
             *     Shape v; v.tag = Shape_TAG_Circle; v.Circle._0 = _0; return v;
             * } */
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                size_t pc = (node->data.enum_decl.variant_param_counts != NULL)
                    ? node->data.enum_decl.variant_param_counts[i] : 0;
                const char *vname = node->data.enum_decl.variants[i];
                if (pc == 0) {
                    /* No-data variant: macro constant */
                    codebuf_write(ctx->out,
                        "#define %s_%s() ((%s){ .tag = %s_TAG_%s })\n",
                        ename, vname, ename, ename, vname);
                } else {
                    /* Data variant: constructor function */
                    codebuf_write(ctx->out,
                        "static inline %s %s_%s(", ename, ename, vname);
                    for (size_t p = 0; p < pc; p++) {
                        ASTNode *pt = node->data.enum_decl.variant_params[i][p];
                        const char *ctype = "int32_t";
                        if (pt != NULL && pt->type == AST_TYPE
                            && pt->data.type.name != NULL)
                            ctype = pergyra_ast_type_to_c(pt);
                        if (p > 0) codebuf_write(ctx->out, ", ");
                        codebuf_write(ctx->out, "%s _%zu", ctype, p);
                    }
                    codebuf_write(ctx->out, ") {\n");
                    codebuf_write(ctx->out,
                        "    %s _v; _v.tag = %s_TAG_%s;\n", ename, ename, vname);
                    for (size_t p = 0; p < pc; p++)
                        codebuf_write(ctx->out,
                            "    _v.%s._%zu = _%zu;\n", vname, p, p);
                    codebuf_write(ctx->out, "    return _v;\n}\n");
                }
            }
            codebuf_write(ctx->out, "\n");
        }
        break;
    }
    case AST_CONTINUE:
        write_indent(ctx);
        codebuf_write(ctx->out, "continue;\n");
        break;
    case AST_WITH_STMT:
        emit_with_stmt(node, ctx);
        break;
    case AST_PARALLEL_BLOCK:
        emit_parallel_block(node, ctx);
        break;
    case AST_BLOCK:
        emit_block(node, ctx);
        break;
    case AST_LAMBDA_EXPR:
        {
            char *expr = emit_expression(node, ctx);
            if (expr != NULL && expr[0] != '\0') {
                write_indent(ctx);
                codebuf_write(ctx->out, "%s;\n", expr);
            }
            free(expr);
            break;
        }
    case AST_ACTOR_DECL:
        emit_actor_decl(node, ctx);
        break;
    case AST_SELECT_STMT:
        emit_select_stmt(node, ctx);
        break;
    case AST_ASYNC_BLOCK:
        emit_async_block(node, ctx);
        break;
    default: {
        /* Expression statement (including event invoke) */
        char *expr = emit_expression(node, ctx);
        if (expr != NULL && expr[0] != '\0') {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s;\n", expr);
            if (node->type == AST_CALL)
                emit_zone_action_effect_runtime(node, ctx);
        }
        free(expr);
        break;
    }
    }
}

void
emit_block(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    if (node->type == AST_BLOCK) {
        int saved_slot_count = ctx->slot_var_count;
        int saved_typed_count = ctx->typed_var_count;
        for (size_t i = 0; i < node->data.block.count; i++)
            emit_statement(node->data.block.statements[i], ctx);

        /* Slot sugar: auto-release slot vars declared in this scope (LIFO).
         * Skip slots already explicitly released by the user. */
        for (int i = ctx->slot_var_count - 1; i >= saved_slot_count; i--) {
            SlotVarEntry *e = &ctx->slot_vars[i];
            if (e->released) continue;
            write_indent(ctx);
            if (e->is_secure) {
                codebuf_write(ctx->out,
                    "pgy_secure_release_%s(&%s, &%s_token);\n",
                    e->inner_type, e->name, e->name);
            } else {
                codebuf_write(ctx->out,
                    "pgy_release_%s(&%s);\n",
                    e->inner_type, e->name);
            }
        }

        ctx->slot_var_count = saved_slot_count;
        ctx->typed_var_count = saved_typed_count;
    } else {
        emit_statement(node, ctx);
    }
}

static ASTNode *
find_intent_actor_local(ASTNode *intent, const char *alias)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && involves->data.intent_involves.alias != NULL
            && strcmp(involves->data.intent_involves.alias, alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

static ASTNode *
find_subject_action_decl(TranspilerCtx *ctx, const char *subject_name, const char *action_name)
{
    ASTNode *decl;
    if (ctx == NULL || ctx->hir == NULL || subject_name == NULL || action_name == NULL)
        return NULL;
    decl = NULL;
    for (size_t i = 0; i < ctx->hir->type_count; i++) {
        ASTNode *stmt = ctx->hir->types[i];
        if (stmt != NULL && stmt->type == AST_CLASS_DECL
            && stmt->data.class_decl.name != NULL
            && strcmp(stmt->data.class_decl.name, subject_name) == 0) {
            decl = stmt;
            break;
        }
    }
    if (decl == NULL || decl->type != AST_CLASS_DECL
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT) {
        return NULL;
    }
    for (size_t i = 0; i < decl->data.class_decl.method_count; i++) {
        ASTNode *method = decl->data.class_decl.methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.is_action
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, action_name) == 0) {
            return method;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_decl_in_hir(TranspilerCtx *ctx, const char *zone_name)
{
    if (ctx == NULL || ctx->hir == NULL || zone_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->zone_count; i++) {
        ASTNode *zone = ctx->hir->zones[i];
        if (zone != NULL && zone->type == AST_ZONE_DECL
            && zone->data.zone_decl.name != NULL
            && strcmp(zone->data.zone_decl.name, zone_name) == 0) {
            return zone;
        }
    }
    return NULL;
}

static const char *
intent_actor_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_actor_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

static const char *
resolve_intent_zone_slot_name(TranspilerCtx *ctx, ASTNode *intent,
                              ASTNode *step, const char *alias);

static const char *
resolve_intent_zone_slot_name_for_zone(TranspilerCtx *ctx, ASTNode *intent,
                                       const char *zone_type_name, const char *alias);

static const char *
intent_step_effective_zone_alias(ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    if (step->data.intent_step.using_expr != NULL
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER) {
        return step->data.intent_step.using_expr->data.identifier.name;
    }
    return step->data.intent_step.transfer_to_alias;
}

static const char *
intent_zone_binding_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_actor_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

static const char *
intent_involves_type_name_local(ASTNode *involves)
{
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || involves->data.intent_involves.subject_type == NULL
        || involves->data.intent_involves.subject_type->type != AST_TYPE) {
        return NULL;
    }
    return involves->data.intent_involves.subject_type->data.type.name;
}

static bool
intent_involves_is_subject_participant(TranspilerCtx *ctx, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name_local(involves);
    if (type_name == NULL)
        return false;
    return is_subject_type_name(ctx, type_name)
        || find_actor_decl(ctx, type_name) != NULL;
}

static bool
intent_involves_uses_pointer_self(TranspilerCtx *ctx, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name_local(involves);
    if (type_name == NULL)
        return false;
    return is_pointer_self_host_type_name(ctx, type_name)
        || find_actor_decl(ctx, type_name) != NULL;
}

static void
emit_intent_step_bind_bound_zone(CodeBuf *out, TranspilerCtx *ctx,
                                 ASTNode *intent, ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type;
    const char *from_alias;
    const char *from_zone_type;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return;

    from_alias = step->data.intent_step.transfer_from_alias;
    from_zone_type = intent_zone_binding_type_name(intent, from_alias);

    if (from_alias != NULL && from_zone_type != NULL) {
        for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
            const char *alias = step->data.intent_step.who_names[i];
            const char *from_slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent,
                from_zone_type, alias);
            const char *to_slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent,
                zone_type, alias);
            if (alias == NULL)
                continue;
            if (from_slot_name != NULL && strcmp(from_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", from_alias, from_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
                    alias, from_slot_name, from_zone_type);
            }
            if (to_slot_name != NULL && strcmp(to_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", zone_alias, to_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_transfer_export(__intent_handle, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\");\n",
                    alias,
                    from_zone_type != NULL ? from_zone_type : "<zone>",
                    from_slot_name != NULL ? from_slot_name : "<unbound>",
                    zone_type,
                    to_slot_name);
            }
        }
        write_indent(ctx);
        codebuf_write(out, "%s_sync(%s);\n", from_zone_type, from_alias);
        if (strcmp(from_alias, zone_alias) != 0 || strcmp(from_zone_type, zone_type) != 0) {
            write_indent(ctx);
            codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
        }
        return;
    }

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name(ctx, intent, step, alias);
        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;
        write_indent(ctx);
        codebuf_write(out, "%s->%s = *%s;\n", zone_alias, slot_name, alias);
        write_indent(ctx);
        codebuf_write(out,
            "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
            alias, slot_name, zone_type);
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

static bool
emit_intent_step_rebind_bound_zone_aliases(CodeBuf *out, TranspilerCtx *ctx,
                                           ASTNode *intent, ASTNode *step,
                                           size_t step_index)
{
    const char *zone_alias;
    const char *zone_type;
    bool rebound = false;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return false;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return false;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        ASTNode *involves = find_intent_actor_local(intent, alias);
        const char *actor_c_type;

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || involves == NULL || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }

        actor_c_type = pergyra_ast_type_to_c(involves->data.intent_involves.subject_type);
        write_indent(ctx);
        codebuf_write(out, "%s *__intent_saved_%zu_%s = %s;\n",
            actor_c_type, step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = &%s->%s;\n", alias, zone_alias, slot_name);
        rebound = true;
    }

    return rebound;
}

static void
emit_intent_step_sync_effective_zone(CodeBuf *out, TranspilerCtx *ctx,
                                     ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type;

    if (out == NULL || ctx == NULL || step == NULL || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return;

    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

static void
emit_intent_step_restore_bound_zone_aliases(CodeBuf *out, TranspilerCtx *ctx,
                                            ASTNode *intent, ASTNode *step,
                                            size_t step_index)
{
    const char *zone_type;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_type == NULL)
        return;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        ASTNode *involves = find_intent_actor_local(intent, alias);

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || involves == NULL || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }

        write_indent(ctx);
        codebuf_write(out, "*__intent_saved_%zu_%s = *%s;\n", step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = __intent_saved_%zu_%s;\n", alias, step_index, alias);
    }
}

static const char *
resolve_intent_zone_slot_name(TranspilerCtx *ctx, ASTNode *intent,
                              ASTNode *step, const char *alias)
{
    if (ctx == NULL || intent == NULL || step == NULL || alias == NULL
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE
        || step->data.intent_step.where_type->data.type.name == NULL) {
        return "<unbound>";
    }
    return resolve_intent_zone_slot_name_for_zone(ctx, intent,
        step->data.intent_step.where_type->data.type.name, alias);
}

static const char *
resolve_intent_zone_slot_name_for_zone(TranspilerCtx *ctx, ASTNode *intent,
                                       const char *zone_type_name, const char *alias)
{
    ASTNode *zone_decl = NULL;
    const char *actor_type = NULL;
    ASTNode *named_match = NULL;
    ASTNode *typed_match = NULL;

    if (ctx == NULL || intent == NULL || zone_type_name == NULL || alias == NULL) {
        return "<unbound>";
    }

    zone_decl = find_zone_decl_in_hir(ctx, zone_type_name);
    actor_type = intent_actor_type_name(intent, alias);
    if (zone_decl == NULL)
        return "<unbound>";

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }
        if (strcmp(slot->data.domain_slot.slot_name, alias) == 0) {
            named_match = slot;
            break;
        }
        if (actor_type != NULL
            && slot->data.domain_slot.type != NULL
            && slot->data.domain_slot.type->type == AST_TYPE
            && slot->data.domain_slot.type->data.type.name != NULL
            && strcmp(slot->data.domain_slot.type->data.type.name, actor_type) == 0) {
            if (typed_match != NULL)
                typed_match = (ASTNode *)(uintptr_t)1;
            else
                typed_match = slot;
        }
    }

    if (named_match != NULL)
        return named_match->data.domain_slot.slot_name;
    if (typed_match != NULL && typed_match != (ASTNode *)(uintptr_t)1)
        return typed_match->data.domain_slot.slot_name;
    return "<unbound>";
}

static bool
intent_action_has_only_self(ASTNode *action_decl)
{
    size_t real_pc = 0;
    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
        return false;
    for (size_t i = 0; i < action_decl->data.func_decl.param_count; i++) {
        FuncParam *p = action_decl->data.func_decl.params[i];
        if (p == NULL || p->name == NULL)
            continue;
        if (p->type == NULL && strcmp(p->name, "self") == 0)
            continue;
        real_pc++;
    }
    return real_pc == 0;
}

static void
emit_intent_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    if (node == NULL || node->type != AST_INTENT_DECL || buf == NULL || ctx == NULL)
        return;
    codebuf_write(buf, "\nbool\n%s(", node->data.intent_decl.name);
    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        const char *pt = "int32_t";
        bool pointer_param = false;
        if (i > 0)
            codebuf_write(buf, ", ");
        if (involves != NULL && involves->data.intent_involves.subject_type != NULL) {
            pt = pergyra_ast_type_to_c(involves->data.intent_involves.subject_type);
            pointer_param = intent_involves_uses_pointer_self(ctx, involves);
        }
        codebuf_write(buf, "%s%s%s", pt, pointer_param ? " *" : " ",
            involves != NULL && involves->data.intent_involves.alias != NULL
                ? involves->data.intent_involves.alias : "actor");
    }
    codebuf_write(buf, ");\n");
}

static bool
transpiler_can_forward_declare_intent_early(TranspilerCtx *ctx, ASTNode *intent)
{
    if (ctx == NULL || intent == NULL || intent->type != AST_INTENT_DECL)
        return false;
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES
            || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }
        if (!transpiler_can_forward_declare_type_early(
                ctx, involves->data.intent_involves.subject_type)) {
            return false;
        }
    }
    return true;
}

static void
emit_intent_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    int saved_slot_count;
    int saved_typed_count;
    bool has_compensate_steps = false;
    bool needs_cleanup_done_label = false;
    size_t subject_count = 0;
    const MIRRoutine *mir_routine = NULL;
    bool emit_cleanup_from_mir = false;
    CodeBuf *saved_out;
    TranspilerCtx *saved_render_ctx;

    if (node == NULL || node->type != AST_INTENT_DECL || buf == NULL || ctx == NULL)
        return;

    saved_slot_count = ctx->slot_var_count;
    saved_typed_count = ctx->typed_var_count;
    saved_out = ctx->out;
    saved_render_ctx = g_type_render_ctx;
    ctx->out = buf;
    g_type_render_ctx = ctx;
    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type), "Bool");
    emit_cleanup_from_mir = transpiler_can_emit_intent_cleanup_from_mir(ctx, node, &mir_routine);
    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
        if (step != NULL && step->type == AST_INTENT_STEP
            && step->data.intent_step.compensate_expr_count > 0) {
            has_compensate_steps = true;
            break;
        }
    }
    if (node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_NONE)
        has_compensate_steps = false;
    needs_cleanup_done_label = !emit_cleanup_from_mir && has_compensate_steps
        && node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_CURRENT;

    codebuf_write(ctx->out, "\nbool\n%s(", node->data.intent_decl.name);
    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        const char *pt = "int32_t";
        char *type_name = NULL;
        bool pointer_param = false;
        if (i > 0)
            codebuf_write(ctx->out, ", ");
        if (involves != NULL && involves->data.intent_involves.subject_type != NULL) {
            pt = pergyra_ast_type_to_c(involves->data.intent_involves.subject_type);
            type_name = render_type_name(involves->data.intent_involves.subject_type);
            pointer_param = intent_involves_uses_pointer_self(ctx, involves);
        }
        codebuf_write(ctx->out, "%s%s%s", pt, pointer_param ? " *" : " ",
            involves != NULL && involves->data.intent_involves.alias != NULL
                ? involves->data.intent_involves.alias : "actor");
        if (type_name != NULL) {
            register_typed_var(ctx, involves->data.intent_involves.alias, type_name);
            TypedVarEntry *entry = lookup_typed_entry(ctx, involves->data.intent_involves.alias);
            if (entry != NULL)
                entry->is_subject_ref = pointer_param;
            free(type_name);
        }
    }
    codebuf_write(ctx->out, ")\n{\n");
    ctx->indent++;

    write_indent(ctx);
    codebuf_write(ctx->out, "bool __intent_result = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "bool __intent_failed = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "(void)__intent_failed;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "int32_t __intent_handle = 0;\n");
    if (has_compensate_steps && node->data.intent_decl.step_count > 0) {
        write_indent(ctx);
        codebuf_write(ctx->out, "bool __intent_step_completed[%zu] = { false };\n",
            node->data.intent_decl.step_count);
    }
    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        if (intent_involves_is_subject_participant(ctx,
                node->data.intent_decl.involves[i])) {
            subject_count++;
        }
    }
    if (subject_count > 0) {
        write_indent(ctx);
        codebuf_write(ctx->out, "void *__intent_subjects[%zu];\n",
            subject_count);
        {
            size_t subject_index = 0;
            for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
                ASTNode *involves = node->data.intent_decl.involves[i];
                const char *alias = (involves != NULL && involves->data.intent_involves.alias != NULL)
                    ? involves->data.intent_involves.alias : "actor";
                if (!intent_involves_is_subject_participant(ctx, involves))
                    continue;
                write_indent(ctx);
                codebuf_write(ctx->out, "__intent_subjects[%zu] = (void *)%s;\n",
                    subject_index++, alias);
            }
        }
    }
    {
        char *priority = node->data.intent_decl.priority_expr != NULL
            ? emit_expression(node->data.intent_decl.priority_expr, ctx)
            : pergyra_strdup("0");
        write_indent(ctx);
        codebuf_write(ctx->out,
            "__intent_handle = pgy_intent_enter_export(\"%s\", %s, %zu, %s, %s);\n",
            node->data.intent_decl.name,
            subject_count > 0 ? "__intent_subjects" : "NULL",
            subject_count,
            node->data.intent_decl.is_concurrent ? "true" : "false",
            priority != NULL ? priority : "0");
        free(priority);
    }
    {
        write_indent(ctx);
        codebuf_write(ctx->out, "if (__intent_handle == 0) {\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "__intent_failed = true;\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "__intent_result = false;\n");
        write_indent(ctx);
        if (emit_cleanup_from_mir) {
            codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                node->data.intent_decl.name, mir_routine->cleanup_block);
        } else {
            codebuf_write(ctx->out, "goto __intent_cleanup;\n");
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
        bool rebound_aliases = false;
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        write_indent(ctx);
        codebuf_write(ctx->out, "/* intent step: %s */\n",
            step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_intent_trace_step_export(__intent_handle, \"%s\", \"%s\");\n",
            step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
            (step->data.intent_step.where_type != NULL
                && step->data.intent_step.where_type->type == AST_TYPE
                && step->data.intent_step.where_type->data.type.name != NULL)
                ? step->data.intent_step.where_type->data.type.name : "<zone>");
        for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
            const char *alias = step->data.intent_step.who_names[j];
            const char *slot_name = resolve_intent_zone_slot_name(ctx, node, step, alias);
            write_indent(ctx);
            codebuf_write(ctx->out, "pgy_intent_trace_bind_export(__intent_handle, \"%s\", \"%s\");\n",
                alias != NULL ? alias : "<actor>",
                slot_name != NULL ? slot_name : "<unbound>");
        }
        emit_intent_step_bind_bound_zone(ctx->out, ctx, node, step);
        rebound_aliases = emit_intent_step_rebind_bound_zone_aliases(ctx->out, ctx, node, step, i);

        if (step->data.intent_step.pre_expr != NULL) {
            char *pre = emit_expression(step->data.intent_step.pre_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", pre != NULL ? pre : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"pre:%s\"); __intent_result = false; ",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(pre);
        }

        if (step->data.intent_step.invariant_expr != NULL) {
            char *invariant = emit_expression(step->data.intent_step.invariant_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", invariant != NULL ? invariant : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"invariant-pre:%s\"); __intent_result = false; ",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(invariant);
        }

        if (step->data.intent_step.on_expr_count > 0) {
            for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
                char *on_expr = emit_expression(step->data.intent_step.on_exprs[j], ctx);
                if (on_expr != NULL && on_expr[0] != '\0') {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "%s;\n", on_expr);
                }
                free(on_expr);
            }
        }
        if (step->data.intent_step.intent_expr != NULL) {
            char *intent_expr = emit_expression(step->data.intent_step.intent_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", intent_expr != NULL ? intent_expr : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"intent:%s\"); __intent_result = false; ",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(intent_expr);
        } else if (step->data.intent_step.on_expr_count == 0) {
            for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
                const char *alias = step->data.intent_step.who_names[j];
                ASTNode *involves = find_intent_actor_local(node, alias);
                const char *subject_name = NULL;
                ASTNode *action_decl = NULL;
                if (involves != NULL && involves->data.intent_involves.subject_type != NULL
                    && involves->data.intent_involves.subject_type->type == AST_TYPE) {
                    subject_name = involves->data.intent_involves.subject_type->data.type.name;
                }
                action_decl = find_subject_action_decl(ctx,
                    subject_name, step->data.intent_step.name);
                if (subject_name != NULL && action_decl != NULL
                    && intent_action_has_only_self(action_decl)) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "%s_%s(%s);\n",
                        subject_name, step->data.intent_step.name, alias);
                }
            }
        }
        if (rebound_aliases)
            emit_intent_step_sync_effective_zone(ctx->out, ctx, step);
        else
            emit_intent_step_bind_bound_zone(ctx->out, ctx, node, step);
        if (rebound_aliases)
            emit_intent_step_restore_bound_zone_aliases(ctx->out, ctx, node, step, i);

        if (has_compensate_steps) {
            write_indent(ctx);
            codebuf_write(ctx->out, "__intent_step_completed[%zu] = true;\n", i);
        }

        if (step->data.intent_step.guard_expr != NULL) {
            char *guard = emit_expression(step->data.intent_step.guard_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", guard != NULL ? guard : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"guard:%s\"); __intent_result = false; ",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(guard);
        }

        if (step->data.intent_step.expect_expr != NULL) {
            char *expect = emit_expression(step->data.intent_step.expect_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", expect != NULL ? expect : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"expect:%s\"); __intent_result = false; ",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(expect);
        }

        if (step->data.intent_step.post_expr != NULL) {
            char *post = emit_expression(step->data.intent_step.post_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", post != NULL ? post : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"post:%s\"); __intent_result = false; ",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(post);
        }

        if (step->data.intent_step.invariant_expr != NULL) {
            char *invariant = emit_expression(step->data.intent_step.invariant_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", invariant != NULL ? invariant : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"invariant-post:%s\"); __intent_result = false; ",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(invariant);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_intent_trace_step_ok_export(__intent_handle, \"%s\");\n",
            step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
    }

    write_indent(ctx);
    if (node->data.intent_decl.success_expr != NULL) {
        char *success = emit_expression(node->data.intent_decl.success_expr, ctx);
        codebuf_write(ctx->out, "__intent_result = %s;\n", success != NULL ? success : "true");
        free(success);
    } else {
        codebuf_write(ctx->out, "__intent_result = true;\n");
    }
    write_indent(ctx);
    if (emit_cleanup_from_mir) {
        codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
            node->data.intent_decl.name, mir_routine->cleanup_block);
    } else {
        codebuf_write(ctx->out, "goto __intent_cleanup;\n");
    }

    if (emit_cleanup_from_mir) {
        const MIRBasicBlock *cleanup_block = &mir_routine->blocks[mir_routine->cleanup_block];
        const MIRBasicBlock *rollback_block = mir_routine->has_rollback_block
            ? &mir_routine->blocks[mir_routine->rollback_block] : NULL;
        const MIRBasicBlock *invalidation_block = mir_routine->has_invalidation_block
            ? &mir_routine->blocks[mir_routine->invalidation_block] : NULL;
        write_indent(ctx);
        codebuf_write(ctx->out, "/* cleanup-emitted-from-mir */\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n",
            node->data.intent_decl.name, mir_routine->cleanup_block);
        ctx->indent++;
        transpiler_emit_mir_block_mapping_comment(ctx->out, ctx->indent,
            node->data.intent_decl.name, mir_routine, cleanup_block);
        for (size_t i = 0; i < cleanup_block->instruction_count; i++) {
            const MIRInstruction *inst = &cleanup_block->instructions[i];
            if (inst->kind == MIR_INST_CLEANUP_EDGE || inst->kind == MIR_INST_RESOURCE_OP)
                transpiler_emit_mir_resource_hook(ctx->out, ctx->indent, inst, "__intent_handle", true);
        }
        if (mir_routine->has_rollback_block) {
            write_indent(ctx);
            codebuf_write(ctx->out, "if (__intent_failed) {\n");
            write_indent_to(ctx->out, ctx->indent + 1);
            codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                node->data.intent_decl.name, mir_routine->rollback_block);
            write_indent(ctx);
            codebuf_write(ctx->out, "}\n");
        }
        if (mir_routine->has_invalidation_block) {
            write_indent(ctx);
            codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                node->data.intent_decl.name, mir_routine->invalidation_block);
        } else {
            write_indent(ctx);
            codebuf_write(ctx->out, "if (__intent_handle != 0) pgy_intent_exit_export(__intent_handle);\n");
            write_indent(ctx);
            codebuf_write(ctx->out, "return __intent_result;\n");
        }
        ctx->indent--;
        if (mir_routine->has_rollback_block) {
            write_indent(ctx);
            codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n",
                node->data.intent_decl.name, mir_routine->rollback_block);
            ctx->indent++;
            transpiler_emit_mir_block_mapping_comment(ctx->out, ctx->indent,
                node->data.intent_decl.name, mir_routine, rollback_block);
            if (rollback_block != NULL) {
                for (size_t i = 0; i < rollback_block->instruction_count; i++) {
                    const MIRInstruction *inst = &rollback_block->instructions[i];
                    if (inst->kind == MIR_INST_CLEANUP_EDGE || inst->kind == MIR_INST_RESOURCE_OP)
                        transpiler_emit_mir_resource_hook(ctx->out, ctx->indent, inst, "__intent_handle", true);
                }
            }
            if (has_compensate_steps && node->data.intent_decl.step_count > 0) {
                for (size_t i = node->data.intent_decl.step_count; i-- > 0;) {
                    ASTNode *step = node->data.intent_decl.steps[i];
                    if (step == NULL || step->type != AST_INTENT_STEP
                        || step->data.intent_step.compensate_expr_count == 0)
                        continue;
                    write_indent(ctx);
                    codebuf_write(ctx->out, "if (__intent_step_completed[%zu]) {\n", i);
                    ctx->indent++;
                    for (size_t j = step->data.intent_step.compensate_expr_count; j-- > 0;) {
                        char *expr = emit_expression(step->data.intent_step.compensate_exprs[j], ctx);
                        if (expr != NULL && expr[0] != '\0') {
                            write_indent(ctx);
                            codebuf_write(ctx->out, "%s;\n", expr);
                        }
                        free(expr);
                    }
                    emit_intent_step_bind_bound_zone(ctx->out, ctx, node, step);
                    if (node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_CURRENT) {
                        if (mir_routine->has_invalidation_block) {
                            write_indent(ctx);
                            codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                                node->data.intent_decl.name, mir_routine->invalidation_block);
                        } else {
                            write_indent(ctx);
                            codebuf_write(ctx->out, "if (__intent_handle != 0) pgy_intent_exit_export(__intent_handle);\n");
                            write_indent(ctx);
                            codebuf_write(ctx->out, "return __intent_result;\n");
                        }
                    }
                    ctx->indent--;
                    write_indent(ctx);
                    codebuf_write(ctx->out, "}\n");
                }
            }
            if (mir_routine->has_invalidation_block) {
                write_indent(ctx);
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                    node->data.intent_decl.name, mir_routine->invalidation_block);
            } else {
                write_indent(ctx);
                codebuf_write(ctx->out, "if (__intent_handle != 0) pgy_intent_exit_export(__intent_handle);\n");
                write_indent(ctx);
                codebuf_write(ctx->out, "return __intent_result;\n");
            }
            ctx->indent--;
        }
        if (mir_routine->has_invalidation_block) {
            write_indent(ctx);
            codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n",
                node->data.intent_decl.name, mir_routine->invalidation_block);
            ctx->indent++;
            transpiler_emit_mir_block_mapping_comment(ctx->out, ctx->indent,
                node->data.intent_decl.name, mir_routine, invalidation_block);
            if (invalidation_block != NULL) {
                for (size_t i = 0; i < invalidation_block->instruction_count; i++) {
                    const MIRInstruction *inst = &invalidation_block->instructions[i];
                    if (inst->kind == MIR_INST_CLEANUP_EDGE || inst->kind == MIR_INST_RESOURCE_OP)
                        transpiler_emit_mir_resource_hook(ctx->out, ctx->indent, inst, "__intent_handle", true);
                }
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "if (__intent_handle != 0) pgy_intent_exit_export(__intent_handle);\n");
            write_indent(ctx);
            codebuf_write(ctx->out, "return __intent_result;\n");
            ctx->indent--;
        }
    } else {
        write_indent(ctx);
        codebuf_write(ctx->out, "__intent_cleanup:\n");
        ctx->indent++;
        if (has_compensate_steps && node->data.intent_decl.step_count > 0) {
            write_indent(ctx);
            codebuf_write(ctx->out, "if (__intent_failed) {\n");
            ctx->indent++;
            for (size_t i = node->data.intent_decl.step_count; i-- > 0;) {
                ASTNode *step = node->data.intent_decl.steps[i];
                if (step == NULL || step->type != AST_INTENT_STEP
                    || step->data.intent_step.compensate_expr_count == 0)
                    continue;
                write_indent(ctx);
                codebuf_write(ctx->out, "if (__intent_step_completed[%zu]) {\n", i);
                ctx->indent++;
                for (size_t j = step->data.intent_step.compensate_expr_count; j-- > 0;) {
                    char *expr = emit_expression(step->data.intent_step.compensate_exprs[j], ctx);
                    if (expr != NULL && expr[0] != '\0') {
                        write_indent(ctx);
                        codebuf_write(ctx->out, "%s;\n", expr);
                    }
                    free(expr);
                }
                emit_intent_step_bind_bound_zone(ctx->out, ctx, node, step);
                if (node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_CURRENT) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "goto __intent_cleanup_done;\n");
                }
                ctx->indent--;
                write_indent(ctx);
                codebuf_write(ctx->out, "}\n");
            }
            ctx->indent--;
            write_indent(ctx);
            codebuf_write(ctx->out, "}\n");
        }
        if (needs_cleanup_done_label) {
            write_indent(ctx);
            codebuf_write(ctx->out, "__intent_cleanup_done:\n");
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "if (__intent_handle != 0) pgy_intent_exit_export(__intent_handle);\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "return __intent_result;\n");
    }
    ctx->indent--;

    ctx->indent--;
    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
    codebuf_write(ctx->out, "}\n");
    g_type_render_ctx = saved_render_ctx;
    ctx->out = saved_out;
}

/* -----------------------------------------------------------------
 * Program emitter
 * ----------------------------------------------------------------- */

void
emit_program(const HIRProgram *hir, TranspilerCtx *ctx)
{
    if (hir == NULL)
        return;

    ctx->hir = hir;

    /* File header */
    codebuf_write(ctx->out,
        "/*\n"
        " * Generated by the Pergyra compiler C backend\n"
        " * Do not edit manually.\n"
        " */\n"
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include \"pgy_runtime.h\"\n"
        "#include \"pgy_parallel.h\"\n"
        "#include \"pgy_channel.h\"\n"
        "#ifndef PGY_EVENT_MAX_HANDLERS\n"
        "#define PGY_EVENT_MAX_HANDLERS 16\n"
        "#endif\n\n");

    /*
     * Multi-pass strategy for valid C output:
     *   Pass 1 — ability declarations (vtable typedefs)
     *   Pass 2 — class declarations (type completeness)
     *   Pass 3 — role declarations (vtable instances + methods)
     *   Pass 4 — function declarations (file scope)
     *   Pass 5 — remaining top-level statements → wrapped in main()
     */

    /* Pass 1: abilities (vtable typedefs) */
    for (size_t i = 0; i < hir->ability_count; i++)
        emit_ability_decl(hir->abilities[i], ctx);

    /* Pass 1.5: enums + type aliases */
    for (size_t i = 0; i < hir->type_count; i++) {
        if (hir->types[i] != NULL
            && (hir->types[i]->type == AST_ENUM_DECL
                || hir->types[i]->type == AST_TYPE_ALIAS))
            emit_statement(hir->types[i], ctx);
    }

    /* Pass 2: classes */
    for (size_t i = 0; i < hir->type_count; i++) {
        if (hir->types[i] != NULL && hir->types[i]->type == AST_CLASS_DECL)
            emit_class_decl(hir->types[i], ctx);
    }

    /* Pass 2.5: extern declarations */
    for (size_t i = 0; i < hir->extern_count; i++)
        emit_extern_block(hir->externs[i], ctx);

    /* Pass 2.6: early forward declarations for standalone functions so
     * class/domain hosted methods can call file-scope helpers declared later. */
    for (size_t i = 0; i < hir->function_count; i++) {
        if (transpiler_can_forward_declare_func_early(ctx, hir->functions[i]))
            emit_func_forward_decl(hir->functions[i], ctx->out, ctx);
    }
    for (size_t i = 0; i < hir->intent_count; i++) {
        if (transpiler_can_forward_declare_intent_early(ctx, hir->intents[i]))
            emit_intent_forward_decl(hir->intents[i], ctx->out, ctx);
    }

    /* Pass 3: roles (vtable instances + free functions) */
    for (size_t i = 0; i < hir->role_count; i++)
        emit_role_decl(hir->roles[i], ctx);

    /* Pass 3.5: parties (struct + methods) */
    for (size_t i = 0; i < hir->party_count; i++)
        emit_party_decl(hir->parties[i], ctx);

    /* Pass 3.7: systemics (struct + methods) */
    for (size_t i = 0; i < hir->systemic_count; i++)
        emit_systemic_decl(hir->systemics[i], ctx);

    /* Pass 3.75: relations and effects (must precede zones that reference them) */
    for (size_t i = 0; i < hir->relation_count; i++)
        emit_relation_decl(hir->relations[i], ctx);
    for (size_t i = 0; i < hir->effect_count; i++)
        emit_effect_decl(hir->effects[i], ctx);

    /* Pass 3.8: zones (struct + methods + sync helpers) */
    for (size_t i = 0; i < hir->zone_count; i++)
        emit_zone_decl(hir->zones[i], ctx);

    /* Pass 3.85: now that zones are declared, emit intent prototypes that
     * depend on zone types before world methods are emitted. */
    for (size_t i = 0; i < hir->intent_count; i++) {
        if (!transpiler_can_forward_declare_intent_early(ctx, hir->intents[i]))
            emit_intent_forward_decl(hir->intents[i], ctx->out, ctx);
    }
    for (size_t i = 0; i < hir->function_count; i++) {
        if (!transpiler_can_forward_declare_func_early(ctx, hir->functions[i])
            && transpiler_can_forward_declare_func_after_zones(ctx, hir->functions[i])) {
            emit_func_forward_decl(hir->functions[i], ctx->out, ctx);
        }
    }

    /* Pass 3.9: worlds (struct + methods) */
    for (size_t i = 0; i < hir->world_count; i++)
        emit_world_decl(hir->worlds[i], ctx);

    /* Pass 3.95: actors (struct + methods) */
    for (size_t i = 0; i < hir->actor_count; i++)
        emit_actor_decl(hir->actors[i], ctx);

    for (size_t i = 0; i < hir->event_count; i++)
        emit_event_decl(hir->events[i], ctx);

    for (size_t i = 0; i < hir->function_count; i++)
        emit_func_forward_decl(hir->functions[i], ctx->decls, ctx);
    for (size_t i = 0; i < hir->intent_count; i++)
        emit_intent_forward_decl(hir->intents[i], ctx->decls, ctx);

    /* Pass 4: functions — emit in two sub-passes so that helpers
     * (parallel context structs, wrapper functions) generated during
     * function emission are available.  First pass: emit all functions
     * into a temporary buffer. */
    {
        CodeBuf *func_buf = codebuf_create();
        CodeBuf *saved_out = ctx->out;
        ctx->out = func_buf;
        for (size_t i = 0; i < hir->function_count; i++)
            emit_func_decl(hir->functions[i], ctx);
        for (size_t i = 0; i < hir->intent_count; i++)
            emit_intent_decl(hir->intents[i], func_buf, ctx);
        ctx->out = saved_out;

        /* Emit forward declarations after function emission so late-added
         * helper declarations, including generic specializations, are visible. */
        if (ctx->decls->len > 0) {
            codebuf_write(ctx->out, "\n");
            codebuf_write_raw(ctx->out, ctx->decls->data, ctx->decls->len);
        }

        /* Emit helpers (parallel context structs + wrappers) first */
        if (ctx->helpers->len > 0) {
            codebuf_write(ctx->out, "\n");
            codebuf_write_raw(ctx->out, ctx->helpers->data, ctx->helpers->len);
        }

        /* Then emit the function bodies */
        if (func_buf->len > 0) {
            codebuf_write_raw(ctx->out, func_buf->data, func_buf->len);
        }
        codebuf_destroy(func_buf);
    }

    /* Check if a Main() function exists */
    /* Generate int main(void) { ... } */
    if (hir->executable_count > 0 || hir->has_main_function) {
        codebuf_write(ctx->out, "\nint\nmain(void)\n{\n");
        ctx->indent++;

        /* Initialize runtime */
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_pool_init(0);\n\n");

        /* Emit top-level statements inside main() */
        for (size_t i = 0; i < hir->executable_count; i++)
            emit_statement(hir->executables[i], ctx);

        /* If Main() exists and no top-level statements, call it */
        if (hir->has_main_function) {
            write_indent(ctx);
            codebuf_write(ctx->out, "Main();\n");
        }

        /* Shutdown runtime */
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_pool_shutdown();\n");

        write_indent(ctx);
        codebuf_write(ctx->out, "return 0;\n");
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

/* -----------------------------------------------------------------
 * Main entry point
 * ----------------------------------------------------------------- */

TranspileResult *
transpile(const HIRProgram *hir, const char *output_path)
{
    return transpile_with_mir(hir, NULL, output_path);
}

TranspileResult *
transpile_with_mir(const HIRProgram *hir, const MIRProgram *mir, const char *output_path)
{
    TranspileResult *result = calloc(1, sizeof(TranspileResult));
    if (result == NULL)
        return NULL;

    TranspilerCtx *ctx = transpiler_ctx_create();
    if (ctx == NULL) {
        result->success       = false;
        result->error_message = pergyra_strdup("Out of memory");
        return result;
    }

    ctx->mir = mir;
    emit_program(hir, ctx);

    if (output_path != NULL) {
        if (!codebuf_dump_file(ctx->out, output_path)) {
            result->success       = false;
            result->error_message = strdup_fmt(
                "Cannot write output file: %s", output_path);
            transpiler_ctx_destroy(ctx);
            return result;
        }
    }

    result->success = true;
    transpiler_ctx_destroy(ctx);
    return result;
}

void
transpile_result_destroy(TranspileResult *res)
{
    if (res == NULL)
        return;
    free(res->error_message);
    free(res);
}

#include "transpiler_domain_role.inc"

void
emit_event_invoke(ASTNode *node, TranspilerCtx *ctx)
{
    /* Event invoke is handled in emit_call for function-style invocation */
    (void)node;
    (void)ctx;
}

char *
emit_lambda_expr(ASTNode *node, TranspilerCtx *ctx)
{
    int lambda_id = ++ctx->tmp_counter;
    const char *return_type = "int32_t";

    if (node->data.lambda_expr.return_type != NULL) {
        return_type = pergyra_ast_type_to_c(node->data.lambda_expr.return_type);
    } else if (node->data.lambda_expr.body != NULL
               && node->data.lambda_expr.body->type == AST_BLOCK) {
        return_type = "void";
    }

    char *lambda_name = strdup_fmt("pgy_lambda_%d", lambda_id);

    codebuf_write(ctx->decls, "\nstatic %s %s(",
                  return_type, lambda_name);
    for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
        ASTNode *param = node->data.lambda_expr.params[i];
        const char *param_name = NULL;
        const char *param_type = "int32_t";
        if (i > 0)
            codebuf_write(ctx->decls, ", ");
        if (param->type == AST_LET_DECL) {
            param_name = param->data.let_decl.name;
            if (param->data.let_decl.type != NULL)
                param_type = pergyra_ast_type_to_c(param->data.let_decl.type);
        } else {
            param_name = param->data.identifier.name;
        }
        codebuf_write(ctx->decls, "%s %s", param_type, param_name);
    }
    codebuf_write(ctx->decls, ");\n");

    codebuf_write(ctx->helpers, "\nstatic %s %s(",
                  return_type, lambda_name);
    for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
        ASTNode *param = node->data.lambda_expr.params[i];
        const char *param_name = NULL;
        const char *param_type = "int32_t";
        if (i > 0)
            codebuf_write(ctx->helpers, ", ");
        if (param->type == AST_LET_DECL) {
            param_name = param->data.let_decl.name;
            if (param->data.let_decl.type != NULL)
                param_type = pergyra_ast_type_to_c(param->data.let_decl.type);
        } else {
            param_name = param->data.identifier.name;
        }
        codebuf_write(ctx->helpers, "%s %s", param_type, param_name);
    }
    codebuf_write(ctx->helpers, ")\n{\n");

    if (node->data.lambda_expr.body != NULL
        && node->data.lambda_expr.body->type == AST_BLOCK) {
        CodeBuf *saved_out = ctx->out;
        int saved_indent = ctx->indent;
        ctx->out = ctx->helpers;
        ctx->indent = 1;
        emit_block(node->data.lambda_expr.body, ctx);
        ctx->indent = saved_indent;
        ctx->out = saved_out;
    } else if (node->data.lambda_expr.body != NULL) {
        char *expr = emit_expression(node->data.lambda_expr.body, ctx);
        write_indent_to(ctx->helpers, 1);
        codebuf_write(ctx->helpers, "return %s;\n", expr);
        free(expr);
    }

    codebuf_write(ctx->helpers, "}\n");
    return lambda_name;
}

void
emit_include_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *included_role = node->data.include_stmt.role_name;

    codebuf_write(ctx->out, "/* include %s */\n", included_role);
    if (find_role_decl(ctx, included_role) == NULL) {
        codebuf_write(ctx->out, "/* unresolved include %s */\n", included_role);
    }
}

void
emit_impl_ability(ASTNode *node, TranspilerCtx *ctx)
{
    const char *ability_name = node->data.impl_ability.ability_name;
    
    codebuf_write(ctx->out, "/* Impl ability: %s */\n", ability_name);
    
    /* This is handled within emit_role_decl */
    (void)ctx;
}

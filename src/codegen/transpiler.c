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

/* Forward declarations for emitters defined later */
void emit_ability_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_role_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_party_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_roster_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_relation_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_effect_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_zone_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_world_decl(ASTNode *node, TranspilerCtx *ctx);
static void emit_type_alias_decl(ASTNode *node, TranspilerCtx *ctx);
static bool ast_uses_thread_pool(ASTNode *node);
static bool transpiler_requires_thread_pool(const TranspilerCtx *ctx);
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

static bool
ast_uses_thread_pool(ASTNode *node)
{
    if (node == NULL)
        return false;

    switch (node->type) {
    case AST_PARALLEL_BLOCK:
    case AST_ASYNC_BLOCK:
    case AST_SPAWN_EXPR:
        return true;
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            if (ast_uses_thread_pool(node->data.block.statements[i]))
                return true;
        }
        return false;
    case AST_LET_DECL:
        return ast_uses_thread_pool(node->data.let_decl.type)
            || ast_uses_thread_pool(node->data.let_decl.initializer);
    case AST_RETURN:
        return ast_uses_thread_pool(node->data.return_stmt.value);
    case AST_CALL:
        if (ast_uses_thread_pool(node->data.call.callee))
            return true;
        for (size_t i = 0; i < node->data.call.arg_count; i++) {
            if (ast_uses_thread_pool(node->data.call.arguments[i]))
                return true;
        }
        return false;
    case AST_BINARY:
        return ast_uses_thread_pool(node->data.binary.left)
            || ast_uses_thread_pool(node->data.binary.right);
    case AST_UNARY:
        return ast_uses_thread_pool(node->data.unary.operand);
    case AST_ASSIGNMENT:
        return ast_uses_thread_pool(node->data.assignment.target)
            || ast_uses_thread_pool(node->data.assignment.value);
    case AST_MEMBER_ACCESS:
        return ast_uses_thread_pool(node->data.member.object);
    case AST_ARRAY_ACCESS:
        return ast_uses_thread_pool(node->data.array_access.array)
            || ast_uses_thread_pool(node->data.array_access.index);
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < node->data.array_literal.count; i++) {
            if (ast_uses_thread_pool(node->data.array_literal.elements[i]))
                return true;
        }
        return false;
    case AST_IF_STMT:
        return ast_uses_thread_pool(node->data.if_stmt.condition)
            || ast_uses_thread_pool(node->data.if_stmt.then_branch)
            || ast_uses_thread_pool(node->data.if_stmt.else_branch);
    case AST_FOR_LOOP:
        return ast_uses_thread_pool(node->data.for_loop.range_start)
            || ast_uses_thread_pool(node->data.for_loop.range_end)
            || ast_uses_thread_pool(node->data.for_loop.iterable)
            || ast_uses_thread_pool(node->data.for_loop.body);
    case AST_WHILE_LOOP:
        return ast_uses_thread_pool(node->data.while_loop.condition)
            || ast_uses_thread_pool(node->data.while_loop.body);
    case AST_MATCH_STMT:
        if (ast_uses_thread_pool(node->data.match_stmt.subject)
            || ast_uses_thread_pool(node->data.match_stmt.default_body))
            return true;
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
            if (ast_uses_thread_pool(node->data.match_stmt.cases[i]))
                return true;
        }
        return false;
    case AST_MATCH_CASE:
        return ast_uses_thread_pool(node->data.match_case.pattern)
            || ast_uses_thread_pool(node->data.match_case.guard)
            || ast_uses_thread_pool(node->data.match_case.body);
    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
            if (ast_uses_thread_pool(node->data.select_stmt.cases[i]))
                return true;
        }
        return ast_uses_thread_pool(node->data.select_stmt.default_case);
    case AST_TASK_GROUP:
        for (size_t i = 0; i < node->data.task_group.task_count; i++) {
            if (ast_uses_thread_pool(node->data.task_group.tasks[i]))
                return true;
        }
        return false;
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return ast_uses_thread_pool(node->data.event_op.event)
            || ast_uses_thread_pool(node->data.event_op.handler);
    case AST_EVENT_INVOKE:
        if (ast_uses_thread_pool(node->data.event_invoke.event))
            return true;
        for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
            if (ast_uses_thread_pool(node->data.event_invoke.arguments[i]))
                return true;
        }
        return false;
    default:
        return false;
    }
}

static bool
ast_decl_uses_thread_pool(ASTNode *node)
{
    if (node == NULL)
        return false;

    switch (node->type) {
    case AST_FUNC_DECL:
        return ast_uses_thread_pool(node->data.func_decl.body);
    case AST_CLASS_DECL:
        for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
            if (ast_decl_uses_thread_pool(node->data.class_decl.methods[i]))
                return true;
        }
        return false;
    case AST_ENUM_DECL:
        for (size_t i = 0; i < node->data.enum_decl.method_count; i++) {
            if (ast_decl_uses_thread_pool(node->data.enum_decl.methods[i]))
                return true;
        }
        return false;
    case AST_ABILITY_DECL:
        for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
            if (ast_decl_uses_thread_pool(node->data.ability_decl.methods[i]))
                return true;
        }
        return false;
    case AST_ROLE_DECL:
        for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
            ASTNode *impl = node->data.role_decl.impl_abilities[i];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;
            for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
                if (ast_decl_uses_thread_pool(impl->data.impl_ability.methods[j]))
                    return true;
            }
        }
        return false;
    case AST_PARTY_DECL:
        for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
            if (ast_decl_uses_thread_pool(node->data.party_decl.methods[i]))
                return true;
        }
        return false;
    case AST_ROSTER_DECL:
        for (size_t i = 0; i < node->data.roster_decl.method_count; i++) {
            if (ast_decl_uses_thread_pool(node->data.roster_decl.methods[i]))
                return true;
        }
        return false;
    case AST_WORLD_DECL:
        for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
            if (ast_decl_uses_thread_pool(node->data.world_decl.methods[i]))
                return true;
        }
        return false;
    case AST_RELATION_DECL:
        for (size_t i = 0; i < node->data.relation_decl.method_count; i++) {
            if (ast_decl_uses_thread_pool(node->data.relation_decl.methods[i]))
                return true;
        }
        return false;
    case AST_EFFECT_DECL:
        for (size_t i = 0; i < node->data.effect_decl.method_count; i++) {
            if (ast_decl_uses_thread_pool(node->data.effect_decl.methods[i]))
                return true;
        }
        return false;
    case AST_ZONE_DECL:
        for (size_t i = 0; i < node->data.zone_decl.method_count; i++) {
            if (ast_decl_uses_thread_pool(node->data.zone_decl.methods[i]))
                return true;
        }
        return false;
    default:
        return false;
    }
}

static bool
transpiler_requires_thread_pool(const TranspilerCtx *ctx)
{
    ASTNode **functions = NULL;
    ASTNode **types = NULL;
    ASTNode **abilities = NULL;
    ASTNode **roles = NULL;
    ASTNode **parties = NULL;
    ASTNode **rosters = NULL;
    ASTNode **relations = NULL;
    ASTNode **effects = NULL;
    ASTNode **zones = NULL;
    ASTNode **worlds = NULL;
    ASTNode **executables = NULL;
    size_t function_count = 0;
    size_t type_count = 0;
    size_t ability_count = 0;
    size_t role_count = 0;
    size_t party_count = 0;
    size_t roster_count = 0;
    size_t relation_count = 0;
    size_t effect_count = 0;
    size_t zone_count = 0;
    size_t world_count = 0;
    size_t executable_count = 0;
    ASTNode *synthetic_executable_func = NULL;

    if (ctx == NULL)
        return false;

    transpiler_active_inventory(ctx, AST_FUNC_DECL, &functions, &function_count);
    transpiler_active_inventory(ctx, AST_CLASS_DECL, &types, &type_count);
    transpiler_active_inventory(ctx, AST_ABILITY_DECL, &abilities, &ability_count);
    transpiler_active_inventory(ctx, AST_ROLE_DECL, &roles, &role_count);
    transpiler_active_inventory(ctx, AST_PARTY_DECL, &parties, &party_count);
    transpiler_active_inventory(ctx, AST_ROSTER_DECL, &rosters, &roster_count);
    transpiler_active_inventory(ctx, AST_RELATION_DECL, &relations, &relation_count);
    transpiler_active_inventory(ctx, AST_EFFECT_DECL, &effects, &effect_count);
    transpiler_active_inventory(ctx, AST_ZONE_DECL, &zones, &zone_count);
    transpiler_active_inventory(ctx, AST_WORLD_DECL, &worlds, &world_count);

    for (size_t i = 0; i < function_count; i++) {
        if (ast_decl_uses_thread_pool(functions[i]))
            return true;
    }
    for (size_t i = 0; i < type_count; i++) {
        if (ast_decl_uses_thread_pool(types[i]))
            return true;
    }
    for (size_t i = 0; i < ability_count; i++) {
        if (ast_decl_uses_thread_pool(abilities[i]))
            return true;
    }
    for (size_t i = 0; i < role_count; i++) {
        if (ast_decl_uses_thread_pool(roles[i]))
            return true;
    }
    for (size_t i = 0; i < party_count; i++) {
        if (ast_decl_uses_thread_pool(parties[i]))
            return true;
    }
    for (size_t i = 0; i < roster_count; i++) {
        if (ast_decl_uses_thread_pool(rosters[i]))
            return true;
    }
    for (size_t i = 0; i < relation_count; i++) {
        if (ast_decl_uses_thread_pool(relations[i]))
            return true;
    }
    for (size_t i = 0; i < effect_count; i++) {
        if (ast_decl_uses_thread_pool(effects[i]))
            return true;
    }
    for (size_t i = 0; i < zone_count; i++) {
        if (ast_decl_uses_thread_pool(zones[i]))
            return true;
    }
    for (size_t i = 0; i < world_count; i++) {
        if (ast_decl_uses_thread_pool(worlds[i]))
            return true;
    }

    synthetic_executable_func = transpiler_active_synthetic_executable_func(ctx);
    if (synthetic_executable_func != NULL
        && synthetic_executable_func->type == AST_FUNC_DECL
        && ast_uses_thread_pool(synthetic_executable_func->data.func_decl.body)) {
        return true;
    }

    if (ctx->mir != NULL)
        return false;

    transpiler_active_executables(ctx, &executables, &executable_count);
    for (size_t i = 0; i < executable_count; i++) {
        if (ast_uses_thread_pool(executables[i]))
            return true;
    }

    return false;
}

void emit_select_stmt(ASTNode *node, TranspilerCtx *ctx);
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
static const MIRRoutine *transpiler_find_mir_method(const TranspilerCtx *ctx,
                                                    const char *owner_name,
                                                    const ASTNode *method_decl);
static bool transpiler_parse_versioned_name(const char *versioned,
                                            char *base,
                                            size_t base_size,
                                            size_t *version_out);
static char *transpiler_make_c_ssa_name(const char *versioned_name);
static bool transpiler_c_type_uses_scalar_zero(const char *c_type);
static const char *transpiler_find_local_type_name(TranspilerCtx *ctx,
                                                   const ASTNode *func_decl,
                                                   const char *base_name);
static const char *transpiler_infer_local_type_name_from_expr(TranspilerCtx *ctx,
                                                              const ASTNode *func_decl,
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
static const char *transpiler_resolve_active_ssa_name(const TranspilerCtx *ctx,
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
static const char *lookup_typed_var(TranspilerCtx *ctx, const char *var_name);
static const char *transpiler_require_ast_c_type(TranspilerCtx *ctx,
                                                 ASTNode *type_ast,
                                                 const char *surface_desc);
static const char *transpiler_require_type_name_c_type(TranspilerCtx *ctx,
                                                       const char *type_name,
                                                       const char *surface_desc);

#include "transpiler_helpers.inc"
static int transpiler_find_loop_label_depth(const TranspilerCtx *ctx,
                                            const char *label);
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
                                                 TranspilerSSANameMap *out_ssa_map,
                                                 char *reason,
                                                 size_t reason_cap);
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
static bool transpiler_emit_mir_resource_hook(TranspilerCtx *ctx,
                                              CodeBuf *out,
                                              int indent,
                                              const MIRInstruction *inst,
                                              const char *handle_expr,
                                              bool cleanup_hook);
static bool transpiler_validate_mir_emission_block_shape(const MIRBasicBlock *block,
                                                        const char *routine_name,
                                                        bool require_cleanup,
                                                        char *reason,
                                                        size_t reason_cap);
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

static const char *
transpiler_require_ast_c_type(TranspilerCtx *ctx,
                              ASTNode *type_ast,
                              const char *surface_desc)
{
    const char *c_type;
    if (type_ast == NULL) {
        if (ctx != NULL && ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot emit %s in C backend: missing explicit type",
                surface_desc != NULL ? surface_desc : "declaration");
        }
        return NULL;
    }
    c_type = pergyra_ast_type_to_c(type_ast);
    if (c_type == NULL || c_type[0] == '\0') {
        if (ctx != NULL && ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot lower %s to a concrete C type",
                surface_desc != NULL ? surface_desc : "declaration");
        }
        return NULL;
    }
    return c_type;
}

static const char *
transpiler_require_type_name_c_type(TranspilerCtx *ctx,
                                    const char *type_name,
                                    const char *surface_desc)
{
    const char *c_type;
    if (type_name == NULL || type_name[0] == '\0') {
        if (ctx != NULL && ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot emit %s in C backend: missing concrete type name",
                surface_desc != NULL ? surface_desc : "declaration");
        }
        return NULL;
    }
    c_type = pergyra_type_to_c(type_name);
    if (c_type == NULL || c_type[0] == '\0') {
        if (ctx != NULL && ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot lower %s type '%s' to a concrete C type",
                surface_desc != NULL ? surface_desc : "declaration",
                type_name);
        }
        return NULL;
    }
    return c_type;
}

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

    if (node->data.let_decl.is_alias) {
        register_alias_var(ctx, name, init);
        if (ann_type_name != NULL) {
            register_typed_var(ctx, name, ann_type_name);
        } else if (init != NULL) {
            const char *inferred = infer_expression_type_name(ctx, init);
            if (inferred != NULL)
                register_typed_var(ctx, name, inferred);
        }
        free(ann_type_name);
        return;
    }

    if (ann != NULL && ann->type == AST_TYPE
        && ann->data.type.generic_args != NULL
        && ann->data.type.generic_args->count > 0
        && ann->data.type.name != NULL) {
        ASTNode *gc_decl = find_class_decl(ctx, ann->data.type.name);
        if (gc_decl != NULL && class_has_generic_params(gc_decl)) {
            generic_class_spec_name =
                ensure_generic_class_specialization(ctx, gc_decl, ann);
            if (generic_class_spec_name == NULL)
                return;
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
    const char *slot_inner = NULL;

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
        if (ann != NULL) {
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                slot_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                slot_inner = slot_inner_type_name(ann->data.type.name);
            }
        }
        if (slot_inner == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot emit slot claim for '%s': missing explicit Slot<T>/SecureSlot<T> annotation",
                    name != NULL ? name : "(anonymous)");
            }
            free(ann_type_name);
            return;
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
        if (slot_inner == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot emit device slot claim for '%s': missing explicit DeviceSlot<T> annotation",
                    name != NULL ? name : "(anonymous)");
            }
            free(ann_type_name);
            return;
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
            const char *inner = NULL;
            bool source_secure = lookup_slot_is_secure(ctx, source_name);
            const char *ctype = NULL;
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                inner = ann->data.type.generic_args->params[0]->name;
            } else {
                inner = slot_inner_type_name(ann_name);
            }
            if (inner == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "cannot emit %s declaration for '%s': missing explicit inner type",
                        is_move_decl ? "move-token" : "view",
                        name != NULL ? name : "(anonymous)");
                }
                free(ann_type_name);
                return;
            }
            if (source_secure) {
                char secure_name[128];
                snprintf(secure_name, sizeof(secure_name), "SecureSlot<%s>", inner);
                ctype = pergyra_type_to_c(secure_name);
            } else {
                char slot_name_buf[128];
                snprintf(slot_name_buf, sizeof(slot_name_buf), "Slot<%s>", inner);
                ctype = pergyra_type_to_c(slot_name_buf);
            }
            if (ctype == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "cannot lower %s declaration '%s' with inner type '%s' to a concrete slot type",
                        is_move_decl ? "move-token" : "view",
                        name != NULL ? name : "(anonymous)",
                        inner);
                }
                free(ann_type_name);
                return;
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
            const char *sugar_inner = NULL;
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                sugar_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                sugar_inner = slot_inner_type_name(ann_name);
            }
            if (sugar_inner == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "cannot emit slot sugar declaration for '%s': missing explicit Slot<T>/SecureSlot<T> inner type",
                        name != NULL ? name : "(anonymous)");
                }
                free(ann_type_name);
                return;
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
        const char *inner = NULL;
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
        if (inner == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot emit BoxArray declaration for '%s': missing explicit Box<Array<T>> annotation",
                    name != NULL ? name : "(anonymous)");
            }
            free(ann_type_name);
            return;
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
        if (ann_type_name != NULL) {
            register_typed_var(ctx, name, ann_type_name);
        } else {
            char *box_array_type = strdup_fmt("Box<Array<%s>>", inner);
            register_typed_var(ctx, name, box_array_type);
            free(box_array_type);
        }
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

    if (resolved_ann != NULL
        && resolved_ann->type == AST_TYPE
        && resolved_ann->data.type.name != NULL
        && init != NULL
        && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "ToObject") == 0
        && init->data.call.arg_count >= 2
        && init->data.call.arguments[1] != NULL
        && init->data.call.arguments[1]->type == AST_IDENTIFIER) {
        ASTNode *target_decl = find_class_decl(ctx, resolved_ann->data.type.name);
        if (target_decl != NULL
            && target_decl->type == AST_CLASS_DECL
            && target_decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT) {
            const char *source_name = init->data.call.arguments[1]->data.identifier.name;
            register_projection_borrow_var(ctx, name,
                ann_type_name != NULL ? ann_type_name : resolved_ann->data.type.name,
                source_name);
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
    const char *c_type = NULL;
    if (ann != NULL) {
        c_type = pergyra_ast_type_to_c(ann);
    } else if (init != NULL) {
        const char *inferred_type = NULL;
        /* Type inference from initializer */
        if (init->type == AST_NUMBER)  c_type = "int32_t";
        else if (init->type == AST_STRING)  c_type = "char*";
        else if (init->type == AST_BOOLEAN) c_type = "bool";
        else if (init->type == AST_SPAWN_EXPR) c_type = "PgyTaskHandle";
        else if (init->type == AST_CHANNEL_RECV) {
            inferred_type = infer_expression_type_name(ctx, init);
            if (inferred_type != NULL)
                c_type = pergyra_type_to_c(inferred_type);
        }
        else if (init->type == AST_CALL || init->type == AST_ARRAY_LITERAL || init != NULL) {
            inferred_type = infer_expression_type_name(ctx, init);
            if (inferred_type != NULL)
                c_type = pergyra_type_to_c(inferred_type);
        }
    }

    if (c_type == NULL) {
        transpiler_set_backend_error(
            ctx,
            "cannot determine C type for let binding '%s'; explicit annotation or resolvable initializer type is required",
            name != NULL ? name : "<binding>");
        free(ann_type_name);
        return;
    }

    /* Collection constructors: let s: Set<Int> = SetNew()
     * Emit the correct type-specific initializer from the annotation. */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER
        && ann_type_name != NULL
        && strcmp(init->data.call.callee->data.identifier.name, "SetNew") == 0
        && strncmp(ann_type_name, "Set<", 4) == 0) {
        const char *inner = slot_inner_type_name(ann_type_name);
        const char *c_type = pergyra_type_to_c(ann_type_name);
        const char *suffix = collection_runtime_suffix(inner);
        ensure_collection_specialization(ctx, "Set", inner);
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = pgy_set_new_%s();\n",
                      c_type, name, suffix);
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
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
            /* Domain/runtime constructors carry internal state bits.
             * Reuse expression lowering so zone/world/relation/effect
             * literals preserve dirty/ready/runtime metadata. */
            ASTNode *zone_decl = find_zone_decl(ctx, ann_type_name);
            ASTNode *world_decl = find_world_decl(ctx, ann_type_name);
            ASTNode *relation_decl = find_relation_decl(ctx, ann_type_name);
            ASTNode *effect_decl = find_effect_decl(ctx, ann_type_name);
            ASTNode *party_decl = find_party_decl(ctx, ann_type_name);
            ASTNode *roster_decl = find_roster_decl(ctx, ann_type_name);
            if ((zone_decl != NULL && zone_decl->type == AST_ZONE_DECL)
                || (world_decl != NULL && world_decl->type == AST_WORLD_DECL)
                || (party_decl != NULL && party_decl->type == AST_PARTY_DECL)
                || (roster_decl != NULL && roster_decl->type == AST_ROSTER_DECL)
                || (relation_decl != NULL && relation_decl->type == AST_RELATION_DECL)
                || (effect_decl != NULL && effect_decl->type == AST_EFFECT_DECL)) {
                char *init_expr = emit_expression(init, ctx);
                codebuf_write(ctx->out, "%s %s = %s;\n",
                    ann_type_name, name, init_expr != NULL ? init_expr : "0");
                free(init_expr);
            } else {
                codebuf_write(ctx->out, "%s %s = {0};\n", ann_type_name, name);
            }
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
            ctx->expected_type = ann_type_name;
            char *init_expr = emit_expression(init, ctx);
            ctx->expected_type = NULL;
            codebuf_write(ctx->out, "%s = %s;\n", decl, init_expr);
            free(init_expr);
        } else {
            codebuf_write(ctx->out, "%s = 0;\n", decl);
        }
        free(decl);
    } else if (init != NULL) {
        ctx->expected_type = ann_type_name;
        char *init_expr = emit_expression(init, ctx);
        ctx->expected_type = NULL;
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
        if (inferred != NULL) {
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
        const char *pt = NULL;
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p->type != NULL)
            ensure_type_specializations_from_ast(ctx, p->type);
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (pt == NULL) {
            transpiler_set_backend_error(
                ctx,
                "cannot determine parameter type for forward declaration '%s' at argument %zu",
                name != NULL ? name : "<function>",
                i);
            codebuf_destroy(params_sig);
            free(header_decl);
            g_type_render_ctx = saved_render_ctx;
            return;
        }
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
        /* Match by name only - pointer comparison may fail across different AST instances */
        return routine;
    }

    return NULL;
}

static const char *
transpiler_find_mir_intent_meta_arg(const MIRRoutine *routine,
                                    const char *step_name,
                                    const char *inst_name)
{
    if (routine == NULL || inst_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, inst_name) != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }
            return inst->arg0;
        }
    }
    return NULL;
}

static size_t
transpiler_collect_mir_intent_who_aliases(const MIRRoutine *routine,
                                          const char *step_name,
                                          const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentWho") != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc((void *)aliases, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = inst->arg0;
        }
    }

    *aliases_out = aliases;
    return count;
}

static size_t
transpiler_collect_mir_intent_participants(const MIRRoutine *routine,
                                           const char ***aliases_out,
                                           const char ***types_out)
{
    const char **aliases = NULL;
    const char **types = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (types_out != NULL)
        *types_out = NULL;
    if (routine == NULL || aliases_out == NULL || types_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown_aliases;
            const char **grown_types;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentParticipant") != 0)
                continue;
            if (inst->arg0 == NULL || inst->arg1 == NULL)
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown_aliases = malloc(new_capacity * sizeof(const char *));
                grown_types = malloc(new_capacity * sizeof(const char *));
                if (grown_aliases == NULL || grown_types == NULL) {
                    free((void *)grown_aliases);
                    free((void *)grown_types);
                    free((void *)aliases);
                    free((void *)types);
                    return 0;
                }
                if (count > 0) {
                    memcpy((void *)grown_aliases, (const void *)aliases,
                           count * sizeof(const char *));
                    memcpy((void *)grown_types, (const void *)types,
                           count * sizeof(const char *));
                }
                free((void *)aliases);
                free((void *)types);
                aliases = grown_aliases;
                types = grown_types;
                capacity = new_capacity;
            }
            aliases[count] = inst->arg0;
            types[count] = inst->arg1;
            count++;
        }
    }

    *aliases_out = aliases;
    *types_out = types;
    return count;
}

static size_t
transpiler_collect_mir_intent_steps(const MIRRoutine *routine, ASTNode ***steps_out)
{
    ASTNode **steps = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (steps_out != NULL)
        *steps_out = NULL;
    if (routine == NULL || steps_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode **grown;

            if (inst->kind != MIR_INST_STMT || inst->ast == NULL
                || inst->ast->type != AST_INTENT_STEP) {
                continue;
            }
            if (inst->name != NULL && strncmp(inst->name, "Intent", 6) == 0)
                continue;
            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                grown = realloc(steps, new_capacity * sizeof(ASTNode *));
                if (grown == NULL) {
                    free(steps);
                    return 0;
                }
                steps = grown;
                capacity = new_capacity;
            }
            steps[count++] = inst->ast;
        }
    }

    *steps_out = steps;
    return count;
}

static ASTNode *
transpiler_find_mir_intent_check_expr(const MIRRoutine *routine,
                                      const char *step_name,
                                      const char *phase_name)
{
    if (routine == NULL || phase_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind != MIR_INST_STMT || inst->ast == NULL)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentCheck") != 0)
                continue;
            if (inst->arg0 == NULL || strcmp(inst->arg0, phase_name) != 0)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }
            return inst->ast;
        }
    }
    return NULL;
}

static size_t
transpiler_collect_mir_intent_eval_exprs(const MIRRoutine *routine,
                                         const char *step_name,
                                         const char *phase_name,
                                         ASTNode ***exprs_out)
{
    ASTNode **exprs = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (exprs_out != NULL)
        *exprs_out = NULL;
    if (routine == NULL || phase_name == NULL || exprs_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode **grown;

            if (inst->kind != MIR_INST_STMT || inst->ast == NULL)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentEval") != 0)
                continue;
            if (inst->arg0 == NULL || strcmp(inst->arg0, phase_name) != 0)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc(exprs, new_capacity * sizeof(ASTNode *));
                if (grown == NULL) {
                    free(exprs);
                    return 0;
                }
                exprs = grown;
                capacity = new_capacity;
            }
            exprs[count++] = inst->ast;
        }
    }

    *exprs_out = exprs;
    return count;
}

static ASTNode *
transpiler_find_mir_intent_eval_expr(const MIRRoutine *routine,
                                     const char *step_name,
                                     const char *phase_name)
{
    ASTNode **exprs = NULL;
    ASTNode *result = NULL;
    size_t count = transpiler_collect_mir_intent_eval_exprs(
        routine, step_name, phase_name, &exprs);
    if (count > 0)
        result = exprs[0];
    free(exprs);
    return result;
}

static size_t
transpiler_collect_mir_intent_dispatch_aliases(const MIRRoutine *routine,
                                               const char *step_name,
                                               const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentDispatch") != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc((void *)aliases, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = inst->arg0;
        }
    }

    *aliases_out = aliases;
    return count;
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
    for (size_t i = 0; i < TRANSPILE_SSA_NAME_BUCKETS; ++i) {
        if (map->buckets[i].in_use && map->buckets[i].base_name != NULL)
            free((void *)map->buckets[i].base_name);
    }
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
            bucket->base_name = pergyra_strdup(base_name);
            if (bucket->base_name == NULL) {
                bucket->in_use = false;
                return false;
            }
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
    uint32_t line = 0;
    uint32_t column = 0;

    if (out == NULL || routine == NULL || block == NULL)
        return;

    source_stmt = block->source_ast;
    if (source_stmt != NULL) {
        line = source_stmt->line;
        column = source_stmt->column;
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

static const MIRRoutine *
transpiler_find_mir_method(const TranspilerCtx *ctx,
                           const char *owner_name,
                           const ASTNode *method_decl)
{
    const char *target = NULL;

    if (ctx == NULL || ctx->mir == NULL || owner_name == NULL
        || method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->data.func_decl.name == NULL) {
        return NULL;
    }

    target = method_decl->data.func_decl.name;
    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_METHOD)
            continue;
        if (routine->ast == method_decl)
            return routine;
        if (routine->name != NULL
            && routine->owner_name != NULL
            && strcmp(routine->name, target) == 0
            && strcmp(routine->owner_name, owner_name) == 0) {
            return routine;
        }
    }

    return NULL;
}

static const char *
transpiler_resolve_active_ssa_name(const TranspilerCtx *ctx,
                                   const char *base_name)
{
    if (ctx == NULL || ctx->active_ssa_map == NULL || base_name == NULL)
        return NULL;
    return transpiler_resolve_ssa_name(
        (const TranspilerSSANameMap *)ctx->active_ssa_map,
        base_name);
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

static bool
transpiler_c_type_uses_scalar_zero(const char *c_type)
{
    if (c_type == NULL)
        return true;
    if (strchr(c_type, '*') != NULL)
        return true;
    return strcmp(c_type, "int") == 0
           || strcmp(c_type, "int32_t") == 0
           || strcmp(c_type, "int64_t") == 0
           || strcmp(c_type, "size_t") == 0
           || strcmp(c_type, "bool") == 0
           || strcmp(c_type, "float") == 0
           || strcmp(c_type, "double") == 0;
}

static const char *
transpiler_infer_local_type_name_from_expr(TranspilerCtx *ctx,
                                           const ASTNode *func_decl,
                                           ASTNode *expr)
{
    const char *semantic_type = infer_expression_type_name(ctx, expr);
    if (semantic_type != NULL && semantic_type[0] != '\0')
        return semantic_type;
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
            return transpiler_find_local_type_name(ctx, func_decl, expr->data.identifier.name);
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
                    return transpiler_infer_local_type_name_from_expr(ctx, func_decl, expr->data.binary.left);
            }
        case AST_UNARY:
            if (expr->data.unary.op.type == TOKEN_NOT)
                return "Bool";
            return transpiler_infer_local_type_name_from_expr(ctx, func_decl, expr->data.unary.operand);
        default:
            return NULL;
    }
}

static const char *
transpiler_find_local_type_name_in_block(TranspilerCtx *ctx,
                                         const ASTNode *func_decl,
                                         ASTNode *body,
                                         const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            const char *found = transpiler_find_local_type_name_in_block(
                ctx,
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
        return transpiler_infer_local_type_name_from_expr(ctx, func_decl, body->data.let_decl.initializer);
    }
    if (body->type == AST_IF_STMT) {
        const char *found = transpiler_find_local_type_name_in_block(ctx, func_decl, body->data.if_stmt.then_branch, base_name);
        if (found != NULL)
            return found;
        return transpiler_find_local_type_name_in_block(ctx, func_decl, body->data.if_stmt.else_branch, base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_local_type_name_in_block(ctx, func_decl, body->data.while_loop.body, base_name);
    return NULL;
}

static const char *
transpiler_find_local_type_name(TranspilerCtx *ctx,
                                const ASTNode *func_decl,
                                const char *base_name)
{
    const char *typed_name = NULL;

    if (base_name == NULL)
        return NULL;
    if (ctx != NULL) {
        typed_name = lookup_typed_var(ctx, base_name);
        if (typed_name != NULL)
            return typed_name;
    }
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
    return transpiler_find_local_type_name_in_block(ctx, func_decl, func_decl->data.func_decl.body, base_name);
}

static bool
transpiler_mir_type_supported(const char *type_name)
{
    if (type_name == NULL)
        return false;
    if (strcmp(type_name, "Void") == 0)
        return true;
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
transpiler_mir_ast_type_supported(const ASTNode *type_node)
{
    char *type_name = NULL;
    const char *c_type = NULL;

    if (type_node == NULL)
        return true;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        if (!transpiler_mir_ast_type_supported(
                type_node->data.event_handler_type.return_type)) {
            return false;
        }
        for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
            if (!transpiler_mir_ast_type_supported(
                    type_node->data.event_handler_type.param_types[i])) {
                return false;
            }
        }
        return true;
    }

    type_name = render_type_name((ASTNode *)type_node);
    if (type_name == NULL)
        return false;
    if (transpiler_mir_type_supported(type_name)) {
        free(type_name);
        return true;
    }

    c_type = pergyra_ast_type_to_c((ASTNode *)type_node);
    if (c_type == NULL || c_type[0] == '\0') {
        free(type_name);
        return false;
    }
    if (strcmp(type_name, "Unknown") == 0 || strcmp(c_type, "Unknown") == 0) {
        free(type_name);
        return false;
    }

    free(type_name);
    return true;
}

static bool
transpiler_mir_function_signature_supported(const ASTNode *func_decl)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;

    if (!transpiler_mir_ast_type_supported(func_decl->data.func_decl.return_type))
        return false;

    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *param = func_decl->data.func_decl.params[i];
        if (param == NULL || param->type == NULL)
            continue;
        if (!transpiler_mir_ast_type_supported(param->type))
            return false;
    }

    return true;
}

static char *
emit_expression_with_ssa_map(ASTNode *node, TranspilerCtx *ctx,
                             const TranspilerSSANameMap *ssa_map)
{
    const void *saved_active_ssa_map;
    char *result;

    if (node == NULL)
        return pergyra_strdup("0");
    if (ctx == NULL)
        return emit_expression(node, ctx);

    saved_active_ssa_map = ctx->active_ssa_map;
    ctx->active_ssa_map = ssa_map;
    result = emit_expression(node, ctx);
    ctx->active_ssa_map = saved_active_ssa_map;
    return result;
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
                                     TranspilerSSANameMap *out_ssa_map,
                                     char *reason,
                                     size_t reason_cap)
{
    TranspilerSSANameMap ssa_map = {0};
    TranspilerSSANameMap *ssa_map_out = &ssa_map;
    const void *saved_active_ssa_map;
    bool ok = true;

    if (buf == NULL || func_decl == NULL || mir_routine == NULL || block == NULL || ctx == NULL)
        return false;
    if (reason != NULL && reason_cap > 0)
        reason[0] = '\0';

    if (out_ssa_map != NULL)
        ssa_map_out = out_ssa_map;
    if (!transpiler_emit_mir_block_with_ssa_map(ssa_map_out, block)) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap,
                     "MIR emission failed for block %zu in %s: missing SSA entry map",
                     block->id, func_decl->type == AST_FUNC_DECL
                     ? func_decl->data.func_decl.name
                     : func_decl->data.intent_decl.name);
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

    saved_active_ssa_map = ctx->active_ssa_map;
    ctx->active_ssa_map = ssa_map_out;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        ASTNode *stmt = inst->ast;

        if (inst->kind == MIR_INST_DEF
            && stmt != NULL
            && stmt->type == AST_LET_DECL
            && stmt->data.let_decl.name != NULL
            && inst->arg0 != NULL
            && inst->result_name != NULL
            && strcmp(inst->arg0, stmt->data.let_decl.name) == 0) {
            const char *saved_type_hint = ctx->active_type_hint;
            char *rendered_type_hint = NULL;
            char *local_type_name_owned = NULL;
            char *lhs = NULL;
            char *rhs = NULL;

            if (stmt->data.let_decl.type != NULL) {
                rendered_type_hint = render_type_name(stmt->data.let_decl.type);
                ctx->active_type_hint = rendered_type_hint;
            }
            if (rendered_type_hint != NULL)
                local_type_name_owned = pergyra_strdup(rendered_type_hint);
            else if (stmt->data.let_decl.initializer != NULL) {
                const char *inferred =
                    infer_expression_type_name(ctx, stmt->data.let_decl.initializer);
                if (inferred != NULL)
                    local_type_name_owned = pergyra_strdup(inferred);
            }

            lhs = transpiler_make_c_ssa_name(inst->result_name);
            rhs = emit_expression_with_ssa_map(stmt->data.let_decl.initializer, ctx, ssa_map_out);
            ctx->active_type_hint = saved_type_hint;
            free(rendered_type_hint);

            if (rhs == NULL) {
                free(lhs);
                free(local_type_name_owned);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %zu emission failed: unable to render initializer for '%s'",
                             block->id,
                             stmt->data.let_decl.name != NULL
                                 ? stmt->data.let_decl.name
                                 : "<binding>");
                }
                ok = false;
                break;
            }

            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);

            if (!transpiler_ssa_name_map_set(ssa_map_out, stmt->data.let_decl.name,
                                             inst->result_name)) {
                free(local_type_name_owned);
                ok = false;
                break;
            }
            if (local_type_name_owned != NULL)
                register_typed_var(ctx, stmt->data.let_decl.name, local_type_name_owned);
            free(local_type_name_owned);
            continue;
        }

        if (inst->kind == MIR_INST_DEF
            && stmt != NULL
            && stmt->type == AST_ASSIGNMENT
            && stmt->data.assignment.target != NULL
            && stmt->data.assignment.target->type == AST_IDENTIFIER
            && inst->arg0 != NULL
            && inst->result_name != NULL) {
            const char *target_name = stmt->data.assignment.target->data.identifier.name;
            char *lhs = NULL;
            char *rhs = NULL;

            if (target_name == NULL || strcmp(inst->arg0, target_name) != 0)
                continue;

            lhs = transpiler_make_c_ssa_name(inst->result_name);
            rhs = emit_expression_with_ssa_map(stmt->data.assignment.value, ctx, ssa_map_out);
            if (rhs == NULL) {
                free(lhs);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %zu emission failed: unable to render assignment to '%s'",
                             block->id, target_name != NULL ? target_name : "<target>");
                }
                ok = false;
                break;
            }

            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);

            if (!transpiler_ssa_name_map_set(ssa_map_out, target_name, inst->result_name)) {
                ok = false;
                break;
            }
            continue;
        }

        if (inst->kind != MIR_INST_STMT)
            continue;
        if (stmt == NULL
            || stmt->type == AST_IF_STMT
            || stmt->type == AST_WHILE_LOOP
            || stmt->type == AST_RETURN) {
            continue;
        }
        emit_statement(stmt, ctx);
    }

    ctx->active_ssa_map = saved_active_ssa_map;
    return ok;
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
            for (size_t i = 0; i < expr->data.array_literal.count; i++) {
                if (!transpiler_expr_identifiers_mapped(expr->data.array_literal.elements[i], ssa_map,
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
        TranspilerSSANameMap ssa_map = {0};

        if (block == NULL || (require_non_cleanup && block->is_cleanup)
            || !block->is_reachable)
            continue;
        if (!transpiler_emit_mir_block_with_ssa_map(&ssa_map, block))
            return false;
        /* Add function/intent parameters to SSA map - they're valid C identifiers, not SSA vars */
        if (func_decl != NULL) {
            if (func_decl->type == AST_FUNC_DECL) {
                for (size_t p = 0; p < func_decl->data.func_decl.param_count; p++) {
                    FuncParam *param = func_decl->data.func_decl.params[p];
                    if (param != NULL && param->name != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, param->name, param->name);
                    }
                }
            } else if (func_decl->type == AST_INTENT_DECL) {
                /* For intent, extract alias from involves/value bindings */
                for (size_t p = 0; p < func_decl->data.intent_decl.involve_count; p++) {
                    ASTNode *involves = func_decl->data.intent_decl.involves[p];
                    if (involves != NULL && involves->type == AST_INTENT_INVOLVES
                        && involves->data.intent_involves.alias != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, involves->data.intent_involves.alias, involves->data.intent_involves.alias);
                    }
                }
                for (size_t p = 0; p < func_decl->data.intent_decl.value_count; p++) {
                    ASTNode *value = func_decl->data.intent_decl.values[p];
                    if (value != NULL && value->type == AST_INTENT_VALUE
                        && value->data.intent_value.alias != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, value->data.intent_value.alias,
                            value->data.intent_value.alias);
                    }
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

        if (!transpiler_validate_mir_emission_block_shape(block,
                                                          routine_name,
                                                          require_cleanup,
                                                          reason,
                                                          reason_cap)) {
            return false;
        }

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
                && kind != MIR_INST_PHI && kind != MIR_INST_DEF
                && kind != MIR_INST_STMT) {
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
transpiler_validate_mir_emission_block_shape(const MIRBasicBlock *block,
                                            const char *routine_name,
                                            bool require_cleanup,
                                            char *reason,
                                            size_t reason_cap)
{
    bool has_branch = false;
    bool has_return = false;
    size_t branch_count = 0;
    size_t return_count = 0;
    size_t term_index = SIZE_MAX;

    if (block == NULL || routine_name == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_BRANCH) {
            if (term_index == SIZE_MAX) {
                term_index = i;
            }
            has_branch = true;
            branch_count++;
            if (inst->ast == NULL && (reason != NULL && reason_cap > 0)) {
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu branch instruction misses condition AST",
                         routine_name, block->id);
                return false;
            }
        } else if (inst->kind == MIR_INST_RETURN) {
            if (term_index == SIZE_MAX) {
                term_index = i;
            }
            has_return = true;
            return_count++;
        } else if (inst->kind != MIR_INST_RESOURCE_OP
                   && inst->kind != MIR_INST_DEF
                   && inst->kind != MIR_INST_PHI
                   && inst->kind != MIR_INST_CLEANUP_EDGE
                   && inst->kind != MIR_INST_STMT) {
            continue;
        }

        if (term_index != SIZE_MAX && i > term_index
            && inst->kind != MIR_INST_RESOURCE_OP
            && inst->kind != MIR_INST_CLEANUP_EDGE) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu has non-trailing instructions after terminal",
                         routine_name, block->id);
            return false;
        }
    }

    if (branch_count > 1 || return_count > 1 || (has_branch && has_return)) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap,
                     "MIR contract invalid for %s: block %zu has %zu branch(es), %zu return(s)",
                     routine_name, block->id, branch_count, return_count);
        return false;
    }

    if (has_branch) {
        if (!block->has_succ_true || !block->has_succ_false) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu has branch without both true/false successors",
                         routine_name, block->id);
            return false;
        }
        if (block->has_cleanup_succ || block->has_rollback_succ || block->has_invalidation_succ) {
            if (!require_cleanup) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: block %zu has branch plus exceptional successor",
                             routine_name, block->id);
                return false;
            }
        }
        return true;
    }

    if (has_return) {
        if (block->has_succ_true || block->has_succ_false) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu return has explicit successors",
                         routine_name, block->id);
            return false;
        }
        if (!require_cleanup
            && (block->has_cleanup_succ || block->has_rollback_succ
                || block->has_invalidation_succ)) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu return has exceptional successor"
                         " without cleanup-capable routine",
                         routine_name, block->id);
            return false;
        }
        return true;
    }

    if (block->has_succ_false) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap,
                     "MIR contract invalid for %s: block %zu has false successor without branch",
                     routine_name, block->id);
        return false;
    }

    if (block->has_cleanup_succ || block->has_rollback_succ || block->has_invalidation_succ) {
        if (!require_cleanup) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %zu has exceptional successor without cleanup-capable routine",
                         routine_name, block->id);
            return false;
        }
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
    if (routine->ast == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has no declaration AST in MIR", func_decl->data.func_decl.name);
        return false;
    }
    /* cleanup blocks are now fully supported - removed restriction */
    if (!transpiler_mir_function_signature_supported(func_decl)) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has unsupported MIR signature", func_decl->data.func_decl.name);
        return false;
    }
    const bool requires_cleanup = routine->has_cleanup_block;
    if (!transpiler_validate_mir_emission_contract(routine,
                                                   func_decl,
                                                   requires_cleanup,
                                                   requires_cleanup,
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

bool
transpiler_can_emit_function_from_mir_with_reason_for_test(
    const ASTNode *func_decl,
    const MIRProgram *mir,
    char *reason,
    size_t reason_cap)
{
    bool can_emit;
    TranspilerCtx *ctx = transpiler_ctx_create();
    if (ctx == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "Out of memory while creating transpiler context");
        return false;
    }
    ctx->mir = mir;
    can_emit = transpiler_can_emit_function_from_mir_with_reason(ctx, func_decl, NULL,
                                                                reason, reason_cap);
    transpiler_ctx_destroy(ctx);
    return can_emit;
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
            snprintf(reason, reason_cap, "intent cannot lower to MIR: no matching MIR routine (found %zu routines)", ctx != NULL && ctx->mir != NULL ? ctx->mir->routine_count : 0);
        return false;
    }
    if (routine->kind != MIR_SCOPE_INTENT
        || routine->ast == NULL
        || !routine->has_cleanup_block) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "intent %s has no MIR cleanup section (kind=%d, ast=%p, has_cleanup_block=%d)",
                intent_decl->data.intent_decl.name, routine->kind, (void*)routine->ast, routine->has_cleanup_block);
        return false;
    }
    if (!transpiler_validate_mir_emission_contract(routine,
                                                   intent_decl,
                                                   true,
                                                   true,
                                                   reason,
                                                   reason_cap)) {
        if (reason != NULL && reason_cap > 0 && reason[0] == '\0')
            snprintf(reason, reason_cap, "intent %s MIR emission contract validation failed", intent_decl->data.intent_decl.name);
        return false;
    }
    if (!transpiler_has_mapping_for_all_emitted_blocks(routine, intent_decl, false, reason, reason_cap)) {
        if (reason != NULL && reason_cap > 0 && reason[0] == '\0')
            snprintf(reason, reason_cap, "intent %s SSA mapping incomplete", intent_decl->data.intent_decl.name);
        return false;
    }
    if (mir_routine_out != NULL)
        *mir_routine_out = routine;
    return true;
}

bool
transpiler_can_emit_intent_cleanup_from_mir_with_reason_for_test(
    const ASTNode *intent_decl,
    const MIRProgram *mir,
    char *reason,
    size_t reason_cap)
{
    bool can_emit;
    TranspilerCtx *ctx = transpiler_ctx_create();
    if (ctx == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "Out of memory while creating transpiler context");
        return false;
    }
    ctx->mir = mir;
    can_emit = transpiler_can_emit_intent_cleanup_from_mir_with_reason(ctx, intent_decl, NULL,
                                                                       reason, reason_cap);
    transpiler_ctx_destroy(ctx);
    return can_emit;
}

static bool
transpiler_emit_mir_resource_hook(TranspilerCtx *ctx,
                                  CodeBuf *out,
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
        return false;

    if (inst->kind == MIR_INST_RESOURCE_OP) {
        if (!transpiler_emit_mir_resource_op(out, indent, inst, inst->type_layout, NULL)) {
            if (ctx != NULL && ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot emit MIR resource op '%s' for slot '%s': missing typed runtime layout",
                    inst->name != NULL ? inst->name : "<op>",
                    inst->slot_anchor != NULL ? inst->slot_anchor : "<slot>");
            }
            return false;
        }
        if (!cleanup_hook)
            return true;
    }

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
    return true;
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
        const char *pt = NULL;
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (pt == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot determine parameter type for MIR-emitted function '%s' at argument %zu",
                    name != NULL ? name : "<function>",
                    i);
            }
            codebuf_destroy(params_sig);
            free(header_decl);
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            g_type_render_ctx = saved_render_ctx;
            ctx->out = saved_out;
            snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                "%s", saved_return_type);
            return;
        }
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
    header_decl = NULL;
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
            const char *c_type = NULL;
            char *c_name = NULL;
            if ((inst->kind != MIR_INST_DEF && inst->kind != MIR_INST_PHI)
                || inst->result_name == NULL) {
                continue;
            }
            const char *lookup_name =
                (inst->kind == MIR_INST_PHI)
                    ? inst->name
                    : (inst->arg0 != NULL ? inst->arg0 : inst->name);
            type_name = transpiler_find_local_type_name(ctx, node,
                lookup_name);
            if (type_name != NULL)
                c_type = pergyra_type_to_c(type_name);
            if (c_type == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "cannot determine C type for MIR local '%s' in function '%s'",
                        inst->result_name != NULL ? inst->result_name : "<ssa>",
                        name != NULL ? name : "<function>");
                }
                codebuf_destroy(params_sig);
                free(header_decl);
                ctx->slot_var_count = saved_slot_count;
                ctx->typed_var_count = saved_typed_count;
                g_type_render_ctx = saved_render_ctx;
                ctx->out = saved_out;
                snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                    "%s", saved_return_type);
                return;
            }
            c_name = transpiler_make_c_ssa_name(inst->result_name);
            write_indent(ctx);
            if (transpiler_c_type_uses_scalar_zero(c_type))
                codebuf_write(ctx->out, "%s %s = 0;\n", c_type, c_name);
            else
                codebuf_write(ctx->out, "%s %s = (%s){0};\n", c_type, c_name, c_type);
            free(c_name);
        }
    }

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
        block_emitted = transpiler_emit_mir_block_statements(ctx->out, node, mir_routine,
                                                           block, ctx, &block_ssa_map,
                                                           block_reason,
                                                           sizeof(block_reason));
        if (!block_emitted) {
            transpiler_ssa_map_clear(&block_ssa_map);
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "MIR block emission failed in function '%s' at block %zu: %s",
                    name != NULL ? name : "<function>",
                    block->id,
                    block_reason[0] != '\0' ? block_reason : "unknown reason");
            }
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                "%s", saved_return_type);
            g_type_render_ctx = saved_render_ctx;
            ctx->out = saved_out;
            return;
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
                if (!transpiler_emit_mir_resource_hook(ctx, ctx->out, ctx->indent, inst, "0", false)) {
                    ctx->slot_var_count = saved_slot_count;
                    ctx->typed_var_count = saved_typed_count;
                    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                        "%s", saved_return_type);
                    g_type_render_ctx = saved_render_ctx;
                    ctx->out = saved_out;
                    return;
                }
            } else if (inst->kind == MIR_INST_BRANCH) {
                char *cond = emit_expression_with_ssa_map(inst->ast, ctx,
                    &block_ssa_map);
                if (cond == NULL)
                    cond = pergyra_strdup("false");
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
                    codebuf_write(ctx->out, "return %s;\n", ret_expr != NULL ? ret_expr : "0");
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

    /* Emit cleanup blocks if present (for intent compensation) */
    if (mir_routine->has_cleanup_block) {
        write_indent(ctx);
        codebuf_write(ctx->out, "/* cleanup-emitted-from-mir */\n");
        for (size_t i = 0; i < mir_routine->block_count; i++) {
            const MIRBasicBlock *block = &mir_routine->blocks[i];
            if (!block->is_cleanup || !block->is_reachable)
                continue;
            write_indent(ctx);
            codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n", name, block->id);
            for (size_t j = 0; j < block->instruction_count; j++) {
                const MIRInstruction *inst = &block->instructions[j];
                if (inst->kind == MIR_INST_CLEANUP_EDGE) {
                    if (!transpiler_emit_mir_resource_hook(ctx, ctx->out, ctx->indent, inst,
                                                           "__intent_handle", true)) {
                        ctx->slot_var_count = saved_slot_count;
                        ctx->typed_var_count = saved_typed_count;
                        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                            "%s", saved_return_type);
                        g_type_render_ctx = saved_render_ctx;
                        ctx->out = saved_out;
                        return;
                    }
                } else if (inst->kind == MIR_INST_RETURN) {
                    if (inst->ast != NULL) {
                        char *ret_expr = emit_expression(inst->ast, ctx);
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
    bool opened_body = false;
    if (ctx != NULL && ctx->mir != NULL) {
        char reason[256];
        if (transpiler_can_emit_function_from_mir_with_reason(
                ctx, node, &mir_routine, reason, sizeof(reason))) {
            emit_func_decl_from_mir_named(node, mir_routine, emitted_name, buf, ctx);
        } else if (ctx->backend_error == NULL) {
            const char *func_name = (node != NULL && node->type == AST_FUNC_DECL
                                     && node->data.func_decl.name != NULL)
                ? node->data.func_decl.name
                : "(anonymous)";
            ctx->backend_error = strdup_fmt(
                "MIR-only C path missing routine for function '%s': %s",
                func_name,
                reason[0] != '\0' ? reason : "unknown reason");
        }
        return;
    }
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
        const char *pt = NULL;
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "function parameter '%s' of '%s'",
            p != NULL && p->name != NULL ? p->name : "(anonymous)",
            name != NULL ? name : "(anonymous)");
        pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
        if (pt == NULL)
            goto emit_func_decl_named_fail;
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
    opened_body = true;
    free(header_decl);
    header_decl = NULL;
    codebuf_destroy(params_sig);
    params_sig = NULL;

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
    return;

emit_func_decl_named_fail:
    if (opened_body)
        ctx->indent--;
    free(header_decl);
    if (params_sig != NULL)
        codebuf_destroy(params_sig);
    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
        "%s", saved_return_type);
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
            const char *pt = NULL;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "extern parameter '%s' of '%s'",
                p != NULL && p->name != NULL ? p->name : "(anonymous)",
                name != NULL ? name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL)
                return;
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
        else {
            transpiler_set_backend_error(
                ctx,
                "cannot build generic specialization name for class '%s': unresolved generic argument at position %zu",
                class_decl->data.class_decl.name != NULL
                    ? class_decl->data.class_decl.name
                    : "<class>",
                i);
            codebuf_destroy(nbuf);
            return class_decl->data.class_decl.name;
        }
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
        if (ga->params[i] != NULL && ga->params[i]->name != NULL)
            snprintf(b->concrete_type, sizeof(b->concrete_type), "%s",
                     ga->params[i]->name);
        else if (ga->params[i] != NULL && ga->params[i]->constraint != NULL
                 && ga->params[i]->constraint->type == AST_TYPE)
            snprintf(b->concrete_type, sizeof(b->concrete_type), "%s",
                     ga->params[i]->constraint->data.type.name);
        else {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot resolve generic class binding '%s' for specialization '%s'",
                    gp->params[i] != NULL && gp->params[i]->name != NULL
                        ? gp->params[i]->name
                        : "(anonymous)",
                    class_decl != NULL && class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name
                        : "(anonymous)");
            }
            ctx->generic_binding_count = saved_binding_count;
            codebuf_destroy(nbuf);
            return NULL;
        }

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
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "generic class field '%s.%s'",
            spec_name != NULL ? spec_name : "(anonymous)",
            f != NULL && f->name != NULL ? f->name : "(anonymous)");
        ft = transpiler_require_ast_c_type(ctx, f != NULL ? f->type : NULL, surface_desc);
        if (ft == NULL) {
            g_type_render_ctx = saved_render_ctx;
            ctx->generic_binding_count = saved_binding_count;
            codebuf_destroy(nbuf);
            return NULL;
        }
        codebuf_write(ctx->helpers, "    %s %s;\n", ft, f->name);
    }
    codebuf_write(ctx->helpers, "} %s;\n", spec_name);

    /* Container types — suppress unused function warnings for Slot/Box helpers */
    codebuf_write(ctx->helpers,
        "\n#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n"
        "#pragma GCC diagnostic pop\n",
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
            const char *pt = NULL;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "generic class method parameter '%s.%s(%s)'",
                spec_name != NULL ? spec_name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)",
                p != NULL && p->name != NULL ? p->name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL) {
                g_type_render_ctx = saved_render_ctx;
                ctx->generic_binding_count = saved_binding_count;
                codebuf_destroy(nbuf);
                return NULL;
            }
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

    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *f = node->data.class_decl.fields[i];
        if (f != NULL)
            ensure_type_specializations_from_ast_to(ctx, ctx->out, f->type);
    }
    for (size_t i = 0; i < node->data.class_decl.method_count; i++)
        ensure_collection_specializations_from_stmt_to(ctx, ctx->out,
            node->data.class_decl.methods[i]);

    codebuf_write(ctx->out, "\ntypedef struct %s\n{\n", name);

    /* Fields */
    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *f = node->data.class_decl.fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "class field '%s.%s'",
            name != NULL ? name : "(anonymous)",
            f != NULL && f->name != NULL ? f->name : "(anonymous)");
        ft = transpiler_require_ast_c_type(ctx, f != NULL ? f->type : NULL, surface_desc);
        if (ft == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n", ft, f->name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Auto-generate Slot and Result container types for this struct.
     * This allows Slot<MyStruct>, Result<MyStruct> in user code. */
    codebuf_write(ctx->out,
        "\n/* Auto-generated container types for %s */\n"
        "#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n"
        "#pragma GCC diagnostic pop\n",
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
        const MIRRoutine *mir_method = transpiler_find_mir_method(ctx, name, method);
        if (method->type != AST_FUNC_DECL)
            continue;
        if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "MIR-only C path missing routine for class method '%s.%s'",
                    name != NULL ? name : "(anonymous-class)",
                    method->data.func_decl.name != NULL
                        ? method->data.func_decl.name
                        : "(anonymous)");
            }
            return;
        }
        if (mir_method != NULL) {
            char emitted_name[256];
            snprintf(emitted_name, sizeof(emitted_name), "%s_%s", name,
                method->data.func_decl.name);
            emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
            continue;
        }

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
            const char *pt = NULL;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "class method parameter '%s.%s(%s)'",
                name != NULL ? name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)",
                p != NULL && p->name != NULL ? p->name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL)
                return;
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

    const char *inner = NULL;
    if (node->data.with_stmt.slot_type != NULL)
        inner = node->data.with_stmt.slot_type->data.type.name;
    if (inner == NULL) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot emit with-slot alias '%s': missing explicit slot type",
                alias != NULL ? alias : "(anonymous)");
        }
        return;
    }

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

static int
transpiler_find_loop_label_depth(const TranspilerCtx *ctx, const char *label)
{
    if (ctx == NULL || label == NULL)
        return -1;

    for (int i = ctx->loop_depth - 1; i >= 0; i--) {
        if (ctx->loop_labels[i] != NULL
            && strcmp(ctx->loop_labels[i], label) == 0) {
            return i;
        }
    }

    return -1;
}

void
emit_for_loop(ASTNode *node, TranspilerCtx *ctx)
{
    const char *var = node->data.for_loop.variable;
    int loop_slot = ctx->loop_depth;
    int loop_id = ++ctx->tmp_counter;

    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_labels[loop_slot] = node->data.for_loop.label;
        snprintf(ctx->loop_break_labels[loop_slot],
                 sizeof(ctx->loop_break_labels[loop_slot]),
                 "_pgy_loop_break_%d", loop_id);
        snprintf(ctx->loop_continue_labels[loop_slot],
                 sizeof(ctx->loop_continue_labels[loop_slot]),
                 "_pgy_loop_continue_%d", loop_id);
        ctx->loop_break_label_used[loop_slot] = false;
        ctx->loop_continue_label_used[loop_slot] = false;
        ctx->loop_depth++;
    }

    /* for-in collection loop: for item in array { } */
    if (node->data.for_loop.iterable != NULL) {
        char *coll = emit_expression(node->data.for_loop.iterable, ctx);
        const char *coll_type = infer_expression_type_name(ctx,
            node->data.for_loop.iterable);
        const char *elem_type = NULL;
        const char *length_field = "count";  /* default for List */
        if (coll_type != NULL
            && (strncmp(coll_type, "Array<", 6) == 0
                || strncmp(coll_type, "Slice<", 6) == 0)) {
            elem_type = pergyra_type_to_c(slot_inner_type_name(coll_type));
            length_field = "length";  /* Array uses .length, not .count */
        } else if (coll_type != NULL && strncmp(coll_type, "List<", 5) == 0) {
            elem_type = pergyra_type_to_c(slot_inner_type_name(coll_type));
            length_field = "count";  /* List uses .count */
        }
        if (elem_type == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot derive concrete element type for for-in iterable '%s'",
                    coll_type != NULL ? coll_type : "(unknown)");
            }
            free(coll);
            return;
        }

        int idx_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "for (size_t _pgy_idx_%d = 0; "
            "_pgy_idx_%d < %s.%s; "
            "_pgy_idx_%d++)\n",
            idx_id, idx_id, coll, length_field, idx_id);
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = %s.data[_pgy_idx_%d];\n",
            elem_type, var, coll, idx_id);
        register_typed_var(ctx, var, slot_inner_type_name(coll_type));
        if (node->data.for_loop.body != NULL)
            emit_block(node->data.for_loop.body, ctx);
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
            && ctx->loop_continue_label_used[loop_slot]) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s: ;\n",
                ctx->loop_continue_labels[loop_slot]);
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
            && ctx->loop_break_label_used[loop_slot]) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s: ;\n",
                ctx->loop_break_labels[loop_slot]);
        }
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
            ctx->loop_depth--;
            ctx->loop_labels[loop_slot] = NULL;
        }
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
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_continue_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_continue_labels[loop_slot]);
    }
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_break_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_break_labels[loop_slot]);
    }
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_depth--;
        ctx->loop_labels[loop_slot] = NULL;
    }
}

void
emit_while_loop(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(node->data.while_loop.condition, ctx);
    int loop_slot = ctx->loop_depth;
    int loop_id = ++ctx->tmp_counter;
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_labels[loop_slot] = node->data.while_loop.label;
        snprintf(ctx->loop_break_labels[loop_slot],
                 sizeof(ctx->loop_break_labels[loop_slot]),
                 "_pgy_loop_break_%d", loop_id);
        snprintf(ctx->loop_continue_labels[loop_slot],
                 sizeof(ctx->loop_continue_labels[loop_slot]),
                 "_pgy_loop_continue_%d", loop_id);
        ctx->loop_break_label_used[loop_slot] = false;
        ctx->loop_continue_label_used[loop_slot] = false;
        ctx->loop_depth++;
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "while (%s)\n", cond);
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.while_loop.body != NULL)
        emit_block(node->data.while_loop.body, ctx);
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_continue_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_continue_labels[loop_slot]);
    }
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_break_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_break_labels[loop_slot]);
    }
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_depth--;
        ctx->loop_labels[loop_slot] = NULL;
    }
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

    /* Look up in the active program inventory. */
    size_t type_count = 0;
    ASTNode **types = NULL;
    transpiler_active_inventory(ctx, AST_ENUM_DECL, &types, &type_count);
    if (types == NULL) {
        return false;
    }
    for (size_t i = 0; i < type_count; i++) {
        ASTNode *stmt = types[i];
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

        if (mc->data.match_case.patterns != NULL
            && mc->data.match_case.pattern_count > 1) {
            if (i == 0)
                codebuf_write(ctx->out, "if (");
            else
                codebuf_write(ctx->out, "else if (");
            for (size_t p = 0; p < mc->data.match_case.pattern_count; p++) {
                char *pat = emit_expression(mc->data.match_case.patterns[p], ctx);
                if (p > 0)
                    codebuf_write(ctx->out, " || ");
                codebuf_write(ctx->out, "__match_%d == %s", tmp_id, pat);
                free(pat);
            }
        } else if (option_case) {
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
        const char *elem_c_type = NULL;
        const char *inner = NULL;
        if (init_type != NULL
            && (strncmp(init_type, "Array<", 6) == 0
                || strncmp(init_type, "Slice<", 6) == 0)) {
            inner = slot_inner_type_name(init_type);
            elem_c_type = pergyra_type_to_c(inner);
        }
        if (c_init_type == NULL || elem_c_type == NULL || inner == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot lower destructuring initializer of type '%s' to a concrete array element type",
                    init_type != NULL ? init_type : "(unknown)");
            }
            return;
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
        if (party_type == NULL) {
            transpiler_set_backend_error(
                ctx,
                "cannot resolve party type for bind statement '%s.%s = %s'",
                pvar != NULL ? pvar : "<party>",
                slot != NULL ? slot : "<slot>",
                role != NULL ? role : "<role>");
            break;
        }

        /* Find the ability name by scanning the current program view for the
         * party declaration. In MIR-backed emission this view is synthesized
         * from MIRProgram inventory, not from the original HIR. The dyn role
         * slot records the required ability. */
        const char *ability_name = NULL;
        char *ability_tag = NULL;
        {
            ASTNode *it = find_party_decl(ctx, party_type);
            if (it == NULL) {
                transpiler_set_backend_error(
                    ctx,
                    "cannot resolve party declaration '%s' while emitting bind statement '%s.%s = %s'",
                    party_type,
                    pvar != NULL ? pvar : "<party>",
                    slot != NULL ? slot : "<slot>",
                    role != NULL ? role : "<role>");
                break;
            }
            for (size_t ri = 0; ri < it->data.party_decl.role_count; ri++) {
                ASTNode *rs = it->data.party_decl.role_slots[ri];
                if (strcmp(rs->data.role_slot.slot_name, slot) == 0
                    && rs->data.role_slot.ability_count > 0
                    && rs->data.role_slot.required_abilities[0] != NULL) {
                    ability_tag = render_ability_ref_vtable_tag(
                        rs->data.role_slot.required_abilities[0]);
                    ability_name = ability_tag;
                    break;
                }
            }
        }
        if (ability_name == NULL) {
            transpiler_set_backend_error(
                ctx,
                "cannot resolve required ability tag for party slot '%s.%s' while emitting bind statement",
                party_type,
                slot != NULL ? slot : "<slot>");
            free(ability_tag);
            break;
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "%s_bind_%s(&%s, NULL, &%s_%s_vtable_instance);\n",
            party_type, slot, pvar,
            role, ability_name);
        free(ability_tag);
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
    case AST_ROSTER_DECL:
        emit_roster_decl(node, ctx);
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
        if (node->data.break_stmt.label != NULL) {
            int target = transpiler_find_loop_label_depth(
                ctx, node->data.break_stmt.label);
            if (target >= 0) {
                ctx->loop_break_label_used[target] = true;
                codebuf_write(ctx->out, "goto %s;\n",
                    ctx->loop_break_labels[target]);
            } else
                codebuf_write(ctx->out, "break;\n");
        } else {
            codebuf_write(ctx->out, "break;\n");
        }
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
                    const char *ctype = transpiler_require_ast_c_type(
                        ctx,
                        pt,
                        "enum variant payload field");
                    if (ctype == NULL)
                        return;
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
                        const char *ctype = transpiler_require_ast_c_type(
                            ctx,
                            pt,
                            "enum variant constructor parameter");
                        if (ctype == NULL)
                            return;
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

        for (size_t i = 0; i < node->data.enum_decl.method_count; i++) {
            ASTNode *method = node->data.enum_decl.methods[i];
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;
            emit_hosted_method_forward_decl_named(ename, method, false,
                                                  ctx->out, ctx);
        }

        for (size_t i = 0; i < node->data.enum_decl.method_count; i++) {
            ASTNode *method = node->data.enum_decl.methods[i];
            const MIRRoutine *mir_method = transpiler_find_mir_method(ctx, ename, method);
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;
            if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "MIR-only C path missing routine for enum method '%s.%s'",
                        ename != NULL ? ename : "(anonymous-enum)",
                        method->data.func_decl.name != NULL
                            ? method->data.func_decl.name
                            : "(anonymous)");
                }
                return;
            }
            if (mir_method != NULL) {
                char emitted_name[256];
                snprintf(emitted_name, sizeof(emitted_name), "%s_%s", ename,
                    method->data.func_decl.name);
                emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
                continue;
            }

            const char *method_name = method->data.func_decl.name;
            const char *ret_type = "void";
            if (method->data.func_decl.return_type != NULL)
                ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

            codebuf_write(ctx->out, "\n%s\n%s_%s(%s self",
                          ret_type, ename, method_name, ename);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                if (p == NULL || p->name == NULL || strcmp(p->name, "self") == 0)
                    continue;
                const char *pt = NULL;
                char surface_desc[256];
                snprintf(surface_desc, sizeof(surface_desc),
                    "enum method parameter '%s.%s(%s)'",
                    ename != NULL ? ename : "(anonymous)",
                    method_name != NULL ? method_name : "(anonymous)",
                    p != NULL && p->name != NULL ? p->name : "(anonymous)");
                pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
                if (pt == NULL)
                    return;
                codebuf_write(ctx->out, ", %s %s", pt, p->name);
            }
            codebuf_write(ctx->out, ")\n{\n");
            {
                int saved_slot_count = ctx->slot_var_count;
                int saved_typed_count = ctx->typed_var_count;
                const char *saved_class_name = ctx->current_class_name;

                ctx->current_class_name = ename;
                register_typed_var(ctx, "self", ename);
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
                ctx->slot_var_count = saved_slot_count;
                ctx->typed_var_count = saved_typed_count;
                ctx->current_class_name = saved_class_name;
            }
            codebuf_write(ctx->out, "}\n");
        }
        break;
    }
    case AST_CONTINUE:
        write_indent(ctx);
        if (node->data.continue_stmt.label != NULL) {
            int target = transpiler_find_loop_label_depth(
                ctx, node->data.continue_stmt.label);
            if (target >= 0) {
                ctx->loop_continue_label_used[target] = true;
                codebuf_write(ctx->out, "goto %s;\n",
                    ctx->loop_continue_labels[target]);
            } else
                codebuf_write(ctx->out, "continue;\n");
        } else {
            codebuf_write(ctx->out, "continue;\n");
        }
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
        int saved_alias_count = ctx->alias_var_count;
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
        ctx->alias_var_count = saved_alias_count;
    } else {
        emit_statement(node, ctx);
    }
}

static ASTNode *
find_intent_participant_local(ASTNode *intent, const char *alias)
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
    if (ctx == NULL || subject_name == NULL || action_name == NULL)
        return NULL;
    decl = find_subject_host_decl(ctx, subject_name);
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
find_zone_decl_in_program_view(TranspilerCtx *ctx, const char *zone_name)
{
    return find_zone_decl(ctx, zone_name);
}

static const char *
intent_participant_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_participant_local(intent, alias);
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
    ASTNode *involves = find_intent_participant_local(intent, alias);
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
        || find_subject_host_decl(ctx, type_name) != NULL;
}

static bool
intent_involves_uses_pointer_self(TranspilerCtx *ctx, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name_local(involves);
    if (type_name == NULL)
        return false;
    return is_pointer_self_host_type_name(ctx, type_name)
        || find_subject_host_decl(ctx, type_name) != NULL;
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
        ASTNode *involves = find_intent_participant_local(intent, alias);
        const char *participant_c_type;

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || involves == NULL || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }

        participant_c_type = pergyra_ast_type_to_c(involves->data.intent_involves.subject_type);
        write_indent(ctx);
        codebuf_write(out, "%s *__intent_saved_%zu_%s = %s;\n",
            participant_c_type, step_index, alias, alias);
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
        ASTNode *involves = find_intent_participant_local(intent, alias);

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
    const char *participant_type = NULL;
    ASTNode *named_match = NULL;
    ASTNode *typed_match = NULL;

    if (ctx == NULL || intent == NULL || zone_type_name == NULL || alias == NULL) {
        return "<unbound>";
    }

    zone_decl = find_zone_decl_in_program_view(ctx, zone_type_name);
    participant_type = intent_participant_type_name(intent, alias);
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
        if (participant_type != NULL
            && slot->data.domain_slot.type != NULL
            && slot->data.domain_slot.type->type == AST_TYPE
            && slot->data.domain_slot.type->data.type.name != NULL
            && strcmp(slot->data.domain_slot.type->data.type.name, participant_type) == 0) {
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
    const MIRRoutine *mir_routine = NULL;
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    size_t participant_count = 0;

    if (node == NULL || node->type != AST_INTENT_DECL || buf == NULL || ctx == NULL)
        return;
    mir_routine = transpiler_find_mir_intent(ctx, node);
    if (mir_routine != NULL) {
        participant_count = transpiler_collect_mir_intent_participants(
            mir_routine, &participant_aliases, &participant_types);
    }
    codebuf_write(buf, "\nbool\n%s(", node->data.intent_decl.name);
    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        const char *pt = NULL;
        const char *alias = involves != NULL && involves->data.intent_involves.alias != NULL
            ? involves->data.intent_involves.alias : "participant";
        const char *participant_type = participant_types != NULL && i < participant_count
            ? participant_types[i]
            : NULL;
        bool pointer_param = false;
        char surface_desc[256];
        if (i > 0)
            codebuf_write(buf, ", ");
        snprintf(surface_desc, sizeof(surface_desc),
            "intent participant '%s' of '%s'",
            alias != NULL ? alias : "(anonymous)",
            node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
        if (participant_type != NULL) {
            pt = transpiler_require_type_name_c_type(ctx, participant_type, surface_desc);
            pointer_param = is_pointer_self_host_type_name(ctx, participant_type);
        } else if (involves != NULL && involves->data.intent_involves.subject_type != NULL) {
            pt = transpiler_require_ast_c_type(
                ctx, involves->data.intent_involves.subject_type, surface_desc);
            pointer_param = intent_involves_uses_pointer_self(ctx, involves);
        }
        if (pt == NULL) {
            free((void *)participant_aliases);
            free((void *)participant_types);
            return;
        }
        if (participant_aliases != NULL && i < participant_count
            && participant_aliases[i] != NULL) {
            alias = participant_aliases[i];
        }
        codebuf_write(buf, "%s%s%s", pt, pointer_param ? " *" : " ", alias);
    }
    for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
        ASTNode *value = node->data.intent_decl.values[i];
        const char *pt = NULL;
        char surface_desc[256];
        if (node->data.intent_decl.involve_count > 0 || i > 0)
            codebuf_write(buf, ", ");
        snprintf(surface_desc, sizeof(surface_desc),
            "intent value '%s' of '%s'",
            value != NULL && value->data.intent_value.alias != NULL
                ? value->data.intent_value.alias
                : "value",
            node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
        pt = transpiler_require_ast_c_type(
            ctx,
            value != NULL ? value->data.intent_value.value_type : NULL,
            surface_desc);
        if (pt == NULL) {
            free((void *)participant_aliases);
            free((void *)participant_types);
            return;
        }
        codebuf_write(buf, "%s %s", pt,
            value != NULL && value->data.intent_value.alias != NULL
                ? value->data.intent_value.alias : "value");
    }
    codebuf_write(buf, ");\n");
    free((void *)participant_aliases);
    free((void *)participant_types);
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
    for (size_t i = 0; i < intent->data.intent_decl.value_count; i++) {
        ASTNode *value = intent->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE
            || value->data.intent_value.value_type == NULL) {
            continue;
        }
        if (!transpiler_can_forward_declare_type_early(
                ctx, value->data.intent_value.value_type)) {
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
    bool mir_only_intent = false;
    size_t subject_count = 0;
    ASTNode **mir_steps = NULL;
    ASTNode **step_nodes = NULL;
    size_t step_count = 0;
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    size_t participant_count = 0;
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
    mir_only_intent = ctx->mir != NULL;
    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type), "Bool");
    emit_cleanup_from_mir = transpiler_can_emit_intent_cleanup_from_mir(ctx, node, &mir_routine);
    if (mir_routine != NULL) {
        participant_count = transpiler_collect_mir_intent_participants(
            mir_routine, &participant_aliases, &participant_types);
    }
    if (mir_routine != NULL)
        step_count = transpiler_collect_mir_intent_steps(mir_routine, &mir_steps);
    if (step_count == 0) {
        step_nodes = node->data.intent_decl.steps;
        step_count = node->data.intent_decl.step_count;
    } else {
        step_nodes = mir_steps;
    }
    if (mir_only_intent && node->data.intent_decl.step_count > 0 && mir_routine == NULL) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "MIR-only C path missing routine for intent '%s'",
                node->data.intent_decl.name != NULL
                    ? node->data.intent_decl.name
                    : "(anonymous-intent)");
        }
        free(mir_steps);
        free((void *)participant_aliases);
        free((void *)participant_types);
        g_type_render_ctx = saved_render_ctx;
        ctx->out = saved_out;
        return;
    }
    if (mir_only_intent && node->data.intent_decl.step_count > 0 && step_count == 0) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "MIR-only C path missing intent step sequence for '%s'",
                node->data.intent_decl.name != NULL
                    ? node->data.intent_decl.name
                    : "(anonymous-intent)");
        }
        free(mir_steps);
        free((void *)participant_aliases);
        free((void *)participant_types);
        g_type_render_ctx = saved_render_ctx;
        ctx->out = saved_out;
        return;
    }
    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = step_nodes[i];
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
        const char *pt = NULL;
        const char *alias = involves != NULL && involves->data.intent_involves.alias != NULL
            ? involves->data.intent_involves.alias : "participant";
        const char *participant_type = participant_types != NULL && i < participant_count
            ? participant_types[i]
            : NULL;
        char *type_name = NULL;
        bool pointer_param = false;
        char surface_desc[256];
        if (i > 0)
            codebuf_write(ctx->out, ", ");
        snprintf(surface_desc, sizeof(surface_desc),
            "intent participant '%s' of '%s'",
            alias != NULL ? alias : "(anonymous)",
            node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
        if (participant_type != NULL) {
            pt = transpiler_require_type_name_c_type(ctx, participant_type, surface_desc);
            type_name = pergyra_strdup(participant_type);
            pointer_param = is_pointer_self_host_type_name(ctx, participant_type);
        } else if (involves != NULL && involves->data.intent_involves.subject_type != NULL) {
            pt = transpiler_require_ast_c_type(
                ctx, involves->data.intent_involves.subject_type, surface_desc);
            type_name = render_type_name(involves->data.intent_involves.subject_type);
            pointer_param = intent_involves_uses_pointer_self(ctx, involves);
        }
        if (pt == NULL)
            goto intent_emit_fail;
        if (participant_aliases != NULL && i < participant_count
            && participant_aliases[i] != NULL) {
            alias = participant_aliases[i];
        }
        codebuf_write(ctx->out, "%s%s%s", pt, pointer_param ? " *" : " ", alias);
        if (type_name != NULL) {
            register_typed_var(ctx, alias, type_name);
            TypedVarEntry *entry = lookup_typed_entry(ctx, alias);
            if (entry != NULL)
                entry->is_subject_ref = pointer_param;
            free(type_name);
        }
    }
    for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
        ASTNode *value = node->data.intent_decl.values[i];
        const char *pt = NULL;
        char *type_name = NULL;
        char surface_desc[256];
        if (node->data.intent_decl.involve_count > 0 || i > 0)
            codebuf_write(ctx->out, ", ");
        snprintf(surface_desc, sizeof(surface_desc),
            "intent value '%s' of '%s'",
            value != NULL && value->data.intent_value.alias != NULL
                ? value->data.intent_value.alias
                : "value",
            node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
        pt = transpiler_require_ast_c_type(
            ctx,
            value != NULL ? value->data.intent_value.value_type : NULL,
            surface_desc);
        if (pt == NULL)
            goto intent_emit_fail;
        if (value != NULL && value->data.intent_value.value_type != NULL) {
            type_name = render_type_name(value->data.intent_value.value_type);
        }
        codebuf_write(ctx->out, "%s %s", pt,
            value != NULL && value->data.intent_value.alias != NULL
                ? value->data.intent_value.alias : "value");
        if (type_name != NULL) {
            register_typed_var(ctx, value->data.intent_value.alias, type_name);
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
    if (has_compensate_steps && step_count > 0) {
        write_indent(ctx);
        codebuf_write(ctx->out, "bool __intent_step_completed[%zu] = { false };\n",
            step_count);
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
                    ? involves->data.intent_involves.alias : "participant";
                if (participant_aliases != NULL && i < participant_count
                    && participant_aliases[i] != NULL) {
                    alias = participant_aliases[i];
                }
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

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = step_nodes[i];
        const char *saved_zone_name = ctx->current_zone_name;
        const char *saved_overlay_receiver = ctx->current_overlay_receiver_expr;
        const char *step_name = NULL;
        ASTNode *pre_expr = NULL;
        ASTNode *guard_expr = NULL;
        ASTNode *post_expr = NULL;
        ASTNode *expect_expr = NULL;
        ASTNode *invariant_pre_expr = NULL;
        ASTNode *invariant_post_expr = NULL;
        ASTNode **on_exprs = NULL;
        size_t on_expr_count = 0;
        ASTNode *subintent_expr = NULL;
        const char *step_zone_name = NULL;
        const char *step_zone_alias = NULL;
        const char *step_from_alias = NULL;
        const char **who_aliases = NULL;
        size_t who_alias_count = 0;
        const char **dispatch_aliases = NULL;
        size_t dispatch_alias_count = 0;
        bool rebound_aliases = false;
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        step_name = step->data.intent_step.name;
        if (mir_routine != NULL) {
            pre_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "pre");
            guard_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "guard");
            post_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "post");
            expect_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "expect");
            invariant_pre_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "invariant-pre");
            invariant_post_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "invariant-post");
            on_expr_count = transpiler_collect_mir_intent_eval_exprs(
                mir_routine, step_name, "on", &on_exprs);
            subintent_expr = transpiler_find_mir_intent_eval_expr(mir_routine, step_name, "intent");
            step_zone_name = transpiler_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneWhere");
            step_zone_alias = transpiler_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneAlias");
            step_from_alias = transpiler_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneFrom");
            who_alias_count = transpiler_collect_mir_intent_who_aliases(mir_routine, step_name, &who_aliases);
            dispatch_alias_count = transpiler_collect_mir_intent_dispatch_aliases(mir_routine, step_name, &dispatch_aliases);
        }
        if (mir_only_intent) {
            if (step->data.intent_step.pre_expr != NULL && pre_expr == NULL)
                goto mir_step_missing_pre;
            if (step->data.intent_step.guard_expr != NULL && guard_expr == NULL)
                goto mir_step_missing_guard;
            if (step->data.intent_step.post_expr != NULL && post_expr == NULL)
                goto mir_step_missing_post;
            if (step->data.intent_step.expect_expr != NULL && expect_expr == NULL)
                goto mir_step_missing_expect;
            if (step->data.intent_step.invariant_expr != NULL
                && (invariant_pre_expr == NULL || invariant_post_expr == NULL))
                goto mir_step_missing_invariant;
            if (step->data.intent_step.intent_expr != NULL && subintent_expr == NULL)
                goto mir_step_missing_subintent;
            if (step->data.intent_step.on_expr_count > 0 && on_expr_count == 0)
                goto mir_step_missing_on;
            if (step->data.intent_step.where_type != NULL && step_zone_name == NULL)
                goto mir_step_missing_zone_where;
            if (intent_step_effective_zone_alias(step) != NULL && step_zone_alias == NULL)
                goto mir_step_missing_zone_alias;
            if (step->data.intent_step.transfer_from_alias != NULL && step_from_alias == NULL)
                goto mir_step_missing_zone_from;
            if (step->data.intent_step.who_count > 0 && who_alias_count == 0)
                goto mir_step_missing_who;
            if (step->data.intent_step.on_expr_count == 0
                && step->data.intent_step.intent_expr == NULL
                && step->data.intent_step.who_count > 0
                && dispatch_alias_count == 0) {
                goto mir_step_missing_dispatch;
            }
        } else {
            if (pre_expr == NULL)
                pre_expr = step->data.intent_step.pre_expr;
            if (guard_expr == NULL)
                guard_expr = step->data.intent_step.guard_expr;
            if (post_expr == NULL)
                post_expr = step->data.intent_step.post_expr;
            if (expect_expr == NULL)
                expect_expr = step->data.intent_step.expect_expr;
            if (invariant_pre_expr == NULL)
                invariant_pre_expr = step->data.intent_step.invariant_expr;
            if (invariant_post_expr == NULL)
                invariant_post_expr = step->data.intent_step.invariant_expr;
            if (on_expr_count == 0) {
                on_exprs = step->data.intent_step.on_exprs;
                on_expr_count = step->data.intent_step.on_expr_count;
            }
            if (subintent_expr == NULL)
                subintent_expr = step->data.intent_step.intent_expr;
            if (step_zone_name == NULL
                && step->data.intent_step.where_type != NULL
                && step->data.intent_step.where_type->type == AST_TYPE) {
                step_zone_name = step->data.intent_step.where_type->data.type.name;
            }
            if (step_zone_alias == NULL)
                step_zone_alias = intent_step_effective_zone_alias(step);
            if (step_from_alias == NULL)
                step_from_alias = step->data.intent_step.transfer_from_alias;
            if (who_alias_count == 0) {
                who_aliases = (const char **)step->data.intent_step.who_names;
                who_alias_count = step->data.intent_step.who_count;
            }
            if (dispatch_alias_count == 0) {
                dispatch_aliases = (const char **)step->data.intent_step.who_names;
                dispatch_alias_count = step->data.intent_step.who_count;
            }
        }
        if (step_zone_name != NULL)
            ctx->current_zone_name = step_zone_name;
        if (step_zone_alias != NULL)
            ctx->current_overlay_receiver_expr = step_zone_alias;

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
        for (size_t j = 0; j < who_alias_count; j++) {
            const char *alias = who_aliases[j];
            const char *slot_name = resolve_intent_zone_slot_name(ctx, node, step, alias);
            write_indent(ctx);
            codebuf_write(ctx->out, "pgy_intent_trace_bind_export(__intent_handle, \"%s\", \"%s\");\n",
                alias != NULL ? alias : "<participant>",
                slot_name != NULL ? slot_name : "<unbound>");
        }
        emit_intent_step_bind_bound_zone(ctx->out, ctx, node, step);
        rebound_aliases = emit_intent_step_rebind_bound_zone_aliases(ctx->out, ctx, node, step, i);

        if (pre_expr != NULL) {
            char *pre = emit_expression(pre_expr, ctx);
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

        if (invariant_pre_expr != NULL) {
            char *invariant = emit_expression(invariant_pre_expr, ctx);
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

        if (on_expr_count > 0) {
            for (size_t j = 0; j < on_expr_count; j++) {
                char *on_expr = emit_expression(on_exprs[j], ctx);
                if (on_expr != NULL && on_expr[0] != '\0') {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "%s;\n", on_expr);
                }
                free(on_expr);
            }
        }
        if (subintent_expr != NULL) {
            char *intent_expr = emit_expression(subintent_expr, ctx);
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
        } else if (on_expr_count == 0) {
            for (size_t j = 0; j < dispatch_alias_count; j++) {
                const char *alias = dispatch_aliases[j];
                ASTNode *involves = find_intent_participant_local(node, alias);
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
        ctx->current_zone_name = saved_zone_name;
        ctx->current_overlay_receiver_expr = saved_overlay_receiver;

        if (has_compensate_steps) {
            write_indent(ctx);
            codebuf_write(ctx->out, "__intent_step_completed[%zu] = true;\n", i);
        }

        if (guard_expr != NULL) {
            char *guard = emit_expression(guard_expr, ctx);
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

        if (expect_expr != NULL) {
            char *expect = emit_expression(expect_expr, ctx);
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

        if (post_expr != NULL) {
            char *post = emit_expression(post_expr, ctx);
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

        if (invariant_post_expr != NULL) {
            char *invariant = emit_expression(invariant_post_expr, ctx);
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
        free(on_exprs);
        free((void *)who_aliases);
        free((void *)dispatch_aliases);
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
                if (!transpiler_emit_mir_resource_hook(ctx, ctx->out, ctx->indent, inst, "__intent_handle", true))
                    goto intent_emit_fail;
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
                            if (!transpiler_emit_mir_resource_hook(ctx, ctx->out, ctx->indent, inst, "__intent_handle", true))
                                goto intent_emit_fail;
                    }
            }
            if (has_compensate_steps && step_count > 0) {
                for (size_t i = step_count; i-- > 0;) {
                    ASTNode *step = step_nodes[i];
                    ASTNode **compensate_exprs = NULL;
                    size_t compensate_expr_count = 0;
                    const char *zone_type_name = NULL;
                    const char *zone_alias = NULL;
                    const char *from_alias = NULL;
                    const char **who_aliases = NULL;
                    size_t who_alias_count = 0;
                    if (step == NULL || step->type != AST_INTENT_STEP
                        || step->data.intent_step.compensate_expr_count == 0)
                        continue;
                    if (mir_routine != NULL) {
                        compensate_expr_count = transpiler_collect_mir_intent_eval_exprs(
                            mir_routine, step->data.intent_step.name, "compensate", &compensate_exprs);
                        zone_type_name = transpiler_find_mir_intent_meta_arg(
                            mir_routine, step->data.intent_step.name, "IntentZoneWhere");
                        zone_alias = transpiler_find_mir_intent_meta_arg(
                            mir_routine, step->data.intent_step.name, "IntentZoneAlias");
                        from_alias = transpiler_find_mir_intent_meta_arg(
                            mir_routine, step->data.intent_step.name, "IntentZoneFrom");
                        who_alias_count = transpiler_collect_mir_intent_who_aliases(
                            mir_routine, step->data.intent_step.name, &who_aliases);
                    }
                    if (mir_only_intent && step->data.intent_step.compensate_expr_count > 0
                        && compensate_expr_count == 0)
                        goto mir_step_missing_compensate;
                    if (!mir_only_intent && compensate_expr_count == 0) {
                        compensate_expr_count = step->data.intent_step.compensate_expr_count;
                        compensate_exprs = step->data.intent_step.compensate_exprs;
                    }
                    if (mir_only_intent) {
                        if (step->data.intent_step.where_type != NULL && zone_type_name == NULL)
                            goto mir_step_missing_zone_where;
                        if (intent_step_effective_zone_alias(step) != NULL && zone_alias == NULL)
                            goto mir_step_missing_zone_alias;
                        if (step->data.intent_step.transfer_from_alias != NULL && from_alias == NULL)
                            goto mir_step_missing_zone_from;
                        if (step->data.intent_step.who_count > 0 && who_alias_count == 0)
                            goto mir_step_missing_who;
                    }
                    write_indent(ctx);
                    codebuf_write(ctx->out, "if (__intent_step_completed[%zu]) {\n", i);
                    ctx->indent++;
                    for (size_t j = compensate_expr_count; j-- > 0;) {
                        char *expr = emit_expression(compensate_exprs[j], ctx);
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
                    free(compensate_exprs);
                    free((void *)who_aliases);
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
                            if (!transpiler_emit_mir_resource_hook(ctx, ctx->out, ctx->indent, inst, "__intent_handle", true))
                                goto intent_emit_fail;
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
        if (has_compensate_steps && step_count > 0) {
            write_indent(ctx);
            codebuf_write(ctx->out, "if (__intent_failed) {\n");
            ctx->indent++;
            for (size_t i = step_count; i-- > 0;) {
                ASTNode *step = step_nodes[i];
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
    free(mir_steps);
    free((void *)participant_aliases);
    free((void *)participant_types);
    g_type_render_ctx = saved_render_ctx;
    ctx->out = saved_out;
    return;

mir_step_missing_pre:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent pre check carrier");
    goto intent_emit_fail;
mir_step_missing_guard:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent guard check carrier");
    goto intent_emit_fail;
mir_step_missing_post:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent post check carrier");
    goto intent_emit_fail;
mir_step_missing_expect:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent expect check carrier");
    goto intent_emit_fail;
mir_step_missing_invariant:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent invariant check carrier");
    goto intent_emit_fail;
mir_step_missing_subintent:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent subintent eval carrier");
    goto intent_emit_fail;
mir_step_missing_on:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent on-eval carrier");
    goto intent_emit_fail;
mir_step_missing_zone_where:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent zone where metadata");
    goto intent_emit_fail;
mir_step_missing_zone_alias:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent zone alias metadata");
    goto intent_emit_fail;
mir_step_missing_zone_from:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent transfer-from metadata");
    goto intent_emit_fail;
mir_step_missing_who:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent who metadata");
    goto intent_emit_fail;
mir_step_missing_dispatch:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent dispatch carrier");
    goto intent_emit_fail;
mir_step_missing_compensate:
    if (ctx->backend_error == NULL)
        ctx->backend_error = pergyra_strdup("MIR-only C path missing intent compensate eval carrier");
    goto intent_emit_fail;

intent_emit_fail:
    free(mir_steps);
    free((void *)participant_aliases);
    free((void *)participant_types);
    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
    g_type_render_ctx = saved_render_ctx;
    ctx->out = saved_out;
}

/* -----------------------------------------------------------------
 * Program emitter
 * ----------------------------------------------------------------- */

void
emit_program(const HIRProgram *hir, TranspilerCtx *ctx)
{
    const MIRProgram *mir = (ctx != NULL) ? ctx->mir : NULL;
    ASTNode **abilities = NULL;
    size_t ability_count = 0;
    ASTNode **types = NULL;
    size_t type_count = 0;
    ASTNode **externs = NULL;
    size_t extern_count = 0;
    ASTNode **functions = NULL;
    size_t function_count = 0;
    ASTNode **intents = NULL;
    size_t intent_count = 0;
    ASTNode **roles = NULL;
    size_t role_count = 0;
    ASTNode **parties = NULL;
    size_t party_count = 0;
    ASTNode **rosters = NULL;
    size_t roster_count = 0;
    ASTNode **relations = NULL;
    size_t relation_count = 0;
    ASTNode **effects = NULL;
    size_t effect_count = 0;
    ASTNode **zones = NULL;
    size_t zone_count = 0;
    ASTNode **worlds = NULL;
    size_t world_count = 0;
    ASTNode **events = NULL;
    size_t event_count = 0;
    ASTNode **executables = NULL;
    size_t executable_count = 0;
    ASTNode *synthetic_executable_func = NULL;
    bool has_main_function = false;
    bool has_top_level_exec = false;

    if (hir == NULL && mir == NULL)
        return;

    ctx->hir = (mir == NULL) ? hir : NULL;
    transpiler_active_externs(ctx, &externs, &extern_count);
    transpiler_active_inventory(ctx, AST_ABILITY_DECL, &abilities, &ability_count);
    transpiler_active_inventory(ctx, AST_CLASS_DECL, &types, &type_count);
    transpiler_active_inventory(ctx, AST_FUNC_DECL, &functions, &function_count);
    transpiler_active_inventory(ctx, AST_INTENT_DECL, &intents, &intent_count);
    transpiler_active_inventory(ctx, AST_ROLE_DECL, &roles, &role_count);
    transpiler_active_inventory(ctx, AST_PARTY_DECL, &parties, &party_count);
    transpiler_active_inventory(ctx, AST_ROSTER_DECL, &rosters, &roster_count);
    transpiler_active_inventory(ctx, AST_RELATION_DECL, &relations, &relation_count);
    transpiler_active_inventory(ctx, AST_EFFECT_DECL, &effects, &effect_count);
    transpiler_active_inventory(ctx, AST_ZONE_DECL, &zones, &zone_count);
    transpiler_active_inventory(ctx, AST_WORLD_DECL, &worlds, &world_count);
    transpiler_active_inventory(ctx, AST_EVENT_DECL, &events, &event_count);
    synthetic_executable_func = transpiler_active_synthetic_executable_func(ctx);
    has_main_function = transpiler_active_has_main_function(ctx);
    has_top_level_exec = transpiler_active_has_top_level_exec(ctx);
    if (ctx->hir != NULL)
        transpiler_active_executables(ctx, &executables, &executable_count);

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
    for (size_t i = 0; i < ability_count; i++)
        emit_ability_decl(abilities[i], ctx);

    /* Pass 1.5: enums + type aliases */
    for (size_t i = 0; i < type_count; i++) {
        if (types[i] != NULL
            && (types[i]->type == AST_ENUM_DECL
                || types[i]->type == AST_TYPE_ALIAS))
            emit_statement(types[i], ctx);
    }

    /* Pass 2: classes */
    for (size_t i = 0; i < type_count; i++) {
        if (types[i] != NULL && types[i]->type == AST_CLASS_DECL)
            emit_class_decl(types[i], ctx);
    }

    /* Pass 2.5: extern declarations */
    for (size_t i = 0; i < extern_count; i++)
        emit_extern_block(externs[i], ctx);

    /* Pass 2.6: early forward declarations for standalone functions so
     * class/domain hosted methods can call file-scope helpers declared later. */
    for (size_t i = 0; i < function_count; i++) {
        if (transpiler_can_forward_declare_func_early(ctx, functions[i]))
            emit_func_forward_decl(functions[i], ctx->out, ctx);
    }
    if (synthetic_executable_func != NULL
        && transpiler_can_forward_declare_func_early(ctx, synthetic_executable_func)) {
        emit_func_forward_decl(synthetic_executable_func, ctx->out, ctx);
    }
    for (size_t i = 0; i < intent_count; i++) {
        if (transpiler_can_forward_declare_intent_early(ctx, intents[i]))
            emit_intent_forward_decl(intents[i], ctx->out, ctx);
    }

    /* Pass 3: roles (vtable instances + free functions) */
    for (size_t i = 0; i < role_count; i++)
        emit_role_decl(roles[i], ctx);

    /* Pass 3.5: parties (struct + methods) */
    for (size_t i = 0; i < party_count; i++)
        emit_party_decl(parties[i], ctx);

    /* Pass 3.7: rosters (struct + methods) */
    for (size_t i = 0; i < roster_count; i++)
        emit_roster_decl(rosters[i], ctx);

    /* Pass 3.75: relations and effects (must precede zones that reference them) */
    for (size_t i = 0; i < relation_count; i++)
        emit_relation_decl(relations[i], ctx);
    for (size_t i = 0; i < effect_count; i++)
        emit_effect_decl(effects[i], ctx);

    /* Pass 3.8: zones (struct + methods + sync helpers) */
    for (size_t i = 0; i < zone_count; i++)
        emit_zone_decl(zones[i], ctx);

    /* Pass 3.85: now that zones are declared, emit intent prototypes that
     * depend on zone types before world methods are emitted. */
    for (size_t i = 0; i < intent_count; i++) {
        if (!transpiler_can_forward_declare_intent_early(ctx, intents[i]))
            emit_intent_forward_decl(intents[i], ctx->out, ctx);
    }
    for (size_t i = 0; i < function_count; i++) {
        if (!transpiler_can_forward_declare_func_early(ctx, functions[i])
            && transpiler_can_forward_declare_func_after_zones(ctx, functions[i])) {
            emit_func_forward_decl(functions[i], ctx->out, ctx);
        }
    }
    if (synthetic_executable_func != NULL
        && !transpiler_can_forward_declare_func_early(ctx, synthetic_executable_func)
        && transpiler_can_forward_declare_func_after_zones(ctx, synthetic_executable_func)) {
        emit_func_forward_decl(synthetic_executable_func, ctx->out, ctx);
    }

    /* Pass 3.9: worlds (struct + methods) */
    for (size_t i = 0; i < world_count; i++)
        emit_world_decl(worlds[i], ctx);

    for (size_t i = 0; i < event_count; i++)
        emit_event_decl(events[i], ctx);

    for (size_t i = 0; i < function_count; i++)
        emit_func_forward_decl(functions[i], ctx->decls, ctx);
    if (synthetic_executable_func != NULL)
        emit_func_forward_decl(synthetic_executable_func, ctx->decls, ctx);
    for (size_t i = 0; i < intent_count; i++)
        emit_intent_forward_decl(intents[i], ctx->decls, ctx);

    /* Pass 4: functions — emit in two sub-passes so that helpers
     * (parallel context structs, wrapper functions) generated during
     * function emission are available.  First pass: emit all functions
     * into a temporary buffer. */
    {
        CodeBuf *func_buf = codebuf_create();
        CodeBuf *saved_out = ctx->out;
        ctx->out = func_buf;
        for (size_t i = 0; i < function_count; i++)
            emit_func_decl(functions[i], ctx);
        if (synthetic_executable_func != NULL)
            emit_func_decl(synthetic_executable_func, ctx);
        for (size_t i = 0; i < intent_count; i++)
            emit_intent_decl(intents[i], func_buf, ctx);
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
    if (has_top_level_exec || has_main_function) {
        bool needs_thread_pool = transpiler_requires_thread_pool(ctx);
        codebuf_write(ctx->out, "\nint\nmain(void)\n{\n");
        ctx->indent++;

        /* Initialize runtime */
        if (needs_thread_pool) {
            write_indent(ctx);
            codebuf_write(ctx->out, "pgy_pool_init(0);\n\n");
        }

        /* Emit top-level statements inside main() */
        if (synthetic_executable_func != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "__pgy_top_level_exec();\n");
        } else if (ctx->hir != NULL) {
            for (size_t i = 0; i < executable_count; i++)
                emit_statement(executables[i], ctx);
        }

        /* If Main() exists and no top-level statements, call it */
        if (has_main_function) {
            write_indent(ctx);
            codebuf_write(ctx->out, "Main();\n");
        }

        /* Shutdown runtime */
        if (needs_thread_pool) {
            write_indent(ctx);
            codebuf_write(ctx->out, "pgy_pool_shutdown();\n");
        }

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
transpile_from_mir(const MIRProgram *mir, const char *output_path)
{
    return transpile_with_mir(NULL, mir, output_path);
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
    emit_program(mir != NULL ? NULL : hir, ctx);

    if (ctx->backend_error != NULL) {
        result->success = false;
        result->error_message = pergyra_strdup(ctx->backend_error);
        transpiler_ctx_destroy(ctx);
        return result;
    }

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
    result->uses_intent_observability = ctx->uses_intent_observability;
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
    const char *return_type = NULL;

    if (node->data.lambda_expr.return_type != NULL) {
        return_type = pergyra_ast_type_to_c(node->data.lambda_expr.return_type);
    } else if (node->data.lambda_expr.body != NULL
               && node->data.lambda_expr.body->type == AST_BLOCK) {
        return_type = "void";
    } else if (node->data.lambda_expr.body != NULL) {
        const char *inferred_return_type =
            infer_expression_type_name(ctx, node->data.lambda_expr.body);
        if (inferred_return_type != NULL)
            return_type = pergyra_type_to_c(inferred_return_type);
    }
    if (return_type == NULL) {
        transpiler_set_backend_error(
            ctx,
            "cannot determine lambda return type; explicit return type is required for non-block lambda bodies");
        return pergyra_strdup("0");
    }

    char *lambda_name = strdup_fmt("pgy_lambda_%d", lambda_id);

    codebuf_write(ctx->decls, "\nstatic %s %s(",
                  return_type, lambda_name);
    for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
        ASTNode *param = node->data.lambda_expr.params[i];
        const char *param_name = NULL;
        const char *param_type = NULL;
        if (i > 0)
            codebuf_write(ctx->decls, ", ");
        if (param->type == AST_LET_DECL) {
            param_name = param->data.let_decl.name;
            if (param->data.let_decl.type != NULL)
                param_type = pergyra_ast_type_to_c(param->data.let_decl.type);
        } else {
            param_name = param->data.identifier.name;
        }
        if (param_type == NULL) {
            transpiler_set_backend_error(
                ctx,
                "cannot determine lambda parameter type for '%s' at argument %zu",
                lambda_name,
                i);
            free(lambda_name);
            return pergyra_strdup("0");
        }
        codebuf_write(ctx->decls, "%s %s", param_type, param_name);
    }
    codebuf_write(ctx->decls, ");\n");

    codebuf_write(ctx->helpers, "\nstatic %s %s(",
                  return_type, lambda_name);
    for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
        ASTNode *param = node->data.lambda_expr.params[i];
        const char *param_name = NULL;
        const char *param_type = NULL;
        if (i > 0)
            codebuf_write(ctx->helpers, ", ");
        if (param->type == AST_LET_DECL) {
            param_name = param->data.let_decl.name;
            if (param->data.let_decl.type != NULL)
                param_type = pergyra_ast_type_to_c(param->data.let_decl.type);
        } else {
            param_name = param->data.identifier.name;
        }
        if (param_type == NULL) {
            transpiler_set_backend_error(
                ctx,
                "cannot determine lambda parameter type for '%s' at argument %zu",
                lambda_name,
                i);
            free(lambda_name);
            return pergyra_strdup("0");
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
        transpiler_set_backend_error(
            ctx,
            "cannot resolve included role '%s' while emitting include statement",
            included_role != NULL ? included_role : "<role>");
    }
}

void
emit_impl_ability(ASTNode *node, TranspilerCtx *ctx)
{
    const char *ability_name =
        (node->data.impl_ability.ability_ref != NULL
         && node->data.impl_ability.ability_ref->type == AST_TYPE)
        ? node->data.impl_ability.ability_ref->data.type.name : NULL;
    
    codebuf_write(ctx->out, "/* Impl ability: %s */\n", ability_name);
    
    /* This is handled within emit_role_decl */
    (void)ctx;
}

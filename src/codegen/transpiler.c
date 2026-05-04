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
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_enum.h"
#include "transpiler_extern.h"
#include "transpiler_log_normalize.h"
#include "transpiler_nominal.h"
#include "transpiler_operator.h"
#include "transpiler_program.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"
#include "transpiler_thread_pool.h"
#include "transpiler_type_alias.h"
#include "transpiler_type_declarator.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"
#include "../common/string_compat.h"
#include "../semantic/builtin_kind.h"
#include "../semantic/diag_codes.h"

/* Forward declarations for emitters defined later */
void emit_ability_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_role_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_party_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_roster_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_relation_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_effect_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_zone_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_world_decl(ASTNode *node, TranspilerCtx *ctx);
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
static const MIRRoutine *transpiler_find_role_impl_mir_method(
    const TranspilerCtx *ctx,
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
static const char *transpiler_let_slot_inner_from_call_type_arg(ASTNode *call);
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
static bool transpiler_validate_mir_emission_contract(const TranspilerCtx *ctx,
                                                     const MIRRoutine *routine,
                                                     const ASTNode *decl,
                                                     bool require_cleanup,
                                                     bool require_cleanup_blocks,
                                                     char *reason,
                                                     size_t reason_cap);
static bool transpiler_has_mapping_for_all_emitted_blocks(const TranspilerCtx *ctx,
                                                        const MIRRoutine *routine,
                                                        const ASTNode *func_decl,
                                                        bool require_non_cleanup,
                                                        char *reason,
                                                        size_t reason_cap);
static bool class_has_generic_params(ASTNode *node);
static const char *ensure_generic_class_specialization(
    TranspilerCtx *ctx, ASTNode *class_decl, ASTNode *ann);

#include "transpiler_helpers.h"
#include "transpiler_defer_emit.h"
#include "transpiler_base_a_emitters.h"
#include "transpiler_base_b_emitters.h"
#include "transpiler_intent_emit.h"

/* -----------------------------------------------------------------
 * Program emitter
 * ----------------------------------------------------------------- */

void
emit_program(TranspilerCtx *ctx)
{
    const MIRProgram *mir = (ctx != NULL) ? ctx->mir : NULL;
    ASTNode *synthetic_executable_func = NULL;
    ASTNode **abilities = NULL;
    ASTNode **types = NULL;
    ASTNode **externs = NULL;
    ASTNode **functions = NULL;
    ASTNode **intents = NULL;
    ASTNode **roles = NULL;
    ASTNode **parties = NULL;
    ASTNode **rosters = NULL;
    ASTNode **relations = NULL;
    ASTNode **effects = NULL;
    ASTNode **zones = NULL;
    ASTNode **worlds = NULL;
    ASTNode **events = NULL;
    size_t ability_count = 0;
    size_t type_count = 0;
    size_t exten_count = 0;
    size_t function_count = 0;
    size_t intent_count = 0;
    size_t role_count = 0;
    size_t party_count = 0;
    size_t roster_count = 0;
    size_t relation_count = 0;
    size_t effect_count = 0;
    size_t zone_count = 0;
    size_t world_count = 0;
    size_t event_count = 0;
    bool has_main_function = false;
    bool has_top_level_exec = false;

    if (mir == NULL)
        return;

    synthetic_executable_func = transpiler_active_synthetic_executable_func(ctx);
    has_main_function = transpiler_active_has_main_function(ctx);
    has_top_level_exec = transpiler_active_has_top_level_exec(ctx);
    transpiler_active_inventory(ctx, AST_ABILITY_DECL, &abilities, &ability_count);
    transpiler_active_inventory(ctx, AST_CLASS_DECL, &types, &type_count);
    transpiler_active_externs(ctx, &externs, &exten_count);
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

    /* File header */
    codebuf_write(ctx->out,
        "/*\n"
        " * Generated by the Pergyra compiler C backend\n"
        " * Do not edit manually.\n"
        " */\n"
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n");
    codebuf_write(ctx->out,
        "#ifndef PGY_INTENT_OBSERVABILITY_ENABLED\n"
        "#define PGY_INTENT_OBSERVABILITY_ENABLED %d\n"
        "#endif\n",
        ctx->uses_intent_observability ? 1 : 0);
    codebuf_write(ctx->out,
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
        ASTNode *type_decl = types[i];
        if (type_decl != NULL
            && (type_decl->type == AST_ENUM_DECL
                || type_decl->type == AST_TYPE_ALIAS))
            emit_statement(type_decl, ctx);
    }

    /* Pass 2: classes */
    for (size_t i = 0; i < type_count; i++) {
        ASTNode *type_decl = types[i];
        if (type_decl != NULL && type_decl->type == AST_CLASS_DECL)
            emit_class_decl(type_decl, ctx);
    }

    /* Pass 2.5: extern declarations */
    for (size_t i = 0; i < exten_count; i++)
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

#include "transpiler_domain_role_emit.h"

static const char *
transpiler_infer_lambda_param_c_type(ASTNode *lambda_node, ASTNode *param_node)
{
    const char *param_name = NULL;
    ASTNode *body = NULL;
    ASTNode *ret_value = NULL;

    if (lambda_node == NULL || param_node == NULL)
        return NULL;
    if (param_node->type == AST_IDENTIFIER)
        param_name = param_node->data.identifier.name;
    else if (param_node->type == AST_LET_DECL)
        param_name = param_node->data.let_decl.name;
    if (param_name == NULL || lambda_node->data.lambda_expr.return_type == NULL)
        return NULL;

    body = lambda_node->data.lambda_expr.body;
    if (body == NULL)
        return NULL;
    if (body->type == AST_IDENTIFIER) {
        ret_value = body;
    } else if (body->type == AST_BLOCK
               && body->data.block.count == 1
               && body->data.block.statements[0] != NULL
               && body->data.block.statements[0]->type == AST_RETURN) {
        ret_value = body->data.block.statements[0]->data.return_stmt.value;
    }

    if (ret_value != NULL
        && ret_value->type == AST_IDENTIFIER
        && ret_value->data.identifier.name != NULL
        && strcmp(ret_value->data.identifier.name, param_name) == 0) {
        return pergyra_ast_type_to_c(lambda_node->data.lambda_expr.return_type);
    }
    return NULL;
}

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
    int saved_typed_var_count = ctx->typed_var_count;

    /* Expose lambda parameters to typed-var lookup so body-side type
     * inference (infer_expression_type_name) can resolve them. Scope is
     * unwound via saved_typed_var_count at end-of-function. */
    for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
        ASTNode *param = node->data.lambda_expr.params[i];
        const char *param_name = NULL;
        char *param_type_name = NULL;

        if (param == NULL)
            continue;
        if (param->type == AST_LET_DECL) {
            param_name = param->data.let_decl.name;
            if (param->data.let_decl.type != NULL)
                param_type_name = render_type_name(param->data.let_decl.type);
        } else if (param->type == AST_IDENTIFIER) {
            param_name = param->data.identifier.name;
        }
        if (param_name != NULL && param_type_name != NULL)
            register_typed_var(ctx, param_name, param_type_name);
        free(param_type_name);
    }

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
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine lambda return type; explicit return type is required for non-block lambda bodies");
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
            param_type = transpiler_infer_lambda_param_c_type(node, param);
        }
        if (param_type == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine lambda parameter type for '%s' at argument %llu",
                lambda_name,
                (unsigned long long) i);
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
            param_type = transpiler_infer_lambda_param_c_type(node, param);
        }
        if (param_type == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine lambda parameter type for '%s' at argument %llu",
                lambda_name,
                (unsigned long long) i);
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
    ctx->typed_var_count = saved_typed_var_count;
    return lambda_name;
}

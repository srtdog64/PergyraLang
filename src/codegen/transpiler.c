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
#include "transpiler_mir_role_lookup.h"
#include "transpiler_mir_ssa_entry.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_ssa_utils.h"
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
static const char *transpiler_find_local_type_name(TranspilerCtx *ctx,
                                                   const ASTNode *func_decl,
                                                   const char *base_name);
static const char *transpiler_infer_local_type_name_from_expr(TranspilerCtx *ctx,
                                                              const ASTNode *func_decl,
                                                              ASTNode *expr);
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

static void
emit_c_nominal_forward_typedef(CodeBuf *out, const char *name)
{
    if (out == NULL || name == NULL || name[0] == '\0')
        return;
    codebuf_write(out, "typedef struct %s %s;\n", name, name);
}

static void
emit_c_nominal_forward_decls(TranspilerCtx *ctx,
                             ASTNode **types,
                             size_t type_count,
                             ASTNode **parties,
                             size_t party_count,
                             ASTNode **rosters,
                             size_t roster_count,
                             ASTNode **relations,
                             size_t relation_count,
                             ASTNode **effects,
                             size_t effect_count,
                             ASTNode **zones,
                             size_t zone_count,
                             ASTNode **worlds,
                             size_t world_count)
{
    CodeBuf *out = ctx != NULL ? ctx->out : NULL;

    if (out == NULL)
        return;
    for (size_t i = 0; i < type_count; i++) {
        ASTNode *type_decl = types[i];
        if (type_decl != NULL && type_decl->type == AST_CLASS_DECL)
            emit_c_nominal_forward_typedef(out, ast_class_name(type_decl));
    }
    for (size_t i = 0; i < party_count; i++)
        emit_c_nominal_forward_typedef(out, ast_party_name(parties[i]));
    for (size_t i = 0; i < roster_count; i++)
        emit_c_nominal_forward_typedef(out, ast_roster_name(rosters[i]));
    for (size_t i = 0; i < relation_count; i++)
        emit_c_nominal_forward_typedef(out, ast_relation_name(relations[i]));
    for (size_t i = 0; i < effect_count; i++)
        emit_c_nominal_forward_typedef(out, ast_effect_name(effects[i]));
    for (size_t i = 0; i < zone_count; i++)
        emit_c_nominal_forward_typedef(out, ast_zone_name(zones[i]));
    for (size_t i = 0; i < world_count; i++)
        emit_c_nominal_forward_typedef(out, ast_world_name(worlds[i]));
    codebuf_write(out, "\n");
}

/* -----------------------------------------------------------------
 * Program emitter
 * ----------------------------------------------------------------- */

void
emit_program(TranspilerCtx *ctx)
{
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

    if (!transpiler_active_has_mir(ctx))
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
     *   Pass 1 ??ability declarations (vtable typedefs)
     *   Pass 2 ??class declarations (type completeness)
     *   Pass 3 ??role declarations (vtable instances + methods)
     *   Pass 4 ??function declarations (file scope)
     *   Pass 5 ??remaining top-level statements ??wrapped in main()
     */

    /* Pass 0.5: nominal forward typedefs used by ability signatures. */
    emit_c_nominal_forward_decls(ctx,
                                 types,
                                 type_count,
                                 parties,
                                 party_count,
                                 rosters,
                                 roster_count,
                                 relations,
                                 relation_count,
                                 effects,
                                 effect_count,
                                 zones,
                                 zone_count,
                                 worlds,
                                 world_count);

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

    /* Pass 4: functions ??emit in two sub-passes so that helpers
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

void
transpiler_emit_zone_hosted_methods_bridge(
    const char *name,
    const TranspilerHostedMethodView *method_view,
    TranspilerCtx *ctx)
{
    transpiler_emit_zone_hosted_methods(name, method_view, ctx);
}

void
emit_event_invoke(ASTNode *node, TranspilerCtx *ctx)
{
    /* Event invoke is handled in emit_call for function-style invocation */
    (void)node;
    (void)ctx;
}

#include "transpiler_lambda_emit.h"

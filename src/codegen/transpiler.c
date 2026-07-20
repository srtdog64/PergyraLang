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
#include <inttypes.h>

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_enum.h"
#include "transpiler_extern.h"
#include "transpiler_func_forward_helpers.h"
#include "transpiler_inventory_view.h"
#include "transpiler_log_normalize.h"
#include "transpiler_mir_signature.h"
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
#include "transpiler_mir_ssa_entry.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_ssa_utils.h"
#include "../compiler/mir_decl_headers.h"
#include "transpiler_relation_effect_emit.h"
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
#include "transpiler_helpers.h"
#include "transpiler_defer_emit.h"
#include "../compiler/verified_projection_plan.h"
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

static bool
emit_c_nominal_forward_decls_for_kind(
    TranspilerCtx *ctx,
    const MIRDeclHeaderInventory *inventory,
    ASTNodeType decl_type)
{
    CodeBuf *out = ctx != NULL ? ctx->out : NULL;

    if (out == NULL || inventory == NULL)
        return false;
    for (size_t i = 0; i < inventory->count; i++) {
        const MIRDeclHeader *header =
            mir_decl_header_inventory_get(inventory, i);
        ASTNodeType header_type = mir_decl_header_ast_type_or(
            header, AST_PROGRAM);
        const char *name;

        if (header == NULL || header_type != decl_type)
            continue;
        name = mir_decl_header_name(header);
        if (name == NULL || name[0] == '\0') {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR nominal declaration header is missing its name for type %d",
                (int)decl_type);
            return false;
        }
        emit_c_nominal_forward_typedef(out, name);
    }
    return true;
}

static bool
emit_c_nominal_forward_decls(TranspilerCtx *ctx)
{
    MIRDeclHeaderInventory inventory;

    if (ctx == NULL || ctx->out == NULL)
        return false;
    transpiler_active_decl_header_inventory(ctx, &inventory);
    /* Keep the historical category order, but source every name and kind
     * from the MIR declaration-header owner. No AST declaration payload is
     * needed for this pass. */
    if (!emit_c_nominal_forward_decls_for_kind(
            ctx, &inventory, AST_CLASS_DECL)
        || !emit_c_nominal_forward_decls_for_kind(
            ctx, &inventory, AST_PARTY_DECL)
        || !emit_c_nominal_forward_decls_for_kind(
            ctx, &inventory, AST_ROSTER_DECL)
        || !emit_c_nominal_forward_decls_for_kind(
            ctx, &inventory, AST_RELATION_DECL)
        || !emit_c_nominal_forward_decls_for_kind(
            ctx, &inventory, AST_EFFECT_DECL)
        || !emit_c_nominal_forward_decls_for_kind(
            ctx, &inventory, AST_ZONE_DECL)
        || !emit_c_nominal_forward_decls_for_kind(
            ctx, &inventory, AST_WORLD_DECL)) {
        return false;
    }
    codebuf_write(ctx->out, "\n");
    return true;
}

static const char *
transpiler_c_executable_emitted_name(const char *name)
{
    if (name != NULL && strcmp(name, "main") == 0)
        return "__pgy_user_main_lowercase";
    return name;
}

static bool
transpiler_is_synthetic_executable_func(ASTNode *fn)
{
    const char *name;

    if (fn == NULL || fn->type != AST_FUNC_DECL)
        return false;
    name = ast_declaration_name(fn);
    return name != NULL && strcmp(name, "__pgy_top_level_exec") == 0;
}

/* -----------------------------------------------------------------
 * Program emitter
 * ----------------------------------------------------------------- */

static void
transpiler_emit_machine_layer_runtime_bind(TranspilerCtx *ctx)
{
    const PgyVerifiedProjectionPlanRow *plan;

    if (!transpiler_machine_layer_projection_is_bound(ctx))
        return;
    plan = ctx->projection_plan;
    write_indent(ctx);
    codebuf_write(ctx->out,
        "if (!pgy_machine_layer_runtime_bind_mapping_export(UINT64_C(%" PRIu64
        "), UINT64_C(%" PRIu64 "), UINT64_C(%" PRIu64
        "), UINT64_C(%" PRIu64 "), UINT32_C(%" PRIu32
        "), UINT32_C(%" PRIu32 "))) {\n",
        plan->machine_layer_manifest_fingerprint,
        plan->machine_layer_physical_manifest_fingerprint,
        plan->machine_layer_physical_grant_base,
        plan->machine_layer_physical_grant_size,
        plan->machine_layer_physical_grant_mode,
        plan->machine_layer_runtime_provider_required ? 1u : 0u);
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out,
        "fputs(\"machine-layer runtime bind rejected\\n\", stderr);\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "return 1;\n");
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

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
    const char *main_function_name = NULL;

    if (!transpiler_active_has_mir(ctx))
        return;

    has_main_function = transpiler_active_has_main_function(ctx);
    main_function_name = transpiler_active_main_function_name(ctx);
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
    for (size_t i = 0; i < function_count; i++) {
        if (transpiler_is_synthetic_executable_func(functions[i])) {
            synthetic_executable_func = functions[i];
            break;
        }
    }
    if (has_top_level_exec && synthetic_executable_func == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing synthetic top-level executable function '__pgy_top_level_exec'");
        return;
    }
    if (has_main_function
        && (main_function_name == NULL
            || transpiler_active_decl_header_of_type(
                   ctx, AST_FUNC_DECL, main_function_name) == NULL)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing registered executable function '%s'",
            main_function_name != NULL ? main_function_name : "Main");
        return;
    }

    /* File header */
    codebuf_write(ctx->out,
        "/*\n"
        " * Generated by the Pergyra compiler C backend\n"
        " * Do not edit manually.\n"
        " *\n"
        " * Build contract (the pgy driver passes these automatically):\n"
        " *   -fwrapv               signed Int/Long arithmetic is defined as\n"
        " *                         two's-complement wrap, matching the LLVM leg\n"
        " *   -fno-strict-aliasing  runtime containers rely on byte-level views\n"
        " * Recompiling this file without those flags reopens undefined\n"
        " * behavior that the Pergyra toolchain has closed.\n"
        " */\n"
        "#if !defined(__GNUC__) && !defined(__clang__)\n"
        "#error \"Pergyra emitted C requires a compiler honoring -fwrapv and -fno-strict-aliasing (GCC or Clang)\"\n"
        "#endif\n"
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n"
        "#include <stdatomic.h>\n"
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
        "#include \"pgy_lane_scheduler.h\"\n"
        "#include \"pgy_channel.h\"\n"
        "#ifndef PGY_EVENT_MAX_HANDLERS\n"
        "#define PGY_EVENT_MAX_HANDLERS 16\n"
        "#endif\n\n");

    /*
     * Multi-pass strategy for valid C output:
     *   Pass 1: ability declarations (vtable typedefs)
     *   Pass 2: class declarations (type completeness)
     *   Pass 3: role declarations (vtable instances + methods)
     *   Pass 4: function declarations (file scope)
     *   Pass 5: remaining top-level statements wrapped in main()
     */

    /* Pass 0.5: nominal forward typedefs used by ability signatures. */
    if (!emit_c_nominal_forward_decls(ctx))
        return;

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
    if (transpiler_active_has_mir(ctx)) {
        MIRDeclHeaderInventory header_inventory;
        transpiler_active_decl_header_inventory(ctx, &header_inventory);
        for (size_t i = 0; i < header_inventory.count; i++) {
            const MIRDeclHeader *header =
                mir_decl_header_inventory_get(&header_inventory, i);
            if (header != NULL
                && mir_decl_header_ast_type_or(header, AST_PROGRAM)
                    == AST_CLASS_DECL) {
                emit_class_decl_from_mir_header(header, ctx);
            }
        }
    } else {
        for (size_t i = 0; i < type_count; i++) {
            ASTNode *type_decl = types[i];
            if (type_decl != NULL && type_decl->type == AST_CLASS_DECL)
                emit_class_decl(type_decl, ctx);
        }
    }

    /* Pass 2.5: extern declarations */
    for (size_t i = 0; i < exten_count; i++)
        emit_extern_block(externs[i], ctx);

    /* Pass 2.6: early forward declarations for standalone functions so
     * class/domain hosted methods can call file-scope helpers declared later. */
    for (size_t i = 0; i < function_count; i++) {
        if (transpiler_is_synthetic_executable_func(functions[i]))
            continue;
        if (transpiler_can_forward_declare_func_early(ctx, functions[i])) {
            emit_func_forward_decl_named(
                functions[i],
                transpiler_c_executable_emitted_name(
                    ast_declaration_name(functions[i])),
                ctx->out,
                ctx);
        }
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

    /* Pass 3.75: relations and effects (must precede zones that reference
     * them). Active MIR selects the declaration header directly; the AST
     * inventory remains only for the legacy non-MIR path. */
    if (transpiler_active_has_mir(ctx)) {
        MIRDeclHeaderInventory header_inventory;
        transpiler_active_decl_header_inventory(ctx, &header_inventory);
        for (size_t i = 0; i < header_inventory.count; i++) {
            const MIRDeclHeader *header =
                mir_decl_header_inventory_get(&header_inventory, i);
            if (header == NULL)
                continue;
            if (mir_decl_header_ast_type_or(header, AST_PROGRAM)
                    == AST_RELATION_DECL)
                emit_relation_decl_from_mir_header(header, ctx);
        }
        for (size_t i = 0; i < header_inventory.count; i++) {
            const MIRDeclHeader *header =
                mir_decl_header_inventory_get(&header_inventory, i);
            if (header == NULL)
                continue;
            if (mir_decl_header_ast_type_or(header, AST_PROGRAM)
                    == AST_EFFECT_DECL)
                emit_effect_decl_from_mir_header(header, ctx);
        }
    } else {
        for (size_t i = 0; i < relation_count; i++)
            emit_relation_decl(relations[i], ctx);
        for (size_t i = 0; i < effect_count; i++)
            emit_effect_decl(effects[i], ctx);
    }

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
        if (transpiler_is_synthetic_executable_func(functions[i]))
            continue;
        if (!transpiler_can_forward_declare_func_early(ctx, functions[i])
            && transpiler_can_forward_declare_func_after_zones(ctx, functions[i])) {
            emit_func_forward_decl_named(
                functions[i],
                transpiler_c_executable_emitted_name(
                    ast_declaration_name(functions[i])),
                ctx->out,
                ctx);
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

    for (size_t i = 0; i < function_count; i++) {
        if (!transpiler_mir_or_ast_function_is_generic(
                transpiler_find_mir_function(ctx, functions[i]), functions[i]))
            emit_func_forward_decl_named(
                functions[i],
                transpiler_c_executable_emitted_name(
                    ast_declaration_name(functions[i])),
                ctx->decls,
                ctx);
    }
    if (synthetic_executable_func != NULL)
        emit_func_forward_decl(synthetic_executable_func, ctx->decls, ctx);
    for (size_t i = 0; i < intent_count; i++)
        emit_intent_forward_decl(intents[i], ctx->decls, ctx);

    /* Pass 4: functions. Emit in two sub-passes so that helpers
     * (parallel context structs, wrapper functions) generated during
     * function emission are available.  First pass: emit all functions
     * into a temporary buffer. */
    {
        CodeBuf *func_buf = codebuf_create();
        CodeBuf *saved_out = ctx->out;
        ctx->out = func_buf;
        for (size_t i = 0; i < function_count; i++) {
            if (transpiler_is_synthetic_executable_func(functions[i]))
                continue;
            if (!transpiler_mir_or_ast_function_is_generic(
                    transpiler_find_mir_function(ctx, functions[i]),
                    functions[i]))
                emit_func_decl_named(
                    functions[i],
                    transpiler_c_executable_emitted_name(
                        ast_declaration_name(functions[i])),
                    ctx->out,
                    ctx);
        }
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

        /* Parallel/async wrappers precede the helpers: helper-resident
         * functions (generic specializations) reference the wrappers by
         * name, and the wrappers' own callees are already covered by the
         * forward declarations above. */
        if (ctx->wrappers->len > 0) {
            codebuf_write(ctx->out, "\n");
            codebuf_write_raw(ctx->out, ctx->wrappers->data,
                              ctx->wrappers->len);
        }

        /* Emit helpers (generic specializations and other late defs) */
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
    /* Generate int main(int argc, char **argv) { ... } */
    if (has_top_level_exec || has_main_function) {
        bool needs_thread_pool = transpiler_requires_thread_pool(ctx);
        codebuf_write(ctx->out, "\nint\nmain(int argc, char **argv)\n{\n");
        ctx->indent++;

        /* Arm the resource-budget wall-clock deadline first, so the host's
         * PGY_BUDGET_WALL_MS bounds the whole run (no-op unless set). */
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_budget_wall_arm_export();\n");

        /* Initialize runtime */
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_args_init(argc, argv);\n");
        transpiler_emit_machine_layer_runtime_bind(ctx);
        if (needs_thread_pool) {
            write_indent(ctx);
            codebuf_write(ctx->out, "pgy_pool_init(0);\n");
        }
        codebuf_write(ctx->out, "\n");

        /* Emit top-level statements inside main() */
        if (synthetic_executable_func != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "__pgy_top_level_exec();\n");
        }

        /* If Main() exists and no top-level statements, call it */
        if (has_main_function) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s();\n",
                transpiler_c_executable_emitted_name(main_function_name));
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
emit_event_invoke(ASTNode *node, TranspilerCtx *ctx)
{
    /* Event invoke is handled in emit_call for function-style invocation */
    (void)node;
    (void)ctx;
}

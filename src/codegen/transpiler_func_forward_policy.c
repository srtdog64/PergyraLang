/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend function-forward declaration policy.
 */

#include <stdlib.h>
#include <string.h>

#include "codegen_type_mapping.h"
#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_func_forward_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_mir_signature.h"
#include "../parser/ast_api.h"

static int
transpiler_forward_allowed_type_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const char *allowed = *(const char * const *)entry;

    return strcmp(name, allowed);
}

static bool
transpiler_forward_type_name_is_allowed(const char *name)
{
    static const char *allowed_names[] = {
        "Array",
        "Bool",
        "Box",
        "Byte",
        "Channel",
        "Char",
        "DeviceSlot",
        "Float",
        "Future",
        "Int",
        "Option",
        "Qubit",
        "Rc",
        "RemoteFuture",
        "Result",
        "SecureSlot",
        "Slice",
        "Slot",
        "String",
        "Void",
        "Weak",
    };
    const char **match;

    if (name == NULL)
        return false;
    match = (const char **)bsearch(&name,
        allowed_names,
        sizeof(allowed_names) / sizeof(allowed_names[0]),
        sizeof(allowed_names[0]),
        transpiler_forward_allowed_type_compare);
    return match != NULL;
}

bool
transpiler_can_forward_declare_type_name_early(TranspilerCtx *ctx,
                                               const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return true;
    /* A user enum may take a builtin type's spelling (`enum Result`); its
     * typedef is emitted with the user enums, after the early prototypes,
     * so the builtin early-forward row does not cover it. */
    if (transpiler_forward_type_name_is_allowed(type_name))
        return !transpiler_decl_exists_local(ctx, AST_ENUM_DECL, type_name);
    return transpiler_decl_exists_local(ctx, AST_CLASS_DECL, type_name);
}

bool
transpiler_can_forward_declare_type_early(TranspilerCtx *ctx,
                                          ASTNode *type_node)
{
    const char *name;

    if (ctx == NULL || type_node == NULL)
        return true;
    if (type_node->type != AST_TYPE || ast_type_name(type_node) == NULL)
        return true;

    name = ast_type_name(type_node);
    return transpiler_can_forward_declare_type_name_early(ctx, name);
}

static bool
transpiler_can_forward_declare_type_after_zones(TranspilerCtx *ctx,
                                                ASTNode *type_node)
{
    const char *name = NULL;

    if (transpiler_can_forward_declare_type_early(ctx, type_node))
        return true;
    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE
        || ast_type_name(type_node) == NULL)
        return false;

    name = ast_type_name(type_node);
    if (transpiler_active_has_mir(ctx)) {
        if (transpiler_active_decl_header_of_type(
                ctx, AST_WORLD_DECL, name) != NULL) {
            return false;
        }
    } else if (transpiler_find_named_decl_local(
                   ctx, AST_WORLD_DECL, name) != NULL) {
        return false;
    }
    return transpiler_has_known_nominal_type(ctx, name);
}

static TranspilerFuncForwardStage
transpiler_func_forward_stage_later(TranspilerFuncForwardStage a,
                                    TranspilerFuncForwardStage b)
{
    if (a == TRANSPILER_FUNC_FORWARD_STAGE_NONE
        || b == TRANSPILER_FUNC_FORWARD_STAGE_NONE)
        return TRANSPILER_FUNC_FORWARD_STAGE_NONE;
    return a > b ? a : b;
}

/* Runtime container families whose C specialization the forward emitter can
 * publish in the prototype's own stream (ensure_type_specializations_*). Any
 * other constructed name keeps the late file-scope prototype. */
static bool
transpiler_forward_type_name_container_arity(const char *type_name,
                                             size_t *arity_out)
{
    static const struct {
        const char *base;
        size_t arity;
    } families[] = {
        { "Array", 1 }, { "HashMap", 2 }, { "List", 1 },
        { "Option", 1 }, { "Queue", 1 }, { "Result", 2 },
    };

    for (size_t i = 0; i < sizeof(families) / sizeof(families[0]); i++) {
        size_t len = strlen(families[i].base);
        if (strncmp(type_name, families[i].base, len) == 0
            && type_name[len] == '<') {
            *arity_out = families[i].arity;
            return true;
        }
    }
    return false;
}

/* The first program stage where `type_name` can be spelled in a prototype.
 * `element` marks a type that a container specialization stores by value, so
 * its layout must be complete rather than only forward-typedef'd (pass 0.5
 * typedefs every class and domain nominal; layouts follow the type pass,
 * the zone pass, and the world pass). */
static TranspilerFuncForwardStage
transpiler_forward_type_name_stage(TranspilerCtx *ctx,
                                   const char *type_name,
                                   bool element,
                                   unsigned depth)
{
    TranspilerFuncForwardStage stage =
        TRANSPILER_FUNC_FORWARD_STAGE_NOMINAL_LAYOUTS;
    size_t arity = 0;

    if (type_name == NULL || depth > 8)
        return TRANSPILER_FUNC_FORWARD_STAGE_NONE;
    /* A user `enum Result` / `enum Option` takes the enum row below. */
    if (transpiler_forward_type_name_is_allowed(type_name)
        && !transpiler_decl_exists_local(ctx, AST_ENUM_DECL, type_name))
        return TRANSPILER_FUNC_FORWARD_STAGE_EARLY;
    if (transpiler_forward_type_name_container_arity(type_name, &arity)) {
        for (size_t i = 0; i < arity; i++) {
            char arg[256];
            copy_constructed_arg_name_at(type_name, (int)i, arg, sizeof(arg));
            stage = transpiler_func_forward_stage_later(stage,
                transpiler_forward_type_name_stage(ctx, arg, true, depth + 1));
        }
        return stage;
    }
    if (transpiler_decl_exists_local(ctx, AST_CLASS_DECL, type_name))
        return element ? TRANSPILER_FUNC_FORWARD_STAGE_NOMINAL_LAYOUTS
                       : TRANSPILER_FUNC_FORWARD_STAGE_EARLY;
    if (transpiler_decl_exists_local(ctx, AST_ENUM_DECL, type_name))
        return TRANSPILER_FUNC_FORWARD_STAGE_NOMINAL_LAYOUTS;
    if (transpiler_decl_exists_local(ctx, AST_WORLD_DECL, type_name))
        return element ? TRANSPILER_FUNC_FORWARD_STAGE_HOSTED_BODIES
                       : TRANSPILER_FUNC_FORWARD_STAGE_NOMINAL_LAYOUTS;
    if (transpiler_decl_exists_local(ctx, AST_ROLE_DECL, type_name))
        return TRANSPILER_FUNC_FORWARD_STAGE_AFTER_ZONES;
    if (transpiler_has_known_nominal_type(ctx, type_name))
        return element ? TRANSPILER_FUNC_FORWARD_STAGE_AFTER_ZONES
                       : TRANSPILER_FUNC_FORWARD_STAGE_NOMINAL_LAYOUTS;
    return TRANSPILER_FUNC_FORWARD_STAGE_NONE;
}

/* Signatures without a MIR type name keep the historical AST admission. */
static TranspilerFuncForwardStage
transpiler_forward_ast_type_stage(TranspilerCtx *ctx, ASTNode *type_node)
{
    if (transpiler_can_forward_declare_type_early(ctx, type_node))
        return TRANSPILER_FUNC_FORWARD_STAGE_EARLY;
    if (transpiler_can_forward_declare_type_after_zones(ctx, type_node))
        return TRANSPILER_FUNC_FORWARD_STAGE_AFTER_ZONES;
    return TRANSPILER_FUNC_FORWARD_STAGE_NONE;
}

TranspilerFuncForwardStage
transpiler_func_forward_stage(TranspilerCtx *ctx, ASTNode *func)
{
    const MIRRoutine *routine;
    TranspilerFuncForwardStage stage = TRANSPILER_FUNC_FORWARD_STAGE_EARLY;

    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return TRANSPILER_FUNC_FORWARD_STAGE_NONE;

    routine = transpiler_find_mir_function(ctx, func);
    if (transpiler_active_has_mir(ctx) && routine == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing function forward routine for '%s'",
            ast_declaration_name(func) != NULL
                ? ast_declaration_name(func)
                : "(anonymous)");
        return TRANSPILER_FUNC_FORWARD_STAGE_NONE;
    }
    if (transpiler_active_has_mir(ctx)) {
        if (!transpiler_mir_routine_signature_supported_strict(ctx, routine))
            return TRANSPILER_FUNC_FORWARD_STAGE_NONE;
    } else if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
                   routine,
                   func,
                   TRANSPILER_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES,
                   "MIR-only C path missing function forward signature metadata for '%s'",
                   "MIR-only C path missing function forward return type-name metadata for '%s'",
                   "MIR-only C path missing function forward parameter type-name metadata for '%s'")) {
        return TRANSPILER_FUNC_FORWARD_STAGE_NONE;
    }
    if (transpiler_mir_or_ast_function_is_generic(routine, func)
        || routine == NULL)
        return TRANSPILER_FUNC_FORWARD_STAGE_NONE;

    if (transpiler_mir_routine_return_type_name(routine) != NULL) {
        stage = transpiler_func_forward_stage_later(stage,
            transpiler_forward_type_name_stage(ctx,
                transpiler_mir_routine_return_type_name(routine), false, 0));
    } else {
        stage = transpiler_func_forward_stage_later(stage,
            transpiler_forward_ast_type_stage(ctx,
                transpiler_mir_routine_return_type(routine)));
    }

    size_t param_count = transpiler_mir_routine_param_count(routine);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *p = transpiler_mir_routine_param(routine, i);
        const char *param_type_name =
            transpiler_mir_routine_param_type_name(routine, i);
        if (p == NULL)
            continue;
        if (param_type_name != NULL) {
            stage = transpiler_func_forward_stage_later(stage,
                transpiler_forward_type_name_stage(ctx, param_type_name,
                    false, 0));
        } else if (p->type != NULL) {
            stage = transpiler_func_forward_stage_later(stage,
                transpiler_forward_ast_type_stage(ctx, p->type));
        }
    }
    return stage;
}

TranspilerFuncForwardStage *
transpiler_func_forward_stages_create(TranspilerCtx *ctx,
                                      ASTNode **functions,
                                      size_t function_count)
{
    TranspilerFuncForwardStage *stages;

    if (function_count == 0)
        return NULL;
    stages = calloc(function_count, sizeof(*stages));
    if (stages == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "C function forward stage schedule could not be allocated");
        return NULL;
    }
    for (size_t i = 0; i < function_count; i++)
        stages[i] = transpiler_func_forward_stage(ctx, functions[i]);
    return stages;
}

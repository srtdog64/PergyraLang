/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker control-flow and ownership analysis
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "type_checker_internal.h"

typedef enum
{
    FLOW_NONE        = 0,
    FLOW_FALLTHROUGH = 1 << 0,
    FLOW_BREAK       = 1 << 1,
    FLOW_CONTINUE    = 1 << 2,
    FLOW_RETURN      = 1 << 3
} FlowFlags;

#include "type_checker_flow_resources.inc"
#include "type_checker_flow_effects.inc"

typedef struct
{
    ResourceConsumeSnapshot break_states;
    ResourceConsumeSnapshot continue_states;
    bool                 has_break_states;
    bool                 has_continue_states;
    Scope               *loop_scope;
} LoopFlowState;

static FlowFlags type_check_statement_flow(ASTNode *node,
                                           SemanticContext *ctx,
                                           LoopFlowState *loop_flow);
static FlowFlags type_check_block_flow(ASTNode *node,
                                       SemanticContext *ctx,
                                       LoopFlowState *loop_flow);
static FlowFlags type_check_if_stmt_flow(ASTNode *node,
                                         SemanticContext *ctx,
                                         LoopFlowState *loop_flow);
static FlowFlags type_check_match_stmt_flow(ASTNode *node,
                                            SemanticContext *ctx,
                                            LoopFlowState *loop_flow);
static FlowFlags type_check_with_stmt_flow(ASTNode *node,
                                           SemanticContext *ctx,
                                           LoopFlowState *loop_flow);

static bool
match_pattern_is_named_variant(ASTNode *pat, const char **name_out,
                               ASTNode ***args_out, size_t *arg_count_out)
{
    *name_out = NULL;
    *args_out = NULL;
    *arg_count_out = 0;

    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        *name_out = pat->data.identifier.name;
        return *name_out != NULL;
    }

    if (pat->type == AST_MEMBER_ACCESS
        && pat->data.member.object != NULL
        && pat->data.member.object->type == AST_IDENTIFIER
        && pat->data.member.name != NULL) {
        *name_out = pat->data.member.name;
        return true;
    }

    if (pat->type != AST_CALL || pat->data.call.callee == NULL) {
        return false;
    }

    if (pat->data.call.callee->type == AST_IDENTIFIER
        && pat->data.call.callee->data.identifier.name != NULL) {
        *name_out = pat->data.call.callee->data.identifier.name;
    } else if (pat->data.call.callee->type == AST_MEMBER_ACCESS
        && pat->data.call.callee->data.member.object != NULL
        && pat->data.call.callee->data.member.object->type == AST_IDENTIFIER
        && pat->data.call.callee->data.member.name != NULL) {
        *name_out = pat->data.call.callee->data.member.name;
    } else {
        return false;
    }

    *args_out = pat->data.call.arguments;
    *arg_count_out = pat->data.call.arg_count;
    return true;
}

static ASTNode *
find_enum_decl_for_type(SemanticContext *ctx, const Type *type)
{
    ASTNode *prog;

    if (ctx == NULL || type == NULL || type->kind != TYPE_KIND_ENUM
        || type->name == NULL || ctx->program_root == NULL)
        return NULL;

    prog = ctx->program_root;
    if (prog->type != AST_PROGRAM)
        return NULL;

    for (size_t i = 0; i < prog->data.program.count; i++) {
        ASTNode *stmt = prog->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ENUM_DECL
            || stmt->data.enum_decl.name == NULL)
            continue;
        if (strcmp(stmt->data.enum_decl.name, type->name) == 0)
            return stmt;
    }

    return NULL;
}

static bool
declare_match_binding(SemanticContext *ctx, ASTNode *binding_node, Type *binding_type)
{
    const char *name;
    Symbol *binding;

    if (ctx == NULL || binding_node == NULL || binding_type == NULL)
        return false;
    if (binding_node->type != AST_IDENTIFIER
        || binding_node->data.identifier.name == NULL) {
        semantic_error(ctx, binding_node,
            "Destructuring pattern currently requires identifier bindings");
        return false;
    }

    name = binding_node->data.identifier.name;
    if (scope_lookup_current(ctx->scope, name) != NULL) {
        semantic_error(ctx, binding_node,
            "Duplicate match binding '%s' in the same case scope", name);
        return false;
    }

    binding = symbol_create_variable(name, binding_type,
        binding_node->line, binding_node->column);
    if (binding == NULL || !scope_declare(ctx->scope, binding)) {
        semantic_error(ctx, binding_node,
            "Failed to declare match binding '%s'", name);
        return false;
    }

    return true;
}

static bool
type_check_special_match_pattern(ASTNode *pat, Type *subj_type,
                                 SemanticContext *ctx, bool *handled)
{
    const char *variant = NULL;
    ASTNode **args = NULL;
    size_t arg_count = 0;

    *handled = false;

    if (!match_pattern_is_named_variant(pat, &variant, &args, &arg_count)
        || variant == NULL || subj_type == NULL) {
        return true;
    }

    if (type_is_constructed_named(subj_type, "Option")) {
        Type *inner = type_get_constructed_arg(subj_type, 0);
        *handled = true;

        if (strcmp(variant, "Some") == 0) {
            if (arg_count != 1) {
                semantic_error_with_hints(ctx, "PGY_SEM_MATCH_PATTERN_INVALID", "semantic:match:pattern_shape", "align-pattern-arity-or-kind", pat,
                    "Some pattern requires exactly one binding");
                return false;
            }
            return declare_match_binding(ctx, args[0], inner);
        }
        if (strcmp(variant, "None") == 0) {
            if (arg_count != 0) {
                semantic_error_with_hints(ctx, "PGY_SEM_MATCH_PATTERN_INVALID", "semantic:match:pattern_shape", "align-pattern-arity-or-kind", pat,
                    "None pattern does not take payload bindings");
                return false;
            }
            return true;
        }

        semantic_error_with_hints(ctx, "PGY_SEM_MATCH_PATTERN_INVALID", "semantic:match:pattern_shape", "align-pattern-arity-or-kind", pat,
            "Option<T> match only supports Some(...) and None patterns");
        return false;
    }

    if (type_is_constructed_named(subj_type, "Result")) {
        *handled = true;

        if (strcmp(variant, "Ok") == 0) {
            if (arg_count != 1) {
                semantic_error(ctx, pat,
                    "Ok pattern requires exactly one binding");
                return false;
            }
            return declare_match_binding(ctx, args[0],
                type_get_constructed_arg(subj_type, 0));
        }
        if (strcmp(variant, "Err") == 0) {
            if (arg_count != 1) {
                semantic_error(ctx, pat,
                    "Err pattern requires exactly one binding");
                return false;
            }
            return declare_match_binding(ctx, args[0], TYPE_STRING);
        }

        semantic_error(ctx, pat,
            "Result<T> match only supports Ok(...) and Err(...) patterns");
        return false;
    }

    if (subj_type->kind == TYPE_KIND_ENUM) {
        ASTNode *enum_decl = find_enum_decl_for_type(ctx, subj_type);
        Symbol *variant_sym;

        *handled = true;
        if (enum_decl == NULL)
            return true;

        for (size_t i = 0; i < enum_decl->data.enum_decl.variant_count; i++) {
            const char *enum_variant = enum_decl->data.enum_decl.variants[i];
            size_t param_count = enum_decl->data.enum_decl.variant_param_counts != NULL
                ? enum_decl->data.enum_decl.variant_param_counts[i] : 0;

            if (enum_variant == NULL || strcmp(enum_variant, variant) != 0)
                continue;

            if (arg_count != param_count) {
                semantic_error_with_hints(ctx, "PGY_SEM_MATCH_PATTERN_INVALID", "semantic:match:pattern_shape", "align-pattern-arity-or-kind", pat,
                    "Enum variant '%s' expects %zu payload bindings, got %zu",
                    variant, param_count, arg_count);
                return false;
            }

            if (param_count == 0)
                return true;

            variant_sym = scope_lookup(ctx->scope, variant);
            if (variant_sym == NULL || variant_sym->type == NULL
                || variant_sym->type->kind != TYPE_KIND_FUNCTION) {
                semantic_error_with_hints(ctx, "PGY_SEM_MATCH_PATTERN_INVALID", "semantic:match:pattern_shape", "align-pattern-arity-or-kind", pat,
                    "Enum variant '%s' is missing constructor type information",
                    variant);
                return false;
            }

            for (size_t p = 0; p < param_count; p++) {
                Type *binding_type =
                    (p < variant_sym->type->data.function.param_count)
                    ? variant_sym->type->data.function.param_types[p]
                    : TYPE_UNKNOWN;
                if (!declare_match_binding(ctx, args[p], binding_type))
                    return false;
            }
            return true;
        }

        semantic_error(ctx, pat,
            "Unknown enum variant '%s' for match subject '%s'",
            variant, subj_type->name != NULL ? subj_type->name : "<enum>");
        return false;
    }

    return true;
}

static bool
match_case_has_or_patterns(ASTNode *mc)
{
    return mc != NULL && mc->type == AST_MATCH_CASE
        && mc->data.match_case.patterns != NULL
        && mc->data.match_case.pattern_count > 1;
}

static bool
match_case_uses_named_variant_pattern(ASTNode *mc)
{
    size_t count;
    ASTNode **patterns;

    if (!match_case_has_or_patterns(mc))
        return false;

    patterns = mc->data.match_case.patterns;
    count = mc->data.match_case.pattern_count;
    for (size_t i = 0; i < count; i++) {
        const char *name = NULL;
        ASTNode **args = NULL;
        size_t arg_count = 0;
        if (match_pattern_is_named_variant(patterns[i], &name, &args, &arg_count))
            return true;
    }
    return false;
}

static bool
type_check_match_case_patterns(ASTNode *mc, Type *subj_type,
                               SemanticContext *ctx)
{
    size_t count = 1;
    ASTNode **patterns = &mc->data.match_case.pattern;

    if (mc == NULL || mc->type != AST_MATCH_CASE)
        return true;

    if (mc->data.match_case.patterns != NULL
        && mc->data.match_case.pattern_count > 0) {
        patterns = mc->data.match_case.patterns;
        count = mc->data.match_case.pattern_count;
    }

    if (match_case_uses_named_variant_pattern(mc)) {
        /* OR patterns with named variants (e.g. case North | South) are now
         * allowed for simple enum variants (no payload destructuring). */
        for (size_t i = 0; i < count; i++) {
            const char *name = NULL;
            ASTNode **args = NULL;
            size_t arg_count = 0;
            if (match_pattern_is_named_variant(patterns[i], &name, &args, &arg_count)) {
                /* Allow simple variant names without payload bindings */
                if (arg_count > 0) {
                    semantic_error(ctx, patterns[i],
                        "OR patterns with variant destructuring (e.g. case .Some(x) | .None) "
                        "are not yet supported; split into separate cases");
                    return false;
                }
            }
        }
        /* Continue to pattern checking below — allow simple variant names */
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode *pat = patterns[i];
        bool handled = false;
        if (pat == NULL)
            continue;
        if (!type_check_special_match_pattern(pat, subj_type, ctx, &handled))
            continue;
        if (!handled) {
            Type *pat_type = type_check_expression(pat, ctx);
            if (!type_is_assignable(pat_type, subj_type) &&
                !type_is_assignable(subj_type, pat_type)) {
                semantic_error(ctx, pat,
                    "Case pattern type '%s' incompatible with match subject '%s'",
                    pat_type->name, subj_type->name);
            }
        }
    }
    return true;
}

static bool
pattern_covers_variant(ASTNode *pat, const Type *subj_type,
                       const char *variant_name)
{
    const char *name = NULL;
    ASTNode **args = NULL;
    size_t arg_count = 0;

    if (pat == NULL || subj_type == NULL || variant_name == NULL)
        return false;
    if (!match_pattern_is_named_variant(pat, &name, &args, &arg_count)
        || name == NULL || strcmp(name, variant_name) != 0) {
        return false;
    }

    if (type_is_constructed_named(subj_type, "Option")) {
        if (strcmp(variant_name, "Some") == 0)
            return arg_count == 1;
        if (strcmp(variant_name, "None") == 0)
            return arg_count == 0;
        return false;
    }

    if (type_is_constructed_named(subj_type, "Result")) {
        if (strcmp(variant_name, "Ok") == 0 || strcmp(variant_name, "Err") == 0)
            return arg_count == 1;
        return false;
    }

    return subj_type->kind == TYPE_KIND_ENUM;
}

static bool
case_list_covers_variant(ASTNode *node, const Type *subj_type,
                         const char *variant_name)
{
    if (node == NULL || subj_type == NULL || variant_name == NULL)
        return false;

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        if (mc == NULL || mc->type != AST_MATCH_CASE)
            continue;
        if (mc->data.match_case.guard != NULL)
            continue;

        /* Check single pattern */
        if (!match_case_has_or_patterns(mc)
            && pattern_covers_variant(mc->data.match_case.pattern, subj_type, variant_name))
            return true;

        /* Check OR patterns: any of the patterns covering the variant is enough */
        if (match_case_has_or_patterns(mc)) {
            size_t count = mc->data.match_case.pattern_count;
            ASTNode **patterns = mc->data.match_case.patterns;
            for (size_t j = 0; j < count; j++) {
                if (pattern_covers_variant(patterns[j], subj_type, variant_name))
                    return true;
            }
        }
    }

    return false;
}

static void
append_missing_variant(char *buf, size_t buf_size, const char *variant_name, bool *first)
{
    if (buf == NULL || buf_size == 0 || variant_name == NULL || first == NULL)
        return;

    if (!*first)
        strncat(buf, ", ", buf_size - strlen(buf) - 1);
    strncat(buf, variant_name, buf_size - strlen(buf) - 1);
    *first = false;
}

static size_t
collect_match_variant_space(const Type *subj_type, SemanticContext *ctx,
                            const char ***variants_out)
{
    static const char *option_variants[] = { "Some", "None" };
    static const char *result_variants[] = { "Ok", "Err" };

    *variants_out = NULL;

    if (subj_type == NULL)
        return 0;
    if (type_is_constructed_named(subj_type, "Option")) {
        *variants_out = option_variants;
        return 2;
    }
    if (type_is_constructed_named(subj_type, "Result")) {
        *variants_out = result_variants;
        return 2;
    }
    if (subj_type->kind == TYPE_KIND_ENUM) {
        ASTNode *enum_decl = find_enum_decl_for_type(ctx, subj_type);
        if (enum_decl == NULL)
            return 0;
        *variants_out = (const char **)enum_decl->data.enum_decl.variants;
        return enum_decl->data.enum_decl.variant_count;
    }

    return 0;
}

static void
check_match_redundancy(ASTNode *node, Type *subj_type, SemanticContext *ctx)
{
    const char **variants = NULL;
    size_t variant_count = collect_match_variant_space(subj_type, ctx, &variants);
    bool *seen;
    size_t covered = 0;

    if (node == NULL || subj_type == NULL || ctx == NULL
        || variants == NULL || variant_count == 0)
        return;

    seen = calloc(variant_count, sizeof(bool));
    if (seen == NULL)
        return;

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        if (mc == NULL || mc->type != AST_MATCH_CASE
            || mc->data.match_case.guard != NULL) {
            continue;
        }

        for (size_t v = 0; v < variant_count; v++) {
            const char *variant = variants[v];
            if (variant == NULL)
                continue;
            if (match_case_has_or_patterns(mc)
                || !pattern_covers_variant(mc->data.match_case.pattern, subj_type, variant))
                continue;

            if (seen[v]) {
                semantic_warning(ctx, mc->data.match_case.pattern,
                    "Redundant match case for '%s'; an earlier case already covers it",
                    variant);
            } else {
                seen[v] = true;
                covered++;
            }
            break;
        }
    }

    /* Defensive default after full variant coverage is allowed silently.
     * Users may write default as a safety net against future enum expansion. */
    (void)covered;
    (void)variant_count;

    free(seen);
}

static void
check_match_exhaustiveness(ASTNode *node, Type *subj_type, SemanticContext *ctx)
{
    char missing[256] = {0};
    bool first = true;
    bool found_missing = false;
    const char **variants = NULL;
    size_t variant_count = collect_match_variant_space(subj_type, ctx, &variants);

    if (node == NULL || subj_type == NULL || ctx == NULL)
        return;
    if (node->data.match_stmt.default_body != NULL)
        return;

    for (size_t i = 0; i < variant_count; i++) {
        const char *variant = variants[i];
        if (variant == NULL)
            continue;
        if (!case_list_covers_variant(node, subj_type, variant)) {
            append_missing_variant(missing, sizeof(missing), variant, &first);
            found_missing = true;
        }
    }

    if (found_missing) {
        semantic_error_with_hints(ctx, "PGY_SEM_MATCH_PATTERN_INVALID", "semantic:match:pattern_shape", "align-pattern-arity-or-kind", node,
            "Non-exhaustive match for '%s'; missing cases: %s",
            subj_type->name != NULL ? subj_type->name : "<unknown>",
            missing);
    }
}

#include "type_checker_flow_loops.inc"

static FlowFlags
type_check_block_flow(ASTNode *node, SemanticContext *ctx,
                      LoopFlowState *loop_flow)
{
    if (node == NULL)
        return FLOW_FALLTHROUGH;

    if (node->type != AST_BLOCK)
        return type_check_statement_flow(node, ctx, loop_flow);

    FlowFlags flags = FLOW_FALLTHROUGH;
    for (size_t i = 0; i < node->data.block.count; i++) {
        if ((flags & FLOW_FALLTHROUGH) == 0)
            break;

        FlowFlags stmt_flags =
            type_check_statement_flow(node->data.block.statements[i], ctx, loop_flow);

        flags &= ~FLOW_FALLTHROUGH;
        flags |= (stmt_flags & (FLOW_FALLTHROUGH
                              | FLOW_BREAK
                              | FLOW_CONTINUE
                              | FLOW_RETURN));
    }

    return flags;
}

static FlowFlags
type_check_if_stmt_flow(ASTNode *node, SemanticContext *ctx,
                        LoopFlowState *loop_flow)
{
    Type *cond = type_check_expression(node->data.if_stmt.condition, ctx);
    uint32_t effect_base = ctx->current_function_effects;
    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    FlowFlags flags = FLOW_NONE;
    FlowFlags then_flags = FLOW_NONE;
    uint32_t then_effect_delta = EFFECT_NONE;
    uint32_t else_effect_delta = EFFECT_NONE;

    if (!type_equals(cond, TYPE_BOOL)) {
        semantic_error(ctx, node,
            "If condition must be Bool, got '%s'", cond->name);
    }

    restore_resource_states(&base);
    ctx->current_function_effects = effect_base;
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    then_flags = type_check_block_flow(node->data.if_stmt.then_branch, ctx, loop_flow);
    scope_exit(&ctx->scope);
    then_effect_delta = effect_delta_from_baseline(effect_base,
        ctx->current_function_effects);
    flags |= (then_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
    if (then_flags & FLOW_FALLTHROUGH) {
        ResourceConsumeSnapshot then_snap = snapshot_resource_states(ctx);
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &then_snap);
        destroy_resource_snapshot(&then_snap);
        flags |= FLOW_FALLTHROUGH;
    }

    if (node->data.if_stmt.else_branch != NULL) {
        FlowFlags else_flags = FLOW_NONE;
        restore_resource_states(&base);
        ctx->current_function_effects = effect_base;
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        else_flags =
            type_check_statement_flow(node->data.if_stmt.else_branch, ctx, loop_flow);
        scope_exit(&ctx->scope);
        else_effect_delta = effect_delta_from_baseline(effect_base,
            ctx->current_function_effects);
        flags |= (else_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (else_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot else_snap = snapshot_resource_states(ctx);
            merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &else_snap);
            destroy_resource_snapshot(&else_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else {
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
        else_effect_delta = EFFECT_NONE;
    }

    flow_record_branch_effect_conflict_labeled(ctx, node,
        then_effect_delta, "then branch",
        else_effect_delta,
        node->data.if_stmt.else_branch != NULL ? "else branch" : "implicit fallthrough path");
    ctx->current_function_effects = type_effect_mask_join(
        effect_base,
        type_effect_mask_join(then_effect_delta, else_effect_delta));

    if (has_fallthrough)
        restore_resource_states(&fallthrough);
    else
        restore_resource_states(&base);

    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&fallthrough);
    return flags;
}

static FlowFlags
type_check_match_stmt_flow(ASTNode *node, SemanticContext *ctx,
                           LoopFlowState *loop_flow)
{
    Type *subj_type = type_check_expression(node->data.match_stmt.subject, ctx);
    uint32_t effect_base = ctx->current_function_effects;
    uint32_t merged_effect_delta = EFFECT_NONE;
    uint32_t previous_case_delta = EFFECT_NONE;
    bool have_previous_case_delta = false;
    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    FlowFlags flags = FLOW_NONE;

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        uint32_t case_effect_delta = EFFECT_NONE;
        restore_resource_states(&base);
        ctx->current_function_effects = effect_base;
        scope_enter(&ctx->scope, SCOPE_BLOCK);

        if (mc->data.match_case.pattern != NULL) {
            type_check_match_case_patterns(mc, subj_type, ctx);
        }

        if (mc->data.match_case.guard != NULL) {
            Type *guard_type = type_check_expression(mc->data.match_case.guard, ctx);
            if (!type_equals(guard_type, TYPE_BOOL)) {
                semantic_error(ctx, mc->data.match_case.guard,
                    "Case guard must be Bool, got '%s'", guard_type->name);
            }
        }

        FlowFlags case_flags =
            type_check_block_flow(mc->data.match_case.body, ctx, loop_flow);
        scope_exit(&ctx->scope);
        case_effect_delta = effect_delta_from_baseline(effect_base,
            ctx->current_function_effects);
        if (merged_effect_delta != EFFECT_NONE)
            flow_record_branch_effect_conflict_labeled(ctx, mc,
                merged_effect_delta, "merged prior cases",
                case_effect_delta, "current case");
        else if (have_previous_case_delta)
            flow_record_branch_effect_conflict_labeled(ctx, mc,
                previous_case_delta, "previous case",
                case_effect_delta, "current case");
        merged_effect_delta =
            type_effect_mask_join(merged_effect_delta, case_effect_delta);
        previous_case_delta = case_effect_delta;
        have_previous_case_delta = true;
        flags |= (case_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (case_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot case_snap = snapshot_resource_states(ctx);
            merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &case_snap);
            destroy_resource_snapshot(&case_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    }

    if (node->data.match_stmt.default_body != NULL) {
        FlowFlags default_flags = FLOW_NONE;
        uint32_t default_effect_delta = EFFECT_NONE;
        restore_resource_states(&base);
        ctx->current_function_effects = effect_base;
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        default_flags =
            type_check_block_flow(node->data.match_stmt.default_body, ctx, loop_flow);
        scope_exit(&ctx->scope);
        default_effect_delta = effect_delta_from_baseline(effect_base,
            ctx->current_function_effects);
        if (merged_effect_delta != EFFECT_NONE)
            flow_record_branch_effect_conflict_labeled(ctx, node,
                merged_effect_delta, "merged explicit cases",
                default_effect_delta, "default case");
        else if (have_previous_case_delta)
            flow_record_branch_effect_conflict_labeled(ctx, node,
                previous_case_delta, "previous case",
                default_effect_delta, "default case");
        merged_effect_delta =
            type_effect_mask_join(merged_effect_delta, default_effect_delta);
        flags |= (default_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (default_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot default_snap = snapshot_resource_states(ctx);
            merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &default_snap);
            destroy_resource_snapshot(&default_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else {
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
    }

    check_match_redundancy(node, subj_type, ctx);
    check_match_exhaustiveness(node, subj_type, ctx);
    ctx->current_function_effects =
        type_effect_mask_join(effect_base, merged_effect_delta);

    if (has_fallthrough)
        restore_resource_states(&fallthrough);
    else
        restore_resource_states(&base);

    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&fallthrough);
    return flags;
}

static FlowFlags
type_check_with_stmt_flow(ASTNode *node, SemanticContext *ctx,
                          LoopFlowState *loop_flow)
{
    scope_enter(&ctx->scope, SCOPE_WITH);

    ASTNode *slot_type_node = node->data.with_stmt.slot_type;
    const char *alias = node->data.with_stmt.alias;
    bool is_secure = node->data.with_stmt.is_secure;

    Type *inner = resolve_type_node(slot_type_node, ctx);
    Type *slot_type = type_create_slot(inner, is_secure);

    Symbol *sym = symbol_create_slot(alias, slot_type, is_secure, NULL,
                                     node->line, node->column);
    scope_declare(ctx->scope, sym);
    scope_register_slot(ctx->scope, sym);

    FlowFlags flags = type_check_block_flow(node->data.with_stmt.body, ctx, loop_flow);

    scope_auto_release_slots(ctx->scope);
    scope_exit(&ctx->scope);
    return flags;
}

static FlowFlags
type_check_statement_flow(ASTNode *node, SemanticContext *ctx,
                          LoopFlowState *loop_flow)
{
    if (node == NULL)
        return FLOW_FALLTHROUGH;

    switch (node->type) {
    case AST_BLOCK:
        return type_check_block_flow(node, ctx, loop_flow);
    case AST_IF_STMT:
        return type_check_if_stmt_flow(node, ctx, loop_flow);
    case AST_MATCH_STMT:
        return type_check_match_stmt_flow(node, ctx, loop_flow);
    case AST_WITH_STMT:
        return type_check_with_stmt_flow(node, ctx, loop_flow);
    case AST_UNSAFE_BLOCK:
        if (node->data.unsafe_block.body != NULL)
            return type_check_block_flow(node->data.unsafe_block.body, ctx, loop_flow);
        return FLOW_FALLTHROUGH;
    case AST_DEFER_STMT:
        /* Type-check deferred body, but save/restore slot states.
         * Defer bodies run at scope exit, so their Release() calls
         * should not affect the current scope's slot tracking. */
        if (node->data.defer_stmt.body != NULL) {
            /* Collect all slot symbols and their current states */
            typedef struct { Symbol* sym; SlotState saved_state; } SlotStateSave;
            SlotStateSave saves[256];
            size_t save_count = 0;
            Scope *cur = ctx->scope;
            while (cur != NULL) {
                for (size_t i = 0; i < cur->symbol_count && save_count < 256; i++) {
                    Symbol *s = cur->symbols[i];
                    if (s != NULL && s->kind == SYMBOL_SLOT) {
                        saves[save_count].sym = s;
                        saves[save_count].saved_state = s->slot_info.state;
                        save_count++;
                    }
                }
                cur = cur->parent;
            }
            /* Check the defer body */
            FlowFlags body_flags = type_check_block_flow(node->data.defer_stmt.body, ctx, loop_flow);
            /* Restore slot states */
            for (size_t i = 0; i < save_count; i++) {
                saves[i].sym->slot_info.state = saves[i].saved_state;
            }
            return body_flags;
        }
        return FLOW_FALLTHROUGH;
    case AST_RETURN:
        type_check_return_stmt(node, ctx);
        return FLOW_RETURN;
    case AST_BREAK:
        if (ctx->loop_depth <= 0) {
            semantic_error_with_hints(ctx, "PGY_SEM_LOOP_CONTROL_INVALID", "semantic:loop_control", "move-into-loop-or-fix-label", node, "'break' used outside of loop");
            return FLOW_NONE;
        }
        if (node->data.break_stmt.label != NULL) {
            bool found = false;
            for (int i = ctx->loop_depth - 1; i >= 0; i--) {
                if (ctx->loop_labels[i] != NULL
                    && strcmp(ctx->loop_labels[i], node->data.break_stmt.label) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                semantic_error_with_hints(ctx, "PGY_SEM_LOOP_CONTROL_INVALID", "semantic:loop_control", "move-into-loop-or-fix-label", node, "Unknown loop label '%s' in break",
                    node->data.break_stmt.label);
                return FLOW_NONE;
            }
        }
        {
            ResourceConsumeSnapshot snap = snapshot_resource_states_from_scope(
                loop_flow != NULL && loop_flow->loop_scope != NULL
                    ? loop_flow->loop_scope
                    : ctx->scope);
            loop_flow_record(loop_flow, true, &snap);
            destroy_resource_snapshot(&snap);
        }
        return FLOW_BREAK;
    case AST_CONTINUE:
        if (ctx->loop_depth <= 0) {
            semantic_error_with_hints(ctx, "PGY_SEM_LOOP_CONTROL_INVALID", "semantic:loop_control", "move-into-loop-or-fix-label", node, "'continue' used outside of loop");
            return FLOW_NONE;
        }
        if (node->data.continue_stmt.label != NULL) {
            bool found = false;
            for (int i = ctx->loop_depth - 1; i >= 0; i--) {
                if (ctx->loop_labels[i] != NULL
                    && strcmp(ctx->loop_labels[i], node->data.continue_stmt.label) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                semantic_error_with_hints(ctx, "PGY_SEM_LOOP_CONTROL_INVALID", "semantic:loop_control", "move-into-loop-or-fix-label", node, "Unknown loop label '%s' in continue",
                    node->data.continue_stmt.label);
                return FLOW_NONE;
            }
        }
        {
            ResourceConsumeSnapshot snap = snapshot_resource_states_from_scope(
                loop_flow != NULL && loop_flow->loop_scope != NULL
                    ? loop_flow->loop_scope
                    : ctx->scope);
            loop_flow_record(loop_flow, false, &snap);
            destroy_resource_snapshot(&snap);
        }
        return FLOW_CONTINUE;
    default:
        type_check_statement(node, ctx);
        return FLOW_FALLTHROUGH;
    }
}

bool
type_check_block(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return true;

    (void)type_check_block_flow(node, ctx, NULL);
    return !ctx->has_error;
}

bool
type_check_if_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_if_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}


bool
type_check_match_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_match_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}

bool
type_check_with_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_with_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}

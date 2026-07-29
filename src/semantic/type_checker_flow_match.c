#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "match_binding_type_fact.h"
#include "type_checker_flow_match_internal.h"
#include "../common/match_variant_policy.h"
#include "../common/string_compat.h"

static bool
match_binding_type_fact_reserve(SemanticContext *ctx, size_t needed)
{
    size_t capacity;
    PgyMatchBindingTypeFact *grown;

    if (ctx == NULL)
        return false;
    if (needed <= ctx->match_binding_type_fact_capacity)
        return true;
    capacity = ctx->match_binding_type_fact_capacity == 0
        ? 8
        : ctx->match_binding_type_fact_capacity;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*grown))
        return false;
    grown = realloc(ctx->match_binding_type_facts,
                    capacity * sizeof(*grown));
    if (grown == NULL)
        return false;
    ctx->match_binding_type_facts = grown;
    ctx->match_binding_type_fact_capacity = capacity;
    return true;
}

bool
semantic_match_binding_type_fact_record(SemanticContext *ctx,
                                        const ASTNode *match_case_node,
                                        size_t binding_index,
                                        size_t binding_count,
                                        const Type *binding_type)
{
    uint32_t function_id;
    uint32_t match_case_id;
    const char *type_name;
    PgyMatchBindingTypeFact *fact;

    if (ctx == NULL || match_case_node == NULL || binding_type == NULL
        || binding_count == 0 || binding_index >= binding_count)
        return false;
    /* Match binding rows are routine-local MIR input.  A standalone semantic
     * unit check has no HIR routine consumer, so it must not manufacture an
     * unresolvable row. */
    if (ctx->current_function_decl == NULL)
        return true;
    function_id = ast_node_stable_id(ctx->current_function_decl);
    match_case_id = ast_node_stable_id(match_case_node);
    type_name = type_name_or_unknown(binding_type);
    if (function_id == 0 || match_case_id == 0 || type_name == NULL
        || type_name[0] == '\0' || strcmp(type_name, "Unknown") == 0
        || strcmp(type_name, "<unknown>") == 0)
        return false;

    for (size_t i = 0; i < ctx->match_binding_type_fact_count; i++) {
        fact = &ctx->match_binding_type_facts[i];
        if (fact->function_syntax_id != function_id
            || fact->match_case_syntax_id != match_case_id
            || fact->binding_index != binding_index)
            continue;
        return fact->binding_count == binding_count
            && strcmp(fact->binding_type_name, type_name) == 0;
    }
    if (!match_binding_type_fact_reserve(ctx,
            ctx->match_binding_type_fact_count + 1))
        return false;
    fact = &ctx->match_binding_type_facts[
        ctx->match_binding_type_fact_count];
    fact->function_syntax_id = function_id;
    fact->match_case_syntax_id = match_case_id;
    fact->binding_index = binding_index;
    fact->binding_count = binding_count;
    fact->binding_type_name = pergyra_strdup(type_name);
    if (fact->binding_type_name == NULL)
        return false;
    ctx->match_binding_type_fact_count++;
    return true;
}

void
pgy_match_binding_type_facts_destroy(PgyMatchBindingTypeFact *facts,
                                     size_t count)
{
    if (facts == NULL)
        return;
    for (size_t i = 0; i < count; i++)
        free(facts[i].binding_type_name);
    free(facts);
}

bool
match_pattern_is_named_variant(ASTNode *pat, const char **name_out,
                               ASTNode ***args_out, size_t *arg_count_out)
{
    *name_out = NULL;
    *args_out = NULL;
    *arg_count_out = 0;
    if (pat == NULL)
        return false;
    if (pat->type == AST_IDENTIFIER) {
        *name_out = ast_identifier_name(pat);
        return *name_out != NULL;
    }
    if (pat->type == AST_MEMBER_ACCESS
        && ast_member_object(pat) != NULL
        && ast_member_object(pat)->type == AST_IDENTIFIER
        && ast_member_name(pat) != NULL) {
        *name_out = ast_member_name(pat);
        return true;
    }
    if (pat->type != AST_CALL || ast_call_callee(pat) == NULL)
        return false;
    ASTNode *callee = ast_call_callee(pat);
    if (callee->type == AST_IDENTIFIER
        && ast_identifier_name(callee) != NULL) {
        *name_out = ast_identifier_name(callee);
    } else if (callee->type == AST_MEMBER_ACCESS
        && ast_member_object(callee) != NULL
        && ast_member_object(callee)->type == AST_IDENTIFIER
        && ast_member_name(callee) != NULL) {
        *name_out = ast_member_name(callee);
    } else {
        return false;
    }
    *args_out = ast_call_arguments(pat, arg_count_out);
    return true;
}

ASTNode *
find_enum_decl_for_type(SemanticContext *ctx, const Type *type)
{
    if (ctx == NULL || type == NULL || type->kind != TYPE_KIND_ENUM
        || type->name == NULL) {
        return NULL;
    }

    return semantic_find_enum_decl_by_name(ctx, type->name);
}

static bool
declare_match_binding(SemanticContext *ctx, ASTNode *match_case_node,
                      ASTNode *binding_node, size_t binding_index,
                      size_t binding_count, Type *binding_type)
{
    const char *name;
    Symbol *binding;

    if (ctx == NULL || binding_node == NULL || binding_type == NULL)
        return false;
    if (binding_node->type != AST_IDENTIFIER
        || ast_identifier_name(binding_node) == NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MATCH_PATTERN_INVALID,
            PGY_CAUSE_MATCH_PATTERN_SHAPE,
            PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
            binding_node,
            "Destructuring pattern currently requires identifier bindings");
        return false;
    }

    name = ast_identifier_name(binding_node);
    if (scope_lookup_current(ctx->scope, name) != NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MATCH_PATTERN_INVALID,
            PGY_CAUSE_MATCH_PATTERN_SHAPE,
            PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
            binding_node,
            "Duplicate match binding '%s' in the same case scope", name);
        return false;
    }
    if (!semantic_match_binding_type_fact_record(
            ctx, match_case_node, binding_index, binding_count,
            binding_type)) {
        semantic_error(ctx, binding_node,
            "Match binding type fact capture failed");
        return false;
    }

    binding = symbol_create_variable(name, binding_type,
        binding_node->line, binding_node->column);
    if (binding == NULL || !scope_declare(ctx->scope, binding)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MATCH_PATTERN_INVALID,
            PGY_CAUSE_MATCH_PATTERN_SHAPE,
            PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
            binding_node,
            "Failed to declare match binding '%s'", name);
        return false;
    }

    return true;
}

static bool
type_check_special_match_pattern(ASTNode *match_case_node, ASTNode *pat,
                                  Type *subj_type,
                                  SemanticContext *ctx, bool *handled)
{
    const char *variant = NULL;
    ASTNode **args = NULL;
    size_t arg_count = 0;
    PgyMatchVariantKind variant_kind;

    *handled = false;

    if (!match_pattern_is_named_variant(pat, &variant, &args, &arg_count)
        || variant == NULL || subj_type == NULL) {
        return true;
    }
    variant_kind = pgy_match_variant_lookup(variant);

    if (type_is_constructed_named(subj_type, "Option")) {
        Type *inner = type_get_constructed_arg(subj_type, 0);
        *handled = true;
        if (variant_kind == PGY_MATCH_VARIANT_SOME) {
            if (arg_count != 1) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MATCH_PATTERN_INVALID,
                    PGY_CAUSE_MATCH_PATTERN_SHAPE,
                    PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
                    pat,
                    "Some pattern requires exactly one binding");
                return false;
            }
            return declare_match_binding(
                ctx, match_case_node, args[0], 0, 1, inner);
        }
        if (variant_kind == PGY_MATCH_VARIANT_NONE_CTOR) {
            if (arg_count != 0) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MATCH_PATTERN_INVALID,
                    PGY_CAUSE_MATCH_PATTERN_SHAPE,
                    PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
                    pat,
                    "None pattern does not take payload bindings");
                return false;
            }
            return true;
        }

        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MATCH_PATTERN_INVALID,
            PGY_CAUSE_MATCH_PATTERN_SHAPE,
            PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
            pat,
            "Option<T> match only supports Some(...) and None patterns");
        return false;
    }

    if (type_is_constructed_named(subj_type, "Result")) {
        *handled = true;
        if (variant_kind == PGY_MATCH_VARIANT_OK) {
            if (arg_count != 1) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MATCH_PATTERN_INVALID,
                    PGY_CAUSE_MATCH_PATTERN_SHAPE,
                    PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
                    pat,
                    "Ok pattern requires exactly one binding");
                return false;
            }
            return declare_match_binding(ctx, match_case_node, args[0], 0, 1,
                type_get_constructed_arg(subj_type, 0));
        }
        if (variant_kind == PGY_MATCH_VARIANT_ERR) {
            if (arg_count != 1) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MATCH_PATTERN_INVALID,
                    PGY_CAUSE_MATCH_PATTERN_SHAPE,
                    PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
                    pat,
                    "Err pattern requires exactly one binding");
                return false;
            }
            {
                Type *err_type = type_get_constructed_arg(subj_type, 1);
                return declare_match_binding(
                    ctx, match_case_node, args[0], 0, 1,
                    err_type != NULL ? err_type : TYPE_STRING);
            }
        }

        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MATCH_PATTERN_INVALID,
            PGY_CAUSE_MATCH_PATTERN_SHAPE,
            PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
            pat,
            "Result<T> match only supports Ok(...) and Err(...) patterns");
        return false;
    }

    if (subj_type->kind == TYPE_KIND_ENUM) {
        ASTNode *enum_decl = find_enum_decl_for_type(ctx, subj_type);
        Symbol *variant_sym;

        *handled = true;
        if (enum_decl == NULL)
            return true;
        size_t variant_count = 0;
        char **variants = ast_enum_variants(enum_decl, &variant_count);
        for (size_t i = 0; i < variant_count; i++) {
            const char *enum_variant = variants != NULL ? variants[i] : NULL;
            size_t param_count = ast_enum_variant_param_count(enum_decl, i);

            if (enum_variant == NULL || strcmp(enum_variant, variant) != 0)
                continue;

            if (arg_count != param_count) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MATCH_PATTERN_INVALID,
                    PGY_CAUSE_MATCH_PATTERN_SHAPE,
                    PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
                    pat,
                    "Enum variant '%s' expects %llu payload bindings, got %llu",
                    variant, (unsigned long long)param_count,
                    (unsigned long long)arg_count);
                return false;
            }

            if (param_count == 0)
                return true;

            variant_sym = scope_lookup(ctx->scope, variant);
            if (variant_sym == NULL || variant_sym->type == NULL
                || variant_sym->type->kind != TYPE_KIND_FUNCTION) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MATCH_PATTERN_INVALID,
                    PGY_CAUSE_MATCH_PATTERN_SHAPE,
                    PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
                    pat,
                    "Enum variant '%s' is missing constructor type information",
                    variant);
                return false;
            }

            for (size_t p = 0; p < param_count; p++) {
                Type *binding_type =
                    (p < type_function_param_count(variant_sym->type))
                    ? type_function_param_type(variant_sym->type, p)
                    : TYPE_UNKNOWN;
                if (!declare_match_binding(ctx, match_case_node, args[p], p,
                        param_count, binding_type))
                    return false;
            }
            return true;
        }

        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MATCH_PATTERN_INVALID,
            PGY_CAUSE_MATCH_PATTERN_SHAPE,
            PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
            pat,
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
        && ast_match_case_patterns(mc, NULL) != NULL
        && ast_match_case_pattern_count(mc) > 1;
}

static bool
match_case_uses_named_variant_pattern(ASTNode *mc)
{
    size_t count;
    ASTNode **patterns;

    if (!match_case_has_or_patterns(mc))
        return false;
    patterns = ast_match_case_patterns(mc, NULL);
    count = ast_match_case_pattern_count(mc);
    for (size_t i = 0; i < count; i++) {
        const char *name = NULL;
        ASTNode **args = NULL;
        size_t arg_count = 0;
        if (match_pattern_is_named_variant(patterns[i],
                                           &name,
                                           &args,
                                           &arg_count)) {
            return true;
        }
    }
    return false;
}

bool
type_check_match_case_patterns(ASTNode *mc, Type *subj_type,
                               SemanticContext *ctx)
{
    size_t count = 1;
    ASTNode **patterns;
    ASTNode *single_pattern;

    if (mc == NULL || mc->type != AST_MATCH_CASE)
        return true;

    single_pattern = ast_match_case_pattern(mc);
    patterns = &single_pattern;
    if (ast_match_case_patterns(mc, NULL) != NULL
        && ast_match_case_pattern_count(mc) > 0) {
        patterns = ast_match_case_patterns(mc, NULL);
        count = ast_match_case_pattern_count(mc);
    }
    if (match_case_uses_named_variant_pattern(mc)) {
        for (size_t i = 0; i < count; i++) {
            const char *name = NULL;
            ASTNode **args = NULL;
            size_t arg_count = 0;
            if (match_pattern_is_named_variant(patterns[i],
                                               &name,
                                               &args,
                                               &arg_count)
                && arg_count > 0) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MATCH_PATTERN_INVALID,
                    PGY_CAUSE_MATCH_PATTERN_SHAPE,
                    PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
                    patterns[i],
                    "OR patterns with variant destructuring (e.g. case .Some(x) | .None) "
                    "are not yet supported; split into separate cases");
                return false;
            }
        }
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode *pat = patterns[i];
        bool handled = false;
        if (pat == NULL)
            continue;
        if (!type_check_special_match_pattern(
                mc, pat, subj_type, ctx, &handled))
            continue;
        if (!handled) {
            Type *pat_type = type_check_expression(pat, ctx);
            if (!type_is_assignable(pat_type, subj_type)
                && !type_is_assignable(subj_type, pat_type)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MATCH_PATTERN_INVALID,
                    PGY_CAUSE_MATCH_PATTERN_SHAPE,
                    PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
                    pat,
                    "Case pattern type '%s' incompatible with match subject '%s'",
                    pat_type->name, subj_type->name);
            }
        }
    }
    return true;
}

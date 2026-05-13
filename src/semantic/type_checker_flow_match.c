#include <string.h>

#include "diag_codes.h"
#include "type_checker_flow_match_internal.h"

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
    if (pat->type != AST_CALL || pat->data.call.callee == NULL)
        return false;
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

ASTNode *
find_enum_decl_for_type(SemanticContext *ctx, const Type *type)
{
    ASTNode *prog;

    if (ctx == NULL || type == NULL || type->kind != TYPE_KIND_ENUM
        || type->name == NULL || ctx->program_root == NULL) {
        return NULL;
    }

    prog = ctx->program_root;
    if (prog->type != AST_PROGRAM)
        return NULL;

    for (size_t i = 0; i < prog->data.program.count; i++) {
        ASTNode *stmt = prog->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ENUM_DECL
            || ast_enum_name(stmt) == NULL) {
            continue;
        }
        if (strcmp(ast_enum_name(stmt), type->name) == 0)
            return stmt;
    }

    return NULL;
}

static bool
declare_match_binding(SemanticContext *ctx, ASTNode *binding_node,
                      Type *binding_type)
{
    const char *name;
    Symbol *binding;

    if (ctx == NULL || binding_node == NULL || binding_type == NULL)
        return false;
    if (binding_node->type != AST_IDENTIFIER
        || binding_node->data.identifier.name == NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MATCH_PATTERN_INVALID,
            PGY_CAUSE_MATCH_PATTERN_SHAPE,
            PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
            binding_node,
            "Destructuring pattern currently requires identifier bindings");
        return false;
    }

    name = binding_node->data.identifier.name;
    if (scope_lookup_current(ctx->scope, name) != NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MATCH_PATTERN_INVALID,
            PGY_CAUSE_MATCH_PATTERN_SHAPE,
            PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
            binding_node,
            "Duplicate match binding '%s' in the same case scope", name);
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
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MATCH_PATTERN_INVALID,
                    PGY_CAUSE_MATCH_PATTERN_SHAPE,
                    PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
                    pat,
                    "Some pattern requires exactly one binding");
                return false;
            }
            return declare_match_binding(ctx, args[0], inner);
        }
        if (strcmp(variant, "None") == 0) {
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
        if (strcmp(variant, "Ok") == 0) {
            if (arg_count != 1) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MATCH_PATTERN_INVALID,
                    PGY_CAUSE_MATCH_PATTERN_SHAPE,
                    PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
                    pat,
                    "Ok pattern requires exactly one binding");
                return false;
            }
            return declare_match_binding(ctx, args[0],
                type_get_constructed_arg(subj_type, 0));
        }
        if (strcmp(variant, "Err") == 0) {
            if (arg_count != 1) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MATCH_PATTERN_INVALID,
                    PGY_CAUSE_MATCH_PATTERN_SHAPE,
                    PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
                    pat,
                    "Err pattern requires exactly one binding");
                return false;
            }
            return declare_match_binding(ctx, args[0], TYPE_STRING);
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
                    (p < variant_sym->type->data.function.param_count)
                    ? variant_sym->type->data.function.param_types[p]
                    : TYPE_UNKNOWN;
                if (!declare_match_binding(ctx, args[p], binding_type))
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

    if (mc == NULL || mc->type != AST_MATCH_CASE)
        return true;

    patterns = &mc->data.match_case.pattern;
    if (mc->data.match_case.patterns != NULL
        && mc->data.match_case.pattern_count > 0) {
        patterns = mc->data.match_case.patterns;
        count = mc->data.match_case.pattern_count;
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
        if (!type_check_special_match_pattern(pat, subj_type, ctx, &handled))
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

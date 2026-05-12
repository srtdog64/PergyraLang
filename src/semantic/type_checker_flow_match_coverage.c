#include <stdint.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_flow_match_internal.h"
#include "../common/string_compat.h"

static bool
match_case_has_or_patterns(ASTNode *mc)
{
    return mc != NULL && mc->type == AST_MATCH_CASE
        && mc->data.match_case.patterns != NULL
        && mc->data.match_case.pattern_count > 1;
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
        if (strcmp(variant_name, "Ok") == 0
            || strcmp(variant_name, "Err") == 0) {
            return arg_count == 1;
        }
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

        if (!match_case_has_or_patterns(mc)
            && pattern_covers_variant(mc->data.match_case.pattern,
                                      subj_type,
                                      variant_name)) {
            return true;
        }

        if (match_case_has_or_patterns(mc)) {
            size_t count = mc->data.match_case.pattern_count;
            ASTNode **patterns = mc->data.match_case.patterns;
            for (size_t j = 0; j < count; j++) {
                if (pattern_covers_variant(patterns[j],
                                           subj_type,
                                           variant_name)) {
                    return true;
                }
            }
        }
    }

    return false;
}

static void
append_missing_variant(char *buf,
                       size_t buf_size,
                       const char *variant_name,
                       bool *first)
{
    if (buf == NULL || buf_size == 0 || variant_name == NULL || first == NULL)
        return;

    if (!*first)
        pergyra_str_append(buf, buf_size, ", ");
    pergyra_str_append(buf, buf_size, variant_name);
    *first = false;
}

static size_t
collect_match_variant_space(const Type *subj_type,
                            SemanticContext *ctx,
                            const char ***variants_out)
{
    static const char *option_variants[] = { "Some", "None" };
    static const char *result_variants[] = { "Ok", "Err" };

    if (variants_out == NULL)
        return 0;
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

void
check_match_redundancy(ASTNode *node, Type *subj_type, SemanticContext *ctx)
{
    const char **variants = NULL;
    size_t variant_count = collect_match_variant_space(subj_type, ctx, &variants);
    bool *seen;
    size_t covered = 0;

    if (node == NULL || subj_type == NULL || ctx == NULL
        || variants == NULL || variant_count == 0) {
        return;
    }
    if (variant_count > SIZE_MAX / sizeof(bool))
        return;

    seen = pgy_arena_calloc(&ctx->scratch_arena,
        variant_count * sizeof(bool));
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
                || !pattern_covers_variant(mc->data.match_case.pattern,
                                           subj_type,
                                           variant)) {
                continue;
            }

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

    (void)covered;
    (void)variant_count;
}

void
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
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MATCH_PATTERN_INVALID,
            PGY_CAUSE_MATCH_PATTERN_SHAPE,
            PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
            node,
            "Non-exhaustive match for '%s'; missing cases: %s",
            subj_type->name != NULL ? subj_type->name : "<unknown>",
            missing);
    }
}

bool
match_stmt_has_total_case_coverage(ASTNode *node, Type *subj_type,
                                   SemanticContext *ctx)
{
    const char **variants = NULL;
    size_t variant_count;

    if (node == NULL || subj_type == NULL)
        return false;
    if (node->data.match_stmt.default_body != NULL)
        return true;

    variant_count = collect_match_variant_space(subj_type, ctx, &variants);
    if (variants == NULL || variant_count == 0)
        return false;

    for (size_t i = 0; i < variant_count; i++) {
        const char *variant = variants[i];
        if (variant == NULL)
            continue;
        if (!case_list_covers_variant(node, subj_type, variant))
            return false;
    }

    return true;
}

#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"

char *
format_generic_subject_signature(const char *name, GenericParams *params)
{
    size_t total_len;
    char *result;
    char *cursor;

    if (name == NULL)
        return tc_strdup_fmt("<generic>");
    size_t param_count = ast_generic_param_count(params);
    if (param_count == 0)
        return tc_strdup_fmt("%s", name);

    total_len = strlen(name) + 2; /* '<' + '>' */
    for (size_t i = 0; i < param_count; i++) {
        GenericParam *gp = ast_generic_param_at(params, i);
        const char *param_name = ast_generic_param_name(gp);
        if (param_name == NULL)
            param_name = "<type>";
        total_len += strlen(param_name);
        if (i + 1 < param_count)
            total_len += 2; /* ", " */
    }

    result = malloc(total_len + 1);
    if (result == NULL)
        return tc_strdup_fmt("%s", name);

    cursor = result;
    memcpy(cursor, name, strlen(name));
    cursor += strlen(name);
    *cursor++ = '<';

    for (size_t i = 0; i < param_count; i++) {
        GenericParam *gp = ast_generic_param_at(params, i);
        const char *param_name = ast_generic_param_name(gp);
        if (param_name == NULL)
            param_name = "<type>";

        memcpy(cursor, param_name, strlen(param_name));
        cursor += strlen(param_name);
        if (i + 1 < param_count) {
            memcpy(cursor, ", ", 2);
            cursor += 2;
        }
    }
    *cursor++ = '>';
    *cursor = '\0';

    return result;
}

const char *
format_generic_subject_signature_scratch(SemanticContext *ctx,
                                         const char *name,
                                         GenericParams *params)
{
    size_t total_len;
    char *result;
    char *cursor;

    if (ctx == NULL)
        return format_generic_subject_signature(name, params);
    if (name == NULL)
        return pgy_arena_strdup(&ctx->scratch_arena, "<generic>");
    size_t param_count = ast_generic_param_count(params);
    if (param_count == 0)
        return pgy_arena_strdup(&ctx->scratch_arena, name);

    total_len = strlen(name) + 2;
    for (size_t i = 0; i < param_count; i++) {
        GenericParam *gp = ast_generic_param_at(params, i);
        const char *param_name = ast_generic_param_name(gp);
        if (param_name == NULL)
            param_name = "<type>";
        total_len += strlen(param_name);
        if (i + 1 < param_count)
            total_len += 2;
    }

    result = pgy_arena_alloc(&ctx->scratch_arena, total_len + 1);
    if (result == NULL)
        return pgy_arena_strdup(&ctx->scratch_arena, name);

    cursor = result;
    memcpy(cursor, name, strlen(name));
    cursor += strlen(name);
    *cursor++ = '<';

    for (size_t i = 0; i < param_count; i++) {
        GenericParam *gp = ast_generic_param_at(params, i);
        const char *param_name = ast_generic_param_name(gp);
        if (param_name == NULL)
            param_name = "<type>";

        memcpy(cursor, param_name, strlen(param_name));
        cursor += strlen(param_name);
        if (i + 1 < param_count) {
            memcpy(cursor, ", ", 2);
            cursor += 2;
        }
    }
    *cursor++ = '>';
    *cursor = '\0';

    return result;
}

static char *
format_effective_generic_type_list(const char *name, Type **types, size_t count)
{
    size_t total_len;
    char *result;
    char *cursor;

    if (name == NULL)
        return tc_strdup_fmt("<generic>");
    if (types == NULL || count == 0)
        return tc_strdup_fmt("%s", name);

    total_len = strlen(name) + 2; /* '<' + '>' */
    for (size_t i = 0; i < count; i++) {
        const char *type_name =
            (types[i] != NULL && types[i]->name != NULL)
                ? types[i]->name : "<type>";
        total_len += strlen(type_name);
        if (i + 1 < count)
            total_len += 2; /* ", " */
    }

    result = malloc(total_len + 1);
    if (result == NULL)
        return tc_strdup_fmt("%s", name);

    cursor = result;
    memcpy(cursor, name, strlen(name));
    cursor += strlen(name);
    *cursor++ = '<';

    for (size_t i = 0; i < count; i++) {
        const char *type_name =
            (types[i] != NULL && types[i]->name != NULL)
                ? types[i]->name : "<type>";

        memcpy(cursor, type_name, strlen(type_name));
        cursor += strlen(type_name);
        if (i + 1 < count) {
            memcpy(cursor, ", ", 2);
            cursor += 2;
        }
    }
    *cursor++ = '>';
    *cursor = '\0';

    return result;
}

const char *
format_effective_generic_type_list_scratch(SemanticContext *ctx,
                                           const char *name,
                                           Type **types,
                                           size_t count)
{
    size_t total_len;
    char *result;
    char *cursor;

    if (ctx == NULL)
        return format_effective_generic_type_list(name, types, count);
    if (name == NULL)
        return pgy_arena_strdup(&ctx->scratch_arena, "<generic>");
    if (types == NULL || count == 0)
        return pgy_arena_strdup(&ctx->scratch_arena, name);

    total_len = strlen(name) + 2;
    for (size_t i = 0; i < count; i++) {
        const char *type_name =
            (types[i] != NULL && types[i]->name != NULL)
                ? types[i]->name : "<type>";
        total_len += strlen(type_name);
        if (i + 1 < count)
            total_len += 2;
    }

    result = pgy_arena_alloc(&ctx->scratch_arena, total_len + 1);
    if (result == NULL)
        return pgy_arena_strdup(&ctx->scratch_arena, name);

    cursor = result;
    memcpy(cursor, name, strlen(name));
    cursor += strlen(name);
    *cursor++ = '<';

    for (size_t i = 0; i < count; i++) {
        const char *type_name =
            (types[i] != NULL && types[i]->name != NULL)
                ? types[i]->name : "<type>";

        memcpy(cursor, type_name, strlen(type_name));
        cursor += strlen(type_name);
        if (i + 1 < count) {
            memcpy(cursor, ", ", 2);
            cursor += 2;
        }
    }
    *cursor++ = '>';
    *cursor = '\0';

    return result;
}

bool
identifier_is_borrowed_boundary_param(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *func_decl;
    const char *ident_name;

    if (expr == NULL || ctx == NULL
        || expr->type != AST_IDENTIFIER
        || ast_identifier_name(expr) == NULL) {
        return false;
    }

    func_decl = ctx->current_function_decl;
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;

    ident_name = ast_identifier_name(expr);
    for (size_t i = 0; i < ast_func_param_count(func_decl); i++) {
        FuncParam *param = ast_func_param(func_decl, i);
        Type *param_type;

        if (param == NULL || param->name == NULL || param->type == NULL)
            continue;
        if (param->mode != PARAM_MODE_REF)
            continue;
        if (strcmp(param->name, ident_name) != 0)
            continue;

        param_type = domain_resolve_type_ref(param->type, ctx);
        return type_is_general_boundary_type(param_type, ctx);
    }

    return false;
}

size_t
generic_params_required_count(GenericParams *params)
{
    size_t required = 0;
    size_t param_count = ast_generic_param_count(params);
    for (size_t i = 0; i < param_count; i++) {
        GenericParam *param = ast_generic_param_at(params, i);
        if (param != NULL && ast_generic_param_default_type(param) == NULL)
            required++;
    }
    return required;
}

ASTNode **
collect_effective_generic_arg_nodes(GenericParams *decl_params,
                                    GenericParams *provided_args,
                                    const ASTNode *site,
                                    SemanticContext *ctx,
                                    const char *owner_kind,
                                    const char *owner_name,
                                    size_t *out_count)
{
    size_t decl_count;
    size_t provided_count;
    size_t required_count;
    ASTNode **effective = NULL;

    if (out_count != NULL)
        *out_count = 0;
    if (decl_params == NULL)
        return NULL;

    decl_count = ast_generic_param_count(decl_params);
    provided_count = ast_generic_param_count(provided_args);
    required_count = generic_params_required_count(decl_params);

    if (decl_count == 0) {
        if (provided_count == 0)
            return NULL;
        if (ctx != NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                site,
                "%s '%s' does not accept generic type arguments.\n"
                "Reason:\n"
                "- this declaration has no generic parameters\n"
                "- supplied type arguments therefore have nowhere to bind\n"
                "Fix:\n"
                "- remove the generic arguments at the use site\n"
                "- or declare generic parameters on %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
        }
        return NULL;
    }

    if (provided_count > decl_count) {
        if (ctx != NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                site,
                "%s '%s' accepts at most %llu generic argument(s), got %llu.\n"
                "Reason:\n"
                "- more type arguments were supplied than there are generic parameters\n"
                "- effective generic argument derivation cannot match extras safely\n"
                "Fix:\n"
                "- remove the extra generic argument(s)\n"
                "- or add matching generic parameters to %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                (unsigned long long) decl_count, (unsigned long long) provided_count,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
        }
        return NULL;
    }

    if (provided_count < required_count) {
        if (ctx != NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                site,
                "%s '%s' requires at least %llu generic argument(s), got %llu.\n"
                "Reason:\n"
                "- some generic parameters have no default type argument\n"
                "- effective generic argument derivation therefore cannot close the contract\n"
                "Fix:\n"
                "- provide the missing generic argument(s)\n"
                "- or declare trailing default type arguments on %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                (unsigned long long) required_count, (unsigned long long) provided_count,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
        }
        return NULL;
    }

    effective = calloc(decl_count > 0 ? decl_count : 1, sizeof(ASTNode *));
    if (effective == NULL)
        return NULL;

    for (size_t i = 0; i < decl_count; i++) {
        ASTNode *arg = NULL;
        if (provided_args != NULL && i < provided_count) {
            GenericParam *provided = ast_generic_param_at(provided_args, i);
            arg = ast_generic_param_constraint(provided);
            if (arg != NULL)
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    owner_name != NULL ? owner_name : "<anonymous>",
                    arg,
                    "provided generic argument lookup");
        } else {
            GenericParam *decl_param = ast_generic_param_at(decl_params, i);
            arg = ast_generic_param_default_type(decl_param);
            if (arg != NULL)
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    owner_name != NULL ? owner_name : "<anonymous>",
                    arg,
                    "omitted default generic argument lookup");
        }

        if (arg == NULL) {
            if (ctx != NULL) {
                GenericParam *decl_param = ast_generic_param_at(decl_params, i);
                const char *param_name = ast_generic_param_name(decl_param);
                if (param_name == NULL)
                    param_name = "<type-param>";
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                    PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                    site,
                    "%s '%s' is missing generic argument for parameter '%s'.\n"
                    "Reason:\n"
                    "- this parameter has no provided argument and no usable default\n"
                    "- effective generic argument derivation stopped at '%s'\n"
                    "Fix:\n"
                    "- provide a type argument for '%s'\n"
                    "- or declare a default type argument for '%s'",
                    owner_kind != NULL ? owner_kind : "declaration",
                    owner_name != NULL ? owner_name : "<anonymous>",
                    param_name,
                    param_name,
                    param_name,
                    param_name);
            }
            free(effective);
            return NULL;
        }

        effective[i] = arg;
    }

    if (out_count != NULL)
        *out_count = decl_count;
    return effective;
}

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../common/string_compat.h"
#include "type_checker_internal.h"
#include "diag_codes.h"

static Type *
class_decl_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_type_ref_or_materialize(ctx, type_ref);
}

bool
type_check_class_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.class_decl.name;
    ASTNode *saved_nominal = ctx->current_nominal_decl;

    /* Register class generic parameters as opaque metadata-visible types for
     * field and method signature resolution. */
    bool has_generics = (node->data.class_decl.generic_params != NULL
                         && node->data.class_decl.generic_params->count > 0);
    if (has_generics) {
        validate_generic_param_defaults(node->data.class_decl.generic_params,
            ctx, node, "class");
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = node->data.class_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    Type *class_type = calloc(1, sizeof(Type));
    if (class_type == NULL) {
        if (has_generics) scope_exit(&ctx->scope);
        return false;
    }
    class_type->kind = TYPE_KIND_CLASS;
    class_type->nominal_flavor = nominal_flavor_from_decl(node);
    class_type->name = pergyra_strdup(name);

    Symbol *class_sym = symbol_create_function(name, class_type,
                                                node->line, node->column);
    class_sym->kind = SYMBOL_CLASS;

    /* Declare in the outer scope (step out of temporary generic scope).
     * Pass 1 may already have registered a nominal placeholder so that
     * forward references in earlier declarations can resolve. */
    {
        Scope *target = has_generics ? ctx->scope->parent : ctx->scope;
        Scope *saved = ctx->scope;
        ctx->scope = target;
        Symbol *existing = scope_lookup_current(ctx->scope, name);
        if (existing != NULL
            && existing->kind == SYMBOL_CLASS
            && existing->decl_line == (uint32_t)node->line
            && existing->decl_col == (uint32_t)node->column) {
            existing->type = class_type;
            symbol_destroy(class_sym);
        } else if (!scope_declare(ctx->scope, class_sym)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_REDECLARATION,
                PGY_CAUSE_CLASS_DUPLICATE_NAME,
                PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
                node, "Redeclaration of class '%s'", name);
            symbol_destroy(class_sym);
            ctx->scope = saved;
            if (has_generics) scope_exit(&ctx->scope);
            return false;
        }
        ctx->scope = saved;
    }

    /* Close the temporary generic-params scope before entering the real
     * class scope — the class scope will re-register generic params so
     * they're visible in the body. */
    if (has_generics)
        scope_exit(&ctx->scope);

    validate_where_clause_bounds(node->data.class_decl.where_clause, ctx, node);
    validate_generic_param_default_bounds(
        node->data.class_decl.generic_params,
        node->data.class_decl.where_clause,
        ctx,
        node,
        "class",
        name);

    /* Check methods — type-check each in a temporary class scope,
     * then register the mangled name (ClassName_MethodName) in the
     * parent scope so that callers can find it. */
    scope_enter(&ctx->scope, SCOPE_CLASS);
    ctx->current_nominal_decl = node;

    /* Re-register generic type params inside the class scope */
    if (has_generics) {
        GenericParams *gp = node->data.class_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }
    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *field = node->data.class_decl.fields[i];
        Type *field_type;
        Symbol *field_sym;

        if (field == NULL || field->name == NULL || field->type == NULL)
            continue;

        field_type = class_decl_resolve_type_ref(field->type, ctx);
        if (field->is_vessel_field) {
            ASTNode *field_decl = NULL;
            if (field_type != NULL && field_type->kind == TYPE_KIND_CLASS
                && field_type->name != NULL) {
                field_decl = find_type_decl_by_name(ctx->program_root, field_type->name);
            }
            if (field_decl == NULL
                || field_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_DOMAIN_VESSEL_REQUIRED,
                    PGY_FIX_DECLARE_VESSEL_TYPE,
                    field->type,
                    "subject vessel field '%s' must reference a vessel type",
                    field->name != NULL ? field->name : "<field>");
            }
        }
        field_sym = symbol_create_variable(field->name, field_type,
            node->line, node->column);
        if (field_sym != NULL)
            scope_declare(ctx->scope, field_sym);
    }
    /* struct declarations cannot have methods — use class or object */
    if (node->data.class_decl.nominal_kind == NOMINAL_DECL_STRUCT
        && node->data.class_decl.method_count > 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, node,
            "struct '%s' cannot have methods; use 'class', 'subject', or 'object' instead",
            name != NULL ? name : "<struct>");
    }

    for (size_t i = 0; i < node->data.class_decl.method_count; i++)
        type_check_func_decl(node->data.class_decl.methods[i], ctx);

    /* Collect method signatures before the class scope is destroyed */
    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        const char *mname = method->data.func_decl.name;
        if (mname == NULL)
            continue;
        Symbol *msym = scope_lookup_current(ctx->scope, mname);
        if (msym == NULL || msym->kind != SYMBOL_FUNCTION)
            continue;

        /* Build mangled name: ClassName_MethodName */
        size_t len = strlen(name) + 1 + strlen(mname) + 1;
        char *mangled = malloc(len);
        if (mangled == NULL)
            continue;
        snprintf(mangled, len, "%s_%s", name, mname);

        /* The method's func_type already includes 'self' as the first
         * parameter (registered by type_check_func_decl), so reuse
         * the original signature directly. */
        Type *mangled_ft = msym->type;

        /* Register in parent scope (outside class) */
        Symbol *mangled_sym = symbol_create_function(
            mangled, mangled_ft, method->line, method->column);
        /* Temporarily step out to declare in parent */
        Scope *class_scope = ctx->scope;
        ctx->scope = class_scope->parent;
        if (!scope_declare(ctx->scope, mangled_sym))
            symbol_destroy(mangled_sym);
        ctx->scope = class_scope;
        free(mangled);
    }

    scope_exit(&ctx->scope);
    ctx->current_nominal_decl = saved_nominal;

    return !ctx->has_error;
}

bool
type_check_extern_block(ASTNode *node, SemanticContext *ctx)
{
    for (size_t i = 0; i < node->data.extern_block.count; i++) {
        ASTNode *decl = node->data.extern_block.declarations[i];
        if (decl != NULL && decl->type == AST_FUNC_DECL)
            type_check_func_decl(decl, ctx);
    }
    return !ctx->has_error;
}

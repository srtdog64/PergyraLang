#include "type_checker_internal.h"
#include "diag_codes.h"

bool
type_check_enum_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name;
    ASTNode *saved_nominal;
    size_t method_count = 0;
    ASTNode **methods;
    Symbol *existing;

    if (node == NULL || node->type != AST_ENUM_DECL || ctx == NULL)
        return true;

    name = ast_enum_name(node);
    existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL
        && !symbol_is_forward_declaration_for(existing,
            SYMBOL_CLASS, ast_node_stable_id(node))) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_SCOPE_DUPLICATE_SYMBOL,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of enum '%s'", name);
        return false;
    }
    symbol_complete_forward_declaration(existing);

    saved_nominal = ctx->current_nominal_decl;
    methods = ast_enum_methods(node, &method_count);

    scope_enter(&ctx->scope, SCOPE_CLASS);
    ctx->current_nominal_decl = node;

    for (size_t i = 0; i < method_count; i++)
        type_check_func_decl(methods != NULL ? methods[i] : NULL, ctx);

    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods != NULL ? methods[i] : NULL;
        const char *method_name = ast_declaration_name(method);
        Symbol *msym;
        char *mangled;
        Symbol *mangled_sym;
        Scope *enum_scope;

        if (method == NULL || method->type != AST_FUNC_DECL
            || method_name == NULL || name == NULL)
            continue;
        msym = scope_lookup_current(ctx->scope, method_name);
        if (msym == NULL || msym->kind != SYMBOL_FUNCTION)
            continue;
        /* symbol_create_function duplicates this scratch string. */
        mangled = pgy_arena_fmt(&ctx->scratch_arena,
                                "%s_%s", name, method_name);
        if (mangled == NULL)
            continue;
        mangled_sym = symbol_create_function(
            mangled, msym->type, method->line, method->column);
        enum_scope = ctx->scope;
        ctx->scope = enum_scope->parent;
        if (!scope_declare(ctx->scope, mangled_sym))
            symbol_destroy(mangled_sym);
        ctx->scope = enum_scope;
    }

    scope_exit(&ctx->scope);
    ctx->current_nominal_decl = saved_nominal;
    return !ctx->has_error;
}

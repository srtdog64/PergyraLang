#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../common/string_compat.h"
#include "type_checker_internal.h"
#include "diag_codes.h"
#include "compiler/decl_field_model.h"

static void
class_declare_field_symbol(SemanticContext *ctx, const char *field_name,
                           Type *field_type, ASTNode *node)
{
    Symbol *field_sym;

    /* A `Slot<T>` / `SecureSlot<T>` typed field is an owning slot, not a plain
     * value: declare it as a slot symbol so Release / Write / Read recognize it
     * the same way a function-local slot binding is recognized. */
    if (field_type != NULL && field_type->kind == TYPE_KIND_SLOT)
    {
        Symbol *slot_sym = symbol_create_slot(field_name, field_type,
            type_slot_is_secure(field_type), NULL,
            node->line, node->column);
        if (slot_sym == NULL)
            return;

        scope_declare(ctx->scope, slot_sym);
        scope_register_slot(ctx->scope, slot_sym);
        return;
    }

    field_sym = symbol_create_variable(field_name, field_type,
        node->line, node->column);
    if (field_sym != NULL)
        scope_declare(ctx->scope, field_sym);
}

static const char *
destructure_field_shell_name(const char *callee, size_t index)
{
    if (callee == NULL)
        return NULL;
    if (strcmp(callee, "ClaimSecureSlot") == 0)
        return index == 0 ? "SecureSlot" : "Token";
    if (strcmp(callee, "ClaimSlot") == 0)
        return index == 0 ? "Slot" : NULL;
    return NULL;
}

/* Resolve the type-less placeholder field for `field_name` to `shell<inner>`
 * (e.g. `SecureSlot<Int>`). The struct member emission and the RIR field-slot
 * fact seeding both read ClassField->type, so this is what lets a destructured
 * `(slot, token)` field reach both backends. */
static void
class_assign_field_type_ref(ASTNode *node, const char *field_name,
                            const char *shell, ASTNode *inner_node)
{
    size_t field_count = 0;
    ClassField **fields;

    if (shell == NULL || field_name == NULL)
        return;

    fields = ast_class_fields(node, &field_count);
    for (size_t i = 0; i < field_count; i++)
    {
        ClassField *field = fields != NULL ? fields[i] : NULL;
        if (field == NULL || field->name == NULL || field->type != NULL)
            continue;
        if (strcmp(field->name, field_name) != 0)
            continue;

        field->type = ast_create_generic_type(shell,
            inner_node != NULL ? ast_clone(inner_node) : NULL);
        return;
    }
}

static void
class_set_destructure_field_types(ASTNode *node)
{
    size_t group_count = ast_class_field_destructure_count(node);

    for (size_t gi = 0; gi < group_count; gi++)
    {
        ASTNode *group = ast_class_field_destructure_at(node, gi);
        ASTNode *init;
        const char *callee;
        ASTNode *inner_node = NULL;

        if (group == NULL)
            continue;
        init = ast_let_destructure_initializer(group);
        if (init == NULL || init->type != AST_CALL
            || ast_call_callee(init) == NULL)
            continue;

        callee = ast_identifier_name(ast_call_callee(init));
        if (ast_call_generic_arg_count(init) >= 1)
            inner_node = ast_generic_param_constraint(
                ast_call_generic_arg(init, 0));

        for (size_t ni = 0; ni < ast_let_destructure_name_count(group); ni++)
            class_assign_field_type_ref(node,
                ast_let_destructure_name(group, ni),
                destructure_field_shell_name(callee, ni), inner_node);
    }
}

bool
type_check_class_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = ast_class_name(node);
    ASTNode *saved_nominal = ctx->current_nominal_decl;

    /* Register class generic parameters as opaque metadata-visible types for
     * field and method signature resolution. */
    GenericParams *class_generics = ast_class_generic_params(node);
    WhereClause *class_where = ast_class_where_clause(node);
    bool has_generics = (ast_generic_param_count(class_generics) > 0);
    if (has_generics) {
        validate_generic_param_defaults(class_generics, ctx, node, "class");
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = class_generics;
        size_t generic_count = ast_generic_param_count(gp);
        for (size_t gi = 0; gi < generic_count; gi++) {
            GenericParam *param = ast_generic_param_at(gp, gi);
            const char *param_name = ast_generic_param_name(param);
            if (param_name == NULL)
                continue;
            Type *tp = type_create_generic(param_name);
            Symbol *s = symbol_create_variable(
                param_name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_TYPE_PARAM;
            scope_declare(ctx->scope, s);
        }
    }

    Type *class_type = type_alloc();
    if (class_type == NULL) {
        if (has_generics) scope_exit(&ctx->scope);
        return false;
    }
    class_type->kind = TYPE_KIND_CLASS;
    class_type->nominal_flavor = nominal_flavor_from_decl(node);
    class_type->name = pergyra_strdup(name);

    Symbol *class_sym = symbol_create_function(name, class_type,
                                                node->line, node->column);
    symbol_mark_declaration(class_sym, ast_node_stable_id(node), false);
    class_sym->kind = SYMBOL_CLASS;

    /* Declare in the outer scope (step out of temporary generic scope).
     * Pass 1 may already have registered a nominal placeholder so that
     * forward references in earlier declarations can resolve. */
    {
        Scope *target = has_generics ? ctx->scope->parent : ctx->scope;
        Scope *saved = ctx->scope;
        ctx->scope = target;
        Symbol *existing = scope_lookup_current(ctx->scope, name);
        if (symbol_is_forward_declaration_for(existing,
                SYMBOL_CLASS, ast_node_stable_id(node))) {
            existing->type = class_type;
            symbol_complete_forward_declaration(existing);
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

    validate_where_clause_bounds(class_where, ctx, node);
    validate_generic_param_default_bounds(
        class_generics,
        class_where,
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
        GenericParams *gp = class_generics;
        size_t generic_count = ast_generic_param_count(gp);
        for (size_t gi = 0; gi < generic_count; gi++) {
            GenericParam *param = ast_generic_param_at(gp, gi);
            const char *param_name = ast_generic_param_name(param);
            if (param_name == NULL)
                continue;
            Type *tp = type_create_generic(param_name);
            Symbol *s = symbol_create_variable(
                param_name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_TYPE_PARAM;
            scope_declare(ctx->scope, s);
        }
    }
    /* F2 (docs/144) Phase 3b: declaration validation consumes the pre-semantic
       field-shape model. The generic-shell fixup above is an AST *writer* and
       legitimately stays on ast_class_fields. */
    PgyDeclField *fields = NULL;
    size_t field_count = pgy_class_decl_field_model_build(node, &fields);
    for (size_t i = 0; i < field_count; i++) {
        Type *field_type;

        if (fields[i].name == NULL || fields[i].type_ast == NULL)
            continue;

        field_type = semantic_host_resolve_type_ref(fields[i].type_ast, ctx);
        if (fields[i].is_vessel_field) {
            ASTNode *field_decl = semantic_host_decl_for_type(ctx, field_type);
            if (field_decl == NULL
                || ast_class_nominal_kind(field_decl) != NOMINAL_DECL_VESSEL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_DOMAIN_VESSEL_REQUIRED,
                    PGY_FIX_DECLARE_VESSEL_TYPE,
                    fields[i].type_ast,
                    "subject vessel field '%s' must reference a vessel type",
                    fields[i].name);
            }
        }
        class_declare_field_symbol(ctx, fields[i].name, field_type, node);
    }
    pgy_decl_field_model_free(fields, field_count);

    /* Class-body destructuring groups (`let (a, b) = ClaimSecureSlot<T>(...)`).
     * Reuse the statement-level destructure checker so the bound names enter
     * the class scope as the correct slot / token / value symbols, exactly as
     * a function body would. Methods are checked below in this same scope, so
     * `Write(_healthSlot, v, _healthToken)` then resolves against real slot and
     * token symbols instead of the type-less placeholder fields. */
    {
        size_t destructure_count = ast_class_field_destructure_count(node);
        for (size_t di = 0; di < destructure_count; di++)
        {
            ASTNode *destructure = ast_class_field_destructure_at(node, di);
            if (destructure == NULL)
                continue;

            type_check_let_destructure_stmt(destructure, ctx);
        }
    }

    /* Resolve the placeholder field types for the destructured names so the
     * struct members emit and the backends can lower field-slot resource ops.
     * Done after the scope pre-pass so the field loop above never re-declares
     * these names (their placeholder type was still NULL during that loop). */
    class_set_destructure_field_types(node);

    /* struct declarations cannot have methods — use class or object */
    size_t method_count = 0;
    ASTNode **methods = ast_class_methods(node, &method_count);
    if (ast_class_nominal_kind(node) == NOMINAL_DECL_STRUCT
        && method_count > 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, node,
            "struct '%s' cannot have methods; use 'class', 'subject', or 'object' instead",
            name != NULL ? name : "<struct>");
    }

    for (size_t i = 0; i < method_count; i++)
        type_check_func_decl(methods != NULL ? methods[i] : NULL, ctx);

    /* Collect method signatures before the class scope is destroyed */
    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods != NULL ? methods[i] : NULL;
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        const char *mname = ast_declaration_name(method);
        if (mname == NULL)
            continue;
        Symbol *msym = scope_lookup_current(ctx->scope, mname);
        if (msym == NULL || msym->kind != SYMBOL_FUNCTION)
            continue;

        /* Build mangled name: ClassName_MethodName */
        size_t name_len = strlen(name);
        size_t method_len = strlen(mname);
        if (method_len > ((size_t)-1) - name_len - 2)
            continue;
        size_t len = name_len + method_len + 2;
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
    size_t decl_count = 0;

    (void)ast_extern_block_declarations(node, &decl_count);
    for (size_t i = 0; i < decl_count; i++) {
        ASTNode *decl = ast_extern_block_declaration(node, i);
        if (decl != NULL && decl->type == AST_FUNC_DECL)
            type_check_func_decl(decl, ctx);
    }
    return !ctx->has_error;
}

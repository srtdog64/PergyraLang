#include <string.h>

#include "type_checker_internal.h"

static ASTNode *
host_lookup_program(SemanticContext *ctx)
{
    return ctx != NULL ? ctx->program_root : NULL;
}

uint32_t
semantic_current_routine_syntax_id(SemanticContext *ctx)
{
    ASTNode *owner;

    if (ctx == NULL)
        return 0;
    owner = ctx->current_function_decl != NULL
        ? ctx->current_function_decl
        : host_lookup_program(ctx);
    return ast_node_stable_id(owner);
}

static ASTNode *
host_find_type_decl_by_name(ASTNode *program, const char *type_name)
{
    if (program == NULL || program->type != AST_PROGRAM || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        if (stmt == NULL || stmt->type != AST_CLASS_DECL)
            continue;
        if (ast_class_name(stmt) != NULL
            && strcmp(ast_class_name(stmt), type_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
host_find_ability_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *ability_name = ast_ability_name(stmt);
        if (stmt == NULL || stmt->type != AST_ABILITY_DECL
            || ability_name == NULL)
            continue;
        if (strcmp(ability_name, name) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
host_find_callable_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *stmt_name;

        if (stmt == NULL)
            continue;
        stmt_name = stmt->is_async_decl
            ? ast_async_func_name(stmt)
            : ast_declaration_name(stmt);
        if (stmt->type == AST_FUNC_DECL
            && stmt_name != NULL
            && strcmp(stmt_name, name) == 0)
            return stmt;
        if (stmt->type == AST_EVENT_DECL
            && ast_event_name(stmt) != NULL
            && strcmp(ast_event_name(stmt), name) == 0)
            return stmt;
        if (stmt->type == AST_INTENT_DECL
            && ast_intent_decl_name(stmt) != NULL
            && strcmp(ast_intent_decl_name(stmt), name) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
host_find_enum_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        if (stmt != NULL && stmt->type == AST_ENUM_DECL
            && ast_enum_name(stmt) != NULL
            && strcmp(ast_enum_name(stmt), name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
host_find_function_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *stmt_name;

        if (stmt == NULL || stmt->type != AST_FUNC_DECL)
            continue;
        stmt_name = stmt->is_async_decl
            ? ast_async_func_name(stmt)
            : ast_declaration_name(stmt);
        if (stmt_name != NULL && strcmp(stmt_name, name) == 0)
            return stmt;
    }

    return NULL;
}

static const char *
host_domain_decl_name(ASTNode *decl, ASTNodeType decl_type)
{
    if (decl == NULL || decl->type != decl_type)
        return NULL;

    switch (decl_type) {
    case AST_RELATION_DECL:
        return ast_relation_name(decl);
    case AST_EFFECT_DECL:
        return ast_effect_name(decl);
    case AST_ZONE_DECL:
        return ast_zone_name(decl);
    case AST_WORLD_DECL:
        return ast_world_name(decl);
    case AST_PARTY_DECL:
        return ast_party_name(decl);
    case AST_ROSTER_DECL:
        return ast_roster_name(decl);
    default:
        return NULL;
    }
}

static ASTNode *
host_find_domain_decl_by_name(ASTNode *program,
                              ASTNodeType decl_type,
                              const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *decl_name = host_domain_decl_name(stmt, decl_type);
        if (decl_name != NULL && strcmp(decl_name, name) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
constructor_decl_for_symbol_kind(ASTNode *program, SymbolKind kind,
                                 const char *name)
{
    if (program == NULL || name == NULL)
        return NULL;

    switch (kind) {
    case SYMBOL_CLASS:
        return host_find_type_decl_by_name(program, name);
    case SYMBOL_PARTY:
        return host_find_domain_decl_by_name(program, AST_PARTY_DECL, name);
    case SYMBOL_ROSTER:
        return host_find_domain_decl_by_name(program, AST_ROSTER_DECL, name);
    case SYMBOL_WORLD:
        return host_find_domain_decl_by_name(program, AST_WORLD_DECL, name);
    case SYMBOL_ZONE:
        return host_find_domain_decl_by_name(program, AST_ZONE_DECL, name);
    case SYMBOL_RELATION:
        return host_find_domain_decl_by_name(program, AST_RELATION_DECL, name);
    case SYMBOL_EFFECT:
        return host_find_domain_decl_by_name(program, AST_EFFECT_DECL, name);
    default:
        return NULL;
    }
}

ASTNode *
semantic_find_type_alias_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;
    ASTNode *program;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_TYPE_ALIAS, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;

    program = host_lookup_program(ctx);
    if (program == NULL || program->type != AST_PROGRAM)
        return NULL;
    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *alias_name = ast_type_alias_name(stmt);
        if (stmt != NULL && stmt->type == AST_TYPE_ALIAS
            && alias_name != NULL && strcmp(alias_name, name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

ASTNode *
semantic_find_zone_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_ZONE_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_ZONE_DECL, name);
}

ASTNode *
semantic_find_relation_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_RELATION_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_RELATION_DECL, name);
}

ASTNode *
semantic_find_effect_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_EFFECT_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_EFFECT_DECL, name);
}

ASTNode *
semantic_find_world_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_WORLD_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_WORLD_DECL, name);
}

ASTNode *
semantic_find_party_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_PARTY_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_PARTY_DECL, name);
}

ASTNode *
semantic_find_roster_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_ROSTER_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_domain_decl_by_name(host_lookup_program(ctx),
                                         AST_ROSTER_DECL, name);
}

ASTNode *
semantic_find_class_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_CLASS_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_type_decl_by_name(host_lookup_program(ctx), name);
}

ASTNode *
semantic_find_intent_decl_by_name(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return semantic_host_index_find_decl_by_name(ctx, AST_INTENT_DECL, name);
}

ASTNode *
semantic_find_ability_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_ABILITY_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_ability_decl_by_name(host_lookup_program(ctx), name);
}

ASTNode *
semantic_find_enum_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_ENUM_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_enum_decl_by_name(host_lookup_program(ctx), name);
}

ASTNode *
semantic_find_function_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_FUNC_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_function_decl_by_name(host_lookup_program(ctx), name);
}

ASTNode *
semantic_find_callable_decl_by_name(SemanticContext *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_FUNC_DECL, name);
    if (decl != NULL)
        return decl;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_EVENT_DECL, name);
    if (decl != NULL)
        return decl;
    decl = semantic_host_index_find_decl_by_name(ctx, AST_INTENT_DECL, name);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return host_find_callable_decl_by_name(host_lookup_program(ctx), name);
}

ASTNode *
semantic_constructor_decl_for_symbol_kind(SemanticContext *ctx,
                                          SymbolKind kind,
                                          const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    switch (kind) {
    case SYMBOL_CLASS:
        return semantic_find_class_decl_by_name(ctx, name);
    case SYMBOL_PARTY:
        return semantic_find_party_decl_by_name(ctx, name);
    case SYMBOL_ROSTER:
        return semantic_find_roster_decl_by_name(ctx, name);
    case SYMBOL_WORLD:
        return semantic_find_world_decl_by_name(ctx, name);
    case SYMBOL_ZONE:
        return semantic_find_zone_decl_by_name(ctx, name);
    case SYMBOL_RELATION:
        return semantic_find_relation_decl_by_name(ctx, name);
    case SYMBOL_EFFECT:
        return semantic_find_effect_decl_by_name(ctx, name);
    default:
        return constructor_decl_for_symbol_kind(host_lookup_program(ctx),
                                                kind, name);
    }
}

/* The index owns the selected declaration identity, including membership in
 * its original host; a matching spelling alone is not a current callable. */
static bool
host_nested_callable_current(SemanticContext *ctx,
                             const SemanticNestedCallable *row)
{
    if (ctx == NULL || row == NULL || row->decl == NULL ||
        row->decl->type != AST_FUNC_DECL || row->alias_owner == NULL ||
        row->id == 0 || row->id != ast_node_stable_id(row->decl) ||
        row->alias_owner_id == 0 ||
        row->alias_owner_id != ast_node_stable_id(row->alias_owner)) return false;
    ASTNode *owner = row->alias_owner;
    ASTNode **methods = NULL;
    size_t count = 0;
    if (owner->type == AST_EXTERN_BLOCK)
        methods = ast_extern_block_declarations(owner, &count);
    else if (owner->type == AST_ROLE_DECL) {
        for (size_t i = 0; i < ast_role_impl_count(owner); i++) {
            ASTNode *impl = ast_role_impl(owner, i);
            if (impl == NULL) return false;
            if (impl->type == AST_OVERRIDE_FUNC &&
                ast_override_func_decl(impl) == row->decl) return true;
            if (impl->type == AST_IMPL_ABILITY)
                for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++)
                    if (ast_impl_ability_method(impl, j) == row->decl) return true;
        }
        return false;
    } else methods = semantic_host_decl_methods(owner, &count);
    if (count != 0 && methods == NULL) return false;
    for (size_t i = 0; i < count; i++)
        if (methods[i] == row->decl) return true;
    return false;
}

static const SemanticNestedCallable *
host_nested_callable_by_id(SemanticContext *ctx, uint32_t id)
{
    size_t low = 0, high = ctx->host_decl_index.nested_count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        const SemanticNestedCallable *row = &ctx->host_decl_index.nested_callables[mid];
        if (id < row->id) high = mid;
        else if (id > row->id) low = mid + 1;
        else return host_nested_callable_current(ctx, row) ? row : NULL;
    }
    return NULL;
}

ASTNode *
semantic_host_index_find_selected_callable(SemanticContext *ctx,
                                           SymbolKind kind,
                                           uint32_t declaration_id,
                                           const char *source_name)
{
    if (ctx == NULL || declaration_id == 0 || source_name == NULL ||
        (kind != SYMBOL_FUNCTION && kind != SYMBOL_INTENT)) return NULL;
    ASTNodeType type = kind == SYMBOL_INTENT ? AST_INTENT_DECL : AST_FUNC_DECL;
    ASTNode *decl = semantic_host_index_find_decl_by_name(ctx, type, source_name);
    if (decl != NULL && ast_node_stable_id(decl) == declaration_id) return decl;
    if (kind == SYMBOL_FUNCTION) {
        decl = semantic_host_index_find_decl_by_name(ctx, AST_EVENT_DECL, source_name);
        if (decl != NULL && ast_node_stable_id(decl) == declaration_id) return decl;
    }
    const SemanticNestedCallable *row = host_nested_callable_by_id(ctx, declaration_id);
    if (kind != SYMBOL_FUNCTION || row == NULL) return NULL;
    const char *name = ast_declaration_name(row->decl);
    ASTNode *owner = row->alias_owner;
    const char *owner_name = owner->type == AST_EXTERN_BLOCK
        ? NULL : ast_declaration_name(owner);
    if (name == NULL || (owner->type != AST_EXTERN_BLOCK && owner_name == NULL))
        return NULL;
    if (strcmp(source_name, name) == 0) {
        if (owner->type == AST_EXTERN_BLOCK || ctx->current_role_decl == owner)
            return row->decl;
        const SemanticNestedCallable *caller = ctx->current_function_decl == NULL
            ? NULL : host_nested_callable_by_id(ctx,
                ast_node_stable_id(ctx->current_function_decl));
        return caller != NULL && caller->decl == ctx->current_function_decl &&
            caller->alias_owner == owner ? row->decl : NULL;
    }
    if (owner_name == NULL) return NULL;
    size_t owner_len = strlen(owner_name), name_len = strlen(name);
    if (owner_len > SIZE_MAX - name_len - 2 ||
        strlen(source_name) != owner_len + name_len + 1 ||
        strncmp(source_name, owner_name, owner_len) != 0 ||
        (source_name[owner_len] != '_' && source_name[owner_len] != '.') ||
        strcmp(source_name + owner_len + 1, name) != 0) return NULL;
    return row->decl;
}

bool semantic_host_index_selected_call_formal(SemanticContext *ctx,
    const ASTNode *call, const ASTNode *decl, const Type *receiver_type,
    const ASTNode *caller, size_t actual, size_t *formal)
{
    if (ctx == NULL || call == NULL || call->type != AST_CALL ||
        decl == NULL || decl->type != AST_FUNC_DECL || formal == NULL ||
        ast_node_stable_id(decl) == 0 ||
        ast_call_semantic_callee_decl_id(call) != ast_node_stable_id(decl) ||
        actual > ast_call_arg_count(call)) return false;
    const ASTNode *target = ast_call_callee(call);
    if (target == NULL || (target->type != AST_IDENTIFIER &&
        target->type != AST_MEMBER_ACCESS)) return false;
    const SemanticNestedCallable *selected = host_nested_callable_by_id(ctx,
        ast_node_stable_id(decl));
    if (selected == NULL) {
        const char *name = ast_declaration_name(decl);
        if (name == NULL || semantic_host_index_find_decl_by_name(ctx,
                AST_FUNC_DECL, name) != decl) return false;
    } else if (selected->decl != decl) return false;
    size_t offset = 0;
    if (selected != NULL) {
        const ASTNode *owner = selected->alias_owner;
        const char *method_name = ast_declaration_name(decl);
        const char *owner_name = owner->type == AST_EXTERN_BLOCK
            ? NULL : ast_declaration_name(owner);
        if (method_name == NULL || (owner->type != AST_EXTERN_BLOCK &&
            owner_name == NULL)) return false;
        if (owner->type == AST_EXTERN_BLOCK) {
            const char *spelling = target->type == AST_IDENTIFIER
                ? ast_identifier_name(target) : ast_member_name(target);
            if (spelling == NULL || strcmp(spelling, method_name) != 0) return false;
        } else if (target->type == AST_MEMBER_ACCESS && receiver_type != NULL &&
            semantic_host_decl_for_type(ctx, receiver_type) == owner &&
            ast_member_name(target) != NULL &&
            strcmp(ast_member_name(target), method_name) == 0) {
            FuncParam *self = ast_func_param(decl, 0);
            offset = self != NULL && self->name != NULL &&
                strcmp(self->name, "self") == 0 && self->type == NULL ? 1 : 0;
        } else if (target->type == AST_IDENTIFIER && caller != NULL &&
            ast_identifier_name(target) != NULL &&
            strcmp(ast_identifier_name(target), method_name) == 0) {
            const SemanticNestedCallable *caller_row = host_nested_callable_by_id(ctx,
                ast_node_stable_id(caller));
            if (caller_row == NULL || caller_row->decl != caller ||
                caller_row->alias_owner != owner) return false;
            FuncParam *self = ast_func_param(decl, 0);
            offset = self != NULL && self->name != NULL &&
                strcmp(self->name, "self") == 0 && self->type == NULL ? 1 : 0;
        } else if (target->type == AST_IDENTIFIER) {
            const char *spelling = ast_identifier_name(target);
            size_t owner_length = strlen(owner_name);
            if (spelling == NULL || strlen(spelling) <= owner_length ||
                strncmp(spelling, owner_name, owner_length) != 0 ||
                spelling[owner_length] != '_' ||
                strcmp(spelling + owner_length + 1, method_name) != 0) return false;
        } else {
            const ASTNode *qualifier = ast_member_object(target);
            if (qualifier == NULL || qualifier->type != AST_IDENTIFIER ||
                ast_identifier_name(qualifier) == NULL ||
                strcmp(ast_identifier_name(qualifier), owner_name) != 0 ||
                ast_member_name(target) == NULL ||
                strcmp(ast_member_name(target), method_name) != 0) return false;
        }
    }
    if (offset > ast_func_param_count(decl) ||
        ast_call_arg_count(call) != ast_func_param_count(decl) - offset ||
        actual > SIZE_MAX - offset) return false;
    size_t index = actual + offset;
    if (actual == ast_call_arg_count(call)) {
        *formal = index;
        return true;
    }
    FuncParam *param = ast_func_param(decl, index);
    if (param == NULL || ast_func_param_stable_id(param) == 0) return false;
    *formal = index;
    return true;
}

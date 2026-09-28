#include "type_checker_internal.h"
#include "type_checker_ability_match_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "diag_codes.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static ASTNode *
find_party_role_slot(ASTNode *party_decl, const char *slot_name)
{
    size_t count;

    if (party_decl == NULL || party_decl->type != AST_PARTY_DECL
        || slot_name == NULL) {
        return NULL;
    }

    count = ast_party_role_count(party_decl);
    for (size_t i = 0; i < count; i++) {
        ASTNode *slot = ast_party_role(party_decl, i);
        const char *candidate = ast_role_slot_name(slot);
        if (candidate != NULL && strcmp(candidate, slot_name) == 0)
            return slot;
    }

    return NULL;
}

static bool
role_satisfies_party_slot(ASTNode *role_decl, ASTNode *role_slot,
                          SemanticContext *ctx,
                          const char **missing_ability_out)
{
    size_t ability_count;

    if (missing_ability_out != NULL)
        *missing_ability_out = NULL;

    if (role_decl == NULL || role_slot == NULL || ctx == NULL)
        return false;

    ability_count = ast_role_slot_required_ability_count(role_slot);
    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *ability_ref = ast_role_slot_required_ability(role_slot, i);
        if (!semantic_role_decl_has_ability(ctx, role_decl, ability_ref)) {
            if (missing_ability_out != NULL) {
                const char *ability_name = ability_ref != NULL
                    ? ast_type_name(ability_ref)
                    : NULL;
                *missing_ability_out = ability_name != NULL
                    ? ability_name
                    : "<ability>";
            }
            return false;
        }
    }

    return true;
}

static bool
semantic_role_satisfies_party_slot(SemanticContext *ctx,
                                   ASTNode *role_decl,
                                   ASTNode *role_slot,
                                   const char **missing_ability_out)
{
    if (ctx == NULL)
        return false;
    return role_satisfies_party_slot(role_decl, role_slot, ctx,
                                    missing_ability_out);
}


/* The scope that declares `name`, searching outward from the current one. */
static Scope *
bind_declaring_scope(SemanticContext *ctx, const char *name)
{
    for (Scope *s = ctx->scope; s != NULL; s = s->parent) {
        if (scope_lookup_current(s, name) != NULL)
            return s;
    }
    return NULL;
}

static bool
bind_scope_encloses(const Scope *outer, const Scope *inner)
{
    for (const Scope *s = inner; s != NULL; s = s->parent) {
        if (s == outer)
            return true;
    }
    return false;
}

static bool
bind_refuse(SemanticContext *ctx, ASTNode *node, const char *format, ...)
{
    char message[512];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_ROLE_CONTRACT_INVALID,
        PGY_CAUSE_ROLE_CONTRACT,
        PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
        node, "%s", message);
    return false;
}

/* docs/206 section 1, PartySlotBinding.v: the slot borrows the subject's
 * identity and the role supplies the witness. The subject is a local of this
 * function whose scope encloses the party's, so it outlives the party
 * (scoped_borrow_live); its type is the role's `for` target, which the role
 * body reads as `self`. Only a dyn slot is bound at run time. */
static bool
bind_admit_subject(ASTNode *node, SemanticContext *ctx,
                   Symbol *party_symbol, ASTNode *role_decl,
                   ASTNode *role_slot, const char *party_type_name)
{
    const char *party_var = ast_bind_statement_party_var(node);
    const char *slot_name = ast_bind_statement_slot_name(node);
    const char *role_name = ast_bind_statement_role_name(node);
    ASTNode *subject = ast_bind_statement_subject(node);
    const char *subject_name = ast_identifier_name(subject);
    Scope *party_scope;
    Scope *subject_scope;
    Symbol *subject_symbol;
    Type *subject_type;
    Type *for_type;

    if (!ast_role_slot_is_dynamic(role_slot)) {
        return bind_refuse(ctx, node,
            "party slot '%s.%s' is not a `dyn role slot`; only a dyn slot is bound at run time",
            party_type_name, slot_name);
    }
    party_scope = bind_declaring_scope(ctx, party_var);
    if (party_symbol->is_parameter || party_symbol->is_host_field
        || party_scope == NULL || party_scope->kind == SCOPE_GLOBAL
        || party_scope->kind == SCOPE_CLASS) {
        return bind_refuse(ctx, node,
            "bind target '%s' must be a local party binding of this function; "
            "a party borrows its subjects only while the function runs",
            party_var);
    }
    if (subject == NULL || subject->type != AST_IDENTIFIER || subject_name == NULL) {
        return bind_refuse(ctx, node,
            "bind '%s.%s' needs a local subject: `bind %s.%s = <subject> as Role;`",
            party_var, slot_name, party_var, slot_name);
    }
    subject_symbol = scope_lookup(ctx->scope, subject_name);
    subject_scope = bind_declaring_scope(ctx, subject_name);
    if (subject_symbol == NULL || subject_symbol->kind != SYMBOL_VARIABLE
        || subject_symbol->is_host_field || subject_symbol->is_parameter
        || subject_scope == NULL || subject_scope->kind == SCOPE_GLOBAL
        || subject_scope->kind == SCOPE_CLASS) {
        return bind_refuse(ctx, subject,
            "bind subject '%s' must be a local `let` binding of this function",
            subject_name);
    }
    subject_type = type_check_expression(subject, ctx);
    for_type = semantic_host_resolve_type_ref(
        semantic_role_for_type_node(role_decl), ctx);
    if (subject_type == NULL || for_type == NULL
        || !type_equals(subject_type, for_type)) {
        return bind_refuse(ctx, subject,
            "bind subject '%s' has type '%s', but role '%s' is for '%s'",
            subject_name,
            subject_type != NULL && subject_type->name != NULL
                ? subject_type->name : "<unknown>",
            role_name,
            for_type != NULL && for_type->name != NULL
                ? for_type->name : "<unknown>");
    }
    if (!bind_scope_encloses(subject_scope, party_scope)) {
        return bind_refuse(ctx, subject,
            "bind subject '%s' is declared in a scope that ends before party '%s'; "
            "declare the subject where it outlives the party",
            subject_name, party_var);
    }
    return true;
}

bool
type_check_bind_stmt(ASTNode *node, SemanticContext *ctx)
{
    const char *party_var = ast_bind_statement_party_var(node);
    const char *slot_name = ast_bind_statement_slot_name(node);
    const char *role_name = ast_bind_statement_role_name(node);
    Symbol *party_symbol;
    ASTNode *party_decl;
    ASTNode *role_slot;
    ASTNode *role_decl;
    const char *party_type_name;
    const char *missing_ability = NULL;

    if (ctx == NULL || node == NULL)
        return true;

    if (party_var == NULL || slot_name == NULL || role_name == NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_ROLE_CONTRACT_INVALID,
            PGY_CAUSE_ROLE_CONTRACT,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            node,
            "bind statement requires party variable, slot name, and role name");
        return false;
    }

    party_symbol = scope_lookup(ctx->scope, party_var);
    if (party_symbol == NULL || party_symbol->type == NULL
        || party_symbol->type->name == NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_ROLE_CONTRACT_INVALID,
            PGY_CAUSE_ROLE_CONTRACT,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            node,
            "bind target party variable '%s' is not declared in this scope",
            party_var);
        return false;
    }

    party_type_name = party_symbol->type->name;
    party_decl = semantic_find_party_decl_by_name(ctx, party_type_name);
    if (party_decl == NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_ROLE_CONTRACT_INVALID,
            PGY_CAUSE_ROLE_CONTRACT,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            node,
            "bind target '%s' has type '%s', which is not a party declaration",
            party_var, party_type_name);
        return false;
    }

    role_slot = find_party_role_slot(party_decl, slot_name);
    if (role_slot == NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_ROLE_CONTRACT_INVALID,
            PGY_CAUSE_ROLE_CONTRACT,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            node,
            "party '%s' has no role slot named '%s' for bind target '%s.%s'",
            party_type_name, slot_name, party_var, slot_name);
        return false;
    }

    if (ast_role_slot_required_ability_count(role_slot) == 0) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_ROLE_CONTRACT_INVALID,
            PGY_CAUSE_ROLE_CONTRACT,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            node,
            "party role slot '%s.%s' has no required ability for bind dispatch",
            party_type_name, slot_name);
        return false;
    }

    role_decl = semantic_find_role_decl_by_name(ctx, role_name);
    if (role_decl == NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_ROLE_CONTRACT_INVALID,
            PGY_CAUSE_ROLE_CONTRACT,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            node,
            "bind role '%s' is not declared", role_name);
        return false;
    }

    if (!semantic_role_satisfies_party_slot(ctx, role_decl, role_slot,
                                            &missing_ability)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_ROLE_CONTRACT_INVALID,
            PGY_CAUSE_ROLE_CONTRACT,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            node,
            "bind role '%s' does not satisfy party slot '%s.%s' ability contract; missing ability '%s'",
            role_name, party_type_name, slot_name,
            missing_ability != NULL ? missing_ability : "<ability>");
        return false;
    }

    if (!bind_admit_subject(node, ctx, party_symbol, role_decl, role_slot,
                            party_type_name))
        return false;
    semantic_bound_party_note_bind(ctx, party_var);

    return !ctx->has_error;
}

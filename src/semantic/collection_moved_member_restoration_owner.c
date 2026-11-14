/* A collection field moved into a local retains its exact source path. This
 * owner rejects overlapping source uses and admits only an exact move-back;
 * released inout carriers must be restored on the straight-line exit path. */
#include "collection_ownership_fact_internal.h"

#include <stdint.h>
#include <string.h>

#include "diag_codes.h"
#include "loan_copy_landing_owner.h"
#include "symbol_table.h"
#include "type_checker.h"
#include "type_checker/type_checker_internal.h"
#include "../parser/ast_api.h"

static Symbol *
collection_member_move_binding(const PgyCollectionOwnershipFact *fact,
                               SemanticContext *ctx)
{
    for (Scope *scope = ctx != NULL ? ctx->scope : NULL;
         scope != NULL;
         scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *symbol = scope->symbols[i];
            if (symbol != NULL && symbol->decl_syntax_id != 0
                && symbol->decl_syntax_id == fact->binding_syntax_id)
                return symbol;
        }
    }
    return NULL;
}

static bool
collection_member_move_from_current_parameter_mode(
    const PgyCollectionOwnershipFact *fact,
    const Symbol *binding,
    SemanticContext *ctx,
    ParamMode mode,
    size_t *formal_out)
{
    if (formal_out != NULL) *formal_out = SIZE_MAX;
    if (ctx == NULL || fact == NULL || binding == NULL
        || fact->function_syntax_id != current_function_syntax_id(ctx)
        || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
        || fact->binding_syntax_id == 0
        || binding->decl_syntax_id != fact->binding_syntax_id
        || binding->is_parameter || binding->is_host_field
        || fact->source_binding_syntax_id == 0
        || fact->source_birth_serial == 0)
        return false;
    ASTNode *function_decl = ctx->current_function_decl;
    if (function_decl == NULL || function_decl->type != AST_FUNC_DECL) return false;
    for (size_t i = 0; i < ast_func_param_count(function_decl); i++) {
        FuncParam *param = ast_func_param(function_decl, i);
        if (param == NULL || param->mode != mode
            || ast_func_param_stable_id(param) != fact->source_binding_syntax_id
            || param->name == NULL)
            continue;
        Symbol *root = scope_lookup(ctx->scope, param->name);
        bool current = root != NULL && root->is_parameter
            && root->param_mode == mode
            && root->decl_syntax_id == fact->source_binding_syntax_id
            && !root->is_consumed
            && root->birth_serial == fact->source_birth_serial;
        if (current && formal_out != NULL) *formal_out = i;
        return current;
    }
    return false;
}

bool
collection_member_move_from_current_inout_parameter(
    const PgyCollectionOwnershipFact *fact,
    const Symbol *binding,
    SemanticContext *ctx)
{
    return collection_member_move_from_current_inout_parameter_at(
        fact, binding, ctx, NULL);
}

bool
collection_member_move_from_current_inout_parameter_at(
    const PgyCollectionOwnershipFact *fact,
    const Symbol *binding,
    SemanticContext *ctx,
    size_t *formal_out)
{
    return collection_member_move_from_current_parameter_mode(
        fact, binding, ctx, PARAM_MODE_MUT_REF, formal_out);
}

bool
collection_member_move_from_current_default_parameter(
    const PgyCollectionOwnershipFact *fact,
    const Symbol *binding,
    SemanticContext *ctx,
    size_t *formal_out)
{
    return collection_member_move_from_current_parameter_mode(
        fact, binding, ctx, PARAM_MODE_DEFAULT, formal_out);
}

static bool
direct_owned_string_drop(const ASTNode *statement,
                         const ASTNode *expected_receiver)
{
    if (statement == NULL || statement->type != AST_CALL
        || ast_call_arg_count(statement) != 1)
        return false;
    ASTNode *callee = ast_call_callee(statement);
    ASTNode *receiver = ast_call_argument(statement, 0);
    const char *callee_name = callee != NULL && callee->type == AST_IDENTIFIER
        ? ast_identifier_name(callee) : NULL;
    return callee != NULL && callee->type == AST_IDENTIFIER
        && callee_name != NULL
        && strcmp(callee_name, "ArrayDropOwnedStrings") == 0
        && receiver != NULL && receiver->type == AST_IDENTIFIER
        && (expected_receiver == NULL || receiver == expected_receiver);
}

static const PgyCollectionOwnershipFact *
direct_member_restore_fact(const ASTNode *statement,
                           SemanticContext *ctx)
{
    uint32_t root_binding_id;
    uint32_t field_syntax_id;
    if (statement == NULL || statement->type != AST_ASSIGNMENT)
        return NULL;
    ASTNode *target = ast_assignment_target(statement);
    ASTNode *value = ast_assignment_value(statement);
    if (value == NULL || value->type != AST_IDENTIFIER
        || !collection_member_move_identity(target, ctx,
            &root_binding_id, &field_syntax_id))
        return NULL;
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(value));
    uint32_t binding_id = receiver_binding_syntax_id(value, binding);
    const PgyCollectionOwnershipFact *fact =
        semantic_collection_ownership_fact_find(ctx,
            current_function_syntax_id(ctx), binding_id);
    if (fact == NULL || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
        || fact->source_binding_syntax_id != root_binding_id
        || fact->origin_syntax_id != field_syntax_id
        || !collection_member_move_path_matches_fact(
            fact, target, ctx, false)
        || !collection_member_move_from_current_inout_parameter(
            fact, binding, ctx))
        return NULL;
    return fact;
}

static bool
direct_member_drop_fact(const ASTNode *statement,
                        SemanticContext *ctx)
{
    if (!direct_owned_string_drop(statement, NULL))
        return false;
    ASTNode *receiver = ast_call_argument(statement, 0);
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(receiver));
    const PgyCollectionOwnershipFact *fact =
        semantic_collection_ownership_fact_find(ctx,
            current_function_syntax_id(ctx),
            receiver_binding_syntax_id(receiver, binding));
    return fact != NULL
        && fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE
        && collection_member_move_from_current_inout_parameter(
            fact, binding, ctx);
}

bool
collection_member_move_release_path_current(
    const PgyCollectionOwnershipFact *fact,
    const Symbol *binding,
    const ASTNode *receiver,
    SemanticContext *ctx)
{
    if (!collection_member_move_from_current_inout_parameter(
            fact, binding, ctx)
        || receiver == NULL || receiver->type != AST_IDENTIFIER)
        return false;
    ASTNode *body = ast_func_body(ctx->current_function_decl);
    if (body == NULL || body->type != AST_BLOCK)
        return false;
    size_t drop_index = SIZE_MAX;
    for (size_t i = 0; i < ast_block_statement_count(body); i++) {
        ASTNode *statement = ast_block_statement(body, i);
        if (direct_owned_string_drop(statement, receiver)) {
            drop_index = i;
            break;
        }
    }
    if (drop_index == SIZE_MAX)
        return false;
    for (size_t i = drop_index + 1;
         i < ast_block_statement_count(body); i++) {
        ASTNode *statement = ast_block_statement(body, i);
        const PgyCollectionOwnershipFact *restored =
            direct_member_restore_fact(statement, ctx);
        if (restored == fact)
            return true;
        /* Several exact field carriers may be retired before their empty
         * descriptors are restored. No other statement can bridge them. */
        if (restored == NULL && !direct_member_drop_fact(statement, ctx))
            return false;
    }
    return false;
}

bool
semantic_collection_reject_moved_member_use(ASTNode *member_access,
                                             SemanticContext *ctx)
{
    PgyCollectionMemberPath path;
    const char *field_name;

    if (!collection_member_move_path_identity(member_access, ctx, &path))
        return false;
    Symbol *root = scope_lookup(ctx->scope,
        ast_identifier_name(path.root));
    uint64_t root_birth = root != NULL ? root->birth_serial : 0;
    for (Scope *scope = ctx->scope; scope != NULL;
         scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *moved = scope->symbols[i];
            if (moved == NULL || moved->is_consumed
                || moved->array_storage_member_move_source == NULL
                || !moved->has_escaped_array_storage
                || moved->array_storage_member_move_root_binding_id
                    != path.root_binding_id
                || (moved->array_storage_member_move_root_birth_serial != 0
                    && root_birth != 0
                    && moved->array_storage_member_move_root_birth_serial
                        != root_birth)
                || !collection_member_move_path_matches_source(
                    moved->array_storage_member_move_source,
                    moved->array_storage_member_move_source_id,
                    moved->array_storage_member_move_root_binding_id,
                    member_access, ctx, true))
                continue;
            if (semantic_loan_copy_landing_select_typed_carrier(ctx,
                    moved->array_storage_member_move_source, moved->type)) {
                semantic_loan_copy_landing_error_with_hints(ctx,
                    PGY_CODE_SEM_MOVE_FROM_RELEASED,
                    PGY_CAUSE_MOVE_FROM_RELEASED,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                    moved->array_storage_member_move_source,
                    "An Array field used before replacement requires an independent copy at the extraction site");
                semantic_loan_copy_landing_refusal_tail_seal(ctx);
                return false;
            }
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_MOVE_FROM_RELEASED,
                PGY_CAUSE_MOVE_FROM_RELEASED,
                PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE,
                member_access,
                "An Array field was moved into '%s' and cannot be used before exact replacement",
                moved->name != NULL ? moved->name : "<array>");
            return true;
        }
    }
    for (size_t i = 0; i < ctx->collection_ownership_fact_count; i++) {
        const PgyCollectionOwnershipFact *fact =
            &ctx->collection_ownership_facts[i];
        if (fact->function_syntax_id != current_function_syntax_id(ctx)
            || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
            || fact->disposition == PGY_COLLECTION_DISPOSITION_RETIRED
            || fact->source_binding_syntax_id != path.root_binding_id
            || !collection_member_move_path_matches_fact(
                fact, member_access, ctx, true))
            continue;
        if (fact->source_birth_serial != 0 && root_birth != 0
            && fact->source_birth_serial != root_birth)
            continue;
        /* The move fact owns the exact extraction expression.  When Clone
         * copies that field completely, land the copy at the extraction
         * rather than asking source code to spell a carrier.  This analysis
         * remains deferred; only the reloaded AST, where the initializer is
         * Clone(member), retires the member-move fact. */
        Symbol *moved = collection_member_move_binding(fact, ctx);
        if (moved != NULL && !moved->is_consumed
            && semantic_loan_copy_landing_select_typed_carrier(
                ctx, fact->member_move_source, moved->type)) {
            semantic_loan_copy_landing_error_with_hints(ctx,
                PGY_CODE_SEM_MOVE_FROM_RELEASED,
                PGY_CAUSE_MOVE_FROM_RELEASED,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                fact->member_move_source,
                "A collection field used after extraction requires an independent copy at the extraction site");
            semantic_loan_copy_landing_refusal_tail_seal(ctx);
            return false;
        }
        field_name = ast_member_name(member_access);
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MOVE_FROM_RELEASED,
            PGY_CAUSE_MOVE_FROM_RELEASED,
            PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE,
            member_access,
            "Field '%s' was moved into collection binding syntax %u and cannot be used again.\n"
            "Reason:\n"
            "- field extraction transfers the collection descriptor and its storage lifetime\n"
            "- using the source field again would recreate a shallow alias\n"
            "Fix:\n"
            "- use Clone(source.%s) when both values must remain live\n"
            "- or keep using only the moved local binding",
            field_name != NULL ? field_name : "<field>",
            fact->binding_syntax_id,
            field_name != NULL ? field_name : "<field>");
        return true;
    }
    return false;
}

Type *
semantic_collection_released_member_restore_value_type(
    ASTNode *target,
    ASTNode *value,
    SemanticContext *ctx)
{
    uint32_t root_binding_id;
    uint32_t field_syntax_id;
    uint32_t value_binding_id;
    Symbol *value_binding;

    if (value == NULL || value->type != AST_IDENTIFIER || ctx == NULL
        || !collection_member_move_identity(target, ctx,
            &root_binding_id, &field_syntax_id))
        return NULL;
    value_binding = scope_lookup(ctx->scope, ast_identifier_name(value));
    value_binding_id = receiver_binding_syntax_id(value, value_binding);
    if (value_binding == NULL || value_binding_id == 0
        || !value_binding->is_consumed)
        return NULL;
    for (size_t i = 0; i < ctx->collection_ownership_fact_count; i++) {
        const PgyCollectionOwnershipFact *fact =
            &ctx->collection_ownership_facts[i];
        if (fact->function_syntax_id != current_function_syntax_id(ctx)
            || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
            || fact->disposition !=
                PGY_COLLECTION_DISPOSITION_RELEASED_MEMBER_MOVE
            || fact->binding_syntax_id != value_binding_id
            || fact->source_binding_syntax_id != root_binding_id
            || fact->origin_syntax_id != field_syntax_id
            || !collection_member_move_path_matches_fact(
                fact, target, ctx, false)
            || !collection_member_move_from_current_inout_parameter(
                fact, value_binding, ctx))
            continue;
        return value_binding->type;
    }
    return NULL;
}

bool
semantic_collection_restore_moved_member(ASTNode *target,
                                         ASTNode *value,
                                         SemanticContext *ctx)
{
    uint32_t root_binding_id;
    uint32_t field_syntax_id;
    uint32_t value_binding_id;
    Symbol *value_binding;

    if (value == NULL || value->type != AST_IDENTIFIER || ctx == NULL
        || !collection_member_move_identity(target, ctx,
            &root_binding_id, &field_syntax_id))
        return false;
    value_binding = scope_lookup(ctx->scope, ast_identifier_name(value));
    value_binding_id = receiver_binding_syntax_id(value, value_binding);
    if (value_binding == NULL || value_binding_id == 0)
        return false;

    for (size_t i = 0; i < ctx->collection_ownership_fact_count; i++) {
        PgyCollectionOwnershipFact *fact =
            &ctx->collection_ownership_facts[i];
        bool released = fact->disposition ==
            PGY_COLLECTION_DISPOSITION_RELEASED_MEMBER_MOVE;
        if (fact->function_syntax_id != current_function_syntax_id(ctx)
            || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
            || (fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
                && !released)
            || fact->binding_syntax_id != value_binding_id
            || fact->source_binding_syntax_id != root_binding_id
            || fact->origin_syntax_id != field_syntax_id
            || !collection_member_move_path_matches_fact(
                fact, target, ctx, false)
            || (released
                ? (!value_binding->is_consumed
                    || !collection_member_move_from_current_inout_parameter(
                        fact, value_binding, ctx))
                : value_binding->is_consumed))
            continue;
        fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
        value_binding->is_consumed = true;
        return true;
    }
    return false;
}

bool
semantic_collection_require_no_released_member_move(
    ASTNode *function_decl,
    SemanticContext *ctx)
{
    if (ctx == NULL || function_decl == NULL)
        return false;
    uint32_t function_id = ast_node_stable_id(function_decl);
    if (function_id == 0)
        return false;
    ASTNode *first_copy = NULL;
    for (Scope *scope = ctx->scope; scope != NULL;
         scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *moved = scope->symbols[i];
            if (moved == NULL || moved->is_consumed
                || moved->array_storage_member_move_source == NULL
                || !moved->has_escaped_array_storage)
                continue;
            if (!ctx->has_error
                && semantic_loan_copy_landing_select_typed_carrier(ctx,
                    moved->array_storage_member_move_source, moved->type)) {
                if (first_copy == NULL)
                    first_copy = (ASTNode *)moved->array_storage_member_move_source;
                continue;
            } else if (!ctx->has_error) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                    moved->array_storage_member_move_source,
                    "An inout Array field extraction was not replaced before function exit");
            }
            return false;
        }
    }
    if (first_copy != NULL) {
        /* Each selected extraction is independently cloneable and the
         * current result remains unusable until the driver reloads the AST
         * with every Clone.  Batch the function-exit obligations so sibling
         * fields do not require one whole-program analysis apiece. */
        semantic_loan_copy_landing_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            first_copy,
            "An inout Array field extraction requires exact fresh replacement or an independent copy");
        semantic_loan_copy_landing_refusal_tail_seal(ctx);
        return false;
    }
    for (size_t i = 0; i < ctx->collection_ownership_fact_count; i++) {
        const PgyCollectionOwnershipFact *fact =
            &ctx->collection_ownership_facts[i];
        if (fact->function_syntax_id != function_id
            || fact->disposition !=
                PGY_COLLECTION_DISPOSITION_RELEASED_MEMBER_MOVE)
            continue;
        if (!ctx->has_error)
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                function_decl,
                "Released aggregate field carrier syntax %u was not restored to source field syntax %u",
                fact->binding_syntax_id, fact->origin_syntax_id);
        return false;
    }
    return true;
}

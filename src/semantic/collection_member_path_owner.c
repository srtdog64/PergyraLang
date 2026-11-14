/* Stable root-to-leaf identities for collection fields moved out of nominal
 * values. This owner is shared by semantic transfer checks and MIR carriage;
 * it never grants ownership from a matching field name alone. */
#include "collection_ownership_fact_internal.h"

#include <string.h>
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_release_owner.h"
#include "default_aggregate_move_rule.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "symbol_table.h"
#include "type_checker/type_checker_internal.h"
#include "../parser/ast_api.h"

bool
collection_member_move_path_identity(const ASTNode *member_access,
                                     SemanticContext *ctx,
                                     PgyCollectionMemberPath *out)
{
    const ASTNode *members[PGY_COLLECTION_MEMBER_PATH_LIMIT];
    const ASTNode *root;
    Symbol *binding;
    Type *value_type;
    size_t member_count = 0;

    if (out != NULL)
        memset(out, 0, sizeof(*out));
    if (member_access == NULL || member_access->type != AST_MEMBER_ACCESS
        || ctx == NULL || out == NULL)
        return false;
    root = member_access;
    while (root != NULL && root->type == AST_MEMBER_ACCESS) {
        if (member_count == PGY_COLLECTION_MEMBER_PATH_LIMIT)
            return false;
        members[member_count++] = root;
        root = ast_member_object(root);
    }
    if (root == NULL || root->type != AST_IDENTIFIER)
        return false;
    binding = scope_lookup(ctx->scope, ast_identifier_name(root));
    out->root_binding_id = receiver_binding_syntax_id((ASTNode *)root, binding);
    out->root = root;
    value_type = binding != NULL ? binding->type : NULL;
    out->root_type = value_type;
    if (out->root_binding_id == 0 || value_type == NULL || member_count == 0)
        return false;
    for (size_t depth = member_count; depth > 0; depth--) {
        const ASTNode *member = members[depth - 1];
        const char *field_name = ast_member_name(member);
        ASTNode *decl = semantic_host_decl_for_type(ctx, value_type);
        bool found = false;
        if (field_name == NULL || decl == NULL || decl->type != AST_CLASS_DECL)
            return false;
        size_t field_count = projection_source_field_count(decl);
        out->leaf_receiver_type = value_type;
        for (size_t i = 0; i < field_count; i++) {
            PgyDeclField field = projection_source_field_at(decl, i);
            if (field.name == NULL || strcmp(field.name, field_name) != 0)
                continue;
            if (found || field.declaration_syntax_id == 0
                || field.type_ast == NULL)
                return false;
            out->field_ids[out->field_count++] = field.declaration_syntax_id;
            value_type = projection_resolve_type_ref(field.type_ast, ctx);
            found = value_type != NULL && value_type != TYPE_UNKNOWN;
        }
        if (!found)
            return false;
    }
    out->result_type = value_type;
    return out->field_count == member_count;
}

static Symbol *
collection_member_binding_by_id(SemanticContext *ctx, uint32_t binding_id)
{
    for (Scope *scope = ctx != NULL ? ctx->scope : NULL;
         scope != NULL;
         scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *symbol = scope->symbols[i];
            if (symbol != NULL && symbol->decl_syntax_id == binding_id)
                return symbol;
        }
    }
    return NULL;
}

static bool
collection_pending_value_current(SemanticContext *ctx, ASTNode *function,
                                 const ASTNode *value,
                                 PublishedNode **row_out)
{
    if (row_out != NULL) *row_out = NULL;
    if (ctx == NULL || function == NULL || value == NULL
        || !semantic_param_publication_pending(ctx))
        return false;
    PublishedNode *row = publication_node(
        ctx->function_param_publications, value, false);
    bool current = row != NULL && row->node == value
        && row->id != 0 && row->id == ast_node_stable_id(value)
        && row->value_present && !row->meta
        && row->value_shape == value->type
        && row->value_type != NULL && row->value_type != TYPE_UNKNOWN
        && row->caller == function && row->caller_id != 0
        && row->caller_id == ast_node_stable_id(function)
        && (value->type != AST_IDENTIFIER
            || (row->binding_id != 0
                && row->binding_id
                    == ast_identifier_binding_syntax_id(value)));
    if (current && row_out != NULL) *row_out = row;
    return current;
}

static bool
collection_pending_member_path_current(SemanticContext *ctx,
                                       ASTNode *function,
                                       const ASTNode *value,
                                       PgyCollectionMemberPath *out)
{
    PgyCollectionMemberPath path;
    if (!collection_member_move_path_identity(value, ctx, &path))
        return false;
    PublishedNode *row = NULL;
    PublishedNode *root_row = NULL;
    const ASTNode *receiver = ast_member_object(value);
    if (path.field_count == 0 || receiver == NULL
        || !collection_pending_value_current(ctx, function, value, &row)
        || !collection_pending_value_current(
            ctx, function, path.root, &root_row)
        || root_row->binding_id != path.root_binding_id
        || !type_equals(root_row->value_type, path.root_type))
        return false;
    const PublishedProjection *projection = &row->projection;
    if (!projection->present || projection->member != value
        || projection->member_id != ast_node_stable_id(value)
        || projection->receiver != receiver
        || projection->receiver_id != ast_node_stable_id(receiver)
        || projection->root != path.root
        || projection->root_id != ast_node_stable_id(path.root)
        || projection->root_binding_id != path.root_binding_id
        || projection->caller != function
        || projection->caller_id != ast_node_stable_id(function)
        || projection->field_id
            != path.field_ids[path.field_count - 1]
        || !type_equals(
            projection->receiver_type, path.leaf_receiver_type)
        || !type_equals(projection->result_type, path.result_type)
        || !type_equals(projection->field_type, path.result_type)
        || !type_equals(row->value_type, path.result_type))
        return false;
    if (out != NULL) *out = path;
    return true;
}

bool
collection_published_typed_member_path_current(
    const ASTNode *member_access, SemanticContext *ctx,
    ASTNode **function_out, PgyCollectionMemberPath *path_out)
{
    const ASTNode *members[PGY_COLLECTION_MEMBER_PATH_LIMIT];
    const ASTNode *root = member_access;
    PgyCollectionMemberPath path = {0};
    FunctionParamPublicationStore *store = ctx != NULL
        ? ctx->function_param_publications : NULL;
    PublishedNode *root_row;
    ASTNode *function;
    const Type *value_type;
    size_t member_count = 0;

    if (function_out != NULL) *function_out = NULL;
    if (path_out != NULL) memset(path_out, 0, sizeof(*path_out));
    if (ctx == NULL || store == NULL || path_out == NULL
        || member_access == NULL || member_access->type != AST_MEMBER_ACCESS
        || !semantic_param_publication_inputs_ready(ctx))
        return false;
    while (root != NULL && root->type == AST_MEMBER_ACCESS) {
        if (member_count == PGY_COLLECTION_MEMBER_PATH_LIMIT)
            return false;
        members[member_count++] = root;
        root = ast_member_object(root);
    }
    if (root == NULL || root->type != AST_IDENTIFIER || member_count == 0)
        return false;
    root_row = publication_node(store, root, false);
    function = root_row != NULL ? (ASTNode *)root_row->caller : NULL;
    path.root = root;
    path.root_binding_id = ast_identifier_binding_syntax_id(root);
    path.root_type = root_row != NULL ? root_row->value_type : NULL;
    if (root_row == NULL || function == NULL
        || function->type != AST_FUNC_DECL || path.root_binding_id == 0
        || root_row->node != root || root_row->id != ast_node_stable_id(root)
        || !root_row->value_present || root_row->meta
        || root_row->binding_id != path.root_binding_id
        || root_row->caller_id != ast_node_stable_id(function)
        || path.root_type == NULL || path.root_type == TYPE_UNKNOWN)
        return false;
    value_type = path.root_type;
    for (size_t depth = member_count; depth > 0; depth--) {
        const ASTNode *member = members[depth - 1];
        const ASTNode *receiver = ast_member_object(member);
        PublishedNode *row = publication_node(store, member, false);
        const PublishedProjection *projection = row != NULL
            ? &row->projection : NULL;
        if (row == NULL || projection == NULL || !row->value_present
            || row->meta || row->node != member
            || row->id != ast_node_stable_id(member)
            || row->caller != function
            || row->caller_id != ast_node_stable_id(function)
            || !projection->present || projection->member != member
            || projection->member_id != ast_node_stable_id(member)
            || projection->receiver != receiver
            || projection->receiver_id != ast_node_stable_id(receiver)
            || projection->root != root
            || projection->root_id != ast_node_stable_id(root)
            || projection->root_binding_id != path.root_binding_id
            || projection->caller != function
            || projection->caller_id != ast_node_stable_id(function)
            || projection->field_id == 0
            || !type_equals(projection->receiver_type, value_type)
            || !type_equals(projection->field_type, row->value_type)
            || !type_equals(projection->result_type, row->value_type)
            || row->value_type == NULL || row->value_type == TYPE_UNKNOWN)
            return false;
        path.leaf_receiver_type = value_type;
        path.field_ids[path.field_count++] = projection->field_id;
        value_type = row->value_type;
    }
    path.result_type = value_type;
    if (path.field_count != member_count)
        return false;
    if (function_out != NULL) *function_out = function;
    *path_out = path;
    return true;
}

bool
collection_published_parameter_member_path(
    const ASTNode *member_access, SemanticContext *ctx,
    ASTNode **function_out, size_t *formal_out, ParamMode *mode_out,
    PgyCollectionMemberPath *path_out)
{
    ASTNode *function = NULL;
    PgyCollectionMemberPath path;
    size_t found = SIZE_MAX;
    ParamMode found_mode = PARAM_MODE_DEFAULT;

    if (function_out != NULL) *function_out = NULL;
    if (formal_out != NULL) *formal_out = SIZE_MAX;
    if (mode_out != NULL) *mode_out = PARAM_MODE_DEFAULT;
    if (path_out != NULL) memset(path_out, 0, sizeof(*path_out));
    if (path_out == NULL
        || !collection_published_typed_member_path_current(
            member_access, ctx, &function, &path)
        || !collection_array_string_type(path.result_type))
        return false;
    for (size_t i = 0; i < ast_func_param_count(function); i++) {
        FuncParam *formal = ast_func_param(function, i);
        if (formal == NULL
            || ast_func_param_stable_id(formal) != path.root_binding_id)
            continue;
        if (found != SIZE_MAX
            || (formal->mode != PARAM_MODE_DEFAULT
                && formal->mode != PARAM_MODE_MUT_REF))
            return false;
        found = i;
        found_mode = formal->mode;
    }
    if (found == SIZE_MAX)
        return false;
    if (function_out != NULL) *function_out = function;
    if (formal_out != NULL) *formal_out = found;
    if (mode_out != NULL) *mode_out = found_mode;
    *path_out = path;
    return true;
}

bool
collection_published_local_member_path(
    const ASTNode *member_access, SemanticContext *ctx,
    ASTNode **function_out, PgyCollectionMemberPath *path_out)
{
    ASTNode *function = NULL;
    PgyCollectionMemberPath path;
    FunctionParamPublicationStore *store = ctx != NULL
        ? ctx->function_param_publications : NULL;
    PublishedNode *definition;
    ASTNode *declaration;

    if (function_out != NULL) *function_out = NULL;
    if (path_out != NULL) memset(path_out, 0, sizeof(*path_out));
    if (store == NULL || path_out == NULL
        || !collection_published_typed_member_path_current(
            member_access, ctx, &function, &path)
        || !collection_array_string_type(path.result_type))
        return false;
    /* This identifies a typed place. The member-requirement owner separately
     * proves its initializer and every replacement before granting cleanup. */
    definition = publication_node_by_id(store, path.root_binding_id);
    declaration = definition != NULL ? (ASTNode *)definition->node : NULL;
    if (declaration == NULL || declaration->type != AST_LET_DECL
        || ast_node_stable_id(declaration) != path.root_binding_id
        || ast_let_is_alias(declaration)
        || ast_let_initializer(declaration) == NULL
        || path.root_type == NULL || path.root_type == TYPE_UNKNOWN
        || !semantic_default_aggregate_value(ctx, path.root_type)
        || type_is_constructed_named(path.root_type, "Array"))
        return false;
    if (function_out != NULL) *function_out = function;
    *path_out = path;
    return true;
}

bool
collection_current_parameter_member_path(
    const ASTNode *member_access, SemanticContext *ctx,
    size_t *formal_out, ParamMode *mode_out,
    PgyCollectionMemberPath *path_out)
{
    ASTNode *function = ctx != NULL ? ctx->current_function_decl : NULL;
    PgyCollectionMemberPath path;
    Symbol *root;
    size_t formal = SIZE_MAX;
    ParamMode mode = PARAM_MODE_DEFAULT;

    if (formal_out != NULL) *formal_out = SIZE_MAX;
    if (mode_out != NULL) *mode_out = PARAM_MODE_DEFAULT;
    if (path_out != NULL) memset(path_out, 0, sizeof(*path_out));
    if (ctx == NULL || function == NULL || function->type != AST_FUNC_DECL
        || path_out == NULL || !semantic_param_publication_pending(ctx)
        || !collection_pending_member_path_current(
            ctx, function, member_access, &path)
        || path.root == NULL || path.root->type != AST_IDENTIFIER
        || path.root_binding_id == 0 || path.field_count == 0
        || !collection_array_string_type(path.result_type))
        return false;
    root = collection_member_binding_by_id(ctx, path.root_binding_id);
    if (root == NULL || !root->is_parameter || root->is_host_field
        || root->is_consumed)
        return false;
    for (size_t i = 0; i < ast_func_param_count(function); i++) {
        FuncParam *candidate = ast_func_param(function, i);
        if (candidate == NULL
            || ast_func_param_stable_id(candidate) != path.root_binding_id)
            continue;
        if (formal != SIZE_MAX
            || (candidate->mode != PARAM_MODE_DEFAULT
                && candidate->mode != PARAM_MODE_MUT_REF))
            return false;
        formal = i;
        mode = candidate->mode;
    }
    if (formal == SIZE_MAX || root->param_mode != mode)
        return false;
    if (formal_out != NULL) *formal_out = formal;
    if (mode_out != NULL) *mode_out = mode;
    *path_out = path;
    return true;
}

bool
collection_current_local_member_path(
    const ASTNode *member_access, SemanticContext *ctx,
    PgyCollectionMemberPath *path_out)
{
    ASTNode *function = ctx != NULL ? ctx->current_function_decl : NULL;
    PgyCollectionMemberPath path;
    Symbol *root;
    FunctionParamPublicationStore *store = ctx != NULL
        ? ctx->function_param_publications : NULL;
    PublishedNode *definition;
    ASTNode *declaration;

    if (path_out != NULL) memset(path_out, 0, sizeof(*path_out));
    if (ctx == NULL || function == NULL || function->type != AST_FUNC_DECL
        || store == NULL || path_out == NULL
        || !semantic_param_publication_pending(ctx)
        || !collection_pending_member_path_current(
            ctx, function, member_access, &path)
        || path.root == NULL || path.root->type != AST_IDENTIFIER
        || path.root_binding_id == 0 || path.field_count == 0
        || !collection_array_string_type(path.result_type))
        return false;
    root = collection_member_binding_by_id(ctx, path.root_binding_id);
    definition = publication_node_by_id(store, path.root_binding_id);
    declaration = definition != NULL ? (ASTNode *)definition->node : NULL;
    if (root == NULL || root->is_parameter || root->is_host_field
        || root->is_consumed
        || !semantic_default_aggregate_value(ctx, root->type)
        || type_is_constructed_named(root->type, "Array")
        || declaration == NULL || declaration->type != AST_LET_DECL
        || ast_node_stable_id(declaration) != path.root_binding_id
        || ast_let_is_alias(declaration)
        || ast_let_initializer(declaration) == NULL
        || !type_equals(root->type, path.root_type))
        return false;
    *path_out = path;
    return true;
}

static bool
collection_member_move_parameter_path(
    const PgyCollectionOwnershipFact *fact, const Symbol *binding,
    SemanticContext *ctx, size_t *formal_out, ParamMode *mode_out,
    uint32_t *field_ids_out, size_t *field_count_out,
    bool allow_pending_call)
{
    if (formal_out != NULL) *formal_out = SIZE_MAX;
    if (mode_out != NULL) *mode_out = PARAM_MODE_DEFAULT;
    if (field_count_out != NULL) *field_count_out = 0;
    ASTNode *function = ctx != NULL ? ctx->current_function_decl : NULL;
    PgyCollectionMemberPath path;
    bool traced_path = ctx != NULL && fact != NULL && function != NULL
        && fact->member_move_source != NULL
        && collection_member_move_path_identity(
            fact->member_move_source, ctx, &path);
    if (ctx == NULL || fact == NULL || binding == NULL
        || field_ids_out == NULL || field_count_out == NULL
        || function == NULL || function->type != AST_FUNC_DECL
        || !semantic_param_publication_pending(ctx)
        || fact->function_syntax_id != ast_node_stable_id(function)
        || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
        || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
        || !fact->member_move_elements_current
        || fact->binding_syntax_id == 0
        || binding->decl_syntax_id != fact->binding_syntax_id
        || binding->is_parameter || binding->is_host_field
        || binding->is_consumed
        || (!allow_pending_call
            && semantic_array_storage_has_pending(binding))
        || fact->member_move_source == NULL
        || fact->member_move_source_id == 0
        || fact->member_move_source_id
            != ast_node_stable_id(fact->member_move_source)
        || fact->source_birth_serial == 0
        || !traced_path
        || path.root_binding_id != fact->source_binding_syntax_id
        || path.field_count == 0
        || path.field_ids[path.field_count - 1]
            != fact->origin_syntax_id)
        return false;
    Symbol *immediate = collection_member_binding_by_id(
        ctx, path.root_binding_id);
    if (immediate == NULL || immediate->is_consumed
        || immediate->birth_serial != fact->source_birth_serial)
        return false;
    memcpy(field_ids_out, path.field_ids,
        path.field_count * sizeof(*field_ids_out));
    *field_count_out = path.field_count;
    uint32_t root_binding_id = path.root_binding_id;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    for (size_t depth = 0; depth <= store->node_count; depth++) {
        size_t found = SIZE_MAX;
        ParamMode found_mode = PARAM_MODE_DEFAULT;
        for (size_t i = 0; i < ast_func_param_count(function); i++) {
            FuncParam *formal = ast_func_param(function, i);
            if (formal == NULL
                || ast_func_param_stable_id(formal) != root_binding_id)
                continue;
            if (found != SIZE_MAX
                || (formal->mode != PARAM_MODE_DEFAULT
                    && formal->mode != PARAM_MODE_MUT_REF))
                return false;
            found = i;
            found_mode = formal->mode;
        }
        if (found != SIZE_MAX) {
            Symbol *root = collection_member_binding_by_id(
                ctx, root_binding_id);
            if (root == NULL || !root->is_parameter || root->is_consumed
                || root->param_mode != found_mode)
                return false;
            if (formal_out != NULL) *formal_out = found;
            if (mode_out != NULL) *mode_out = found_mode;
            return true;
        }
        Symbol *local = collection_member_binding_by_id(
            ctx, root_binding_id);
        PublishedNode *definition = publication_node_by_id(
            store, root_binding_id);
        ASTNode *declaration = definition != NULL
            ? (ASTNode *)definition->node : NULL;
        ASTNode *initializer = declaration != NULL
            && declaration->type == AST_LET_DECL
            ? ast_let_initializer(declaration) : NULL;
        PublishedNode *initializer_row = NULL;
        bool initializer_current = initializer != NULL
            && collection_pending_value_current(
                ctx, function, initializer, &initializer_row);
        if (local == NULL || local->is_parameter || local->is_host_field
            || local->is_mut_binding || declaration == NULL
            || ast_node_stable_id(declaration) != root_binding_id
            || ast_let_is_mutable(declaration) || ast_let_is_alias(declaration)
            || initializer == NULL
            || !initializer_current
            || !type_equals(local->type, initializer_row->value_type)
            || !semantic_default_aggregate_value(ctx, local->type)
            || type_is_constructed_named(local->type, "Array"))
            return false;
        if (initializer->type == AST_IDENTIFIER) {
            root_binding_id = ast_identifier_binding_syntax_id(initializer);
            if (root_binding_id == 0) return false;
            continue;
        }
        PgyCollectionMemberPath prefix;
        bool prefix_current = initializer->type == AST_MEMBER_ACCESS
            && collection_pending_member_path_current(
                ctx, function, initializer, &prefix);
        if (initializer->type != AST_MEMBER_ACCESS
            || !prefix_current
            || prefix.field_count == 0
            || prefix.field_count + *field_count_out
                > PGY_COLLECTION_MEMBER_PATH_LIMIT)
            return false;
        memmove(field_ids_out + prefix.field_count, field_ids_out,
            *field_count_out * sizeof(*field_ids_out));
        memcpy(field_ids_out, prefix.field_ids,
            prefix.field_count * sizeof(*field_ids_out));
        *field_count_out += prefix.field_count;
        root_binding_id = prefix.root_binding_id;
    }
    return false;
}

bool
collection_member_move_current_parameter_path(
    const PgyCollectionOwnershipFact *fact, const Symbol *binding,
    SemanticContext *ctx, size_t *formal_out, ParamMode *mode_out,
    uint32_t *field_ids_out, size_t *field_count_out)
{
    return collection_member_move_parameter_path(
        fact, binding, ctx, formal_out, mode_out,
        field_ids_out, field_count_out, false);
}

bool
collection_member_move_call_entry_parameter_path(
    const PgyCollectionOwnershipFact *fact, const Symbol *binding,
    SemanticContext *ctx, size_t *formal_out, ParamMode *mode_out,
    uint32_t *field_ids_out, size_t *field_count_out)
{
    return collection_member_move_parameter_path(
        fact, binding, ctx, formal_out, mode_out,
        field_ids_out, field_count_out, true);
}

bool
collection_member_move_pending_call_parameter_path(
    const PgyCollectionOwnershipFact *fact, const Symbol *binding,
    SemanticContext *ctx, const ASTNode *call, const ASTNode *callee_decl,
    size_t argument_ordinal, size_t target_formal_ordinal,
    size_t *formal_out, ParamMode *mode_out,
    uint32_t *field_ids_out, size_t *field_count_out,
    size_t *pending_sequence_out)
{
    size_t sequence = SIZE_MAX;
    if (!semantic_array_storage_exact_pending_call_identity(ctx, binding,
            call, callee_decl, argument_ordinal, target_formal_ordinal,
            &sequence)
        || !collection_member_move_parameter_path(
            fact, binding, ctx, formal_out, mode_out,
            field_ids_out, field_count_out, true))
        return false;
    if (pending_sequence_out != NULL)
        *pending_sequence_out = sequence;
    return true;
}

bool
collection_member_move_identity(const ASTNode *member_access,
                                SemanticContext *ctx,
                                uint32_t *root_binding_id_out,
                                uint32_t *field_syntax_id_out)
{
    PgyCollectionMemberPath path;
    if (root_binding_id_out != NULL) *root_binding_id_out = 0;
    if (field_syntax_id_out != NULL) *field_syntax_id_out = 0;
    if (!collection_member_move_path_identity(member_access, ctx, &path))
        return false;
    if (root_binding_id_out != NULL) *root_binding_id_out = path.root_binding_id;
    if (field_syntax_id_out != NULL)
        *field_syntax_id_out = path.field_ids[path.field_count - 1];
    return true;
}

static bool
collection_member_path_prefix(const PgyCollectionMemberPath *prefix,
                              const PgyCollectionMemberPath *path)
{
    if (prefix->root_binding_id != path->root_binding_id
        || prefix->field_count > path->field_count)
        return false;
    for (size_t i = 0; i < prefix->field_count; i++)
        if (prefix->field_ids[i] != path->field_ids[i])
            return false;
    return true;
}

bool
collection_member_move_path_matches_fact(
    const PgyCollectionOwnershipFact *fact, const ASTNode *member_access,
    SemanticContext *ctx, bool overlap)
{
    if (fact == NULL || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
        || fact->member_move_source == NULL || fact->member_move_source_id == 0
        || !collection_member_move_path_matches_source(
            fact->member_move_source, fact->member_move_source_id,
            fact->source_binding_syntax_id, member_access, ctx, overlap))
        return false;
    PgyCollectionMemberPath source;
    return collection_member_move_path_identity(
            fact->member_move_source, ctx, &source)
        && source.field_ids[source.field_count - 1] == fact->origin_syntax_id;
}

bool
collection_member_move_path_matches_source(
    const ASTNode *source_node, uint32_t source_id, uint32_t root_binding_id,
    const ASTNode *member_access, SemanticContext *ctx, bool overlap)
{
    PgyCollectionMemberPath source, candidate;
    if (source_node == NULL || source_id == 0 || root_binding_id == 0
        || ast_node_stable_id(source_node) != source_id
        || !collection_member_move_path_identity(source_node, ctx, &source)
        || !collection_member_move_path_identity(member_access, ctx, &candidate)
        || source.root_binding_id != root_binding_id)
        return false;
    if (overlap)
        return collection_member_path_prefix(&source, &candidate)
            || collection_member_path_prefix(&candidate, &source);
    return source.field_count == candidate.field_count
        && collection_member_path_prefix(&source, &candidate);
}

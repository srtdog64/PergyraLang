/* Stable root-to-leaf identities for collection fields moved out of nominal
 * values. This owner is shared by semantic transfer checks and MIR carriage;
 * it never grants ownership from a matching field name alone. */
#include "collection_ownership_fact_internal.h"

#include <string.h>

#include "symbol_table.h"
#include "type_checker_internal.h"
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
    return out->field_count == member_count;
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

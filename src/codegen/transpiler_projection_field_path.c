/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend projection field-path helpers.
 */

#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_projection_field_path.h"

typedef struct
{
    bool exists;
    bool subject_like;
    const char *type_name;
} TranspilerProjectionFieldInfo;

static const char *
projection_field_type_name(TranspilerCtx *ctx,
                           const char *host_type_name,
                           const TranspilerHostedFieldView *view,
                           size_t index)
{
    const MIRDeclField *field;
    const char *type_name;
    ASTNode *type_node;

    field = transpiler_hosted_field_view_metadata(view, index);
    type_name = transpiler_mir_decl_field_type_name(field);
    if (type_name != NULL && type_name[0] != '\0')
        return type_name;
    if (transpiler_active_has_mir(ctx)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing projection class-field type-name metadata for '%s' index %zu",
            host_type_name != NULL ? host_type_name : "(anonymous-class)",
            index);
        return NULL;
    }
    type_node = transpiler_hosted_field_view_type(view, index);
    return type_node != NULL ? ast_type_name(type_node) : NULL;
}

static TranspilerProjectionFieldInfo
host_projection_class_field_info(TranspilerCtx *ctx,
                                 ASTNode *host_decl,
                                 const char *host_type_name,
                                 const char *field_name)
{
    TranspilerProjectionFieldInfo info = {0};
    TranspilerHostedFieldView field_view;
    size_t field_index = 0;

    if (ctx == NULL || host_decl == NULL || host_decl->type != AST_CLASS_DECL
        || host_type_name == NULL || field_name == NULL)
        return info;

    field_view = transpiler_hosted_class_field_view_from_decl(
        ctx, host_type_name, host_decl);
    if (transpiler_hosted_field_view_missing_mir_metadata(&field_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing projection class-field metadata for '%s'",
            host_type_name);
        return info;
    }
    if (!transpiler_hosted_field_view_find_index(
            &field_view, field_name, &field_index)) {
        return info;
    }

    info.exists = true;
    info.subject_like =
        transpiler_hosted_field_view_is_subject_like(&field_view, field_index);
    info.type_name = projection_field_type_name(
        ctx, host_type_name, &field_view, field_index);
    return info;
}

const char *
assignment_target_root_slot_name(ASTNode *target)
{
    if (target == NULL)
        return NULL;
    if (target->type == AST_IDENTIFIER)
        return ast_identifier_name(target);
    if (target->type == AST_MEMBER_ACCESS) {
        ASTNode *member_object = ast_member_object(target);
        if (member_object != NULL
            && member_object->type == AST_IDENTIFIER
            && ast_identifier_name(member_object) != NULL
            && strcmp(ast_identifier_name(member_object), "self") == 0) {
            return ast_member_name(target);
        }
        return assignment_target_root_slot_name(member_object);
    }
    return NULL;
}

const char *
assignment_target_root_subfield_name(ASTNode *target)
{
    if (target == NULL || target->type != AST_MEMBER_ACCESS)
        return NULL;

    if (ast_member_object(target) != NULL
        && ast_member_object(target)->type == AST_MEMBER_ACCESS) {
        ASTNode *inner = ast_member_object(target);
        ASTNode *inner_object = ast_member_object(inner);
        if (inner_object != NULL
            && inner_object->type == AST_IDENTIFIER
            && ast_identifier_name(inner_object) != NULL
            && strcmp(ast_identifier_name(inner_object), "self") == 0) {
            return ast_member_name(target);
        }
        return assignment_target_root_subfield_name(inner);
    }

    return NULL;
}

bool
host_projection_relevant_field_exists(TranspilerCtx *ctx,
                                      const char *host_type_name,
                                      const char *field_name)
{
    ASTNode *host_decl;

    if (ctx == NULL || host_type_name == NULL || field_name == NULL)
        return false;

    host_decl = transpiler_find_projection_nominal_decl_local(
        ctx, host_type_name);
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL)
        return false;

    {
        const MIRDeclField *mir_field =
            transpiler_find_decl_field_metadata(ctx, host_type_name,
                                                field_name);
        if (mir_field != NULL)
            return !transpiler_mir_decl_field_is_subject_like(mir_field);
    }
    {
        TranspilerProjectionFieldInfo info =
            host_projection_class_field_info(ctx, host_decl,
                host_type_name, field_name);
        return info.exists && !info.subject_like;
    }
}

const char *
host_projection_subject_field_type_name(TranspilerCtx *ctx,
                                        const char *host_type_name,
                                        const char *field_name)
{
    ASTNode *host_decl;
    TranspilerProjectionFieldInfo info;

    if (ctx == NULL || host_type_name == NULL || field_name == NULL)
        return NULL;

    host_decl = transpiler_find_projection_nominal_decl_local(
        ctx, host_type_name);
    info = host_projection_class_field_info(ctx, host_decl,
        host_type_name, field_name);
    return info.exists && info.subject_like ? info.type_name : NULL;
}

const char *
method_projection_write_field_name(TranspilerCtx *ctx,
                                   const char *host_type_name,
                                   const char *root_name,
                                   const char *member_name)
{
    ASTNode *host_decl;

    if (ctx == NULL || host_type_name == NULL || root_name == NULL)
        return NULL;

    host_decl = transpiler_find_projection_nominal_decl_local(
        ctx, host_type_name);
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL)
        return NULL;

    if (member_name == NULL
        && host_projection_relevant_field_exists(
            ctx, host_type_name, root_name)) {
        return root_name;
    }

    {
        const MIRDeclField *mir_field =
            transpiler_find_decl_field_metadata(ctx, host_type_name,
                                                root_name);
        if (mir_field != NULL) {
            if (transpiler_mir_decl_field_is_subject_like(mir_field))
                return member_name;
            return root_name;
        }
    }

    {
        TranspilerProjectionFieldInfo info =
            host_projection_class_field_info(ctx, host_decl,
                host_type_name, root_name);
        if (info.exists) {
            if (info.subject_like)
                return member_name;
            return root_name;
        }
    }

    return NULL;
}

const char *
method_assignment_projection_field_name(TranspilerCtx *ctx,
                                        const char *host_type_name,
                                        ASTNode *target)
{
    ASTNode *cursor = target;
    ASTNode *host_decl;

    if (ctx == NULL || host_type_name == NULL || target == NULL)
        return NULL;

    host_decl = transpiler_find_projection_nominal_decl_local(
        ctx, host_type_name);
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL)
        return NULL;

    if (target->type == AST_IDENTIFIER
        && host_projection_relevant_field_exists(ctx, host_type_name,
            ast_identifier_name(target))) {
        return ast_identifier_name(target);
    }

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        ASTNode *obj = ast_member_object(cursor);
        if (obj != NULL && obj->type == AST_IDENTIFIER
            && ast_identifier_name(obj) != NULL) {
            const char *candidate_name = ast_identifier_name(obj);
            const MIRDeclField *mir_field =
                transpiler_find_decl_field_metadata(ctx, host_type_name,
                                                    candidate_name);
            if (mir_field != NULL) {
                if (transpiler_mir_decl_field_is_subject_like(mir_field))
                    return ast_member_name(cursor);
                return candidate_name;
            } else {
                TranspilerProjectionFieldInfo info =
                    host_projection_class_field_info(ctx, host_decl,
                        host_type_name, candidate_name);
                if (info.exists) {
                    if (info.subject_like)
                        return ast_member_name(cursor);
                    return candidate_name;
                }
            }
        }
        cursor = obj;
    }

    return NULL;
}

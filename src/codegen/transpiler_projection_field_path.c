/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend projection field-path helpers.
 */

#include <string.h>

#include "transpiler_decl_lookup.h"
#include "transpiler_projection_field_path.h"

const char *
assignment_target_root_slot_name(ASTNode *target)
{
    if (target == NULL)
        return NULL;
    if (target->type == AST_IDENTIFIER)
        return target->data.identifier.name;
    if (target->type == AST_MEMBER_ACCESS) {
        ASTNode *member_object = ast_member_object(target);
        if (member_object != NULL
            && member_object->type == AST_IDENTIFIER
            && member_object->data.identifier.name != NULL
            && strcmp(member_object->data.identifier.name, "self") == 0) {
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
            && inner_object->data.identifier.name != NULL
            && strcmp(inner_object->data.identifier.name, "self") == 0) {
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

    host_decl = find_class_decl(ctx, host_type_name);
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL)
        return false;

    size_t field_count = 0;
    ClassField **fields = ast_class_fields(host_decl, &field_count);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = fields != NULL ? fields[i] : NULL;
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return !field->is_vessel_field;
        }
    }

    return false;
}

ClassField *
find_host_field_by_name_local(ASTNode *host_decl, const char *field_name)
{
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL
        || field_name == NULL) {
        return NULL;
    }

    size_t field_count = 0;
    ClassField **fields = ast_class_fields(host_decl, &field_count);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = fields != NULL ? fields[i] : NULL;
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return field;
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

    host_decl = find_class_decl(ctx, host_type_name);
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL)
        return NULL;

    if (target->type == AST_IDENTIFIER
        && host_projection_relevant_field_exists(ctx, host_type_name,
            target->data.identifier.name)) {
        return target->data.identifier.name;
    }

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        ASTNode *obj = ast_member_object(cursor);
        if (obj != NULL && obj->type == AST_IDENTIFIER
            && obj->data.identifier.name != NULL) {
            ClassField *field = find_host_field_by_name_local(
                host_decl, obj->data.identifier.name);
            if (field != NULL) {
                if (field->is_vessel_field)
                    return ast_member_name(cursor);
                return obj->data.identifier.name;
            }
        }
        cursor = obj;
    }

    return NULL;
}

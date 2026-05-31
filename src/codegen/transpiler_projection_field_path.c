/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend projection field-path helpers.
 */

#include <string.h>

#include "../parser/ast_api.h"
#include "host_decl_compat.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_projection_field_path.h"

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
        ClassField *field =
            pgy_host_class_field_compat_find(host_decl, field_name);
        return field != NULL && !field->is_vessel_field;
    }
}

ClassField *
find_host_field_by_name_local(ASTNode *host_decl, const char *field_name)
{
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL
        || field_name == NULL) {
        return NULL;
    }

    return pgy_host_class_field_compat_find(host_decl, field_name);
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
                ClassField *field = find_host_field_by_name_local(
                    host_decl, candidate_name);
                if (field != NULL) {
                    if (field->is_vessel_field)
                        return ast_member_name(cursor);
                    return candidate_name;
                }
            }
        }
        cursor = obj;
    }

    return NULL;
}

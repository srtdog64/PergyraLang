#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_ability_match_internal.h"

bool
any_subject_role_has_ability(ASTNode *program, ASTNode *ability_ref)
{
    if (program == NULL || program->type != AST_PROGRAM
        || ability_ref == NULL) {
        return false;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        ASTNode *type_decl;
        const char *type_name;

        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL) {
            continue;
        }

        type_name = stmt->data.role_decl.for_type->data.type.name;
        type_decl = find_subject_host_decl_by_name(program, type_name);
        if (!decl_is_subject_host(type_decl))
            continue;

        if (role_decl_has_ability(stmt, program, ability_ref, 0))
            return true;
    }

    return false;
}

ASTNode *
any_subject_role_find_base_ability_impl(ASTNode *program,
                                        const char *ability_name)
{
    if (program == NULL || program->type != AST_PROGRAM
        || ability_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        ASTNode *type_decl;
        const char *type_name;

        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL) {
            continue;
        }

        type_name = stmt->data.role_decl.for_type->data.type.name;
        type_decl = find_subject_host_decl_by_name(program, type_name);
        if (!decl_is_subject_host(type_decl))
            continue;

        for (size_t j = 0; j < stmt->data.role_decl.impl_count; j++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[j];
            ASTNode *ability_ref;

            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;
            ability_ref = impl->data.impl_ability.ability_ref;
            if (ability_ref == NULL || ability_ref->type != AST_TYPE
                || ability_ref->data.type.name == NULL) {
                continue;
            }
            if (strcmp(ability_ref->data.type.name, ability_name) == 0)
                return ability_ref;
        }
    }

    return NULL;
}

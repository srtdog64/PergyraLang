#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_ability_match_internal.h"

ASTNode *
semantic_find_role_decl(ASTNode *program, const char *role_name)
{
    if (program == NULL || program->type != AST_PROGRAM || role_name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        const char *stmt_role_name = ast_role_name(stmt);
        if (stmt != NULL && stmt->type == AST_ROLE_DECL
            && stmt_role_name != NULL
            && strcmp(stmt_role_name, role_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

ASTNode *
semantic_role_for_type_node(ASTNode *role_decl)
{
    return ast_role_for_type(role_decl);
}

const char *
semantic_role_for_type_name(ASTNode *role_decl)
{
    ASTNode *for_type = semantic_role_for_type_node(role_decl);

    return ast_type_name(for_type);
}

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
        const char *type_name = semantic_role_for_type_name(stmt);

        if (type_name == NULL)
            continue;

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
        const char *type_name = semantic_role_for_type_name(stmt);

        if (type_name == NULL)
            continue;

        type_decl = find_subject_host_decl_by_name(program, type_name);
        if (!decl_is_subject_host(type_decl))
            continue;

        for (size_t j = 0; j < ast_role_impl_count(stmt); j++) {
            ASTNode *impl = ast_role_impl(stmt, j);
            ASTNode *ability_ref;

            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;
            ability_ref = ast_impl_ability_ref(impl);
            if (ast_impl_ability_name(impl) == NULL) {
                continue;
            }
            if (strcmp(ast_impl_ability_name(impl), ability_name) == 0)
                return ability_ref;
        }
    }

    return NULL;
}

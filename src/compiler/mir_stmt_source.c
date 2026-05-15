#include "mir_stmt_population.h"

#include <string.h>

bool
mir_stmt_is_def_source(const ASTNode *stmt)
{
    if (stmt == NULL)
        return false;
    if (stmt->type == AST_LET_DECL)
        return true;
    if (stmt->type == AST_ASSIGNMENT
        && ast_assignment_target(stmt) != NULL
        && ast_assignment_target(stmt)->type == AST_IDENTIFIER)
        return true;
    return false;
}

const char *
mir_stmt_def_name(const ASTNode *stmt)
{
    if (stmt == NULL)
        return NULL;
    if (stmt->type == AST_LET_DECL)
        return stmt->data.let_decl.name;
    if (stmt->type == AST_ASSIGNMENT
        && ast_assignment_target(stmt) != NULL
        && ast_assignment_target(stmt)->type == AST_IDENTIFIER) {
        return ast_assignment_target(stmt)->data.identifier.name;
    }
    return NULL;
}

bool
mir_let_decl_requires_stmt_preservation(const ASTNode *stmt)
{
    ASTNode *init;
    ASTNode *callee;
    const char *name;

    if (stmt == NULL || stmt->type != AST_LET_DECL)
        return false;

    init = stmt->data.let_decl.initializer;
    if (init == NULL || init->type != AST_CALL)
        return false;

    callee = ast_call_callee(init);
    if (callee == NULL
        || callee->type != AST_IDENTIFIER
        || callee->data.identifier.name == NULL) {
        return false;
    }

    name = callee->data.identifier.name;
    return strcmp(name, "Read") == 0
        || strcmp(name, "ViewRead") == 0
        || strcmp(name, "ViewWrite") == 0
        || strcmp(name, "Move") == 0;
}

bool
mir_stmt_requires_source_local_preservation(const ASTNode *stmt)
{
    return stmt != NULL
        && stmt->type == AST_LET_DECL
        && mir_let_decl_requires_stmt_preservation(stmt);
}

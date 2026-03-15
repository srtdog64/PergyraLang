#ifndef PERGYRA_HIR_H
#define PERGYRA_HIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../parser/ast.h"

typedef enum
{
    HIR_TOPLEVEL_EXTERN,
    HIR_TOPLEVEL_TYPE,
    HIR_TOPLEVEL_ABILITY,
    HIR_TOPLEVEL_ROLE,
    HIR_TOPLEVEL_PARTY,
    HIR_TOPLEVEL_SYSTEMIC,
    HIR_TOPLEVEL_WORLD,
    HIR_TOPLEVEL_ACTOR,
    HIR_TOPLEVEL_EVENT,
    HIR_TOPLEVEL_FUNCTION,
    HIR_TOPLEVEL_EXECUTABLE
} HIRTopLevelKind;

typedef struct
{
    HIRTopLevelKind kind;
    ASTNode        *ast;
    const char     *name;
} HIRTopLevelItem;

typedef struct
{
    HIRTopLevelItem  *items;
    size_t            item_count;

    ASTNode         **externs;
    size_t            extern_count;
    ASTNode         **types;
    size_t            type_count;
    ASTNode         **abilities;
    size_t            ability_count;
    ASTNode         **roles;
    size_t            role_count;
    ASTNode         **parties;
    size_t            party_count;
    ASTNode         **systemics;
    size_t            systemic_count;
    ASTNode         **worlds;
    size_t            world_count;
    ASTNode         **actors;
    size_t            actor_count;
    ASTNode         **events;
    size_t            event_count;
    ASTNode         **functions;
    size_t            function_count;
    ASTNode         **executables;
    size_t            executable_count;

    bool              has_main_function;
} HIRProgram;

HIRProgram *hir_lower(ASTNode *annotated_ast, char **error_message);
void        hir_destroy(HIRProgram *hir);
void        hir_dump(const HIRProgram *hir, FILE *out);
const char *hir_top_level_kind_name(HIRTopLevelKind kind);

#endif

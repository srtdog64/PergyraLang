#ifndef PERGYRA_MIR_TYPE_HELPERS_H
#define PERGYRA_MIR_TYPE_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"

char *mir_claim_abi_type_name_from_ast(const ASTNode *ast);

bool mir_assignment_requires_stmt_preservation(const ASTNode *func_decl,
                                               ASTNode **statements,
                                               size_t statement_count,
                                               size_t stmt_index,
                                               const ASTNode *stmt);

#endif

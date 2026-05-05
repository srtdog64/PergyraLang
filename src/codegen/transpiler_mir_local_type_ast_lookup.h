/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR local type-AST lookup helpers.
 */

#ifndef PERGYRA_TRANSPILER_MIR_LOCAL_TYPE_AST_LOOKUP_H
#define PERGYRA_TRANSPILER_MIR_LOCAL_TYPE_AST_LOOKUP_H

#include "transpiler.h"

ASTNode *transpiler_find_local_type_ast(TranspilerCtx *ctx,
                                        const ASTNode *func_decl,
                                        const char *base_name);

#endif /* PERGYRA_TRANSPILER_MIR_LOCAL_TYPE_AST_LOOKUP_H */

/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend operator-overload lookup helpers.
 */

#ifndef PERGYRA_TRANSPILER_OPERATOR_H
#define PERGYRA_TRANSPILER_OPERATOR_H

#include "transpiler.h"

ASTNode *find_operator_overload_decl(TranspilerCtx *ctx,
                                     const char *type_name,
                                     PgyTokenType op);
bool transpiler_operator_prepare_arguments(TranspilerCtx *ctx,
                                            ASTNode *expr, ASTNode *decl,
                                            char **left, char **right);
ASTNode *find_role_operator_method_decl(TranspilerCtx *ctx,
                                        ASTNode *role,
                                        PgyTokenType op,
                                        int depth);
const MIRDeclMethod *find_role_operator_method_metadata(TranspilerCtx *ctx,
                                                        ASTNode *role,
                                                        PgyTokenType op,
                                                        int depth);

#endif /* PERGYRA_TRANSPILER_OPERATOR_H */

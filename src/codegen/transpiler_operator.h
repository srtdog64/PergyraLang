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
ASTNode *find_role_operator_method_decl(TranspilerCtx *ctx,
                                        ASTNode *role,
                                        PgyTokenType op,
                                        int depth);
bool operator_method_name_matches(PgyTokenType op, const char *name);
const char *operator_overload_suffix(PgyTokenType op);

#endif /* PERGYRA_TRANSPILER_OPERATOR_H */

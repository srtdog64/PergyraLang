/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR role lookup helpers.
 */

#ifndef PERGYRA_TRANSPILER_MIR_ROLE_LOOKUP_H
#define PERGYRA_TRANSPILER_MIR_ROLE_LOOKUP_H

#include "transpiler.h"

const MIRRoutine *transpiler_find_role_impl_mir_method(const TranspilerCtx *ctx,
                                                       const char *owner_name,
                                                       const ASTNode *method_decl);

#endif /* PERGYRA_TRANSPILER_MIR_ROLE_LOOKUP_H */

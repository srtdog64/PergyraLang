/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR signature eligibility policy.
 */

#ifndef PERGYRA_TRANSPILER_MIR_SIGNATURE_H
#define PERGYRA_TRANSPILER_MIR_SIGNATURE_H

#include "transpiler.h"

bool transpiler_mir_type_supported(const char *type_name);
bool transpiler_mir_ast_type_supported(TranspilerCtx *ctx,
                                       const ASTNode *type_node);
bool transpiler_mir_type_name_supported(TranspilerCtx *ctx,
                                        const char *type_name);
bool transpiler_mir_routine_signature_supported(TranspilerCtx *ctx,
                                                const MIRRoutine *routine,
                                                const ASTNode *func_decl);
bool transpiler_mir_function_signature_supported(TranspilerCtx *ctx,
                                                 const ASTNode *func_decl);

#endif /* PERGYRA_TRANSPILER_MIR_SIGNATURE_H */

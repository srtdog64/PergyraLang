#ifndef PGY_TRANSPILER_SPECIALIZATION_REGISTRY_H
#define PGY_TRANSPILER_SPECIALIZATION_REGISTRY_H

#include "transpiler.h"

void ensure_result_specialization_to(TranspilerCtx *ctx,
                                     CodeBuf *dst,
                                     const char *ok_type,
                                     const char *err_type);
void ensure_option_specialization_to(TranspilerCtx *ctx,
                                     CodeBuf *dst,
                                     const char *inner_type);
void ensure_collection_specialization_to(TranspilerCtx *ctx,
                                         CodeBuf *dst,
                                         const char *kind,
                                         const char *inner_type);
void ensure_collection_specialization(TranspilerCtx *ctx,
                                      const char *kind,
                                      const char *inner_type);
void ensure_type_specializations_from_type_name_to(TranspilerCtx *ctx,
                                                   CodeBuf *dst,
                                                   const char *type_name);
void ensure_type_specializations_from_ast_to(TranspilerCtx *ctx,
                                             CodeBuf *dst,
                                             ASTNode *type_node);
void ensure_type_specializations_from_ast(TranspilerCtx *ctx,
                                          ASTNode *type_node);
void ensure_collection_specializations_from_stmt_to(TranspilerCtx *ctx,
                                                    CodeBuf *dst,
                                                    ASTNode *node);
void ensure_collection_specializations_from_mir_routine_to(
    TranspilerCtx *ctx,
    CodeBuf *dst,
    const MIRRoutine *routine);

#endif /* PGY_TRANSPILER_SPECIALIZATION_REGISTRY_H */

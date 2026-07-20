#ifndef PERGYRA_TRANSPILER_TYPE_DECLARATOR_H
#define PERGYRA_TRANSPILER_TYPE_DECLARATOR_H

#include "transpiler.h"

char *pergyra_ast_typed_declarator_in_ctx(TranspilerCtx *ctx,
                                          ASTNode *type_node,
                                          const char *name);
char *pergyra_ast_typed_declarator(ASTNode *type_node, const char *name);
char *pergyra_func_pointer_declarator_from_decl_in_ctx(TranspilerCtx *ctx,
                                                       ASTNode *func_decl,
                                                       const char *name);
char *pergyra_func_pointer_declarator_from_decl(ASTNode *func_decl,
                                                const char *name);
char *pergyra_func_pointer_declarator_from_type_names_in_ctx(
    TranspilerCtx *ctx,
    const char *return_type_name,
    size_t param_count,
    char *const *param_type_names,
    const char *name);
char *pergyra_func_signature_declarator_from_callable_sig_in_ctx(
    TranspilerCtx *ctx,
    const MIRCallableSig *return_sig,
    const char *name,
    const char *params_sig);
char *pergyra_func_signature_declarator_in_ctx(TranspilerCtx *ctx,
                                               ASTNode *return_type,
                                               const char *name,
                                               const char *params_sig);
char *pergyra_func_signature_declarator(ASTNode *return_type,
                                        const char *name,
                                        const char *params_sig);

#endif /* PERGYRA_TRANSPILER_TYPE_DECLARATOR_H */

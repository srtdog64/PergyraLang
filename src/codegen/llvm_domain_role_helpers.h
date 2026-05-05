#ifndef PERGYRA_LLVM_DOMAIN_ROLE_HELPERS_H
#define PERGYRA_LLVM_DOMAIN_ROLE_HELPERS_H

ASTNode *llvm_find_role_decl(LLVMGenCtx *ctx, const char *role_name);

ASTNode *llvm_find_role_operator_method(LLVMGenCtx *ctx, ASTNode *role,
                                        PgyTokenType op, int depth);

#endif /* PERGYRA_LLVM_DOMAIN_ROLE_HELPERS_H */

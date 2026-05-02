#ifndef PGY_LLVM_DOMAIN_FORWARD_H
#define PGY_LLVM_DOMAIN_FORWARD_H

#include "llvm_internal.h"
#include "llvm_inventory_host_methods.h"

void llvm_emit_domain_sync_forward_decl(LLVMGenCtx *ctx,
                                        const char *decl_name,
                                        LLVMTypeRef struct_ty,
                                        LLVMClassTypeEntry *entry);
void llvm_emit_domain_method_forward_decls(LLVMGenCtx *ctx,
                                           const char *decl_name,
                                           LLVMTypeRef struct_ty,
                                           const LLVMHostedMethodView *methods);
void llvm_emit_domain_ability_vtables(LLVMGenCtx *ctx,
                                      ASTNode **abilities,
                                      size_t ability_count);
void llvm_emit_domain_role_forward_decls(LLVMGenCtx *ctx,
                                         ASTNode **roles,
                                         size_t role_count);

#endif

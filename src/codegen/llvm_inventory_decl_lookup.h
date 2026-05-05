/* LLVM MIR-backed declaration lookup helpers. */

#ifndef PGY_LLVM_INVENTORY_DECL_LOOKUP_H
#define PGY_LLVM_INVENTORY_DECL_LOOKUP_H

ASTNode *llvm_bind_current_host_decl(LLVMGenCtx *ctx, ASTNode *host_decl);
void llvm_restore_current_host_decl(LLVMGenCtx *ctx, ASTNode *saved_decl);
void llvm_active_inventory(const LLVMGenCtx *ctx,
                           ASTNodeType decl_type,
                           ASTNode ***nodes_out,
                           size_t *count_out);
const char *llvm_decl_node_name(ASTNode *node);
ASTNode *llvm_find_decl_in_active_inventory(const LLVMGenCtx *ctx,
                                            ASTNodeType decl_type,
                                            const char *name);
bool llvm_param_is_implicit_self(const FuncParam *param);
bool llvm_is_host_decl_type(ASTNodeType decl_type);
const MIRDeclHeader *llvm_find_decl_header_in_context(const LLVMGenCtx *ctx,
                                                      const char *name);
const MIRDeclHeader *llvm_find_host_decl_header_in_context(
    const LLVMGenCtx *ctx,
    const char *name);
ASTNode *llvm_find_host_decl_in_active_inventory(const LLVMGenCtx *ctx,
                                                 const char *name);
ASTNode *llvm_current_host_decl(const LLVMGenCtx *ctx);
const char *llvm_current_host_decl_name(const LLVMGenCtx *ctx);

#endif /* PGY_LLVM_INVENTORY_DECL_LOOKUP_H */

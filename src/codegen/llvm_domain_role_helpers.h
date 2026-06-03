#ifndef PERGYRA_LLVM_DOMAIN_ROLE_HELPERS_H
#define PERGYRA_LLVM_DOMAIN_ROLE_HELPERS_H

ASTNode *llvm_find_role_decl(LLVMGenCtx *ctx, const char *role_name);

ASTNode *llvm_role_for_type_node(ASTNode *role);

const char *llvm_role_for_type_name(ASTNode *role);

ASTNode *llvm_find_role_operator_method(LLVMGenCtx *ctx, ASTNode *role,
                                        PgyTokenType op, int depth);
const MIRDeclMethod *llvm_find_role_operator_method_metadata(
    LLVMGenCtx *ctx,
    ASTNode *role,
    PgyTokenType op,
    int depth);

bool llvm_role_method_symbol_name(char *out,
                                  size_t out_size,
                                  const char *role_name,
                                  const char *method_name);

bool llvm_role_operator_symbol_name(char *out,
                                    size_t out_size,
                                    const char *suffix,
                                    const char *for_type_name);

bool llvm_role_vtable_type_name(char *out,
                                size_t out_size,
                                const char *ability_name);

bool llvm_role_vtable_global_name(char *out,
                                  size_t out_size,
                                  const char *role_name,
                                  const char *ability_name);

const char *llvm_party_slot_first_ability_name(LLVMGenCtx *ctx,
                                               const char *party_type_name,
                                               const char *slot_name);

LLVMValueRef llvm_lookup_role_vtable_global(LLVMGenCtx *ctx,
                                            const char *role_name,
                                            const char *ability_name);

#endif /* PERGYRA_LLVM_DOMAIN_ROLE_HELPERS_H */

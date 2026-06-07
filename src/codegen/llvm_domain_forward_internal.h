#ifndef PGY_LLVM_DOMAIN_FORWARD_INTERNAL_H
#define PGY_LLVM_DOMAIN_FORWARD_INTERNAL_H

#include "llvm_domain_forward.h"

bool llvm_domain_forward_suffix_name(char *out,
                                     size_t out_size,
                                     const char *name,
                                     const char *suffix);
bool llvm_domain_forward_join_name(char *out,
                                   size_t out_size,
                                   const char *left,
                                   const char *right);
bool llvm_domain_forward_operator_name(char *out,
                                       size_t out_size,
                                       const char *suffix,
                                       const char *type_name);
const char *llvm_domain_method_name_metadata_first(
    const MIRDeclMethod *method_meta,
    ASTNode *method,
    bool allow_ast_compat);
size_t llvm_domain_method_param_count_metadata_first(
    const MIRDeclMethod *method_meta,
    ASTNode *method,
    bool allow_ast_compat);
FuncParam *llvm_domain_method_param_metadata_first(
    const MIRDeclMethod *method_meta,
    ASTNode *method,
    size_t index,
    bool allow_ast_compat);
const char *llvm_domain_method_param_type_name_metadata_first(
    const MIRDeclMethod *method_meta,
    ASTNode *method,
    size_t index,
    bool allow_ast_compat);
ASTNode *llvm_domain_method_return_type_metadata_first(
    const MIRDeclMethod *method_meta,
    ASTNode *method,
    bool allow_ast_compat);
const char *llvm_domain_method_return_type_name_metadata_first(
    const MIRDeclMethod *method_meta,
    ASTNode *method,
    bool allow_ast_compat);
LLVMTypeRef llvm_domain_forward_required_param_type(
    LLVMGenCtx *ctx,
    ASTNode *owner,
    FuncParam *param,
    const char *owner_kind,
    const char *owner_name);

#endif /* PGY_LLVM_DOMAIN_FORWARD_INTERNAL_H */

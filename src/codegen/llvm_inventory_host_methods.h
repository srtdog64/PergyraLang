/* LLVM MIR declaration method metadata helpers. */

#ifndef PGY_LLVM_INVENTORY_HOST_METHODS_H
#define PGY_LLVM_INVENTORY_HOST_METHODS_H

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_methods;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} LLVMHostedMethodView;

const MIRDeclMethod *llvm_find_host_method_metadata_in_context(
    const LLVMGenCtx *ctx,
    const char *host_type_name,
    const char *method_name);
LLVMHostedMethodView llvm_hosted_method_view(
    const LLVMGenCtx *ctx,
    const char *host_type_name,
    ASTNode **ast_compat_methods,
    size_t ast_compat_count);
bool llvm_hosted_method_view_missing_mir_metadata(
    const LLVMHostedMethodView *view);
bool llvm_hosted_method_view_missing_mir_method_row(
    const LLVMHostedMethodView *view,
    size_t index);
bool llvm_require_hosted_method_view_rows(
    LLVMGenCtx *ctx,
    const LLVMHostedMethodView *view,
    const char *message_fmt,
    const char *host_name);
LLVMHostedMethodView llvm_hosted_method_view_from_decl(
    const LLVMGenCtx *ctx,
    const char *host_type_name,
    ASTNode *decl);
const MIRDeclMethod *llvm_hosted_method_view_metadata(
    const LLVMHostedMethodView *view,
    size_t index);
ASTNode *llvm_hosted_method_view_source_ast(
    const LLVMHostedMethodView *view,
    size_t index);
const char *llvm_mir_decl_method_name(const MIRDeclMethod *method);
ASTNode *llvm_mir_decl_method_source_ast(const MIRDeclMethod *method);
size_t llvm_mir_decl_method_param_count(const MIRDeclMethod *method);
FuncParam *llvm_mir_decl_method_param(const MIRDeclMethod *method,
                                      size_t index);
const char *llvm_mir_decl_method_param_type_name(
    const MIRDeclMethod *method,
    size_t index);
ASTNode *llvm_mir_decl_method_return_type(const MIRDeclMethod *method);
const char *llvm_mir_decl_method_return_type_name(
    const MIRDeclMethod *method);
bool llvm_mir_decl_method_is_async(const MIRDeclMethod *method);
bool llvm_mir_decl_method_is_action_like(const MIRDeclMethod *method);
const char *llvm_mir_decl_method_within_zone(const MIRDeclMethod *method);
const char *llvm_mir_decl_method_causes_effect(const MIRDeclMethod *method);
const MIRRoutine *llvm_mir_decl_method_routine(const LLVMGenCtx *ctx,
                                               const MIRDeclMethod *method);
ASTNode *llvm_find_host_method_decl_in_context(const LLVMGenCtx *ctx,
                                               const char *host_type_name,
                                               const char *method_name);

#endif /* PGY_LLVM_INVENTORY_HOST_METHODS_H */

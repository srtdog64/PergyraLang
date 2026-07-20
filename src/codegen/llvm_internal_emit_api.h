/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM backend private emitter declarations.
 *
 * Included by llvm_internal_api.h after LLVMGenCtx and registry entry types
 * are defined. Keep implementation bodies out of this owner.
 */

#ifndef PGY_LLVM_INTERNAL_EMIT_API_H
#define PGY_LLVM_INTERNAL_EMIT_API_H

/* Expressions (llvm_expr.c). */
LLVMValueRef llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_current_self_base_ptr(LLVMGenCtx *ctx,
                                        LLVMClassTypeEntry *cls);
LLVMValueRef llvm_identifier_base_ptr(LLVMGenCtx *ctx, const char *name,
                                      LLVMClassTypeEntry *cls);
LLVMValueRef llvm_current_self_call_arg(LLVMGenCtx *ctx);
const char *llvm_operator_overload_suffix(PgyTokenType op);
bool llvm_is_upper_ident(ASTNode *node);
const char *llvm_expr_custom_type_name(ASTNode *node, LLVMGenCtx *ctx);
LLVMClassTypeEntry *llvm_lookup_class_by_type(LLVMGenCtx *ctx,
                                              LLVMTypeRef ty);
LLVMValueRef llvm_emit_number(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_string(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_void_expression_placeholder(LLVMGenCtx *ctx,
                                              ASTNode *node,
                                              const char *owner);
LLVMValueRef llvm_emit_function_call_args(LLVMGenCtx *ctx, LLVMFuncEntry *func,
                                          ASTNode **arg_nodes, size_t argc);
LLVMValueRef llvm_array_data_ptr(LLVMGenCtx *ctx, LLVMValueRef array_value);
LLVMValueRef llvm_array_length_i64(LLVMGenCtx *ctx, LLVMValueRef array_value);
LLVMValueRef llvm_build_option_value(LLVMGenCtx *ctx, LLVMTypeRef inner_ty,
                                     LLVMValueRef has_value,
                                     LLVMValueRef value);
bool llvm_slot_inner_has_external_runtime_helpers(const char *inner);
LLVMValueRef llvm_direct_slot_read(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                                   const char *inner);
void llvm_direct_slot_write(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                            LLVMValueRef value);
void llvm_direct_slot_release(LLVMGenCtx *ctx, LLVMVarEntry *slot_var);
void llvm_emit_structural_secure_slot_write(LLVMGenCtx *ctx,
                                            LLVMVarEntry *slot_var,
                                            LLVMValueRef value);
LLVMValueRef llvm_emit_structural_secure_slot_read(LLVMGenCtx *ctx,
                                                   LLVMVarEntry *slot_var,
                                                   const char *inner);
void llvm_emit_structural_secure_slot_release(LLVMGenCtx *ctx,
                                              LLVMVarEntry *slot_var);
LLVMValueRef llvm_slot_runtime_arg(LLVMGenCtx *ctx, LLVMVarEntry *var);
bool llvm_require_secure_token_var(LLVMGenCtx *ctx, ASTNode *node,
                                   const char *slot_name,
                                   const char *operation_name,
                                   LLVMVarEntry *out);
const char *llvm_call_arg_device_inner(LLVMGenCtx *ctx, ASTNode *node);
ASTNode *llvm_find_named_domain_decl(LLVMGenCtx *ctx, ASTNodeType decl_type,
                                     const char *name);
ASTNode *llvm_find_domain_constructor_decl(LLVMGenCtx *ctx,
                                           const char *name);
ASTNode *llvm_find_function_decl(LLVMGenCtx *ctx, const char *name);
bool llvm_decl_is_extern_function(LLVMGenCtx *ctx, const ASTNode *decl);
ASTNode *llvm_find_intent_decl(LLVMGenCtx *ctx, const char *name);
ASTNode *llvm_find_callable_decl(LLVMGenCtx *ctx, const char *name);
ASTNode *llvm_find_projection_nominal_decl(LLVMGenCtx *ctx,
                                           const char *name);
bool llvm_emit_specialized_method_ondemand(LLVMGenCtx *ctx,
                                           const char *class_name,
                                           const char *method_name);
bool llvm_zone_has_state(LLVMGenCtx *ctx, ASTNode *zone_decl,
                         const char *state_name);
ASTNode *llvm_find_world_state_decl(LLVMGenCtx *ctx, ASTNode *world_decl,
                                    const char *state_name);
ASTNode *llvm_resolve_world_zone_decl(LLVMGenCtx *ctx, ASTNode *world_decl,
                                      const char *slot_name);
bool llvm_zone_has_domain_slot(LLVMGenCtx *ctx,
                               ASTNode *zone_decl,
                               const char *slot_name);
bool llvm_zone_has_layer_slot(LLVMGenCtx *ctx,
                              ASTNode *zone_decl,
                              const char *slot_name);
bool llvm_world_has_zone_slot(LLVMGenCtx *ctx, ASTNode *world_decl,
                              const char *slot_name);
const char *llvm_call_name_or_string_arg(ASTNode *node, size_t index);
LLVMValueRef llvm_domain_query_false(LLVMGenCtx *ctx);
const char *llvm_current_host_class_name(LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_channel_send_expr(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_channel_recv_expr(ASTNode *node, LLVMGenCtx *ctx);
void llvm_expr_set_missing_type_error(LLVMGenCtx *ctx, ASTNode *node,
                                      const char *surface);
LLVMValueRef llvm_emit_checked_collection_get(LLVMGenCtx *ctx,
                                              LLVMValueRef aggregate,
                                              LLVMTypeRef aggregate_type,
                                              LLVMValueRef index,
                                              const char *struct_name);
LLVMValueRef llvm_emit_inline_array_get(LLVMGenCtx *ctx,
                                        LLVMValueRef aggregate,
                                        LLVMTypeRef elem_type,
                                        LLVMValueRef index,
                                        const char *struct_name);
LLVMValueRef llvm_build_checked_fptosi(LLVMGenCtx *ctx, LLVMValueRef value,
                                       LLVMTypeRef target_int_type,
                                       const char *name);

#include "llvm_stmt_internal.h"

/* Declarations (llvm_decl.c). */
void llvm_forward_declare_func(ASTNode *node, LLVMGenCtx *ctx);
void llvm_forward_declare_func_from_mir(const MIRRoutine *routine,
                                        ASTNode *node,
                                        LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_func_from_mir(const MIRRoutine *routine, LLVMGenCtx *ctx);
bool llvm_forward_declare_function_routines_from_inventory(
    LLVMGenCtx *ctx,
    const LLVMMIRRoutineInventory *inventory);
bool llvm_emit_function_routines_from_inventory(
    LLVMGenCtx *ctx,
    const LLVMMIRRoutineInventory *inventory);
bool llvm_validate_function_routine_bodies_from_inventory(
    LLVMGenCtx *ctx,
    const LLVMMIRRoutineInventory *inventory);
bool llvm_mir_validate_cleanup_contract(const MIRRoutine *routine,
                                        LLVMGenCtx *ctx);
bool llvm_intent_involves_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *involves);

/* Domain emitters (llvm_domain.c). */
bool llvm_param_is_implicit_self_local(const FuncParam *param);
const char *llvm_operator_suffix(PgyTokenType op);
bool llvm_operator_method_name_matches(PgyTokenType op, const char *name);
void llvm_stamp_domain_provenance(LLVMGenCtx *ctx,
                                  LLVMClassTypeEntry *decl_cls,
                                  LLVMValueRef self_ptr,
                                  const char *prefix,
                                  const char *name,
                                  unsigned cause);
void llvm_register_domain_structs(LLVMGenCtx *ctx,
                                  ASTNode ***domain_groups,
                                  const size_t *domain_group_counts,
                                  size_t domain_group_count);
void llvm_emit_zone_sync(ASTNode *stmt, const char *decl_name,
                         LLVMClassTypeEntry *decl_cls, LLVMValueRef sync_fn,
                         LLVMGenCtx *ctx);
void llvm_emit_world_sync(ASTNode *stmt, const char *decl_name,
                          LLVMClassTypeEntry *decl_cls, LLVMValueRef sync_fn,
                          LLVMGenCtx *ctx);
void llvm_emit_domain_passes(LLVMGenCtx *ctx);

#endif /* PGY_LLVM_INTERNAL_EMIT_API_H */

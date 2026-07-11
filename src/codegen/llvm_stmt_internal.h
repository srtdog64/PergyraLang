/*
 * LLVM statement emitter private API declarations.
 *
 * Included from llvm_internal_api.h after LLVMGenCtx and registry types are
 * defined. Keep implementation bodies out of this declaration boundary.
 */

#ifndef PGY_LLVM_STMT_INTERNAL_H
#define PGY_LLVM_STMT_INTERNAL_H

void llvm_emit_statement(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_block(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_let_decl(ASTNode *node, LLVMGenCtx *ctx);
bool llvm_stmt_emit_collection_like_let(ASTNode *node, LLVMGenCtx *ctx);
bool llvm_stmt_emit_claim_slot_let(ASTNode *node, LLVMGenCtx *ctx);
bool llvm_stmt_emit_view_or_move_let(ASTNode *node, LLVMGenCtx *ctx);
bool llvm_stmt_emit_slot_sugar_let(ASTNode *node, LLVMGenCtx *ctx);
bool llvm_stmt_register_callable_let_binding(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_with_stmt(ASTNode *node, LLVMGenCtx *ctx);
const char *llvm_stmt_render_type_annotation_copy(LLVMGenCtx *ctx,
                                                  ASTNode *type_ann);
ASTNode *llvm_stmt_current_return_callable_type(LLVMGenCtx *ctx);
LLVMTypeRef llvm_stmt_lambda_return_type(LLVMGenCtx *ctx, ASTNode *expr);
LLVMTypeRef llvm_stmt_lambda_param_type(LLVMGenCtx *ctx, ASTNode *lambda,
                                        ASTNode *param, size_t param_index);
LLVMTypeRef llvm_stmt_lambda_signature_type(LLVMGenCtx *ctx, ASTNode *expr);
/* Closure environment ABI (docs/135 Stage A): a captured lambda's value type is
 * a struct { fn_ptr, env } where fn takes the env pointer as a hidden leading
 * parameter. env_ty_out/fn_ty_out receive the env struct type and the function
 * type when non-NULL. Returns the closure struct type, or NULL on error. */
LLVMTypeRef llvm_closure_struct_type(LLVMGenCtx *ctx, ASTNode *expr,
                                     LLVMTypeRef *env_ty_out,
                                     LLVMTypeRef *fn_ty_out);
const char *llvm_infer_spawn_future_inner(LLVMGenCtx *ctx, ASTNode *spawn_expr);
LLVMValueRef llvm_stmt_create_slot_alloca(LLVMGenCtx *ctx, LLVMTypeRef type,
                                          const char *name);
void llvm_emit_while_loop(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_for_loop(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_match_stmt(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_parallel_block(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_async_block(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_select_stmt(ASTNode *node, LLVMGenCtx *ctx);
/* Join form (docs/181 SS1 rungs 0-2; llvm_stmt_parallel_join.c). */
void llvm_emit_parallel_join_block(ASTNode *node, LLVMGenCtx *ctx);
/* Expression form (docs/181 R2): Array<R> value from per-task gives. */
LLVMValueRef llvm_emit_parallel_join_expr(ASTNode *node, LLVMGenCtx *ctx);
/* R2 result materialization (llvm_stmt_parallel_join_result.c). */
void llvm_pjoin_materialize_result(LLVMGenCtx *ctx, ASTNode *node,
                                   const char *give_name,
                                   LLVMTypeRef give_type,
                                   LLVMTypeRef ctx_struct_type,
                                   LLVMValueRef ctxs, size_t n_captured,
                                   LLVMValueRef n_val, LLVMValueRef i_slot,
                                   LLVMValueRef *result_out);
/* Shared with the join emitter (defined in llvm_stmt_parallel_async.c). */
bool llvm_capture_entry_is_required(LLVMGenCtx *ctx, const ASTNode *body,
                                    LLVMScopeFrame *frame, int index);
bool llvm_emit_task_handle_nonnull_guard(LLVMGenCtx *ctx, ASTNode *site,
                                         LLVMValueRef handle,
                                         const char *reason);
bool llvm_capture_reject_shared_collection(LLVMGenCtx *ctx, ASTNode *site,
                                           const char *boundary,
                                           const char *name,
                                           bool allow_slice_views);
void llvm_stmt_emit_zone_action_effect_runtime(ASTNode *call, LLVMGenCtx *ctx);

#endif /* PGY_LLVM_STMT_INTERNAL_H */

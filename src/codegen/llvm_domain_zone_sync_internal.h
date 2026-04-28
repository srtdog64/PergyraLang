#ifndef PERGYRA_LLVM_DOMAIN_ZONE_SYNC_INTERNAL_H
#define PERGYRA_LLVM_DOMAIN_ZONE_SYNC_INTERNAL_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

void llvm_zone_sync_alloc_previous_state(ASTNode *stmt, LLVMGenCtx *ctx,
                                         LLVMValueRef **prev_state_addrs_out,
                                         LLVMValueRef **prev_layer_addrs_out);
void llvm_zone_sync_snapshot_previous_state(ASTNode *stmt,
                                            LLVMClassTypeEntry *decl_cls,
                                            LLVMValueRef sync_fn,
                                            LLVMGenCtx *ctx,
                                            LLVMValueRef *prev_state_addrs,
                                            LLVMValueRef *prev_layer_addrs);
void llvm_zone_sync_reset_state_and_layers(ASTNode *stmt,
                                           LLVMClassTypeEntry *decl_cls,
                                           LLVMValueRef sync_fn,
                                           LLVMGenCtx *ctx);
void llvm_zone_sync_update_frontier_continue(ASTNode *stmt,
                                             LLVMClassTypeEntry *decl_cls,
                                             LLVMValueRef sync_fn,
                                             LLVMGenCtx *ctx,
                                             LLVMValueRef *prev_state_addrs,
                                             LLVMValueRef *prev_layer_addrs,
                                             LLVMValueRef frontier_continue_addr);
void llvm_zone_sync_emit_relation_clauses(ASTNode *stmt,
                                          LLVMClassTypeEntry *decl_cls,
                                          LLVMValueRef sync_fn,
                                          LLVMGenCtx *ctx);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_DOMAIN_ZONE_SYNC_INTERNAL_H */

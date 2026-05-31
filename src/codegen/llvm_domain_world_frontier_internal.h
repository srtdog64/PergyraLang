#ifndef PERGYRA_LLVM_DOMAIN_WORLD_FRONTIER_INTERNAL_H
#define PERGYRA_LLVM_DOMAIN_WORLD_FRONTIER_INTERNAL_H

#ifdef PGY_LLVM_ENABLED

#include <stddef.h>
#include <stdbool.h>

#include "llvm_internal.h"
#include "llvm_inventory_decl_lookup.h"

bool llvm_world_frontier_field_name(char *out,
                                    size_t out_size,
                                    const char *kind,
                                    const char *name);
bool llvm_world_frontier_sync_name(char *out,
                                   size_t out_size,
                                   const char *zone_type);
void llvm_world_frontier_emit_zone_sync_pass(ASTNode *stmt,
                                             LLVMClassTypeEntry *decl_cls,
                                             LLVMValueRef sync_fn,
                                             LLVMValueRef needs_derived_addr,
                                             LLVMValueRef derived_ptr,
                                             const LLVMHostedWorldZoneSlotView *zone_view,
                                             LLVMGenCtx *ctx);
void llvm_world_frontier_emit_derived_state_pass(ASTNode *stmt,
                                                 LLVMClassTypeEntry *decl_cls,
                                                 LLVMValueRef sync_fn,
                                                 ASTNode **states,
                                                 size_t state_count,
                                                 LLVMValueRef continue_addr,
                                                 LLVMValueRef changed_any_addr,
                                                 LLVMBasicBlockRef loop_check_bb,
                                                 LLVMGenCtx *ctx);
void llvm_world_frontier_emit_pending_zone_dirty(LLVMClassTypeEntry *decl_cls,
                                                LLVMValueRef sync_fn,
                                                LLVMValueRef derived_dirty_addr,
                                                LLVMValueRef derived_ptr,
                                                LLVMValueRef frontier_continue_addr,
                                                LLVMValueRef changed_any_addr,
                                                const LLVMHostedWorldZoneSlotView *zone_view,
                                                LLVMGenCtx *ctx);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_DOMAIN_WORLD_FRONTIER_INTERNAL_H */

#ifndef PGY_LLVM_DOMAIN_ZONE_BIND_HELPERS_H
#define PGY_LLVM_DOMAIN_ZONE_BIND_HELPERS_H

#include "llvm_internal.h"

void llvm_zone_bind_effect_layer(ASTNode *zone_decl,
                                 LLVMClassTypeEntry *zone_cls,
                                 LLVMValueRef sync_fn,
                                 LLVMGenCtx *ctx,
                                 const char *layer_slot_name,
                                 const char *target_slot_name);
void llvm_zone_bind_relation_layer(ASTNode *zone_decl,
                                   LLVMClassTypeEntry *zone_cls,
                                   LLVMValueRef sync_fn,
                                   LLVMGenCtx *ctx,
                                   const char *layer_slot_name,
                                   const char *left_slot_name,
                                   const char *right_slot_name);

#endif

#ifndef PERGYRA_LLVM_DOMAIN_WORLD_SYNC_INTERNAL_H
#define PERGYRA_LLVM_DOMAIN_WORLD_SYNC_INTERNAL_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

ASTNode *llvm_world_sync_find_state_decl(ASTNode *world_decl,
                                         const char *state_name);
bool llvm_world_sync_has_zone_slot(ASTNode *world_decl, const char *slot_name);
void llvm_world_sync_emit_directives(ASTNode *stmt,
                                     LLVMClassTypeEntry *decl_cls,
                                     LLVMValueRef sync_fn,
                                     LLVMGenCtx *ctx);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_DOMAIN_WORLD_SYNC_INTERNAL_H */

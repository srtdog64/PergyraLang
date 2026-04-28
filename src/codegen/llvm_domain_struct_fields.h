#ifndef PGY_LLVM_DOMAIN_STRUCT_FIELDS_H
#define PGY_LLVM_DOMAIN_STRUCT_FIELDS_H

#include "llvm_internal.h"

LLVMTypeRef llvm_zone_effect_pool_struct_type(LLVMGenCtx *ctx,
                                              LLVMTypeRef effect_ty,
                                              int capacity);
void        llvm_domain_add_projection_state_fields(LLVMGenCtx *ctx,
                                                    LLVMClassTypeEntry *entry,
                                                    LLVMTypeRef *ftypes,
                                                    int *field_index,
                                                    ASTNode **slots,
                                                    size_t slot_count,
                                                    ASTNode **refreshes,
                                                    size_t refresh_count);

#endif

#ifndef PGY_LLVM_DOMAIN_STRUCT_REGISTER_FIELDS_H
#define PGY_LLVM_DOMAIN_STRUCT_REGISTER_FIELDS_H

#include "llvm_internal.h"
#include "llvm_inventory_decl_lookup.h"

bool llvm_domain_struct_register_fields(LLVMGenCtx *ctx,
                                        ASTNode *stmt,
                                        LLVMClassTypeEntry *entry,
                                        LLVMTypeRef *ftypes,
                                        ASTNode **refreshes,
                                        size_t refresh_count);

#endif /* PGY_LLVM_DOMAIN_STRUCT_REGISTER_FIELDS_H */

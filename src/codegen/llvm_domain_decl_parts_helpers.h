#ifndef PGY_LLVM_DOMAIN_DECL_PARTS_HELPERS_H
#define PGY_LLVM_DOMAIN_DECL_PARTS_HELPERS_H

#include "llvm_internal.h"

void llvm_domain_decl_parts(ASTNode *stmt,
                            const char **decl_name,
                            ASTNode ***slots,
                            size_t *slot_count,
                            ASTNode ***shared_fields,
                            size_t *shared_count,
                            ASTNode ***refreshes,
                            size_t *refresh_count);

#endif

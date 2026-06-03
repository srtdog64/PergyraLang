#ifndef PGY_LLVM_DOMAIN_DECL_PARTS_HELPERS_H
#define PGY_LLVM_DOMAIN_DECL_PARTS_HELPERS_H

#include "llvm_internal.h"

void llvm_domain_decl_refreshes(ASTNode *stmt,
                                const char **decl_name,
                                ASTNode ***refreshes,
                                size_t *refresh_count);

#endif

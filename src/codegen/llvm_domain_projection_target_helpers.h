#ifndef PERGYRA_LLVM_DOMAIN_PROJECTION_TARGET_HELPERS_H
#define PERGYRA_LLVM_DOMAIN_PROJECTION_TARGET_HELPERS_H

typedef struct ASTNode ASTNode;

bool llvm_domain_slot_is_projection_target(const char *slot_name,
                                           ASTNode **refreshes,
                                           size_t refresh_count);

#endif /* PERGYRA_LLVM_DOMAIN_PROJECTION_TARGET_HELPERS_H */

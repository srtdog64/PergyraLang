#ifndef PGY_LLVM_MIR_MATCH_PATTERN_H
#define PGY_LLVM_MIR_MATCH_PATTERN_H

#ifdef PGY_LLVM_ENABLED

#include <stdbool.h>
#include <stddef.h>

#include "parser/ast.h"

void llvm_mir_match_payload_alloca_name(ASTNode *match_case,
                                        const char *binding,
                                        char *buffer,
                                        size_t buffer_size);
bool llvm_mir_is_option_destructor(ASTNode *pat,
                                   const char **kind,
                                   const char **binding);
bool llvm_mir_is_result_destructor(ASTNode *pat,
                                   const char **kind,
                                   const char **binding);

#endif

#endif /* PGY_LLVM_MIR_MATCH_PATTERN_H */

#ifndef PERGYRA_LLVM_EXPR_BANNER_STRING_HELPERS_H
#define PERGYRA_LLVM_EXPR_BANNER_STRING_HELPERS_H

#include "../common/arena.h"

char *llvm_normalize_banner_string_literal_scratch(const char *src,
                                                   PgyArena *arena);

#endif /* PERGYRA_LLVM_EXPR_BANNER_STRING_HELPERS_H */

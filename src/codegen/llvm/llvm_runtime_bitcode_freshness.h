#ifndef PGY_LLVM_RUNTIME_BITCODE_FRESHNESS_H
#define PGY_LLVM_RUNTIME_BITCODE_FRESHNESS_H

#include <stdbool.h>

bool llvm_runtime_bitcode_is_fresh(const char *bc_path);

#endif /* PGY_LLVM_RUNTIME_BITCODE_FRESHNESS_H */

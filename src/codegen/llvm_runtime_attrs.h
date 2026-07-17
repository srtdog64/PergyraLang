#ifndef PGY_LLVM_RUNTIME_ATTRS_H
#define PGY_LLVM_RUNTIME_ATTRS_H

#include <stdbool.h>

bool llvm_fn_is_panic(const char *fn_name);
bool llvm_fn_is_checked_arith(const char *fn_name);
bool llvm_fn_is_lifecycle_runtime(const char *fn_name);
bool llvm_fn_is_capability_runtime(const char *fn_name);
bool llvm_fn_is_budget_runtime(const char *fn_name);
bool llvm_fn_is_bounds_checked_accessor(const char *fn_name);
bool llvm_fn_is_stateful_runtime(const char *fn_name);
bool llvm_fn_never_returns(const char *fn_name);
bool llvm_fn_is_readnone_runtime(const char *fn_name);
bool llvm_fn_is_readonly_runtime(const char *fn_name);

#endif /* PGY_LLVM_RUNTIME_ATTRS_H */

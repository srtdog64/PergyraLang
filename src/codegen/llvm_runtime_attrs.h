#ifndef PGY_LLVM_RUNTIME_ATTRS_H
#define PGY_LLVM_RUNTIME_ATTRS_H

#include <stdbool.h>

#include <llvm-c/Core.h>

bool llvm_fn_is_runtime_panic_entrypoint(const char *fn_name);
bool llvm_fn_is_checked_arith(const char *fn_name);
bool llvm_fn_is_lifecycle_runtime(const char *fn_name);
bool llvm_fn_is_capability_runtime(const char *fn_name);
bool llvm_fn_is_budget_runtime(const char *fn_name);
bool llvm_fn_is_bounds_checked_accessor(const char *fn_name);
bool llvm_fn_is_stateful_runtime(const char *fn_name);
bool llvm_fn_never_returns(const char *fn_name);
bool llvm_fn_is_readnone_runtime(const char *fn_name);
bool llvm_fn_is_readonly_runtime(const char *fn_name);
/* Attach the runtime attribute facts to one declaration the caller has proved
 * to be a registered runtime entrypoint (LLVMFuncEntry.runtime_entrypoint). */
void llvm_runtime_entrypoint_attrs_apply(LLVMContextRef context,
                                         LLVMValueRef fn);

#endif /* PGY_LLVM_RUNTIME_ATTRS_H */

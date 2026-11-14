#ifndef PGY_LLVM_RUNTIME_ATTRS_H
#define PGY_LLVM_RUNTIME_ATTRS_H

#include <stdbool.h>

#include <llvm-c/Core.h>

bool llvm_fn_is_runtime_panic_entrypoint(const char *fn_name);
bool llvm_fn_is_checked_arith(const char *fn_name);
bool llvm_fn_is_lifecycle_runtime(const char *fn_name);
bool llvm_fn_is_capability_runtime(const char *fn_name);
bool llvm_fn_is_budget_runtime(const char *fn_name);
bool llvm_fn_requires_external_bounds_guard(const char *fn_name);
bool llvm_fn_is_stateful_runtime(const char *fn_name);
bool llvm_fn_never_returns(const char *fn_name);
bool llvm_fn_is_readnone_runtime(const char *fn_name);
bool llvm_fn_is_readonly_runtime(const char *fn_name);
/* Match Clang's C _Bool ABI on every registered runtime declaration.  The
 * zeroext attributes belong to the declaration before call lowering; adding
 * them only after runtime bitcode linking leaves callers free to pass dirty
 * upper bits to separately compiled C functions. */
void llvm_runtime_c_bool_abi_attrs_apply(LLVMContextRef context,
                                         LLVMValueRef fn,
                                         LLVMTypeRef fn_type);
/* Attach the runtime attribute facts to one declaration the caller has proved
 * to be a registered runtime entrypoint (LLVMFuncEntry.runtime_entrypoint). */
void llvm_runtime_entrypoint_attrs_apply(LLVMContextRef context,
                                         LLVMValueRef fn);
/* Only the runtime-bitcode owner may call this for its own definitions. */
void llvm_runtime_definition_attrs_apply(LLVMContextRef context,
                                          LLVMValueRef fn);
bool llvm_runtime_bitcode_has_direct_abi(LLVMValueRef declaration,
                                         LLVMValueRef definition);

#endif /* PGY_LLVM_RUNTIME_ATTRS_H */

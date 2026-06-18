#ifndef PGY_LLVM_RESULT_SPEC_H
#define PGY_LLVM_RESULT_SPEC_H

#ifdef PGY_LLVM_ENABLED

#include <stdbool.h>
#include <stddef.h>

#include <llvm-c/Core.h>

typedef struct LLVMGenCtx LLVMGenCtx;

#define MAX_LLVM_RESULT_SPECS 32

/* Result<T, E> specialization cache: parity with C backend's
 * ensure_result_specialization (transpiler_helpers_core_b.h).
 * LLVM has no preprocessor, so each unique (T, E) gets a named struct
 * {i32 tag, ok_ty value, err_ty err} created once and reused. */
typedef struct
{
    char         suffix[128];   /* "Int_NetError" */
    char         ok_name[64];   /* "Int" */
    char         err_name[64];  /* "NetError" */
    LLVMTypeRef  struct_ty;     /* named struct */
    LLVMTypeRef  ok_ty;
    LLVMTypeRef  err_ty;
} LLVMResultSpecEntry;

/* Context-aware suffix extractor for Result<T,E> call lowering. */
bool llvm_result_suffix_from_context(LLVMGenCtx *ctx,
                                     char *suffix_out, size_t suffix_n,
                                     char *ok_out, size_t ok_n,
                                     char *err_out, size_t err_n);

/* Best-effort resolution of a source-level type name to an LLVM type. */
LLVMTypeRef llvm_resolve_source_type(LLVMGenCtx *ctx, const char *type_name);

/* Fetch or create the cached {i32 tag, ok_ty, err_ty} named struct. */
LLVMResultSpecEntry *llvm_ensure_result_type(LLVMGenCtx *ctx,
                                             const char *ok_name,
                                             const char *err_name);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_RESULT_SPEC_H */

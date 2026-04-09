/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend — currently a MIR-led / HIR-assisted hybrid backend.
 * Ordinary function bodies prefer MIR, but some language surfaces still
 * require HIR fallback today.
 *
 * Build: compile with -DPGY_LLVM_ENABLED and link against LLVM-C.
 */

#ifndef PERGYRA_LLVM_BACKEND_H
#define PERGYRA_LLVM_BACKEND_H

#ifdef PGY_LLVM_ENABLED

#include <stdbool.h>
#include <stddef.h>
#include "../compiler/hir.h"
#include "../compiler/mir.h"

/* -----------------------------------------------------------------
 * Result type for LLVM code generation
 * ----------------------------------------------------------------- */

typedef struct
{
    bool  success;
    char *error_message;  /* NULL on success                  */
    char *ir_text;        /* LLVM IR text (--emit-llvm mode)  */
    bool  uses_intent_observability;
} LLVMGenResult;

/* -----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------- */

/*
 * Generate LLVM IR from lowered HIR and return it as text.
 * Caller must free with llvm_gen_result_destroy().
 */
LLVMGenResult *llvm_codegen(const HIRProgram *hir, const char *module_name);

/*
 * Generate LLVM IR from MIR (preferred) with HIR fallback.
 * Current fallback debt still includes async/lambda functions,
 * intent/domain emission, and main-wrapper metadata.
 * Caller must free with llvm_gen_result_destroy().
 */
LLVMGenResult *llvm_codegen_with_mir(const HIRProgram *hir,
                                      const MIRProgram *mir,
                                      const char *module_name);

/*
 * Generate LLVM IR from MIR and emit a native object file (.o).
 * If mir is NULL, this keeps the legacy behavior and falls back to HIR emission.
 * Even when mir is present, some HIR-assisted paths still remain until the
 * MIR-only backend migration is complete.
 * Caller must free with llvm_gen_result_destroy().
 */
LLVMGenResult *llvm_codegen_to_object_with_mir(const HIRProgram *hir,
                                               const MIRProgram *mir,
                                               const char *module_name,
                                               const char *output_path,
                                               bool release_opt);

/*
 * Generate LLVM IR from lowered HIR, optimize, and emit a native object file (.o).
 * Caller must free with llvm_gen_result_destroy().
 */
LLVMGenResult *llvm_codegen_to_object(const HIRProgram *hir,
                                       const char *module_name,
                                       const char *output_path,
                                       bool release_opt);

/*
 * Free an LLVMGenResult.
 */
void llvm_gen_result_destroy(LLVMGenResult *res);

#else /* !PGY_LLVM_ENABLED - stub declarations */

typedef struct { bool success; char *error_message; char *ir_text; bool uses_intent_observability; } LLVMGenResult;
LLVMGenResult *llvm_codegen(const void *hir, const char *module_name);
LLVMGenResult *llvm_codegen_with_mir(const void *hir, const void *mir, const char *module_name);
LLVMGenResult *llvm_codegen_to_object_with_mir(const void *hir, const void *mir, const char *module_name, const char *output_path, bool release_opt);
LLVMGenResult *llvm_codegen_to_object(const void *hir, const char *module_name, const char *output_path, bool release_opt);
void llvm_gen_result_destroy(LLVMGenResult *res);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_BACKEND_H */

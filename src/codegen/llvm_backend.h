/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend — converts lowered Pergyra HIR to LLVM IR,
 * then compiles to native object code.
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
 * If mir is non-NULL, emits functions using MIR CFG and SSA.
 * Caller must free with llvm_gen_result_destroy().
 */
LLVMGenResult *llvm_codegen_with_mir(const HIRProgram *hir,
                                      const MIRProgram *mir,
                                      const char *module_name);

/*
 * Generate LLVM IR from MIR and emit a native object file (.o).
 * If mir is NULL, this keeps the legacy behavior and falls back to HIR emission.
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

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_BACKEND_H */

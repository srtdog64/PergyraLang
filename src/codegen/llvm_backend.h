/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend.
 *
 * MIR-backed entry paths no longer require the original HIR program as the
 * source of truth during codegen: routine bodies emit from MIR routines, and
 * declaration / top-level inventory is carried through MIRProgram.
 *
 * Remaining debt is declaration-side structure, not routine-body fallback:
 * some helpers still consume AST-carried declaration inventory instead of a
 * dedicated declaration IR layer.
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
 * Legacy whole-program HIR entrypoint.
 * New compiler paths should prefer llvm_codegen_from_mir().
 * Caller must free with llvm_gen_result_destroy().
 */
LLVMGenResult *llvm_codegen(const HIRProgram *hir, const char *module_name);
LLVMGenResult *llvm_codegen_from_mir(const MIRProgram *mir,
                                     const char *module_name);

/*
 * Thin compatibility wrapper for mixed call sites.
 * When `mir` is present, this delegates to the MIR-native path.
 * Otherwise it delegates to the legacy HIR whole-program path.
 * New compiler paths should prefer llvm_codegen_from_mir().
 * Caller must free with llvm_gen_result_destroy().
 */
LLVMGenResult *llvm_codegen_with_mir(const HIRProgram *hir,
                                      const MIRProgram *mir,
                                      const char *module_name);
LLVMGenResult *llvm_codegen_to_object_from_mir(const MIRProgram *mir,
                                               const char *module_name,
                                               const char *output_path,
                                               bool release_opt);

/*
 * Thin compatibility wrapper for object emission.
 * When `mir` is present, this delegates to the MIR-native object path.
 * Otherwise it delegates to the legacy HIR whole-program object path.
 * Caller must free with llvm_gen_result_destroy().
 */
LLVMGenResult *llvm_codegen_to_object_with_mir(const HIRProgram *hir,
                                               const MIRProgram *mir,
                                               const char *module_name,
                                               const char *output_path,
                                               bool release_opt);

/*
 * Legacy whole-program HIR object emission entrypoint.
 * New compiler paths should prefer llvm_codegen_to_object_from_mir().
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
LLVMGenResult *llvm_codegen_from_mir(const void *mir, const char *module_name);
LLVMGenResult *llvm_codegen_with_mir(const void *hir, const void *mir, const char *module_name);
LLVMGenResult *llvm_codegen_to_object_from_mir(const void *mir, const char *module_name, const char *output_path, bool release_opt);
LLVMGenResult *llvm_codegen_to_object_with_mir(const void *hir, const void *mir, const char *module_name, const char *output_path, bool release_opt);
LLVMGenResult *llvm_codegen_to_object(const void *hir, const char *module_name, const char *output_path, bool release_opt);
void llvm_gen_result_destroy(LLVMGenResult *res);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_BACKEND_H */

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

#include <stdbool.h>
#include <stddef.h>
#include "../common/arena.h"

#ifdef PGY_LLVM_ENABLED

#include "../compiler/hir.h"
#include "../compiler/mir.h"

/* -----------------------------------------------------------------
 * Result type for LLVM code generation
 * ----------------------------------------------------------------- */

typedef struct
{
    bool  success;
    PgyArena owned_arena;
    char *error_message;  /* NULL on success                  */
    /* Stable diagnostic code attached to error_message (owning, e.g.
     * "PGY_LLVM_SPEC_LIMIT"). NULL when the failing site has not been
     * assigned a code. Propagated from LLVMGenCtx.error_code. */
    char *error_code;
    /* Optional hint tags (owning strdups). cause_ir tags the IR-level
     * origin (e.g. "llvm:result_spec:capacity_exceeded"); fix_source tags
     * the source-level repair action (e.g. "reuse-shared-error-enum").
     * Both NULL when the failing site did not provide them. Propagated
     * from LLVMGenCtx into CompilerResult and the runner's JSON emit. */
    char *error_cause_ir;
    char *error_fix_source;
    char *ir_text;        /* LLVM IR text (--emit-llvm mode)  */
    bool  uses_intent_observability;
} LLVMGenResult;

/* -----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------- */

LLVMGenResult *llvm_codegen_from_mir(const MIRProgram *mir,
                                     const char *module_name);

LLVMGenResult *llvm_codegen_to_object_from_mir(const MIRProgram *mir,
                                               const char *module_name,
                                               const char *output_path,
                                               bool release_opt);

/*
 * Free an LLVMGenResult.
 */
void llvm_gen_result_destroy(LLVMGenResult *res);

#else /* !PGY_LLVM_ENABLED - stub declarations */

typedef struct {
    bool success;
    PgyArena owned_arena;
    char *error_message;
    char *ir_text;
    bool uses_intent_observability;
} LLVMGenResult;
LLVMGenResult *llvm_codegen_from_mir(const void *mir, const char *module_name);
LLVMGenResult *llvm_codegen_to_object_from_mir(const void *mir, const char *module_name, const char *output_path, bool release_opt);
void llvm_gen_result_destroy(LLVMGenResult *res);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_BACKEND_H */

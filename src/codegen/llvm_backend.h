/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend — converts annotated Pergyra AST to LLVM IR,
 * then compiles to native object code.
 *
 * Build: compile with -DPGY_LLVM_ENABLED and link against LLVM-C.
 */

#ifndef PERGYRA_LLVM_BACKEND_H
#define PERGYRA_LLVM_BACKEND_H

#ifdef PGY_LLVM_ENABLED

#include <stdbool.h>
#include <stddef.h>
#include "../parser/ast.h"

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
 * Generate LLVM IR from an annotated AST and return it as text.
 * Caller must free with llvm_gen_result_destroy().
 */
LLVMGenResult *llvm_codegen(ASTNode *ast, const char *module_name);

/*
 * Generate LLVM IR, optimize, and emit a native object file (.o).
 * Caller must free with llvm_gen_result_destroy().
 */
LLVMGenResult *llvm_codegen_to_object(ASTNode *ast,
                                       const char *module_name,
                                       const char *output_path);

/*
 * Free an LLVMGenResult.
 */
void llvm_gen_result_destroy(LLVMGenResult *res);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_BACKEND_H */

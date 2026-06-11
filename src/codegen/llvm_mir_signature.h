/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM MIR routine signature policy.
 */

#ifndef PGY_LLVM_MIR_SIGNATURE_H
#define PGY_LLVM_MIR_SIGNATURE_H

#ifdef PGY_LLVM_ENABLED

typedef enum LLVMMIRSignatureRequirement
{
    LLVM_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME = 1u << 0,
    LLVM_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES = 1u << 1,
    LLVM_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES =
        LLVM_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME
        | LLVM_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES
} LLVMMIRSignatureRequirement;

bool llvm_mir_routine_signature_metadata_complete_for(
    LLVMGenCtx *ctx,
    const MIRRoutine *routine,
    ASTNode *func_decl,
    unsigned requirements,
    const char *missing_signature_fmt,
    const char *missing_return_type_fmt,
    const char *missing_param_type_fmt);

bool llvm_mir_routine_signature_metadata_complete(
    LLVMGenCtx *ctx,
    const MIRRoutine *routine,
    ASTNode *func_decl,
    const char *missing_signature_fmt,
    const char *missing_return_type_fmt,
    const char *missing_param_type_fmt);

bool llvm_mir_or_ast_function_is_generic(const MIRRoutine *routine,
                                         const ASTNode *func_decl);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_MIR_SIGNATURE_H */

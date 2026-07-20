/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR signature eligibility policy.
 */

#ifndef PERGYRA_TRANSPILER_MIR_SIGNATURE_H
#define PERGYRA_TRANSPILER_MIR_SIGNATURE_H

#include "transpiler.h"

typedef enum TranspilerMIRSignatureRequirement
{
    TRANSPILER_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME = 1u << 0,
    TRANSPILER_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES = 1u << 1,
    TRANSPILER_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES =
        TRANSPILER_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME
        | TRANSPILER_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES
} TranspilerMIRSignatureRequirement;

bool transpiler_mir_type_supported(const char *type_name);
bool transpiler_mir_ast_type_supported(TranspilerCtx *ctx,
                                       const ASTNode *type_node);
bool transpiler_mir_type_name_supported(TranspilerCtx *ctx,
                                        const char *type_name);
bool transpiler_mir_routine_signature_metadata_complete_for(
    TranspilerCtx *ctx,
    const MIRRoutine *routine,
    const ASTNode *func_decl,
    unsigned requirements,
    const char *missing_signature_fmt,
    const char *missing_return_type_fmt,
    const char *missing_param_type_fmt);
bool transpiler_mir_routine_signature_supported(TranspilerCtx *ctx,
                                                const MIRRoutine *routine,
                                                const ASTNode *func_decl);
/* Active MIR backend admission.  This variant is deliberately AST-free:
 * every non-void return and parameter shape must be carried by MIR-owned
 * type-name/callable facts.  The legacy API above remains for source-AST
 * compatibility callers that have not crossed the MIR admission boundary. */
bool transpiler_mir_routine_signature_supported_strict(
    TranspilerCtx *ctx,
    const MIRRoutine *routine);
bool transpiler_mir_routine_param_is_boundary_resource(
    const MIRRoutine *routine,
    size_t param_index);
bool transpiler_mir_routine_param_is_slot_family(
    const MIRRoutine *routine,
    size_t param_index);
bool transpiler_mir_routine_param_is_secure_slot(
    const MIRRoutine *routine,
    size_t param_index);
bool transpiler_mir_routine_param_is_device_slot(
    const MIRRoutine *routine,
    size_t param_index);
bool transpiler_mir_or_ast_function_is_generic(const MIRRoutine *routine,
                                               const ASTNode *func_decl);
bool transpiler_mir_function_signature_supported(TranspilerCtx *ctx,
                                                 const ASTNode *func_decl);

#endif /* PERGYRA_TRANSPILER_MIR_SIGNATURE_H */

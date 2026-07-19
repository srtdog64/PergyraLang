#ifndef PGY_LLVM_GENERIC_METHOD_SPECIALIZATION_H
#define PGY_LLVM_GENERIC_METHOD_SPECIALIZATION_H

#ifdef PGY_LLVM_ENABLED

#include <stdbool.h>

#include "llvm_internal.h"

bool llvm_register_generic_method_specializations(
    LLVMGenCtx *ctx,
    const char *host_name,
    LLVMTypeRef host_type,
    bool pointer_self,
    const MIRDeclMethod *method_meta,
    const MIRRoutine *method_routine);
bool llvm_emit_generic_method_specialization_bodies(
    LLVMGenCtx *ctx,
    const MIRRoutine *method_routine);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_GENERIC_METHOD_SPECIALIZATION_H */

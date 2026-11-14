#ifndef PGY_LLVM_DOMAIN_PROJECTION_VALUE_HELPERS_H
#define PGY_LLVM_DOMAIN_PROJECTION_VALUE_HELPERS_H

#include "llvm_internal.h"

typedef struct LLVMDomainRuntimeProjectionView
    LLVMDomainRuntimeProjectionView;

LLVMValueRef llvm_build_domain_projection_value_from_runtime_facts(
    LLVMGenCtx *ctx,
    LLVMClassTypeEntry *target_cls,
    LLVMClassTypeEntry *source_cls,
    const char *source_type_name,
    const LLVMDomainRuntimeProjectionView *runtime_view,
    const PgyDomainProjectionMemberAssignmentFact *anchor,
    LLVMValueRef source_ptr);

#endif

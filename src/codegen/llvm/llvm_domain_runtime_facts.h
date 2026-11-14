#ifndef PGY_LLVM_DOMAIN_RUNTIME_FACTS_H
#define PGY_LLVM_DOMAIN_RUNTIME_FACTS_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

typedef struct LLVMDomainRuntimeProjectionView
{
    const MIRProgram *mir;
    const char       *owner_name;
    size_t            directive_count;
    bool              valid;
} LLVMDomainRuntimeProjectionView;

LLVMDomainRuntimeProjectionView
llvm_domain_runtime_projection_view(LLVMGenCtx *ctx,
                                    const char *owner_name);

const PgyDomainProjectionMemberAssignmentFact *
llvm_domain_runtime_projection_anchor(
    const LLVMDomainRuntimeProjectionView *view,
    size_t directive_index);

size_t llvm_domain_runtime_projection_member_count(
    const LLVMDomainRuntimeProjectionView *view,
    const PgyDomainProjectionMemberAssignmentFact *anchor);

const PgyDomainProjectionMemberAssignmentFact *
llvm_domain_runtime_projection_member_at(
    const LLVMDomainRuntimeProjectionView *view,
    const PgyDomainProjectionMemberAssignmentFact *anchor,
    size_t member_index);

bool llvm_domain_runtime_projection_same_directive(
    const PgyDomainProjectionMemberAssignmentFact *anchor,
    const PgyDomainProjectionMemberAssignmentFact *candidate);

const PgyDomainParticipantRoleFact *
llvm_domain_runtime_require_participant_role(
    LLVMGenCtx *ctx,
    const char *owner_name,
    PgyDomainParticipantRole role);

const MIRDeclField *llvm_domain_runtime_require_exact_field(
    LLVMGenCtx *ctx,
    const char *owner_name,
    uint32_t field_syntax_id,
    const char *field_name,
    const char *field_type_name,
    const char *consumer_label);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_DOMAIN_RUNTIME_FACTS_H */

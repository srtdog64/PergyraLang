#ifndef PGY_MIR_SOURCE_LOCAL_EXPR_BINDING_FACTS_H
#define PGY_MIR_SOURCE_LOCAL_EXPR_BINDING_FACTS_H

#include "mir.h"

const char *mir_source_local_identifier_type_name(
    const MIRProgram *program,
    const MIRRoutine *routine,
    const char *name);
const char *mir_source_local_member_field_type_name(
    const MIRProgram *program,
    const char *owner_type_name,
    const char *member_name);
const char *mir_source_local_member_method_return_type_name(
    const MIRProgram *program,
    const char *owner_type_name,
    const char *member_name);
const char *mir_source_local_decl_call_type_name(
    const MIRProgram *program,
    const char *name);
const char *mir_source_local_owner_method_return_type_name(
    const MIRProgram *program,
    const MIRRoutine *routine,
    const char *name);

#endif

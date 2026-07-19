#ifndef PGY_MIR_GENERIC_METHOD_SPECIALIZATION_H
#define PGY_MIR_GENERIC_METHOD_SPECIALIZATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct MIRProgram;

typedef struct
{
    uint32_t source_call_syntax_id;
    size_t   caller_routine_index;
    size_t   method_routine_index;
    char    *owner_name;
    char    *method_name;
    char    *specialized_name;
    char   **generic_param_names;
    char   **actual_type_names;
    size_t   binding_count;
} MIRGenericMethodSpecializationFact;

bool mir_generic_method_specializations_capture(
    struct MIRProgram *mir,
    char **error_message);
bool mir_generic_method_specializations_validate(
    const struct MIRProgram *mir,
    char **error_message);
void mir_generic_method_specializations_clear(struct MIRProgram *mir);

size_t mir_generic_method_specialization_count(
    const struct MIRProgram *mir);
const MIRGenericMethodSpecializationFact *
mir_generic_method_specialization_at(const struct MIRProgram *mir,
                                     size_t index);
const MIRGenericMethodSpecializationFact *
mir_generic_method_specialization_for_call(const struct MIRProgram *mir,
                                           uint32_t source_call_syntax_id);

#endif

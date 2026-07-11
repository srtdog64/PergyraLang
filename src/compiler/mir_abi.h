#ifndef PERGYRA_MIR_ABI_H
#define PERGYRA_MIR_ABI_H

#include <stdbool.h>
#include <stdint.h>

/* ABI type layouts are MIR-owned; C/LLVM backends consume these facts instead
 * of inventing layout locally. Source: src/runtime/pgy_abi_spec.h. */
#define MIR_MAX_TYPE_FIELDS 8

typedef struct
{
    const char *field_name;
    uint32_t    offset;
    uint32_t    field_size;
    uint32_t    field_align;
} MIRFieldLayout;

typedef enum
{
    MIR_ABI_REPR_UNTAGGED,
    MIR_ABI_REPR_EXPLICIT_TAG,
    MIR_ABI_REPR_NICHE_RESERVED
} MIRAbiRepresentation;

typedef struct
{
    const char               *abi_type_name;
    uint32_t                  size_bytes;
    uint32_t                  align_bytes;
    uint16_t                  field_count;
    MIRFieldLayout            fields[MIR_MAX_TYPE_FIELDS];
    const char               *runtime_fn;
    const char               *inner_c_type;
    MIRAbiRepresentation      representation;
    const char               *discriminant_field_name;
    int32_t                   primary_tag_value;
    int32_t                   secondary_tag_value;
    const char               *niche_none_pattern;
} MIRTypeLayout;

typedef enum
{
    MIR_PARAM_CARRIAGE_VALUE,
    MIR_PARAM_CARRIAGE_READONLY_REF,
    MIR_PARAM_CARRIAGE_VALUE_RESULT,
    MIR_PARAM_CARRIAGE_OWNER_HANDLE
} MIRParamCarriage;

typedef struct
{
    MIRParamCarriage carriage;
    bool             pass_indirect;
} MIRParamAbiFact;

typedef enum MIRTextBuilderCallShape
{
    MIR_TEXT_BUILDER_CALL_CAPACITY_TO_BUILDER,
    MIR_TEXT_BUILDER_CALL_OUT_CAPACITY_TO_VOID,
    MIR_TEXT_BUILDER_CALL_BUILDER_STRING_TO_VOID,
    MIR_TEXT_BUILDER_CALL_BUILDER_ALLOCATOR_TO_STRING,
    MIR_TEXT_BUILDER_CALL_BUILDER_TO_VOID,
} MIRTextBuilderCallShape;

typedef struct MIRTextBuilderRuntimeRow
{
    const char *source_name;
    const char *operation;
    const char *c_inline_fn;
    const char *llvm_export_fn;
    MIRTextBuilderCallShape c_call_shape;
    MIRTextBuilderCallShape llvm_call_shape;
    const char *materialization;
} MIRTextBuilderRuntimeRow;

#endif /* PERGYRA_MIR_ABI_H */

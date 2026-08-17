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

/*
 * Runtime-call ABI row carried by a lowered MIR resource instruction.
 *
 * The ABI owner may return a static table row or a constructed nominal row,
 * but once lowering has admitted the operation this shape is copied into the
 * routine-owned MIR arena.  Backends consume this fact; they do not recreate
 * a runtime symbol from a source type or a generic suffix.
 */
typedef struct MIRResourceRuntimeRow
{
    const char *domain;
    const char *abi_type_name;
    const char *resource_op_name;
    const char *runtime_fn;
    const char *target_kind;
    const char *materialization;
    const char *call_shape;
    /* Stable logical identity for this row.  It is materialized into MIR
     * after lookup; static/dynamic lookup rows themselves may leave this zero
     * because the owner computes it from the canonical domain/type/op key. */
    uint32_t    runtime_call_abi_id;
} MIRResourceRuntimeRow;

typedef enum
{
    MIR_PARAM_CARRIAGE_VALUE,
    MIR_PARAM_CARRIAGE_READONLY_REF,
    MIR_PARAM_CARRIAGE_VALUE_RESULT,
    MIR_PARAM_CARRIAGE_OWNER_HANDLE
} MIRParamCarriage;

typedef enum
{
    MIR_PARAM_RESOURCE_NONE,
    MIR_PARAM_RESOURCE_SLOT,
    MIR_PARAM_RESOURCE_SECURE_SLOT,
    MIR_PARAM_RESOURCE_DEVICE_SLOT
} MIRParamResourceKind;

typedef struct
{
    MIRParamCarriage    carriage;
    MIRParamResourceKind resource_kind;
    bool                pass_indirect;
    const MIRTypeLayout *type_layout;
    uint32_t             abi_layout_id;
} MIRParamAbiFact;

typedef enum MIRTextBuilderCallShape
{
    MIR_TEXT_BUILDER_CALL_RETURNS_ALLOCATOR,
    MIR_TEXT_BUILDER_CALL_ALLOCATOR_OUT_TO_VOID,
    MIR_TEXT_BUILDER_CALL_ALLOCATOR_PTR_TO_VOID,
    MIR_TEXT_BUILDER_CALL_CAPACITY_TO_BUILDER,
    MIR_TEXT_BUILDER_CALL_OUT_CAPACITY_TO_VOID,
    MIR_TEXT_BUILDER_CALL_BUILDER_STRING_TO_VOID,
    MIR_TEXT_BUILDER_CALL_BUILDER_ALLOCATOR_TO_STRING,
    MIR_TEXT_BUILDER_CALL_BUILDER_TO_VOID,
} MIRTextBuilderCallShape;

typedef struct MIRTextBuilderRuntimeRow
{
    const char *owner_name;
    const char *source_name;
    const char *operation;
    const char *c_inline_fn;
    const char *llvm_export_fn;
    MIRTextBuilderCallShape c_call_shape;
    MIRTextBuilderCallShape llvm_call_shape;
    const char *materialization;
} MIRTextBuilderRuntimeRow;

#endif /* PERGYRA_MIR_ABI_H */

/* LLVM projection materialization from MIR-owned exact assignment facts. */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_projection_value_helpers.h"

#include "llvm_domain_runtime_facts.h"
#include "llvm_internal_api.h"
#include "llvm_mir_store_coercion.h"
#include "../compiler/mir_decl_headers.h"

static LLVMValueRef
llvm_domain_projection_value_error(LLVMGenCtx *ctx, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s", message != NULL ? message
                : "LLVM domain projection value lowering failed");
    }
    return NULL;
}

static LLVMValueRef
llvm_load_domain_projection_path_value_from_runtime_fact(
    LLVMGenCtx *ctx,
    const char *source_type_name,
    LLVMClassTypeEntry *source_cls,
    LLVMValueRef source_ptr,
    const PgyDomainProjectionMemberAssignmentFact *fact)
{
    const char *current_type_name = source_type_name;
    LLVMClassTypeEntry *current_cls = source_cls;
    LLVMValueRef current_ptr = source_ptr;

    if (ctx == NULL || source_type_name == NULL || source_cls == NULL
        || source_ptr == NULL || fact == NULL
        || fact->source_path_segments == NULL
        || fact->source_path_segment_count == 0) {
        return llvm_domain_projection_value_error(ctx,
            "LLVM domain projection runtime path fact is incomplete");
    }

    for (size_t i = 0; i < fact->source_path_segment_count; i++) {
        const PgyDomainProjectionPathSegmentFact *segment =
            &fact->source_path_segments[i];
        LLVMTypeRef field_type;
        LLVMValueRef field_ptr;
        int field_index;

        if (llvm_domain_runtime_require_exact_field(ctx,
                current_type_name, segment->field_syntax_id,
                segment->field_name, segment->field_type_name,
                "projection-value-path") == NULL) {
            return NULL;
        }
        field_index = llvm_class_field_index(current_cls,
            segment->field_name);
        field_type = field_index >= 0
            ? llvm_class_field_type_at_index(current_cls, field_index)
            : NULL;
        if (field_index < 0 || field_type == NULL) {
            return llvm_domain_projection_value_error(ctx,
                "LLVM domain projection exact path field is absent from its class layout");
        }
        field_ptr = LLVMBuildStructGEP2(ctx->builder,
            current_cls->struct_type, current_ptr, (unsigned)field_index,
            llvm_tmp_name(ctx));
        if (i + 1 == fact->source_path_segment_count) {
            return LLVMBuildLoad2(ctx->builder, field_type, field_ptr,
                llvm_tmp_name(ctx));
        }

        current_type_name = segment->field_type_name;
        current_cls = llvm_lookup_class(ctx, current_type_name);
        if (current_cls == NULL) {
            return llvm_domain_projection_value_error(ctx,
                "LLVM domain projection exact nested path class is missing");
        }
        current_ptr = field_ptr;
    }
    return llvm_domain_projection_value_error(ctx,
        "LLVM domain projection runtime path did not produce a value");
}

LLVMValueRef
llvm_build_domain_projection_value_from_runtime_facts(
    LLVMGenCtx *ctx,
    LLVMClassTypeEntry *target_cls,
    LLVMClassTypeEntry *source_cls,
    const char *source_type_name,
    const LLVMDomainRuntimeProjectionView *runtime_view,
    const PgyDomainProjectionMemberAssignmentFact *anchor,
    LLVMValueRef source_ptr)
{
    const MIRDeclHeader *target_header;
    LLVMValueRef projected;
    size_t member_count = 0;

    if (ctx == NULL || target_cls == NULL || source_cls == NULL
        || source_type_name == NULL || runtime_view == NULL
        || !runtime_view->valid
        || runtime_view->mir != llvm_active_mir_identity(ctx)
        || anchor == NULL || source_ptr == NULL) {
        return llvm_domain_projection_value_error(ctx,
            "LLVM domain projection requires an admitted runtime fact group");
    }
    target_header = llvm_find_host_decl_header_in_context(
        ctx, target_cls->class_name);
    if (target_header == NULL) {
        return llvm_domain_projection_value_error(ctx,
            "LLVM domain projection target declaration is foreign to MIR");
    }

    projected = LLVMConstNull(target_cls->struct_type);
    member_count = llvm_domain_runtime_projection_member_count(
        runtime_view, anchor);
    for (size_t i = 0; i < member_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            llvm_domain_runtime_projection_member_at(
                runtime_view, anchor, i);
        LLVMValueRef field_value;
        LLVMTypeRef target_field_type;
        int target_field_index;

        if (fact == NULL)
            return llvm_domain_projection_value_error(ctx,
                "LLVM domain projection runtime member view is incomplete");
        if (llvm_domain_runtime_require_exact_field(ctx,
                target_cls->class_name, fact->target_field_syntax_id,
                fact->target_field_name, fact->target_field_type_name,
                "projection-value-target") == NULL) {
            return NULL;
        }
        target_field_index = llvm_class_field_index(target_cls,
            fact->target_field_name);
        target_field_type = target_field_index >= 0
            ? llvm_class_field_type_at_index(target_cls, target_field_index)
            : NULL;
        if (target_field_index < 0 || target_field_type == NULL) {
            return llvm_domain_projection_value_error(ctx,
                "LLVM domain projection exact target field is absent from its class layout");
        }

        field_value =
            llvm_load_domain_projection_path_value_from_runtime_fact(
                ctx, source_type_name, source_cls, source_ptr, fact);
        if (field_value == NULL || ctx->has_error)
            return NULL;
        field_value = llvm_mir_coerce_value_for_store(ctx, field_value,
            target_field_type);
        if (field_value == NULL
            || LLVMTypeOf(field_value) != target_field_type) {
            return llvm_domain_projection_value_error(ctx,
                "LLVM domain projection semantic assignment has no LLVM store coercion");
        }
        projected = LLVMBuildInsertValue(ctx->builder, projected,
            field_value, (unsigned)target_field_index, llvm_tmp_name(ctx));
    }
    if (member_count == 0
        || member_count != mir_decl_header_field_count(target_header)) {
        return llvm_domain_projection_value_error(ctx,
            "LLVM domain projection runtime member assignments are incomplete");
    }
    return projected;
}

#endif /* PGY_LLVM_ENABLED */

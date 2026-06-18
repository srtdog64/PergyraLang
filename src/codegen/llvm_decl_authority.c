#ifdef PGY_LLVM_ENABLED
#include "llvm_decl_authority.h"
#include "llvm_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_internal.h"

#include "../compiler/mir_decl_headers.h"

static const char *
llvm_decl_current_zone_name(LLVMGenCtx *ctx)
{
    if (ctx == NULL)
        return NULL;
    if (ctx->current_mir_routine != NULL
        && llvm_mir_routine_owner_ast_type(ctx->current_mir_routine)
            == AST_ZONE_DECL) {
        return llvm_mir_routine_owner_name(ctx->current_mir_routine);
    }
    return ctx->current_within_zone_name;
}

static const MIRDeclHeader *
llvm_decl_find_current_zone_header(LLVMGenCtx *ctx)
{
    const char *zone_name = llvm_decl_current_zone_name(ctx);
    return zone_name != NULL
        ? llvm_find_decl_header_in_context_of_type(
            ctx, AST_ZONE_DECL, zone_name)
        : NULL;
}

static void
llvm_decl_zone_authority_backend_error(LLVMGenCtx *ctx, ASTNode *node,
                                       const char *zone_name,
                                       const char *subject_slot_name,
                                       const char *reason)
{
    if (ctx == NULL || ctx->has_error)
        return;

    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "LLVM zone authority check could not be emitted for zone '%s' subject slot '%s': %s",
        zone_name != NULL ? zone_name : "<unknown>",
        subject_slot_name != NULL ? subject_slot_name : "<unknown>",
        reason != NULL ? reason : "missing backend metadata");
}

void
llvm_decl_emit_zone_authority_check(LLVMGenCtx *ctx)
{
    const MIRDeclHeader *zone_header;
    const MIRDeclZoneAuthority *authority;
    LLVMClassTypeEntry *zone_cls;
    LLVMVarEntry self_var;
    bool has_self_var;
    LLVMFuncEntry *check_fn;
    LLVMValueRef self_value;
    LLVMValueRef field_ptr;
    LLVMValueRef participant_value;
    LLVMTypeRef field_type;
    LLVMValueRef args[4];
    int field_index;
    const char *zone_name;
    const char *subject_slot;

    if (ctx == NULL)
        return;

    zone_header = llvm_decl_find_current_zone_header(ctx);
    if (zone_header == NULL) {
        zone_name = llvm_decl_current_zone_name(ctx);
        if (zone_name != NULL) {
            llvm_decl_zone_authority_backend_error(ctx, ctx->current_func_decl,
                zone_name, NULL,
                "current function declares a zone boundary but the zone declaration header is missing from LLVM MIR inventory");
        }
        return;
    }

    if (mir_decl_header_zone_authority_count(zone_header) == 0)
        return;

    authority = mir_decl_header_zone_authority(zone_header, 0);
    zone_name = mir_decl_header_name(zone_header);
    subject_slot = mir_decl_zone_authority_subject_slot_name(authority);
    if (subject_slot == NULL) {
        llvm_decl_zone_authority_backend_error(ctx, ctx->current_func_decl,
            zone_name, NULL,
            "authority metadata is malformed or lacks a subject slot");
        return;
    }

    zone_cls = zone_name != NULL ? llvm_lookup_class(ctx, zone_name) : NULL;
    has_self_var = llvm_scope_lookup_snapshot(ctx, "self", &self_var);
    check_fn = llvm_lookup_function(ctx, "pgy_zone_authority_check_export");
    if (zone_cls == NULL) {
        llvm_decl_zone_authority_backend_error(ctx, ctx->current_func_decl,
            zone_name,
            subject_slot,
            "zone class layout is missing");
        return;
    }
    if (!has_self_var) {
        llvm_decl_zone_authority_backend_error(ctx, ctx->current_func_decl,
            zone_name, subject_slot,
            "implicit self binding is missing");
        return;
    }
    if (check_fn == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM zone authority check runtime export is missing: pgy_zone_authority_check_export");
        return;
    }

    field_index = llvm_class_field_index(zone_cls, subject_slot);
    if (field_index < 0) {
        llvm_decl_zone_authority_backend_error(ctx, ctx->current_func_decl,
            zone_name,
            subject_slot,
            "authority subject slot is missing from the zone class layout");
        return;
    }

    self_value = LLVMBuildLoad2(ctx->builder, self_var.type, self_var.alloca,
        llvm_tmp_name(ctx));
    field_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
        self_value, (unsigned)field_index, llvm_tmp_name(ctx));
    field_type = LLVMStructGetTypeAtIndex(zone_cls->struct_type,
        (unsigned)field_index);
    participant_value = LLVMBuildLoad2(ctx->builder, field_type, field_ptr,
        llvm_tmp_name(ctx));

    args[0] = LLVMBuildBitCast(ctx->builder, self_value, ctx->type_i8ptr,
        llvm_tmp_name(ctx));
    if (LLVMGetTypeKind(field_type) == LLVMPointerTypeKind) {
        args[1] = LLVMBuildBitCast(ctx->builder, participant_value,
            ctx->type_i8ptr, llvm_tmp_name(ctx));
    } else {
        args[1] = LLVMBuildBitCast(ctx->builder, field_ptr, ctx->type_i8ptr,
            llvm_tmp_name(ctx));
    }
    args[2] = LLVMBuildGlobalStringPtr(ctx->builder, zone_name,
        llvm_tmp_name(ctx));
    args[3] = LLVMBuildGlobalStringPtr(ctx->builder, subject_slot,
        llvm_tmp_name(ctx));
    LLVMBuildCall2(ctx->builder, check_fn->fn_type, check_fn->fn, args, 4, "");
}

#endif /* PGY_LLVM_ENABLED */

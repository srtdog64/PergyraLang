/*
 * LLVM constructor channel-field rejection.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_constructor_channel_guard.h"

#include <stdlib.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"

static const char kLlvmMirSharedFieldMetadataMissing[] =
    "<mir-shared-field-metadata>";
static const char kLlvmMirClassFieldMetadataMissing[] =
    "<mir-class-field-metadata>";
static const char kLlvmMirDomainSlotMetadataMissing[] =
    "<mir-domain-slot-metadata>";

void
llvm_constructor_reject_channel_field(ASTNode *node,
                                      LLVMGenCtx *ctx,
                                      const char *field_name)
{
    if (ctx == NULL || ctx->has_error)
        return;
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_PROVIDE_MOVABLE_HANDLE,
        "LLVM backend: Channel field '%s' cannot be aggregate-constructed or default-initialized until movable channel-handle lowering is available",
        field_name != NULL ? field_name : "<field>");
}

static bool
llvm_constructor_field_is_channel(LLVMGenCtx *ctx, ASTNode *field_type)
{
    char *expected_type;
    bool is_channel;

    if (field_type == NULL)
        return false;
    if (field_type->type == AST_CHANNEL_TYPE)
        return true;
    expected_type = llvm_render_type_name_in_ctx(ctx, field_type);
    is_channel = pgy_classify_type(expected_type) == PGY_TK_CHANNEL;
    free(expected_type);
    return is_channel;
}

static const char *
llvm_class_constructor_find_channel_field(LLVMGenCtx *ctx, ASTNode *class_decl)
{
    const char *class_name;
    LLVMHostedFieldView class_fields;

    if (ctx == NULL || class_decl == NULL)
        return NULL;
    class_name = llvm_decl_node_name(class_decl);
    class_fields = llvm_hosted_class_field_view_from_decl(
        ctx, class_name, class_decl);
    if (llvm_hosted_field_view_missing_mir_metadata(&class_fields)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing class-field channel metadata for constructor '%s'",
            class_name != NULL ? class_name : "(anonymous-class)");
        return kLlvmMirClassFieldMetadataMissing;
    }
    for (size_t i = 0; i < class_fields.count; i++) {
        ASTNode *field_type =
            llvm_hosted_field_view_type(&class_fields, i);
        if (llvm_constructor_field_is_channel(ctx, field_type))
            return llvm_hosted_field_view_name(&class_fields, i);
    }
    return NULL;
}

static const char *
llvm_constructor_find_shared_channel_field(
    LLVMGenCtx *ctx,
    const LLVMHostedSharedFieldView *view)
{
    for (size_t i = 0; view != NULL && i < view->count; i++) {
        ASTNode *field_type = llvm_hosted_shared_field_view_type(view, i);
        if (llvm_constructor_field_is_channel(ctx, field_type))
            return llvm_hosted_shared_field_view_name(view, i);
    }
    return NULL;
}

static const char *
llvm_constructor_find_slot_channel_field(LLVMGenCtx *ctx,
                                         const LLVMHostedDomainSlotView *view)
{
    for (size_t i = 0; view != NULL && i < view->count; i++) {
        ASTNode *slot_type =
            llvm_hosted_domain_slot_view_type(view, i);
        if (llvm_constructor_field_is_channel(ctx, slot_type))
            return llvm_hosted_domain_slot_view_name(view, i);
    }
    return NULL;
}

static bool
llvm_constructor_mir_field_is_channel(LLVMGenCtx *ctx,
                                      const MIRDeclField *field)
{
    ASTNode *type_node;
    const char *type_name;

    if (field == NULL)
        return false;

    type_node = llvm_mir_decl_field_type(field);
    if (llvm_constructor_field_is_channel(ctx, type_node))
        return true;

    type_name = llvm_mir_decl_field_type_name(field);
    return pgy_classify_type(type_name) == PGY_TK_CHANNEL;
}

static const char *
llvm_constructor_find_mir_channel_field(LLVMGenCtx *ctx, ASTNode *decl)
{
    const MIRDeclHeader *header;
    const char *host_name;

    if (ctx == NULL || decl == NULL)
        return NULL;

    host_name = llvm_decl_node_name(decl);
    header = llvm_find_host_decl_header_in_context(ctx, host_name);
    for (size_t i = 0; header != NULL
         && i < mir_decl_header_field_count(header); i++) {
        const MIRDeclField *field = mir_decl_header_field(header, i);
        if (llvm_constructor_mir_field_is_channel(ctx, field))
            return mir_decl_field_name(field);
    }
    return NULL;
}

static const char *
llvm_constructor_fail_shared_metadata_missing(LLVMGenCtx *ctx,
                                              const char *decl_name)
{
    llvm_set_mir_inventory_missing(ctx,
        "MIR-only LLVM path missing shared-field channel metadata for constructor '%s'",
        decl_name != NULL ? decl_name : "(anonymous-domain)");
    return kLlvmMirSharedFieldMetadataMissing;
}

static const char *
llvm_constructor_fail_domain_slot_metadata_missing(LLVMGenCtx *ctx,
                                                   const char *decl_name)
{
    llvm_set_mir_inventory_missing(ctx,
        "MIR-only LLVM path missing domain-slot channel metadata for constructor '%s'",
        decl_name != NULL ? decl_name : "(anonymous-domain)");
    return kLlvmMirDomainSlotMetadataMissing;
}

const char *
llvm_constructor_find_host_channel_field(LLVMGenCtx *ctx, ASTNode *decl)
{
    const char *decl_name;
    const char *mir_channel_field;

    if (ctx == NULL || decl == NULL)
        return NULL;

    decl_name = llvm_decl_node_name(decl);

    mir_channel_field = llvm_constructor_find_mir_channel_field(ctx, decl);
    if (mir_channel_field != NULL)
        return mir_channel_field;

    if (decl->type == AST_CLASS_DECL)
        return llvm_class_constructor_find_channel_field(ctx, decl);

    switch (decl->type) {
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL: {
        LLVMHostedSharedFieldView shared =
            llvm_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
        if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared))
            return llvm_constructor_fail_shared_metadata_missing(
                ctx, decl_name);
        return llvm_constructor_find_shared_channel_field(ctx, &shared);
    }
    case AST_RELATION_DECL:
        {
            LLVMHostedDomainSlotView slot_view =
                llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, decl);
            if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view))
                return llvm_constructor_fail_domain_slot_metadata_missing(
                    ctx, decl_name);
            const char *slot =
                llvm_constructor_find_slot_channel_field(ctx, &slot_view);
            if (slot != NULL)
                return slot;
        }
        {
            LLVMHostedSharedFieldView shared =
                llvm_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
            if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared))
                return llvm_constructor_fail_shared_metadata_missing(
                    ctx, decl_name);
            return llvm_constructor_find_shared_channel_field(ctx, &shared);
        }
    case AST_EFFECT_DECL:
        {
            LLVMHostedDomainSlotView slot_view =
                llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, decl);
            if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view))
                return llvm_constructor_fail_domain_slot_metadata_missing(
                    ctx, decl_name);
            const char *slot =
                llvm_constructor_find_slot_channel_field(ctx, &slot_view);
            if (slot != NULL)
                return slot;
        }
        {
            LLVMHostedSharedFieldView shared =
                llvm_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
            if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared))
                return llvm_constructor_fail_shared_metadata_missing(
                    ctx, decl_name);
            return llvm_constructor_find_shared_channel_field(ctx, &shared);
        }
    case AST_ZONE_DECL:
        {
            LLVMHostedDomainSlotView slot_view =
                llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, decl);
            if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view))
                return llvm_constructor_fail_domain_slot_metadata_missing(
                    ctx, decl_name);
            const char *slot =
                llvm_constructor_find_slot_channel_field(ctx, &slot_view);
            if (slot != NULL)
                return slot;
        }
        {
            LLVMHostedSharedFieldView shared =
                llvm_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
            if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared))
                return llvm_constructor_fail_shared_metadata_missing(
                    ctx, decl_name);
            return llvm_constructor_find_shared_channel_field(ctx, &shared);
        }
    case AST_WORLD_DECL: {
        LLVMHostedSharedFieldView shared =
            llvm_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
        if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared))
            return llvm_constructor_fail_shared_metadata_missing(
                ctx, decl_name);
        return llvm_constructor_find_shared_channel_field(ctx, &shared);
    }
    default:
        return NULL;
    }
}

#endif

#include "transpiler_constructor_channel_guard.h"

#include <stdlib.h>

#include "../compiler/mir_decl_headers.h"
#include "../semantic/diag_codes.h"
#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_render.h"

static const char kMirSharedFieldMetadataMissing[] =
    "<mir-shared-field-metadata>";
static const char kMirClassFieldMetadataMissing[] =
    "<mir-class-field-metadata>";
static const char kMirDomainSlotMetadataMissing[] =
    "<mir-domain-slot-metadata>";

bool
transpiler_constructor_field_is_channel(TranspilerCtx *ctx,
                                        ASTNode *field_type)
{
    char *type_name;
    bool is_channel;

    if (field_type == NULL)
        return false;
    if (field_type->type == AST_CHANNEL_TYPE)
        return true;
    type_name = render_type_name_in_ctx(ctx, field_type);
    is_channel = transpiler_type_name_is_channel(type_name);
    free(type_name);
    return is_channel;
}

bool
transpiler_constructor_reject_channel_field(TranspilerCtx *ctx,
                                            const char *field_name)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_PROVIDE_MOVABLE_HANDLE,
        "C backend: Channel field '%s' cannot be aggregate-constructed or default-initialized until movable channel-handle lowering is available",
        field_name != NULL ? field_name : "<field>");
    return false;
}

static const char *
transpiler_constructor_find_shared_channel(
    TranspilerCtx *ctx,
    const TranspilerHostedSharedFieldView *view)
{
    for (size_t i = 0; view != NULL && i < view->count; i++) {
        ASTNode *field_type =
            transpiler_hosted_shared_field_view_type(view, i);
        if (transpiler_constructor_field_is_channel(ctx, field_type))
            return transpiler_hosted_shared_field_view_name(view, i);
    }
    return NULL;
}

static const char *
transpiler_constructor_find_class_channel(
    TranspilerCtx *ctx,
    const TranspilerHostedFieldView *view)
{
    for (size_t i = 0; view != NULL && i < view->count; i++) {
        ASTNode *field_type = transpiler_hosted_field_view_type(view, i);
        if (transpiler_constructor_field_is_channel(ctx, field_type))
            return transpiler_hosted_field_view_name(view, i);
    }
    return NULL;
}

static const char *
transpiler_constructor_find_slot_channel(TranspilerCtx *ctx,
                                         const TranspilerHostedDomainSlotView *view)
{
    for (size_t i = 0; view != NULL && i < view->count; i++) {
        ASTNode *slot_type =
            transpiler_hosted_domain_slot_view_type(view, i);
        if (transpiler_constructor_field_is_channel(ctx, slot_type))
            return transpiler_hosted_domain_slot_view_name(view, i);
    }
    return NULL;
}

static const char *
transpiler_constructor_fail_shared_metadata_missing(TranspilerCtx *ctx,
                                                    const char *decl_name)
{
    transpiler_set_mir_inventory_missing(ctx,
        "MIR-only C path missing shared-field channel metadata for constructor '%s'",
        decl_name != NULL ? decl_name : "(anonymous-domain)");
    return kMirSharedFieldMetadataMissing;
}

static const char *
transpiler_constructor_fail_class_metadata_missing(TranspilerCtx *ctx,
                                                  const char *decl_name)
{
    transpiler_set_mir_inventory_missing(ctx,
        "MIR-only C path missing class-field channel metadata for constructor '%s'",
        decl_name != NULL ? decl_name : "(anonymous-class)");
    return kMirClassFieldMetadataMissing;
}

static const char *
transpiler_constructor_fail_domain_slot_metadata_missing(
    TranspilerCtx *ctx,
    const char *decl_name)
{
    transpiler_set_mir_inventory_missing(ctx,
        "MIR-only C path missing domain-slot channel metadata for constructor '%s'",
        decl_name != NULL ? decl_name : "(anonymous-domain)");
    return kMirDomainSlotMetadataMissing;
}

static bool
transpiler_constructor_mir_field_is_channel(TranspilerCtx *ctx,
                                            const MIRDeclField *field)
{
    ASTNode *type_node;
    const char *type_name;

    if (field == NULL)
        return false;

    type_node = transpiler_mir_decl_field_type(field);
    if (transpiler_constructor_field_is_channel(ctx, type_node))
        return true;

    type_name = transpiler_mir_decl_field_type_name(field);
    return transpiler_type_name_is_channel(type_name);
}

static const char *
transpiler_constructor_find_mir_channel_field(TranspilerCtx *ctx,
                                              ASTNode *decl)
{
    const MIRDeclHeader *header;
    const char *host_name;

    if (ctx == NULL || decl == NULL)
        return NULL;

    host_name = transpiler_decl_name_local(decl);
    header = transpiler_active_host_decl_header(ctx, host_name);
    for (size_t i = 0; header != NULL
         && i < mir_decl_header_field_count(header); i++) {
        const MIRDeclField *field = mir_decl_header_field(header, i);
        if (transpiler_constructor_mir_field_is_channel(ctx, field))
            return mir_decl_field_name(field);
    }
    return NULL;
}

const char *
transpiler_constructor_find_channel_field(TranspilerCtx *ctx, ASTNode *decl)
{
    const char *decl_name;
    const char *mir_channel_field;

    if (ctx == NULL || decl == NULL)
        return NULL;

    decl_name = transpiler_decl_name_local(decl);

    mir_channel_field = transpiler_constructor_find_mir_channel_field(
        ctx, decl);
    if (mir_channel_field != NULL)
        return mir_channel_field;

    if (decl->type == AST_CLASS_DECL) {
        TranspilerHostedFieldView fields =
            transpiler_hosted_class_field_view_from_decl(ctx, decl_name, decl);
        if (transpiler_hosted_field_view_missing_mir_metadata(&fields))
            return transpiler_constructor_fail_class_metadata_missing(
                ctx, decl_name);
        return transpiler_constructor_find_class_channel(ctx, &fields);
    }

    switch (decl->type) {
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL: {
        TranspilerHostedSharedFieldView shared =
            transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
        if (transpiler_hosted_shared_field_view_missing_mir_metadata(&shared))
            return transpiler_constructor_fail_shared_metadata_missing(
                ctx, decl_name);
        return transpiler_constructor_find_shared_channel(ctx, &shared);
    }
    case AST_RELATION_DECL:
        {
            TranspilerHostedDomainSlotView slot_view =
                transpiler_hosted_domain_slot_view_from_decl(ctx, decl_name,
                    decl);
            if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
                    &slot_view)) {
                return transpiler_constructor_fail_domain_slot_metadata_missing(
                    ctx, decl_name);
            }
            const char *slot =
                transpiler_constructor_find_slot_channel(ctx, &slot_view);
            if (slot != NULL)
                return slot;
        }
        {
            TranspilerHostedSharedFieldView shared =
                transpiler_hosted_shared_field_view_from_decl(
                    ctx, decl_name, decl);
            if (transpiler_hosted_shared_field_view_missing_mir_metadata(
                    &shared)) {
                return transpiler_constructor_fail_shared_metadata_missing(
                    ctx, decl_name);
            }
            return transpiler_constructor_find_shared_channel(ctx, &shared);
        }
    case AST_EFFECT_DECL:
        {
            TranspilerHostedDomainSlotView slot_view =
                transpiler_hosted_domain_slot_view_from_decl(ctx, decl_name,
                    decl);
            if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
                    &slot_view)) {
                return transpiler_constructor_fail_domain_slot_metadata_missing(
                    ctx, decl_name);
            }
            const char *slot =
                transpiler_constructor_find_slot_channel(ctx, &slot_view);
            if (slot != NULL)
                return slot;
        }
        {
            TranspilerHostedSharedFieldView shared =
                transpiler_hosted_shared_field_view_from_decl(
                    ctx, decl_name, decl);
            if (transpiler_hosted_shared_field_view_missing_mir_metadata(
                    &shared)) {
                return transpiler_constructor_fail_shared_metadata_missing(
                    ctx, decl_name);
            }
            return transpiler_constructor_find_shared_channel(ctx, &shared);
        }
    case AST_ZONE_DECL:
        {
            TranspilerHostedDomainSlotView slot_view =
                transpiler_hosted_domain_slot_view_from_decl(ctx, decl_name,
                    decl);
            if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
                    &slot_view)) {
                return transpiler_constructor_fail_domain_slot_metadata_missing(
                    ctx, decl_name);
            }
            const char *slot =
                transpiler_constructor_find_slot_channel(ctx, &slot_view);
            if (slot != NULL)
                return slot;
        }
        {
            TranspilerHostedSharedFieldView shared =
                transpiler_hosted_shared_field_view_from_decl(
                    ctx, decl_name, decl);
            if (transpiler_hosted_shared_field_view_missing_mir_metadata(
                    &shared)) {
                return transpiler_constructor_fail_shared_metadata_missing(
                    ctx, decl_name);
            }
            return transpiler_constructor_find_shared_channel(ctx, &shared);
        }
    case AST_WORLD_DECL: {
        TranspilerHostedSharedFieldView shared =
            transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
        if (transpiler_hosted_shared_field_view_missing_mir_metadata(&shared))
            return transpiler_constructor_fail_shared_metadata_missing(
                ctx, decl_name);
        return transpiler_constructor_find_shared_channel(ctx, &shared);
    }
    default:
        return NULL;
    }
}

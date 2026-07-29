#include "transpiler_domain_constructor_emit.h"

#include <stdlib.h>

#include "../common/string_compat.h"
#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_constructor_channel_guard.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_constructor_internal.h"
#include "transpiler_format.h"
#include "transpiler_inventory_view.h"
#include "transpiler_projection.h"

static char *
transpiler_emit_party_constructor(ASTNode *call,
                                  ASTNode *party_decl,
                                  const char *type_name,
                                  TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    const char *decl_name = transpiler_decl_name_local(party_decl);
    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, party_decl);
    size_t shared_count = shared_view.count;
    CodeBuf *fields = codebuf_create();

    if (transpiler_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing shared-field declaration metadata for party '%s'",
            decl_name != NULL ? decl_name : "(anonymous-party)");
        codebuf_destroy(fields);
        return NULL;
    }

    for (size_t i = 0; i < argc && i < shared_count; i++) {
        const char *field_name =
            transpiler_hosted_shared_field_view_name(&shared_view, i);
        char *arg = transpiler_emit_ctor_arg_from_field_abi(ctx,
            transpiler_hosted_shared_field_view_type_name(&shared_view, i),
            field_name,
            ast_call_argument(call, i), i);
        if (arg == NULL) {
            codebuf_destroy(fields);
            return NULL;
        }
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg);
        free(arg);
    }

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *initializer;
        const char *field_name;
        char *init_expr;
        if (i < argc)
            continue;
        initializer = transpiler_hosted_shared_field_view_initializer(
            &shared_view, i);
        if (initializer == NULL)
            continue;
        field_name = transpiler_hosted_shared_field_view_name(&shared_view, i);
        init_expr = transpiler_emit_ctor_arg_from_field_abi(ctx,
            transpiler_hosted_shared_field_view_type_name(&shared_view, i),
            field_name,
            initializer, i);
        if (init_expr == NULL) {
            codebuf_destroy(fields);
            return NULL;
        }
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            init_expr);
        free(init_expr);
    }

    {
        char *result = fields->len > 0
            ? strdup_fmt("(%s){ %s }", type_name, fields->data)
            : strdup_fmt("(%s){0}", type_name);
        codebuf_destroy(fields);
        return result;
    }
}

static char *
transpiler_emit_roster_constructor(ASTNode *call,
                                   ASTNode *roster_decl,
                                   const char *type_name,
                                   TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    const char *decl_name = transpiler_decl_name_local(roster_decl);
    TranspilerHostedRosterSlotView roster_view =
        transpiler_hosted_roster_slot_view_from_decl(ctx, decl_name,
            roster_decl);
    size_t roster_party_count = roster_view.count;
    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, roster_decl);
    size_t shared_count = shared_view.count;
    size_t exposed = roster_party_count + shared_count;
    CodeBuf *fields = codebuf_create();

    if (transpiler_hosted_roster_slot_view_missing_mir_metadata(
            &roster_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing roster-slot declaration metadata for roster '%s'",
            decl_name != NULL ? decl_name : "(anonymous-roster)");
        codebuf_destroy(fields);
        return NULL;
    }
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing shared-field declaration metadata for roster '%s'",
            decl_name != NULL ? decl_name : "(anonymous-roster)");
        codebuf_destroy(fields);
        return NULL;
    }

    for (size_t i = 0; i < argc && i < exposed; i++) {
        const char *field_name = NULL;
        const char *field_type_name = NULL;
        if (i < roster_party_count) {
            field_name = transpiler_hosted_roster_slot_view_name(
                &roster_view, i);
            field_type_name = transpiler_hosted_roster_slot_view_type_name(
                &roster_view, i);
        } else {
            size_t shared_index = i - roster_party_count;
            field_name = transpiler_hosted_shared_field_view_name(
                &shared_view, shared_index);
            field_type_name = transpiler_hosted_shared_field_view_type_name(
                &shared_view, shared_index);
        }
        char *arg = transpiler_emit_ctor_arg_from_field_abi(ctx,
            field_type_name,
            field_name,
            ast_call_argument(call, i), i);
        if (arg == NULL) {
            codebuf_destroy(fields);
            return NULL;
        }
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg);
        free(arg);
    }

    for (size_t i = 0; i < shared_count; i++) {
        size_t absolute_index = roster_party_count + i;
        ASTNode *initializer;
        const char *field_name;
        char *init_expr;
        if (absolute_index < argc)
            continue;
        initializer = transpiler_hosted_shared_field_view_initializer(
            &shared_view, i);
        if (initializer == NULL)
            continue;
        field_name = transpiler_hosted_shared_field_view_name(&shared_view, i);
        init_expr = transpiler_emit_ctor_arg_from_field_abi(ctx,
            transpiler_hosted_shared_field_view_type_name(&shared_view, i),
            field_name,
            initializer, i);
        if (init_expr == NULL) {
            codebuf_destroy(fields);
            return NULL;
        }
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            init_expr);
        free(init_expr);
    }

    {
        char *result = fields->len > 0
            ? strdup_fmt("(%s){ %s }", type_name, fields->data)
            : strdup_fmt("(%s){0}", type_name);
        codebuf_destroy(fields);
        return result;
    }
}

static const char *
transpiler_domain_constructor_input_slot_at(
    const TranspilerHostedDomainSlotView *slot_view,
    size_t input_index,
    size_t *slot_index_out);

static char *
transpiler_emit_relation_effect_constructor(ASTNode *call,
                                            ASTNode *decl,
                                            const char *type_name,
                                            TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    const char *decl_name = transpiler_decl_name_local(decl);
    TranspilerHostedDomainSlotView slot_view =
        transpiler_hosted_domain_slot_view_from_decl(ctx, decl_name, decl);
    size_t slot_count = slot_view.count;
    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
    size_t shared_count = shared_view.count;
    TranspilerHostedZoneRefreshView refresh_view =
        transpiler_hosted_zone_refresh_view_from_decl(ctx, decl_name, decl);
    CodeBuf *fields = codebuf_create();

    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
            &slot_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing domain-slot declaration metadata for domain '%s'",
            decl_name != NULL ? decl_name : "(anonymous-domain)");
        codebuf_destroy(fields);
        return NULL;
    }
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing shared-field declaration metadata for domain '%s'",
            decl_name != NULL ? decl_name : "(anonymous-domain)");
        codebuf_destroy(fields);
        return NULL;
    }
    if (transpiler_hosted_zone_refresh_view_missing_mir_metadata(
            &refresh_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing domain refresh constructor metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-domain)");
        codebuf_destroy(fields);
        return NULL;
    }

    for (size_t i = 0; i < argc; i++) {
        size_t slot_index = 0;
        const char *field_name =
            transpiler_domain_constructor_input_slot_at(
                &slot_view, i, &slot_index);
        const char *field_type_name = field_name != NULL
            ? transpiler_hosted_domain_slot_view_type_name(
                &slot_view, slot_index)
            : NULL;
        if (field_name == NULL || field_type_name == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing admitted domain constructor input metadata for '%s' argument %zu",
                decl_name != NULL ? decl_name : "(anonymous-domain)", i);
            codebuf_destroy(fields);
            return NULL;
        }
        char *arg = transpiler_emit_ctor_arg_from_field_abi(ctx,
            field_type_name,
            field_name,
            ast_call_argument(call, i), i);
        if (arg == NULL) {
            codebuf_destroy(fields);
            return NULL;
        }
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg);
        free(arg);
    }

    for (size_t i = 0; i < shared_count; i++) {
        size_t absolute_index = slot_count + i;
        ASTNode *initializer;
        const char *field_name;
        char *init_expr;
        if (absolute_index < argc)
            continue;
        initializer = transpiler_hosted_shared_field_view_initializer(
            &shared_view, i);
        if (initializer == NULL)
            continue;
        field_name = transpiler_hosted_shared_field_view_name(&shared_view, i);
        init_expr = transpiler_emit_ctor_arg_from_field_abi(ctx,
            transpiler_hosted_shared_field_view_type_name(&shared_view, i),
            field_name,
            initializer, i);
        if (init_expr == NULL) {
            codebuf_destroy(fields);
            return NULL;
        }
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            init_expr);
        free(init_expr);
    }

    for (size_t i = 0; i < slot_count; i++) {
        const char *slot_name =
            transpiler_hosted_domain_slot_view_name(&slot_view, i);
        bool projection_slot =
            transpiler_domain_slot_view_is_projection_slot_in_zone_refresh_view(
                &slot_view, i, &refresh_view);
        if (!projection_slot || slot_name == NULL)
            continue;
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".__projection_dirty_%s = true", slot_name);
    }

    {
        char *result = fields->len > 0
            ? strdup_fmt("(%s){ %s }", type_name, fields->data)
            : strdup_fmt("(%s){0}", type_name);
        codebuf_destroy(fields);
        return result;
    }
}

static const char *
transpiler_domain_constructor_input_slot_at(
    const TranspilerHostedDomainSlotView *slot_view,
    size_t input_index,
    size_t *slot_index_out)
{
    size_t seen = 0;

    if (slot_index_out != NULL)
        *slot_index_out = 0;
    if (slot_view == NULL)
        return NULL;
    for (size_t i = 0; i < slot_view->count; i++) {
        if (!transpiler_hosted_domain_slot_view_is_subject_like(
                slot_view, i)
            && !transpiler_hosted_domain_slot_view_is_binding_like(
                slot_view, i)) {
            continue;
        }
        if (seen == input_index) {
            if (slot_index_out != NULL)
                *slot_index_out = i;
            return transpiler_hosted_domain_slot_view_name(slot_view, i);
        }
        seen++;
    }
    return NULL;
}

static char *
transpiler_emit_zone_constructor(ASTNode *call,
                                 ASTNode *zone_decl,
                                 const char *type_name,
                                 TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    const char *decl_name = transpiler_decl_name_local(zone_decl);
    TranspilerHostedDomainSlotView slot_view =
        transpiler_hosted_domain_slot_view_from_decl(ctx, decl_name,
            zone_decl);
    size_t slot_count = slot_view.count;
    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, zone_decl);
    size_t shared_count = shared_view.count;
    TranspilerHostedZoneRefreshView refresh_view =
        transpiler_hosted_zone_refresh_view_from_decl(ctx, decl_name,
            zone_decl);
    CodeBuf *fields = codebuf_create();

    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
            &slot_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing domain-slot declaration metadata for zone '%s'",
            decl_name != NULL ? decl_name : "(anonymous-zone)");
        codebuf_destroy(fields);
        return NULL;
    }
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing shared-field declaration metadata for zone '%s'",
            decl_name != NULL ? decl_name : "(anonymous-zone)");
        codebuf_destroy(fields);
        return NULL;
    }
    if (transpiler_hosted_zone_refresh_view_missing_mir_metadata(
            &refresh_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone refresh constructor metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-zone)");
        codebuf_destroy(fields);
        return NULL;
    }

    for (size_t i = 0; i < argc; i++) {
        size_t slot_index = 0;
        const char *field_name =
            transpiler_domain_constructor_input_slot_at(
                &slot_view, i, &slot_index);
        const char *field_type_name = field_name != NULL
            ? transpiler_hosted_domain_slot_view_type_name(
                &slot_view, slot_index)
            : NULL;
        if (field_name == NULL || field_type_name == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing admitted zone constructor input metadata for '%s' argument %zu",
                decl_name != NULL ? decl_name : "(anonymous-zone)", i);
            codebuf_destroy(fields);
            return NULL;
        }
        char *arg = transpiler_emit_ctor_arg_from_field_abi(ctx,
            field_type_name,
            field_name,
            ast_call_argument(call, i), i);
        if (arg == NULL) {
            codebuf_destroy(fields);
            return NULL;
        }
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg);
        free(arg);
    }

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *initializer;
        const char *field_name;
        char *init_expr;
        initializer = transpiler_hosted_shared_field_view_initializer(
            &shared_view, i);
        if (initializer == NULL)
            continue;
        field_name = transpiler_hosted_shared_field_view_name(&shared_view, i);
        init_expr = transpiler_emit_ctor_arg_from_field_abi(ctx,
            transpiler_hosted_shared_field_view_type_name(&shared_view, i),
            field_name,
            initializer, i);
        if (init_expr == NULL) {
            codebuf_destroy(fields);
            return NULL;
        }
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            init_expr);
        free(init_expr);
    }

    for (size_t i = 0; i < slot_count; i++) {
        const char *slot_name =
            transpiler_hosted_domain_slot_view_name(&slot_view, i);
        bool projection_slot =
            transpiler_domain_slot_view_is_projection_slot_in_zone_refresh_view(
                &slot_view, i, &refresh_view);
        if (!projection_slot || slot_name == NULL)
            continue;
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".__projection_dirty_%s = true", slot_name);
    }

    {
        char *result = fields->len > 0
            ? strdup_fmt("(%s){ %s }", type_name, fields->data)
            : strdup_fmt("(%s){0}", type_name);
        codebuf_destroy(fields);
        return result;
    }
}

static char *
transpiler_emit_world_constructor(ASTNode *call,
                                  ASTNode *world_decl,
                                  const char *type_name,
                                  TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    CodeBuf *fields = codebuf_create();
    const char *decl_name = transpiler_decl_name_local(world_decl);
    TranspilerHostedWorldRosterSlotView roster_view =
        transpiler_hosted_world_roster_slot_view_from_decl(ctx, decl_name,
            world_decl);
    size_t roster_count = roster_view.count;
    TranspilerHostedWorldZoneSlotView zone_view =
        transpiler_hosted_world_zone_slot_view_from_decl(ctx, decl_name,
            world_decl);
    size_t zone_count = zone_view.count;
    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, world_decl);
    size_t shared_count = shared_view.count;
    size_t exposed = roster_count + zone_count + shared_count;

    if (transpiler_hosted_world_roster_slot_view_missing_mir_metadata(
            &roster_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing roster-slot declaration metadata for world '%s'",
            decl_name != NULL ? decl_name : "(anonymous-world)");
        codebuf_destroy(fields);
        return NULL;
    }
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing shared-field declaration metadata for world '%s'",
            decl_name != NULL ? decl_name : "(anonymous-world)");
        codebuf_destroy(fields);
        return NULL;
    }
    if (transpiler_hosted_world_zone_slot_view_missing_mir_metadata(
            &zone_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone-slot declaration metadata for world '%s'",
            decl_name != NULL ? decl_name : "(anonymous-world)");
        codebuf_destroy(fields);
        return NULL;
    }

    for (size_t i = 0; i < argc && i < exposed; i++) {
        const char *field_name = NULL;
        const char *field_type_name = NULL;
        if (i < roster_count) {
            field_name = transpiler_hosted_world_roster_slot_view_name(
                &roster_view, i);
            field_type_name = transpiler_hosted_world_roster_slot_view_type_name(
                &roster_view, i);
        } else if (i < roster_count + zone_count) {
            field_name = transpiler_hosted_world_zone_slot_view_name(
                &zone_view, i - roster_count);
            field_type_name = transpiler_hosted_world_zone_slot_view_type_name(
                &zone_view, i - roster_count);
        } else {
            field_name = transpiler_hosted_shared_field_view_name(
                &shared_view, i - roster_count - zone_count);
            field_type_name = transpiler_hosted_shared_field_view_type_name(
                &shared_view, i - roster_count - zone_count);
        }
        char *arg = transpiler_emit_ctor_arg_from_field_abi(ctx,
            field_type_name,
            field_name,
            ast_call_argument(call, i), i);
        if (arg == NULL) {
            codebuf_destroy(fields);
            return NULL;
        }
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg);
        free(arg);
    }

    for (size_t i = 0; i < shared_count; i++) {
        size_t absolute_index = roster_count + zone_count + i;
        ASTNode *initializer;
        const char *field_name;
        char *init_expr;
        if (absolute_index < argc)
            continue;
        initializer = transpiler_hosted_shared_field_view_initializer(
            &shared_view, i);
        if (initializer == NULL)
            continue;
        field_name = transpiler_hosted_shared_field_view_name(&shared_view, i);
        init_expr = transpiler_emit_ctor_arg_from_field_abi(ctx,
            transpiler_hosted_shared_field_view_type_name(&shared_view, i),
            field_name,
            initializer, i);
        if (init_expr == NULL) {
            codebuf_destroy(fields);
            return NULL;
        }
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            init_expr);
        free(init_expr);
    }

    for (size_t i = 0; i < zone_count; i++) {
        const char *slot_name =
            transpiler_hosted_world_zone_slot_view_name(&zone_view, i);
        if (slot_name == NULL)
            continue;
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".__zone_dirty_%s = true", slot_name);
    }
    if (fields->len > 0)
        codebuf_write(fields, ", ");
    codebuf_write(fields, ".__world_derived_dirty = true");

    {
        char *result = fields->len > 0
            ? strdup_fmt("(%s){ %s }", type_name, fields->data)
            : strdup_fmt("(%s){0}", type_name);
        codebuf_destroy(fields);
        return result;
    }
}

char *
transpiler_emit_domain_constructor_for_decl(ASTNode *call,
                                            ASTNode *decl,
                                            const char *type_name,
                                            TranspilerCtx *ctx)
{
    if (call == NULL || decl == NULL || type_name == NULL)
        return NULL;
    {
        const char *channel_field =
            transpiler_constructor_find_channel_field(ctx, decl);
        if (channel_field != NULL) {
            transpiler_constructor_reject_channel_field(ctx, channel_field);
            return NULL;
        }
    }

    switch (decl->type) {
    case AST_PARTY_DECL:
        return transpiler_emit_party_constructor(call, decl, type_name, ctx);
    case AST_ROSTER_DECL:
        return transpiler_emit_roster_constructor(call, decl, type_name, ctx);
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
        return transpiler_emit_relation_effect_constructor(
            call, decl, type_name, ctx);
    case AST_ZONE_DECL:
        return transpiler_emit_zone_constructor(call, decl, type_name, ctx);
    case AST_WORLD_DECL:
        return transpiler_emit_world_constructor(call, decl, type_name, ctx);
    default:
        return NULL;
    }
}

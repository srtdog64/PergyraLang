#include "transpiler_domain_constructor_emit.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "parser/ast_api.h"
#include "host_decl_compat.h"
#include "transpiler_context.h"
#include "transpiler_constructor_channel_guard.h"
#include "transpiler_format.h"
#include "transpiler_projection.h"
#include "transpiler_type_render.h"

static char *
transpiler_emit_ctor_arg_with_expected_type(TranspilerCtx *ctx,
                                            ASTNode *field_type,
                                            const char *field_name,
                                            ASTNode *arg)
{
    char *expected_type;
    const char *saved_expected_type;
    char *result;

    if (field_type == NULL)
        return emit_expression(arg, ctx);

    if (transpiler_constructor_field_is_channel(ctx, field_type)) {
        transpiler_constructor_reject_channel_field(ctx, field_name);
        return pergyra_strdup("0");
    }

    expected_type = render_type_name_in_ctx(ctx, field_type);
    saved_expected_type = ctx->expected_type;
    ctx->expected_type = expected_type;
    result = emit_expression(arg, ctx);
    ctx->expected_type = saved_expected_type;
    free(expected_type);
    return result;
}

char *
transpiler_emit_class_constructor_with_type(ASTNode *call,
                                            ASTNode *class_decl,
                                            const char *ctor_type,
                                            TranspilerCtx *ctx)
{
    size_t argc;
    PgyHostClassFieldsCompatView field_view;
    ClassField **fields_list;
    CodeBuf *fields;
    char *result;

    if (call == NULL || class_decl == NULL || ctor_type == NULL)
        return NULL;

    argc = ast_call_arg_count(call);
    fields = codebuf_create();
    field_view = pgy_host_class_fields_compat_view_from_decl(class_decl);
    fields_list = field_view.fields;
    for (size_t i = 0; i < argc && i < field_view.count; i++) {
        ClassField *field = fields_list != NULL ? fields_list[i] : NULL;
        char *arg = transpiler_emit_ctor_arg_with_expected_type(ctx,
            field != NULL ? field->type : NULL,
            field != NULL ? field->name : NULL,
            ast_call_argument(call, i));
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field != NULL && field->name != NULL ? field->name : "field",
            arg != NULL ? arg : "0");
        free(arg);
    }

    if (fields->len > 0)
        result = strdup_fmt("(%s){ %s }", ctor_type, fields->data);
    else
        result = strdup_fmt("(%s){0}", ctor_type);
    codebuf_destroy(fields);
    return result;
}

static char *
transpiler_emit_party_constructor(ASTNode *call,
                                  ASTNode *party_decl,
                                  const char *type_name,
                                  TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    size_t shared_count = ast_party_shared_count(party_decl);
    CodeBuf *fields = codebuf_create();

    for (size_t i = 0; i < argc && i < shared_count; i++) {
        ASTNode *shared = ast_party_shared(party_decl, i);
        const char *field_name = ast_party_shared_name(shared);
        char *arg = transpiler_emit_ctor_arg_with_expected_type(ctx,
            shared != NULL ? ast_party_shared_type(shared) : NULL,
            field_name,
            ast_call_argument(call, i));
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg != NULL ? arg : "0");
        free(arg);
    }

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared;
        const char *field_name;
        char *init_expr;
        if (i < argc)
            continue;
        shared = ast_party_shared(party_decl, i);
        if (shared == NULL || ast_party_shared_initializer(shared) == NULL)
            continue;
        field_name = ast_party_shared_name(shared);
        init_expr = transpiler_emit_ctor_arg_with_expected_type(ctx,
            ast_party_shared_type(shared),
            field_name,
            ast_party_shared_initializer(shared));
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            init_expr != NULL ? init_expr : "0");
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
    size_t roster_party_count = ast_roster_party_count(roster_decl);
    size_t exposed = roster_party_count + ast_roster_shared_count(roster_decl);
    CodeBuf *fields = codebuf_create();

    for (size_t i = 0; i < argc && i < exposed; i++) {
        const char *field_name = NULL;
        ASTNode *field_type = NULL;
        ASTNode *slot = NULL;
        ASTNode *shared = NULL;
        if (i < roster_party_count) {
            slot = ast_roster_party(roster_decl, i);
            field_name = ast_roster_slot_name(slot) != NULL
                ? ast_roster_slot_name(slot) : "field";
        } else {
            shared = ast_roster_shared(roster_decl,
                i - roster_party_count);
            field_name = ast_party_shared_name(shared);
            field_type = ast_party_shared_type(shared);
        }
        char *arg = transpiler_emit_ctor_arg_with_expected_type(ctx,
            field_type,
            field_name,
            ast_call_argument(call, i));
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg != NULL ? arg : "0");
        free(arg);
    }

    for (size_t i = 0; i < ast_roster_shared_count(roster_decl); i++) {
        size_t absolute_index = roster_party_count + i;
        ASTNode *shared;
        const char *field_name;
        char *init_expr;
        if (absolute_index < argc)
            continue;
        shared = ast_roster_shared(roster_decl, i);
        if (shared == NULL || ast_party_shared_initializer(shared) == NULL)
            continue;
        field_name = ast_party_shared_name(shared);
        init_expr = transpiler_emit_ctor_arg_with_expected_type(ctx,
            ast_party_shared_type(shared),
            field_name,
            ast_party_shared_initializer(shared));
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            init_expr != NULL ? init_expr : "0");
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
transpiler_emit_relation_effect_constructor(ASTNode *call,
                                            ASTNode *decl,
                                            const char *type_name,
                                            TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    size_t slot_count = 0;
    PgyHostSharedFieldsCompatView shared_view =
        pgy_host_shared_fields_compat_view_from_decl(decl);
    size_t shared_count = shared_view.count;
    size_t refresh_count = 0;
    ASTNode **slots = decl->type == AST_RELATION_DECL
        ? ast_relation_slots(decl, &slot_count)
        : ast_effect_slots(decl, &slot_count);
    ASTNode **shared_fields = shared_view.fields;
    ASTNode **refreshes = decl->type == AST_RELATION_DECL
        ? ast_relation_refreshes(decl, &refresh_count)
        : ast_effect_refreshes(decl, &refresh_count);
    CodeBuf *fields = codebuf_create();

    for (size_t i = 0; i < argc && i < slot_count + shared_count; i++) {
        const char *field_name = NULL;
        ASTNode *field_type = NULL;
        ASTNode *slot = NULL;
        ASTNode *shared = NULL;
        if (i < slot_count) {
            slot = slots[i];
            field_name = ast_domain_slot_name(slot);
            field_type = ast_domain_slot_type(slot);
        } else {
            shared = shared_fields[i - slot_count];
            field_name = ast_party_shared_name(shared);
            field_type = ast_party_shared_type(shared);
        }
        char *arg = transpiler_emit_ctor_arg_with_expected_type(ctx,
            field_type,
            field_name,
            ast_call_argument(call, i));
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg != NULL ? arg : "0");
        free(arg);
    }

    for (size_t i = 0; i < shared_count; i++) {
        size_t absolute_index = slot_count + i;
        ASTNode *shared;
        const char *field_name;
        char *init_expr;
        if (absolute_index < argc)
            continue;
        shared = shared_fields[i];
        if (shared == NULL || ast_party_shared_initializer(shared) == NULL)
            continue;
        field_name = ast_party_shared_name(shared);
        init_expr = transpiler_emit_ctor_arg_with_expected_type(ctx,
            ast_party_shared_type(shared),
            field_name,
            ast_party_shared_initializer(shared));
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            init_expr != NULL ? init_expr : "0");
        free(init_expr);
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *slot_name = ast_domain_slot_name(slot);
        bool projection_slot = slot != NULL
            && (ast_domain_slot_is_tobject(slot)
                || transpiler_domain_slot_is_projection_target(
                    slot, refreshes, refresh_count));
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
transpiler_emit_zone_constructor(ASTNode *call,
                                 ASTNode *zone_decl,
                                 const char *type_name,
                                 TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    size_t slot_count = 0;
    ASTNode **slots = ast_zone_slots(zone_decl, &slot_count);
    PgyHostSharedFieldsCompatView shared_view =
        pgy_host_shared_fields_compat_view_from_decl(zone_decl);
    size_t shared_count = shared_view.count;
    ASTNode **shared_fields = shared_view.fields;
    size_t refresh_count = 0;
    ASTNode **refreshes = ast_zone_refreshes(zone_decl, &refresh_count);
    CodeBuf *fields = codebuf_create();

    for (size_t i = 0; i < argc && i < slot_count + shared_count; i++) {
        const char *field_name = NULL;
        ASTNode *field_type = NULL;
        ASTNode *slot = NULL;
        ASTNode *shared = NULL;
        if (i < slot_count) {
            slot = slots[i];
            field_name = ast_domain_slot_name(slot);
            field_type = ast_domain_slot_type(slot);
        } else {
            shared = shared_fields[i - slot_count];
            field_name = ast_party_shared_name(shared);
            field_type = ast_party_shared_type(shared);
        }
        char *arg = transpiler_emit_ctor_arg_with_expected_type(ctx,
            field_type,
            field_name,
            ast_call_argument(call, i));
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg != NULL ? arg : "0");
        free(arg);
    }

    for (size_t i = 0; i < shared_count; i++) {
        size_t absolute_index = slot_count + i;
        ASTNode *shared;
        const char *field_name;
        char *init_expr;
        if (absolute_index < argc)
            continue;
        shared = shared_fields[i];
        if (shared == NULL || ast_party_shared_initializer(shared) == NULL)
            continue;
        field_name = ast_party_shared_name(shared);
        init_expr = transpiler_emit_ctor_arg_with_expected_type(ctx,
            ast_party_shared_type(shared),
            field_name,
            ast_party_shared_initializer(shared));
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            init_expr != NULL ? init_expr : "0");
        free(init_expr);
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *slot_name = ast_domain_slot_name(slot);
        bool projection_slot = slot != NULL
            && (ast_domain_slot_is_tobject(slot)
                || transpiler_domain_slot_is_projection_target(
                    slot, refreshes, refresh_count));
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
    size_t roster_count = 0;
    ASTNode **rosters = ast_world_rosters(world_decl, &roster_count);
    size_t zone_count = 0;
    ASTNode **zones = ast_world_zones(world_decl, &zone_count);
    PgyHostSharedFieldsCompatView shared_view =
        pgy_host_shared_fields_compat_view_from_decl(world_decl);
    size_t shared_count = shared_view.count;
    ASTNode **shared_fields = shared_view.fields;
    size_t exposed = roster_count + zone_count + shared_count;

    for (size_t i = 0; i < argc && i < exposed; i++) {
        const char *field_name = NULL;
        ASTNode *field_type = NULL;
        ASTNode *slot = NULL;
        ASTNode *shared = NULL;
        if (i < roster_count) {
            slot = rosters[i];
            field_name = ast_world_roster_slot_name(slot);
        } else if (i < roster_count + zone_count) {
            slot = zones[i - roster_count];
            field_name = ast_world_zone_slot_name(slot);
        } else {
            shared = shared_fields[i - roster_count - zone_count];
            field_name = ast_party_shared_name(shared);
            field_type = ast_party_shared_type(shared);
        }
        char *arg = transpiler_emit_ctor_arg_with_expected_type(ctx,
            field_type,
            field_name,
            ast_call_argument(call, i));
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg != NULL ? arg : "0");
        free(arg);
    }

    for (size_t i = 0; i < shared_count; i++) {
        size_t absolute_index = roster_count + zone_count + i;
        ASTNode *shared;
        const char *field_name;
        char *init_expr;
        if (absolute_index < argc)
            continue;
        shared = shared_fields[i];
        if (shared == NULL || ast_party_shared_initializer(shared) == NULL)
            continue;
        field_name = ast_party_shared_name(shared);
        init_expr = transpiler_emit_ctor_arg_with_expected_type(ctx,
            ast_party_shared_type(shared),
            field_name,
            ast_party_shared_initializer(shared));
        if (fields->len > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            init_expr != NULL ? init_expr : "0");
        free(init_expr);
    }

    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        const char *slot_name = ast_world_zone_slot_name(zone);
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
            return pergyra_strdup("0");
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

char *
transpiler_emit_enum_variant_constructor(ASTNode *call,
                                         const char *qualified_name,
                                         TranspilerCtx *ctx)
{
    size_t argc;
    char **arg_strs;
    size_t buf_len;
    char *result;

    (void)ctx;
    if (call == NULL || qualified_name == NULL)
        return NULL;

    argc = ast_call_arg_count(call);
    if (argc > SIZE_MAX / sizeof(char *))
        return NULL;
    arg_strs = calloc(argc > 0 ? argc : 1, sizeof(char *));
    if (arg_strs == NULL)
        return NULL;
    for (size_t i = 0; i < argc; i++)
        arg_strs[i] = emit_expression(ast_call_argument(call, i), ctx);

    buf_len = strlen(qualified_name) + 3;
    for (size_t i = 0; i < argc; i++) {
        if (arg_strs[i] == NULL) {
            for (size_t j = 0; j < i; j++)
                free(arg_strs[j]);
            free(arg_strs);
            return NULL;
        }
        if (strlen(arg_strs[i]) > SIZE_MAX - buf_len - 2) {
            for (size_t j = 0; j <= i; j++)
                free(arg_strs[j]);
            for (size_t j = i + 1; j < argc; j++)
                free(arg_strs[j]);
            free(arg_strs);
            return NULL;
        }
        buf_len += strlen(arg_strs[i]) + 2;
    }

    result = malloc(buf_len);
    if (result == NULL) {
        for (size_t i = 0; i < argc; i++)
            free(arg_strs[i]);
        free(arg_strs);
        return NULL;
    }
    {
        size_t offset = 0;
        size_t qual_len = strlen(qualified_name);
        memcpy(result + offset, qualified_name, qual_len);
        offset += qual_len;
        result[offset++] = '(';
        for (size_t i = 0; i < argc; i++) {
            if (i > 0) {
                result[offset++] = ',';
                result[offset++] = ' ';
            }
            {
                size_t arg_len = strlen(arg_strs[i]);
                memcpy(result + offset, arg_strs[i], arg_len);
                offset += arg_len;
            }
            free(arg_strs[i]);
        }
        result[offset++] = ')';
        result[offset] = '\0';
    }
    free(arg_strs);
    return result;
}

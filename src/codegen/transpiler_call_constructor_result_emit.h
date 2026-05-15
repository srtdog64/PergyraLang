#ifndef PGY_TRANSPILER_CALL_CONSTRUCTOR_RESULT_EMIT_H
#define PGY_TRANSPILER_CALL_CONSTRUCTOR_RESULT_EMIT_H

#include <stdint.h>

#include "parser/ast_api.h"

static char *
emit_call_domain_constructor(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    /* Tagged union variant constructors: Circle(42) ??Shape_Circle(42) */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = ast_identifier_name(callee);
        ASTNode *class_decl = find_class_decl(ctx, fn);
        if (class_decl != NULL && class_decl->type == AST_CLASS_DECL) {
            const char *ctor_type = fn;
            size_t argc = ast_call_arg_count(call);
            CodeBuf *fields = codebuf_create();
            if (class_has_generic_params(class_decl)) {
                ASTNode *synthetic_type = ast_create_type(fn);
                if (synthetic_type != NULL) {
                    const char *spec_name =
                        ensure_generic_class_specialization(ctx, class_decl, synthetic_type);
                    if (spec_name != NULL)
                        ctor_type = spec_name;
                    ast_destroy(synthetic_type);
                }
            }
            size_t field_count = 0;
            ClassField **fields_list = ast_class_fields(class_decl, &field_count);
            for (size_t i = 0; i < argc && i < field_count; i++) {
                ClassField *field = fields_list != NULL ? fields_list[i] : NULL;
                char *arg = emit_expression(ast_call_argument(call, i), ctx);
                if (i > 0)
                    codebuf_write(fields, ", ");
                codebuf_write(fields, ".%s = %s",
                    field != NULL && field->name != NULL ? field->name : "field",
                    arg != NULL ? arg : "0");
                free(arg);
            }
            char *result;
            if (fields->len > 0)
                result = strdup_fmt("(%s){ %s }", ctor_type, fields->data);
            else
                result = strdup_fmt("(%s){0}", ctor_type);
            codebuf_destroy(fields);
            return result;
        }
        {
            ASTNode *party_decl = find_party_decl(ctx, fn);
            if (party_decl != NULL && party_decl->type == AST_PARTY_DECL) {
                size_t argc = ast_call_arg_count(call);
                size_t shared_count = ast_party_shared_count(party_decl);
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc && i < shared_count; i++) {
                    ASTNode *shared = ast_party_shared(party_decl, i);
                    const char *field_name = ast_party_shared_name(shared);
                    char *arg = emit_expression(ast_call_argument(call, i), ctx);
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
                    if (shared == NULL
                        || ast_party_shared_initializer(shared) == NULL)
                        continue;
                    field_name = ast_party_shared_name(shared);
                    init_expr = emit_expression(
                        ast_party_shared_initializer(shared), ctx);
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        init_expr != NULL ? init_expr : "0");
                    free(init_expr);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *roster_decl = find_roster_decl(ctx, fn);
            if (roster_decl != NULL && roster_decl->type == AST_ROSTER_DECL) {
                size_t argc = ast_call_arg_count(call);
                size_t roster_party_count = ast_roster_party_count(roster_decl);
                size_t exposed = roster_party_count
                    + ast_roster_shared_count(roster_decl);
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc && i < exposed; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(ast_call_argument(call, i), ctx);
                    if (i < roster_party_count) {
                        ASTNode *slot = ast_roster_party(roster_decl, i);
                        field_name = ast_roster_slot_name(slot) != NULL
                            ? ast_roster_slot_name(slot) : "field";
                    } else {
                        ASTNode *shared = ast_roster_shared(roster_decl,
                            i - roster_party_count);
                        field_name = ast_party_shared_name(shared);
                    }
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
                    if (shared == NULL
                        || ast_party_shared_initializer(shared) == NULL)
                        continue;
                    field_name = ast_party_shared_name(shared);
                    init_expr = emit_expression(
                        ast_party_shared_initializer(shared), ctx);
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        init_expr != NULL ? init_expr : "0");
                    free(init_expr);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *relation_decl = find_relation_decl(ctx, fn);
            ASTNode *effect_decl = find_effect_decl(ctx, fn);
            ASTNode *overlay_decl = relation_decl != NULL ? relation_decl : effect_decl;
            if (overlay_decl != NULL) {
                size_t argc = ast_call_arg_count(call);
                size_t slot_count = 0;
                size_t shared_count = 0;
                size_t refresh_count = 0;
                ASTNode **slots = overlay_decl->type == AST_RELATION_DECL
                    ? ast_relation_slots(overlay_decl, &slot_count)
                    : ast_effect_slots(overlay_decl, &slot_count);
                ASTNode **shared_fields = overlay_decl->type == AST_RELATION_DECL
                    ? ast_relation_shared_fields(overlay_decl, &shared_count)
                    : ast_effect_shared_fields(overlay_decl, &shared_count);
                ASTNode **refreshes = overlay_decl->type == AST_RELATION_DECL
                    ? ast_relation_refreshes(overlay_decl, &refresh_count)
                    : ast_effect_refreshes(overlay_decl, &refresh_count);
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc && i < slot_count + shared_count; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(ast_call_argument(call, i), ctx);
                    if (i < slot_count) {
                        ASTNode *slot = slots[i];
                        field_name = ast_domain_slot_name(slot);
                    } else {
                        ASTNode *shared = shared_fields[i - slot_count];
                        field_name = ast_party_shared_name(shared);
                    }
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
                    if (shared == NULL
                        || ast_party_shared_initializer(shared) == NULL)
                        continue;
                    field_name = ast_party_shared_name(shared);
                    init_expr = emit_expression(
                        ast_party_shared_initializer(shared), ctx);
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
                            || domain_slot_is_projection_target_local(
                                slot,
                                refreshes,
                                refresh_count));
                    if (!projection_slot || slot_name == NULL)
                        continue;
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".__projection_dirty_%s = true", slot_name);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *zone_decl = find_zone_decl(ctx, fn);
            if (zone_decl != NULL && zone_decl->type == AST_ZONE_DECL) {
                size_t argc = ast_call_arg_count(call);
                size_t slot_count = 0;
                ASTNode **slots = ast_zone_slots(zone_decl, &slot_count);
                size_t shared_count = 0;
                ASTNode **shared_fields =
                    ast_zone_shared_fields(zone_decl, &shared_count);
                size_t refresh_count = 0;
                ASTNode **refreshes = ast_zone_refreshes(
                    zone_decl, &refresh_count);
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc && i < slot_count + shared_count; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(ast_call_argument(call, i), ctx);
                    if (i < slot_count) {
                        ASTNode *slot = slots[i];
                        field_name = ast_domain_slot_name(slot);
                    } else {
                        ASTNode *shared = shared_fields[i - slot_count];
                        field_name = ast_party_shared_name(shared);
                    }
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
                    if (shared == NULL
                        || ast_party_shared_initializer(shared) == NULL)
                        continue;
                    field_name = ast_party_shared_name(shared);
                    init_expr = emit_expression(
                        ast_party_shared_initializer(shared), ctx);
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
                            || domain_slot_is_projection_target_local(
                                slot,
                                refreshes,
                                refresh_count));
                    if (!projection_slot || slot_name == NULL)
                        continue;
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".__projection_dirty_%s = true", slot_name);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *world_decl = find_world_decl(ctx, fn);
            if (world_decl != NULL && world_decl->type == AST_WORLD_DECL) {
                size_t argc = ast_call_arg_count(call);
                CodeBuf *fields = codebuf_create();
                size_t roster_count = 0;
                ASTNode **rosters = ast_world_rosters(world_decl, &roster_count);
                size_t zone_count = 0;
                ASTNode **zones = ast_world_zones(world_decl, &zone_count);
                size_t shared_count = 0;
                ASTNode **shared_fields =
                    ast_world_shared_fields(world_decl, &shared_count);
                size_t exposed = roster_count + zone_count + shared_count;
                for (size_t i = 0; i < argc && i < exposed; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(ast_call_argument(call, i), ctx);
                    if (i < roster_count) {
                        ASTNode *slot = rosters[i];
                        field_name = ast_world_roster_slot_name(slot);
                    } else if (i < roster_count + zone_count) {
                        ASTNode *slot = zones[i - roster_count];
                        field_name = ast_world_zone_slot_name(slot);
                    } else {
                        ASTNode *shared = shared_fields[i - roster_count - zone_count];
                        field_name = ast_party_shared_name(shared);
                    }
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
                    if (shared == NULL
                        || ast_party_shared_initializer(shared) == NULL)
                        continue;
                    field_name = ast_party_shared_name(shared);
                    init_expr = emit_expression(
                        ast_party_shared_initializer(shared), ctx);
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
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }

        char qualified[128];
        if (lookup_enum_variant_qualified_name_copy(ctx, fn,
                qualified, sizeof(qualified))) {
            /* Emit: EnumName_VariantName(args...) */
            size_t argc = ast_call_arg_count(call);
            if (argc > SIZE_MAX / sizeof(char *))
                return NULL;
            char **arg_strs = calloc(argc > 0 ? argc : 1, sizeof(char *));
            if (arg_strs == NULL)
                return NULL;
            for (size_t i = 0; i < argc; i++)
                arg_strs[i] = emit_expression(ast_call_argument(call, i), ctx);
            /* Build argument list string */
            size_t buf_len = strlen(qualified) + 3;
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
            char *result = malloc(buf_len);
            if (result == NULL) {
                for (size_t i = 0; i < argc; i++)
                    free(arg_strs[i]);
                free(arg_strs);
                return NULL;
            }
            {
                size_t offset = 0;
                size_t qual_len = strlen(qualified);
                memcpy(result + offset, qualified, qual_len);
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
    }

    return NULL;
}

#include "transpiler_call_result_option_builtin_emit.h"

#endif /* PGY_TRANSPILER_CALL_CONSTRUCTOR_RESULT_EMIT_H */


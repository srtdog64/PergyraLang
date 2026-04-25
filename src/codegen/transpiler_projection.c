/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend projection provenance and nominal type predicates.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transpiler_decl_lookup.h"
#include "transpiler_projection.h"
#include "../common/string_compat.h"

static char *
projection_heap_fmt(const char *fmt, ...)
{
    va_list ap;
    int n;
    char *s;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0)
        return pergyra_strdup("");

    s = (char *)malloc((size_t)n + 1);
    if (s == NULL)
        return pergyra_strdup("");

    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

ASTNode *
transpiler_find_zone_domain_slot(ASTNode *zone_decl, const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

bool
transpiler_current_world_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_WORLD_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
        ASTNode *slot = decl->data.world_decl.rosters[i];
        if (slot != NULL && slot->data.world_roster.slot_name != NULL
            && strcmp(slot->data.world_roster.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
        ASTNode *slot = decl->data.world_decl.zones[i];
        if (slot != NULL && slot->data.world_zone.slot_name != NULL
            && strcmp(slot->data.world_zone.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
        ASTNode *shared = decl->data.world_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

ASTNode *
transpiler_find_zone_state_decl(ASTNode *zone_decl, const char *state_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone_decl->data.zone_decl.state_count; i++) {
        ASTNode *state = zone_decl->data.zone_decl.states[i];
        if (state != NULL && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }
    return NULL;
}

ASTNode *
transpiler_find_zone_layer_slot(ASTNode *zone_decl, const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

ASTNode *
transpiler_find_world_zone_slot_decl(ASTNode *world_decl, const char *slot_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

bool
transpiler_world_has_zone_slot(ASTNode *world_decl, const char *slot_name)
{
    return transpiler_find_world_zone_slot_decl(world_decl, slot_name) != NULL;
}

ASTNode *
transpiler_resolve_world_zone_decl(TranspilerCtx *ctx, ASTNode *world_decl,
                                   const char *slot_name)
{
    ASTNode *zone_slot = transpiler_find_world_zone_slot_decl(world_decl, slot_name);
    if (ctx == NULL || zone_slot == NULL || zone_slot->data.world_zone.zone_type == NULL)
        return NULL;
    return find_zone_decl(ctx, zone_slot->data.world_zone.zone_type);
}

static size_t
projection_source_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    return 0;
}

static ClassField *
projection_source_field_at(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    return NULL;
}

int
resolve_projection_source_path_rec(TranspilerCtx *ctx, ASTNode *source_decl,
                                   const char *field_name, unsigned depth,
                                   char **path_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (ctx == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = projection_source_field_count(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = pergyra_strdup(field_name);
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at(source_decl, i);
        ASTNode *vessel_decl;
        char *nested_path = NULL;
        char *prefixed_path;
        int nested_status;

        if (field == NULL
            || field->type == NULL || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL) {
            continue;
        }

        vessel_decl = find_class_decl(ctx, field->type->data.type.name);
        if (vessel_decl == NULL
            || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = resolve_projection_source_path_rec(
            ctx, vessel_decl, field_name, depth + 1, &nested_path);
        if (nested_status != 1) {
            if (nested_path != NULL)
                free(nested_path);
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefixed_path = projection_heap_fmt("%s.%s", field->name, nested_path);
        free(nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
        } else {
            free(prefixed_path);
            free(resolved_path);
            resolved_path = NULL;
        }
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        else
            free(resolved_path);
        return 1;
    }

    if (resolved_path != NULL)
        free(resolved_path);
    return match_count > 1 ? 2 : 0;
}

char *
emit_projection_literal(TranspilerCtx *ctx, ASTNode *target_decl, ASTNode *source_decl,
                        ASTNode *refresh, const char *target_type_name,
                        const char *source_expr)
{
    CodeBuf *buf;
    char *result;
    bool first = true;

    if (target_decl == NULL || source_decl == NULL
        || target_type_name == NULL || source_expr == NULL) {
        return pergyra_strdup("0");
    }

    buf = codebuf_create();
    codebuf_write(buf, "(%s){ ", target_type_name);

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *target_field = target_decl->data.class_decl.fields[i];
        const char *source_field_name = NULL;
        char *source_path = NULL;
        int source_status;

        if (target_field == NULL || target_field->name == NULL)
            continue;

        source_field_name = target_field->name;
        if (refresh != NULL && refresh->type == AST_ZONE_REFRESH) {
            for (size_t j = 0; j < refresh->data.zone_refresh.field_map_count; j++) {
                const char *mapped_target =
                    refresh->data.zone_refresh.mapped_target_fields[j];
                const char *mapped_source =
                    refresh->data.zone_refresh.mapped_source_fields[j];
                if (mapped_target != NULL && mapped_source != NULL
                    && strcmp(mapped_target, target_field->name) == 0) {
                    source_field_name = mapped_source;
                    break;
                }
            }
        }

        source_status = resolve_projection_source_path_rec(
            ctx, source_decl, source_field_name, 0, &source_path);
        if (!first)
            codebuf_write(buf, ", ");
        first = false;

        if (source_status == 1 && source_path != NULL) {
            codebuf_write(buf, ".%s = %s.%s",
                target_field->name, source_expr, source_path);
        } else {
            codebuf_write(buf, ".%s = 0", target_field->name);
        }
        free(source_path);
    }

    codebuf_write(buf, " }");
    result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

bool
is_subject_type_name(TranspilerCtx *ctx, const char *type_name)
{
    ASTNode *decl = find_class_decl(ctx, type_name);
    if (decl != NULL && !decl->data.class_decl.is_struct)
        return decl->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT;
    for (int i = 0; ctx != NULL && i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name, type_name) == 0) {
            const ASTNode *orig = ctx->generic_class_specs[i].class_decl;
            return orig != NULL && !orig->data.class_decl.is_struct
                && orig->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT;
        }
    }
    return false;
}

bool
is_nominal_host_type_name(TranspilerCtx *ctx, const char *type_name)
{
    ASTNode *decl;

    if (ctx == NULL || type_name == NULL)
        return false;

    if (find_enum_decl(ctx, type_name) != NULL)
        return true;
    decl = find_class_decl(ctx, type_name);
    if (decl != NULL && (!decl->data.class_decl.is_struct
        || decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL
        || decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT))
        return true;
    if (find_relation_decl(ctx, type_name) != NULL
        || find_effect_decl(ctx, type_name) != NULL
        || find_zone_decl(ctx, type_name) != NULL
        || find_world_decl(ctx, type_name) != NULL)
        return true;
    for (int i = 0; i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name, type_name) == 0) {
            const ASTNode *orig = ctx->generic_class_specs[i].class_decl;
            return orig != NULL && !orig->data.class_decl.is_struct;
        }
    }
    return false;
}

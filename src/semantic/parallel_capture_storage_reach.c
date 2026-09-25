#include <stdlib.h>
#include <string.h>
#include "parallel_capture_storage_reach.h"
#include "parallel_capture_reach_internal.h"
#include "type_checker_flow_internal.h"
#include "type_checker_resolution_internal.h"
#include "compiler/decl_field_model.h"

/*
 * Storage a parallel task reaches through a captured binding.
 *
 * The capture checks in type_checker_flow_parallel.c decide by name: a
 * collection binding may not be captured, and a binding may be written by at
 * most one task. Two shapes reach shared storage without the name:
 *
 *   - an aggregate whose field holds a collection shares that collection's
 *     storage (`Holder(arr)` copies the Array header, not the elements);
 *   - a method call writes the receiver's fields (`counter.Step(1)`), with no
 *     assignment to `counter` in the task.
 *
 * This file answers the first and owns the host field model both answers
 * read; parallel_capture_write_reach.c answers the second. Both fail closed:
 * a field type or method they cannot resolve counts as reaching storage or
 * writing, so a missed access becomes a rejection, never an admitted race.
 */

static bool
reach_type_is_runtime_transport(const Type *type)
{
    Type *constructor = type_constructed_constructor(type);
    const char *name = constructor != NULL ? constructor->name
                                           : (type != NULL ? type->name : NULL);

    if (name == NULL)
        return false;
    return strcmp(name, "Channel") == 0 || strcmp(name, "Slot") == 0
        || strcmp(name, "SecureSlot") == 0 || strcmp(name, "DeviceSlot") == 0
        || strcmp(name, "Future") == 0 || strcmp(name, "RemoteFuture") == 0;
}

/* A user nominal other than an enum: its value carries fields a method can
 * write, or a reference another task can write through. */
ASTNode *
parallel_reach_nominal_decl(SemanticContext *ctx, const Type *type)
{
    ASTNode *decl = semantic_host_decl_for_type(ctx, type);

    if (decl == NULL || decl->type == AST_ENUM_DECL)
        return NULL;
    return decl;
}

static void
reach_fields_push(ParallelReachFields *fields, const char *name, Type *type)
{
    ParallelReachField *slot = &fields->items[fields->count++];

    slot->name = name;
    slot->type = type == NULL || type == TYPE_UNKNOWN ? NULL : type;
}

static Type *
reach_named_type(SemanticContext *ctx, const char *type_name)
{
    return type_name != NULL
        ? semantic_type_resolution_lookup_metadata_name_or_alias(ctx, type_name)
        : NULL;
}

static ParallelReachFields
reach_class_fields(SemanticContext *ctx, ASTNode *decl)
{
    ParallelReachFields fields = { NULL, 0, true };
    PgyDeclField *model = NULL;
    size_t count = 0;

    if (!pgy_class_decl_field_model_try_build(decl, &model, &count)) {
        fields.complete = false;
        return fields;
    }
    fields.items = count > 0 ? calloc(count, sizeof *fields.items) : NULL;
    fields.complete = count == 0 || fields.items != NULL;
    for (size_t i = 0; fields.items != NULL && i < count; i++)
        reach_fields_push(&fields, model[i].name,
                          flow_resolve_type_ref(model[i].type_ast, ctx));
    pgy_decl_field_model_free(model, count);
    return fields;
}

ParallelReachFields
parallel_reach_host_fields(SemanticContext *ctx, ASTNode *decl)
{
    ParallelReachFields fields = { NULL, 0, true };
    ASTNode **rosters = NULL, **zones = NULL, **layers = NULL;
    size_t roster_count = 0, zone_count = 0, layer_count = 0;
    size_t overlay_count;

    if (decl == NULL)
        return fields;
    if (decl->type == AST_CLASS_DECL)
        return reach_class_fields(ctx, decl);
    if (decl->type == AST_WORLD_DECL) {
        rosters = ast_world_rosters(decl, &roster_count);
        zones = ast_world_zones(decl, &zone_count);
    }
    if (decl->type == AST_ZONE_DECL)
        layers = ast_zone_layer_slots(decl, &layer_count);
    overlay_count = overlay_field_count(decl);
    if (roster_count + zone_count + layer_count + overlay_count == 0)
        return fields;
    fields.items = calloc(roster_count + zone_count + layer_count
                              + overlay_count, sizeof *fields.items);
    if (fields.items == NULL) {
        fields.complete = false;
        return fields;
    }
    for (size_t i = 0; i < roster_count; i++)
        reach_fields_push(&fields, ast_world_roster_slot_name(rosters[i]),
            reach_named_type(ctx, ast_world_roster_type_name(rosters[i])));
    for (size_t i = 0; i < zone_count; i++)
        reach_fields_push(&fields, ast_world_zone_slot_name(zones[i]),
            reach_named_type(ctx, ast_world_zone_type_name(zones[i])));
    for (size_t i = 0; i < layer_count; i++)
        reach_fields_push(&fields, ast_zone_layer_slot_name(layers[i]),
            reach_named_type(ctx, ast_zone_layer_slot_layer_type(layers[i])));
    for (size_t i = 0; i < overlay_count; i++) {
        const char *name = NULL;
        ASTNode *type_node = overlay_field_decl_at(decl, i, &name);

        /* the overlay leaves world rosters and zones to the loops above */
        if (type_node == NULL && name == NULL && decl->type == AST_WORLD_DECL)
            continue;
        reach_fields_push(&fields, name,
                          type_node != NULL
                              ? semantic_host_resolve_type_ref(type_node, ctx)
                              : NULL);
    }
    return fields;
}

/* The declared type of `field_name` on `decl`, or NULL when the declaration
 * has no such field or the field type does not resolve. */
Type *
parallel_reach_field_type(SemanticContext *ctx, ASTNode *decl, const char *field_name)
{
    ParallelReachFields fields;
    Type *resolved = NULL;

    if (decl == NULL || field_name == NULL)
        return NULL;
    fields = parallel_reach_host_fields(ctx, decl);
    for (size_t i = 0; i < fields.count; i++) {
        if (fields.items[i].name != NULL
            && strcmp(fields.items[i].name, field_name) == 0) {
            resolved = fields.items[i].type;
            break;
        }
    }
    free(fields.items);
    return resolved;
}

/* The field path to the storage, innermost name first, and the nominal
 * declarations entered on the way down. */
typedef struct {
    const char *names[PARALLEL_REACH_DEPTH_LIMIT + 2];
    size_t count;
    const char *kind;
    const ASTNode *entered[PARALLEL_REACH_DEPTH_LIMIT + 1];
    size_t entered_count;
} ReachPath;

static void
reach_path_push(ReachPath *path, const char *name)
{
    if (path->count < PARALLEL_REACH_DEPTH_LIMIT + 2)
        path->names[path->count++] = name != NULL ? name : "?";
}

typedef enum {
    REACH_ENTER_NEW,      /* first visit on this path: walk its members */
    REACH_ENTER_CYCLE,    /* already on the path: adds no new storage */
    REACH_ENTER_TOO_DEEP  /* beyond the checked depth: fail closed */
} ReachEnter;

static ReachEnter
reach_enter(ReachPath *path, const ASTNode *decl)
{
    for (size_t i = 0; i < path->entered_count; i++) {
        if (path->entered[i] == decl)
            return REACH_ENTER_CYCLE;
    }
    if (path->entered_count >= PARALLEL_REACH_DEPTH_LIMIT) {
        path->kind = "nesting beyond the checked depth";
        return REACH_ENTER_TOO_DEEP;
    }
    path->entered[path->entered_count++] = decl;
    return REACH_ENTER_NEW;
}

static bool
reach_storage(SemanticContext *ctx, const Type *type, ReachPath *path);

static bool
reach_storage_through_fields(SemanticContext *ctx, ASTNode *decl,
                             ReachPath *path)
{
    ParallelReachFields fields = parallel_reach_host_fields(ctx, decl);
    bool reaches = false;

    if (!fields.complete) {
        path->kind = "field model allocation failed";
        return true;
    }
    for (size_t i = 0; i < fields.count && !reaches; i++) {
        Type *field_type = fields.items[i].type;

        if (field_type == NULL) {
            path->kind = "unresolved field type";
            reaches = true;
        } else if (field_type->kind == TYPE_KIND_GENERIC) {
            continue; /* covered by the instantiation's arguments */
        } else {
            reaches = reach_storage(ctx, field_type, path);
        }
        if (reaches)
            reach_path_push(path, fields.items[i].name);
    }
    free(fields.items);
    return reaches;
}

/* A user enum shares what any variant payload shares (`Full(Array<Int>)`). */
static bool
reach_storage_through_variants(SemanticContext *ctx, ASTNode *decl,
                               ReachPath *path)
{
    size_t variant_count = 0;

    (void)ast_enum_variants(decl, &variant_count);
    for (size_t v = 0; v < variant_count; v++) {
        for (size_t p = 0; p < ast_enum_variant_param_count(decl, v); p++) {
            Type *payload = semantic_type_resolution_lookup_metadata_type_ref(
                ctx, ast_enum_variant_param(decl, v, p));

            if (payload == NULL || payload == TYPE_UNKNOWN) {
                path->kind = "unresolved payload type";
                return true;
            }
            if (payload->kind == TYPE_KIND_GENERIC)
                continue;
            if (reach_storage(ctx, payload, path))
                return true;
        }
    }
    return false;
}

static bool
reach_storage(SemanticContext *ctx, const Type *type, ReachPath *path)
{
    const char *kind;
    ASTNode *decl;
    bool reaches;

    if (type == NULL)
        return false;
    kind = worker_boundary_storage_display_name(type);
    if (kind != NULL) {
        path->kind = kind;
        return true;
    }
    if (reach_type_is_runtime_transport(type))
        return false;
    if (type->kind == TYPE_KIND_TUPLE) {
        for (size_t i = 0; i < type_tuple_arity(type); i++) {
            if (reach_storage(ctx, type_tuple_get_element(type, i), path))
                return true;
        }
        return false;
    }
    if (type->kind == TYPE_KIND_CONSTRUCTED) {
        for (size_t i = 0; i < type_constructed_arg_count(type); i++) {
            if (reach_storage(ctx, type_constructed_arg(type, i), path))
                return true;
        }
    }
    if (type->kind == TYPE_KIND_ENUM) {
        decl = type->name != NULL
            ? semantic_find_enum_decl_by_name(ctx, type->name) : NULL;
    } else if (type->kind == TYPE_KIND_CLASS
               || type->kind == TYPE_KIND_CONSTRUCTED) {
        /* Builtin nominals (String, IoError, ...) have no declaration and no
         * collection fields; user aggregates always resolve to one. */
        decl = parallel_reach_nominal_decl(ctx, type);
    } else {
        return false;
    }
    if (decl == NULL)
        return false;
    switch (reach_enter(path, decl)) {
    case REACH_ENTER_CYCLE:
        return false;
    case REACH_ENTER_TOO_DEEP:
        return true;
    case REACH_ENTER_NEW:
        break;
    }
    reaches = decl->type == AST_ENUM_DECL
        ? reach_storage_through_variants(ctx, decl, path)
        : reach_storage_through_fields(ctx, decl, path);
    path->entered_count--;
    return reaches;
}

bool
parallel_capture_type_reaches_storage(SemanticContext *ctx, const Type *type,
                                      char *path_out, size_t path_cap,
                                      const char **kind_out)
{
    ReachPath path = { { NULL }, 0, NULL, { NULL }, 0 };
    size_t used = 0;

    if (path_out != NULL && path_cap > 0)
        path_out[0] = '\0';
    if (ctx == NULL || type == NULL)
        return false;
    if (worker_boundary_storage_display_name(type) != NULL)
        return false; /* the binding itself is a collection: not an aggregate */
    if (!reach_storage(ctx, type, &path))
        return false;
    for (size_t i = path.count; i > 0 && path_out != NULL; i--) {
        const char *name = path.names[i - 1];
        size_t len = strlen(name);

        if (used + len + 2 > path_cap)
            break;
        if (used > 0)
            path_out[used++] = '.';
        memcpy(path_out + used, name, len);
        used += len;
        path_out[used] = '\0';
    }
    if (kind_out != NULL)
        *kind_out = path.kind;
    return true;
}

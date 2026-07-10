#include "hir_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../common/string_compat.h"

typedef struct
{
    uint32_t source_syntax_id;
    uint32_t routine_id;
} HIRRoutineSourceIndex;

static bool
hir_callgraph_next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    size_t next;

    if (capacity == NULL || elem_size == 0)
        return false;
    if (*capacity == 0) {
        next = initial;
    } else {
        if (*capacity > SIZE_MAX / 2)
            return false;
        next = *capacity * 2;
    }
    if (next > SIZE_MAX / elem_size)
        return false;
    *capacity = next;
    return true;
}

static bool
hir_append_routine_id_unique(uint32_t **items, size_t *count,
                             size_t *capacity, uint32_t value)
{
    if (items == NULL || count == NULL || capacity == NULL)
        return false;
    if (*count > 0 && *items == NULL)
        return false;
    for (size_t i = 0; i < *count; i++) {
        if ((*items)[i] == value)
            return true;
    }

    if (*count == *capacity) {
        size_t next_capacity = *capacity;
        if (!hir_callgraph_next_capacity(&next_capacity, 4, sizeof(uint32_t)))
            return false;
        uint32_t *grown = realloc(*items,
                                  next_capacity * sizeof(uint32_t));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
    }
    (*items)[*count] = value;
    (*count)++;
    return true;
}

static int
hir_routine_source_index_compare(const void *lhs, const void *rhs)
{
    const HIRRoutineSourceIndex *a = (const HIRRoutineSourceIndex *)lhs;
    const HIRRoutineSourceIndex *b = (const HIRRoutineSourceIndex *)rhs;

    if (a->source_syntax_id != b->source_syntax_id)
        return (a->source_syntax_id > b->source_syntax_id)
            - (a->source_syntax_id < b->source_syntax_id);
    return (a->routine_id > b->routine_id)
        - (a->routine_id < b->routine_id);
}

static HIRRoutineSourceIndex *
hir_build_routine_source_index(const HIRMutableRoutineInventory *inventory,
                               size_t *out_count)
{
    HIRRoutineSourceIndex *index;
    size_t count = 0;

    if (out_count == NULL)
        return NULL;
    *out_count = 0;
    if (inventory == NULL || inventory->count == 0)
        return NULL;
    if (inventory->count > SIZE_MAX / sizeof(HIRRoutineSourceIndex))
        return NULL;

    index = calloc(inventory->count, sizeof(HIRRoutineSourceIndex));
    if (index == NULL)
        return NULL;

    for (size_t i = 0; i < inventory->count; i++) {
        HIRRoutine *routine = hir_mutable_routine_inventory_get(inventory, i);
        if (routine == NULL || routine->source_syntax_id == 0)
            continue;
        index[count].source_syntax_id = routine->source_syntax_id;
        index[count].routine_id = routine->routine_id;
        count++;
    }

    qsort(index, count, sizeof(HIRRoutineSourceIndex),
          hir_routine_source_index_compare);
    *out_count = count;
    return index;
}

static uint32_t
hir_lookup_routine_id_by_source(const HIRRoutineSourceIndex *index,
                                size_t count,
                                uint32_t source_syntax_id)
{
    size_t low = 0;
    size_t high = count;

    if (index == NULL || count == 0 || source_syntax_id == 0)
        return 0;

    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (index[mid].source_syntax_id < source_syntax_id) {
            low = mid + 1;
        } else if (index[mid].source_syntax_id > source_syntax_id) {
            high = mid;
        } else {
            return index[mid].routine_id;
        }
    }
    return 0;
}

static bool
hir_source_identity_is_internal_routine_decl(const HIRProgram *hir,
                                             uint32_t source_syntax_id)
{
    if (hir == NULL || source_syntax_id == 0)
        return false;

    for (size_t i = 0; i < hir->decl_count; i++) {
        const HIRDecl *decl = &hir->decls[i];
        if (decl->source_syntax_id != source_syntax_id)
            continue;
        return decl->kind == HIR_TOPLEVEL_FUNCTION
            || decl->kind == HIR_TOPLEVEL_INTENT
            || decl->kind == HIR_TOPLEVEL_EXECUTABLE;
    }
    return false;
}

static bool
hir_materialize_direct_call_edges(HIRProgram *hir, char **error_message)
{
    size_t routine_index_count = 0;
    HIRMutableRoutineInventory inventory;
    hir_mutable_routine_inventory_from_program(hir, &inventory);
    HIRRoutineSourceIndex *routine_index =
        hir_build_routine_source_index(&inventory, &routine_index_count);

    if (inventory.count > 0 && routine_index == NULL)
        goto oom;
    for (size_t i = 1; i < routine_index_count; i++) {
        if (routine_index[i - 1].source_syntax_id
            == routine_index[i].source_syntax_id) {
            free(routine_index);
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR routine source identity collision");
            }
            return false;
        }
    }

    for (size_t i = 0; i < inventory.count; i++) {
        HIRRoutine *routine = hir_mutable_routine_inventory_get(&inventory, i);
        if (routine == NULL)
            goto oom;
        if (routine->direct_call_count > 0
            && (routine->direct_calls == NULL
                || routine->direct_call_decl_ids == NULL)) {
            free(routine_index);
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR direct-call identity facts are incomplete");
            }
            return false;
        }
        for (size_t j = 0; j < routine->direct_call_count; j++) {
            uint32_t source_syntax_id = routine->direct_call_decl_ids[j];
            uint32_t callee = hir_lookup_routine_id_by_source(
                routine_index, routine_index_count, source_syntax_id);
            if (callee == 0) {
                if (hir_source_identity_is_internal_routine_decl(
                        hir, source_syntax_id)) {
                    free(routine_index);
                    if (error_message != NULL) {
                        *error_message = pergyra_strdup(
                            "HIR internal call target has no RoutineId");
                    }
                    return false;
                }
                continue;
            }
            if (!hir_append_routine_id_unique(
                    &routine->callee_routine_ids,
                    &routine->callee_routine_count,
                    &routine->callee_routine_capacity,
                    callee)) {
                goto oom;
            }
        }
    }
    free(routine_index);
    return true;

oom:
    free(routine_index);
    if (error_message != NULL)
        *error_message = pergyra_strdup("Out of memory");
    return false;
}

static HIRRoutine *
hir_routine_by_id(const HIRMutableRoutineInventory *inventory,
                  uint32_t routine_id)
{
    HIRRoutine *routine;

    if (inventory == NULL || routine_id == 0
        || routine_id > inventory->count) {
        return NULL;
    }
    routine = hir_mutable_routine_inventory_get(
        inventory, (size_t)routine_id - 1);
    return routine != NULL && routine->routine_id == routine_id
        ? routine
        : NULL;
}

static bool
hir_routine_is_entry_root(const HIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    return routine->kind == HIR_TOPLEVEL_INTENT
        || routine->kind == HIR_TOPLEVEL_EXECUTABLE
        || routine->is_exported
        || (routine->name != NULL && strcmp(routine->name, "Main") == 0);
}

static void
hir_propagate_entry_reachability(HIRProgram *hir)
{
    HIRMutableRoutineInventory inventory;
    bool changed = true;

    hir_mutable_routine_inventory_from_program(hir, &inventory);
    while (changed) {
        changed = false;
        for (size_t i = 0; i < inventory.count; i++) {
            HIRRoutine *routine = hir_mutable_routine_inventory_get(&inventory, i);
            if (routine == NULL)
                continue;
            if (!routine->is_entry_reachable
                && hir_routine_is_entry_root(routine)) {
                routine->is_entry_reachable = true;
                changed = true;
            }
            if (!routine->is_entry_reachable)
                continue;
            for (size_t j = 0; j < routine->callee_routine_count; j++) {
                uint32_t callee = routine->callee_routine_ids[j];
                HIRRoutine *callee_routine = hir_routine_by_id(
                    &inventory, callee);
                if (callee_routine == NULL || callee_routine->is_entry_reachable) {
                    continue;
                }
                callee_routine->is_entry_reachable = true;
                changed = true;
            }
        }
    }
}

bool
hir_finish_callgraph(HIRProgram *hir, char **error_message)
{
    if (hir == NULL)
        return true;
    if (!hir_materialize_direct_call_edges(hir, error_message))
        return false;
    hir_propagate_entry_reachability(hir);
    return true;
}

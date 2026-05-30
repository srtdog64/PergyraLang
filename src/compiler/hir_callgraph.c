#include "hir_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../common/string_compat.h"

typedef struct
{
    const char *name;
    size_t      index;
} HIRRoutineNameIndex;

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
hir_append_index_unique(size_t **items, size_t *count,
                        size_t *capacity, size_t value)
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
        if (!hir_callgraph_next_capacity(&next_capacity, 4, sizeof(size_t)))
            return false;
        size_t *grown = realloc(*items, next_capacity * sizeof(size_t));
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
hir_routine_name_index_compare(const void *lhs, const void *rhs)
{
    const HIRRoutineNameIndex *a = (const HIRRoutineNameIndex *)lhs;
    const HIRRoutineNameIndex *b = (const HIRRoutineNameIndex *)rhs;
    int name_cmp = strcmp(a->name, b->name);

    if (name_cmp != 0)
        return name_cmp;
    return (a->index > b->index) - (a->index < b->index);
}

static HIRRoutineNameIndex *
hir_build_routine_name_index(const HIRMutableRoutineInventory *inventory,
                             size_t *out_count)
{
    HIRRoutineNameIndex *index;
    size_t count = 0;

    if (out_count == NULL)
        return NULL;
    *out_count = 0;
    if (inventory == NULL || inventory->count == 0)
        return NULL;
    if (inventory->count > SIZE_MAX / sizeof(HIRRoutineNameIndex))
        return NULL;

    index = calloc(inventory->count, sizeof(HIRRoutineNameIndex));
    if (index == NULL)
        return NULL;

    for (size_t i = 0; i < inventory->count; i++) {
        HIRRoutine *routine = hir_mutable_routine_inventory_get(inventory, i);
        if (routine == NULL || routine->name == NULL)
            continue;
        index[count].name = routine->name;
        index[count].index = i;
        count++;
    }

    qsort(index, count, sizeof(HIRRoutineNameIndex),
          hir_routine_name_index_compare);
    *out_count = count;
    return index;
}

static ssize_t
hir_lookup_routine_index_by_name(const HIRRoutineNameIndex *index,
                                 size_t count,
                                 const char *name)
{
    HIRRoutineNameIndex key;
    HIRRoutineNameIndex *found;
    size_t pos;
    size_t best;

    if (index == NULL || count == 0 || name == NULL)
        return -1;

    key.name = name;
    key.index = 0;
    found = bsearch(&key, index, count, sizeof(HIRRoutineNameIndex),
                    hir_routine_name_index_compare);
    if (found == NULL)
        return -1;

    pos = (size_t)(found - index);
    while (pos > 0 && strcmp(index[pos - 1].name, name) == 0)
        pos--;
    best = index[pos].index;
    while (pos < count && strcmp(index[pos].name, name) == 0) {
        if (index[pos].index < best)
            best = index[pos].index;
        pos++;
    }
    return (ssize_t)best;
}

static bool
hir_materialize_direct_call_edges(HIRProgram *hir, char **error_message)
{
    size_t routine_index_count = 0;
    HIRMutableRoutineInventory inventory;
    hir_mutable_routine_inventory_from_program(hir, &inventory);
    HIRRoutineNameIndex *routine_index =
        hir_build_routine_name_index(&inventory, &routine_index_count);

    if (inventory.count > 0 && routine_index == NULL)
        goto oom;

    for (size_t i = 0; i < inventory.count; i++) {
        HIRRoutine *routine = hir_mutable_routine_inventory_get(&inventory, i);
        if (routine == NULL)
            goto oom;
        for (size_t j = 0; j < routine->direct_call_count; j++) {
            ssize_t callee = hir_lookup_routine_index_by_name(
                routine_index, routine_index_count, routine->direct_calls[j]);
            if (callee < 0)
                continue;
            if (!hir_append_index_unique(&routine->callee_routine_ids,
                                         &routine->callee_routine_count,
                                         &routine->callee_routine_capacity,
                                         (size_t)callee)) {
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
                size_t callee = routine->callee_routine_ids[j];
                HIRRoutine *callee_routine =
                    hir_mutable_routine_inventory_get(&inventory, callee);
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

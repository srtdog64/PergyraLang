#ifndef PERGYRA_PARALLEL_CAPTURE_STORAGE_REACH_H
#define PERGYRA_PARALLEL_CAPTURE_STORAGE_REACH_H

#include <stdbool.h>
#include <stddef.h>

#include "type_checker_internal.h"

/*
 * What a parallel task can reach through a captured binding beyond the
 * binding's own name. The capture checks in type_checker_flow_parallel.c
 * reason about names; these two answers close the gaps where the storage a
 * name reaches is shared under another name.
 */

/* Does a value of `type` reach growable collection storage (Array, Slice,
 * List, Queue, Set, HashMap) through its fields, tuple elements, or type
 * arguments? A struct holding a copy of an Array shares the Array's storage,
 * so capturing it is capturing the collection. On success path_out holds the
 * dotted field path from the binding ("inner.data"; empty when the storage is
 * reached through a type argument or tuple element) and *kind_out the storage
 * kind. A field whose type cannot be resolved counts as reaching storage. */
bool parallel_capture_type_reaches_storage(SemanticContext *ctx,
                                           const Type *type,
                                           char *path_out,
                                           size_t path_cap,
                                           const char **kind_out);

/* Can `task` write through the binding `name` of nominal type `type` other
 * than by assigning it? A call of a method that writes `self`, a method that
 * cannot be resolved, or any use of the bare binding that lets its reference
 * flow elsewhere (an argument, an initializer, a returned value) counts. */
bool parallel_task_writes_through_binding(SemanticContext *ctx,
                                          const ASTNode *task,
                                          const char *name,
                                          const Type *type);

#endif /* PERGYRA_PARALLEL_CAPTURE_STORAGE_REACH_H */

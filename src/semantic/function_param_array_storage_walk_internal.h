#ifndef PERGYRA_FUNCTION_PARAM_ARRAY_STORAGE_WALK_INTERNAL_H
#define PERGYRA_FUNCTION_PARAM_ARRAY_STORAGE_WALK_INTERNAL_H

#include "function_param_flow_summary_internal.h"

typedef struct {
    const SlotSummaryOrigin *origin;
    const SlotFunctionLookup *lookup;
    bool *payload_read;
    bool paths;
    bool returns;
    uint32_t *alias_ids;
    const char **alias_names;
    size_t alias_count, alias_capacity;
    uint32_t *scoped_string_stack_calls;
    size_t scoped_string_stack_count, scoped_string_stack_capacity;
    bool scoped_string_stack_failed, direct_string_pop_seen;
    const char *focus;
    bool *buffer_unkept;
} FunctionParamArrayStorageWalk;

bool function_param_array_storage_formal_kind(
    const ASTNode *decl, size_t index, bool *paths);
bool function_param_array_storage_alias_add(
    FunctionParamArrayStorageWalk *walk, uint32_t id, const char *name);
bool function_param_array_storage_alias_named(
    const char *candidate, void *userdata);
bool function_param_array_storage_formal(
    const ASTNode *node, const FunctionParamArrayStorageWalk *walk);
void function_param_array_storage_scoped_string_stack_scan(
    FunctionParamArrayStorageWalk *walk, ASTNode *node);
bool function_param_array_storage_scoped_string_stack_call(
    const FunctionParamArrayStorageWalk *walk, const ASTNode *call);

#endif

/* Array formal identity, aliases, and lexical String stack pairing. */
#include "function_param_flow_summary_internal.h"
#include "function_param_publication_internal.h"
#include "../formal_string_store_landing_owner.h"
#include "../collection_owned_element_requirement_owner.h"
#include "../array_storage_deferred_preservation_owner.h"
#include "../loan_copy_landing_owner.h"
#include "../type_checker/type_checker_resolution_metadata_internal.h"
#include "../type_checker/type_checker_internal.h"
#include "../type_checker/type_checker_builtins_stdlib_collections_internal.h"
#include "../type_checker/type_checker_ownership_internal.h"
#include "../typed_map_query_fact_owner.h"
#include "../../parser/ast_analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "function_param_array_storage_walk_internal.h"
/* An Array formal other than `own` is proved as a descriptor. A by-value,
 * `ref` or `inout` formal of any other type, `self` included, is proved
 * through its access paths; an `inout` one only when nothing is written
 * through it (2026-10-22_unwritten_inout_formal_preserves_storage). An
 * `own` one is not proved. */
bool
function_param_array_storage_formal_kind(const ASTNode *decl, size_t index, bool *paths)
{
    FuncParam *param = ast_func_param(decl, index);
    if (param == NULL || param->mode == PARAM_MODE_OWN) return false;
    const char *type_name = param->type == NULL ? NULL : ast_type_name(param->type);
    *paths = type_name == NULL || strcmp(type_name, "Array") != 0;
    if (!*paths) return true;
    bool self_param = param->type == NULL && param->name != NULL && strcmp(param->name, "self") == 0;
    return (param->type != NULL || self_param) && (param->mode == PARAM_MODE_DEFAULT ||
        param->mode == PARAM_MODE_REF || param->mode == PARAM_MODE_MUT_REF);
}

/* One formal's walk. A local or loop variable that takes a path of the
 * formal, `let ids = arena.ids` or `for block in blocks`, is an alias: it
 * names storage the caller shares, so its uses are the formal's paths. The
 * formal's summary does not follow an alias, so this walk proves where an
 * alias's storage goes, not only that it is kept. A local that takes a call
 * result holding the formal, as `let part = Part(arena)`, is an alias too. */
bool function_param_array_storage_alias_add(FunctionParamArrayStorageWalk *walk, uint32_t id, const char *name)
{
    if (id == 0 || name == NULL) return false;
    if (walk->alias_count == walk->alias_capacity) {
        size_t capacity = walk->alias_capacity == 0 ? 4 : walk->alias_capacity * 2;
        uint32_t *ids = realloc(walk->alias_ids, capacity * sizeof(*ids));
        if (ids == NULL) return false;
        walk->alias_ids = ids;
        const char **names = realloc(walk->alias_names, capacity * sizeof(*names));
        if (names == NULL) return false;
        walk->alias_names = names;
        walk->alias_capacity = capacity;
    }
    walk->alias_ids[walk->alias_count] = id;
    walk->alias_names[walk->alias_count++] = name;
    return true;
}

bool function_param_array_storage_alias_named(const char *candidate, void *userdata)
{
    const FunctionParamArrayStorageWalk *walk = userdata;
    for (size_t i = 0; candidate != NULL && i < walk->alias_count; i++)
        if (strcmp(candidate, walk->alias_names[i]) == 0) return true;
    return false;
}

bool
function_param_array_storage_formal(const ASTNode *node, const FunctionParamArrayStorageWalk *walk)
{
    const SlotSummaryOrigin *origin = walk->origin;
    if (node == NULL || node->type != AST_IDENTIFIER) return false;
    FuncParam *param = ast_func_param(origin->function_decl, origin->param_index);
    if (param == NULL || ast_func_param_stable_id(param) == 0
        || ast_identifier_binding_syntax_id(node) != ast_func_param_stable_id(param)) return false;
    SemanticPublishedValue value = semantic_param_publication_value(walk->lookup->ctx, node);
    return value.ok && !value.meta && type_is_constructed_named(value.type, "Array");
}

/* A borrowed String may be kept in an inout Array only for a lexical stack
 * interval whose matching pop is unavoidable.  Calls inside that interval
 * are checked by the ordinary formal walk; this prepass owns only the local
 * push/pop pairing and the pushed binding's continued identity. */
typedef struct { uint32_t push, source; bool valid; } ScopedStringPush;

static bool scoped_string_call_add(FunctionParamArrayStorageWalk *walk, uint32_t id)
{
    if (id == 0) return false;
    if (walk->scoped_string_stack_count == walk->scoped_string_stack_capacity) {
        size_t capacity = walk->scoped_string_stack_capacity == 0 ? 4
            : walk->scoped_string_stack_capacity * 2;
        uint32_t *grown = realloc(walk->scoped_string_stack_calls,
            capacity * sizeof(*grown));
        if (grown == NULL) return false;
        walk->scoped_string_stack_calls = grown;
        walk->scoped_string_stack_capacity = capacity;
    }
    walk->scoped_string_stack_calls[walk->scoped_string_stack_count++] = id;
    return true;
}

static int scoped_string_formal_mutation(ASTNode *node, FunctionParamArrayStorageWalk *walk,
    uint32_t *source)
{
    if (node == NULL || node->type != AST_CALL || ast_call_arg_count(node) == 0
        || !function_param_array_storage_formal(ast_call_argument(node, 0), walk)) return 0;
    if (semantic_param_publication_checked_array_pop(walk->lookup->ctx, node))
        return -1;
    size_t stored = SIZE_MAX;
    if (!semantic_param_publication_checked_array_store(walk->lookup->ctx,
            node, &stored) || stored != 1 || ast_call_arg_count(node) != 2)
        return 0;
    ASTNode *value = ast_call_argument(node, 1);
    if (value == NULL || (value->type != AST_STRING
            && value->type != AST_IDENTIFIER)) return 0;
    *source = value->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(value) : 0;
    return value->type == AST_STRING || *source != 0 ? 1 : 0;
}

static bool scoped_string_pair_barrier(ASTNode *node, FunctionParamArrayStorageWalk *walk,
    uint32_t source)
{
    if (node == NULL) return false;
    uint32_t ignored = 0;
    if (scoped_string_formal_mutation(node, walk, &ignored) != 0
        || node->type == AST_RETURN || node->type == AST_BREAK
        || node->type == AST_CONTINUE || node->type == AST_FAIL_STMT
        || node->type == AST_GIVE_STMT) return true;
    if (source != 0 && node->type == AST_ASSIGNMENT) {
        ASTNode *target = ast_assignment_target(node);
        if (target != NULL && target->type == AST_IDENTIFIER
            && ast_identifier_binding_syntax_id(target) == source) return true;
    }
    bool supported = false;
    size_t count = function_param_flow_claim_child_count(node, &supported);
    if (!supported || count == SIZE_MAX) return true;
    for (size_t i = 0; i < count; i++)
        if (scoped_string_pair_barrier(
                function_param_flow_claim_child(node, i), walk, source))
            return true;
    return false;
}

void function_param_array_storage_scoped_string_stack_scan(FunctionParamArrayStorageWalk *walk, ASTNode *node);

static void scoped_string_stack_block(FunctionParamArrayStorageWalk *walk, ASTNode *block)
{
    size_t count = ast_block_statement_count(block), depth = 0;
    ScopedStringPush *stack = count == 0 ? NULL : calloc(count, sizeof(*stack));
    if (count != 0 && stack == NULL) { walk->scoped_string_stack_failed = true; return; }
    for (size_t i = 0; i < count && !walk->scoped_string_stack_failed; i++) {
        ASTNode *stmt = ast_block_statement(block, i);
        uint32_t source = 0;
        int mutation = scoped_string_formal_mutation(stmt, walk, &source);
        if (mutation > 0) stack[depth++] = (ScopedStringPush){ast_node_stable_id(stmt), source, true};
        else if (mutation < 0) {
            walk->direct_string_pop_seen = true;
            if (depth != 0) {
                ScopedStringPush pushed = stack[--depth];
                if (pushed.valid) {
                    if (!scoped_string_call_add(walk, ast_node_stable_id(stmt))
                        || !scoped_string_call_add(walk, pushed.push))
                        walk->scoped_string_stack_failed = true;
                }
            }
        } else {
            for (size_t p = 0; p < depth; p++)
                if (stack[p].valid && scoped_string_pair_barrier(
                        stmt, walk, stack[p].source)) stack[p].valid = false;
            function_param_array_storage_scoped_string_stack_scan(walk, stmt);
        }
    }
    free(stack);
}

void function_param_array_storage_scoped_string_stack_scan(FunctionParamArrayStorageWalk *walk, ASTNode *node)
{
    if (node == NULL || walk->scoped_string_stack_failed) return;
    if (node->type == AST_BLOCK) { scoped_string_stack_block(walk, node); return; }
    bool supported = false;
    size_t count = function_param_flow_claim_child_count(node, &supported);
    if (!supported || count == SIZE_MAX) { walk->scoped_string_stack_failed = true; return; }
    for (size_t i = 0; i < count; i++)
        function_param_array_storage_scoped_string_stack_scan(walk, function_param_flow_claim_child(node, i));
}

bool function_param_array_storage_scoped_string_stack_call(const FunctionParamArrayStorageWalk *walk, const ASTNode *call)
{
    uint32_t id = ast_node_stable_id(call);
    for (size_t i = 0; !walk->scoped_string_stack_failed
         && id != 0 && i < walk->scoped_string_stack_count; i++)
        if (walk->scoped_string_stack_calls[i] == id) return true;
    return false;
}

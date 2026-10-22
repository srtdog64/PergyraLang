/* Descriptor and element preservation over the same demanded indexed roots. */
#include "function_param_flow_summary_internal.h"
#include "function_param_publication_internal.h"
#include "type_checker_resolution_metadata_internal.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_stdlib_collections_internal.h"
#include "type_checker_ownership_internal.h"
#include "typed_map_query_fact_owner.h"
#include "../parser/ast_analysis.h"
#include <stdlib.h>
#include <string.h>

/* An Array formal other than `own` is proved as a descriptor. A by-value,
 * `ref` or `inout` formal of any other type, `self` included, is proved
 * through its access paths; an `inout` one only when nothing is written
 * through it (2026-10-22_unwritten_inout_formal_preserves_storage). An
 * `own` one is not proved. */
static bool
array_storage_formal_kind(const ASTNode *decl, size_t index, bool *paths)
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
typedef struct {
    const SlotSummaryOrigin *origin;
    const SlotFunctionLookup *lookup;
    bool *payload_read;
    bool paths;
    /* The formal's summary returns it, so a returned alias is a getter's. */
    bool returns;
    uint32_t *alias_ids;
    const char **alias_names;
    size_t alias_count, alias_capacity;
} StorageWalk;

/* RESULT is a call result that holds storage of a path of the formal. */
enum { STORAGE_ROOT_NONE, STORAGE_ROOT_FORMAL, STORAGE_ROOT_ALIAS, STORAGE_ROOT_RESULT };

static bool storage_alias_add(StorageWalk *walk, uint32_t id, const char *name)
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

static bool storage_alias_named(const char *candidate, void *userdata)
{
    const StorageWalk *walk = userdata;
    for (size_t i = 0; candidate != NULL && i < walk->alias_count; i++)
        if (strcmp(candidate, walk->alias_names[i]) == 0) return true;
    return false;
}

static bool
array_storage_formal(const ASTNode *node, const StorageWalk *walk)
{
    const SlotSummaryOrigin *origin = walk->origin;
    if (node == NULL || node->type != AST_IDENTIFIER) return false;
    FuncParam *param = ast_func_param(origin->function_decl, origin->param_index);
    if (param == NULL || ast_func_param_stable_id(param) == 0
        || ast_identifier_binding_syntax_id(node) != ast_func_param_stable_id(param)) return false;
    SemanticPublishedValue value = semantic_param_publication_value(walk->lookup->ctx, node);
    return value.ok && !value.meta && type_is_constructed_named(value.type, "Array");
}

static unsigned array_storage_use_unproved(ASTNode *node, StorageWalk *walk);

/* A formal that is not an Array holds its storage behind access paths:
 * `arena.atoms`, `arena.blocks[i].ids`. Its by-value copy shares that
 * storage with the caller, and a store through the copy's path changes the
 * caller's storage without updating the caller's descriptors. An Array
 * formal's elements are such paths too: `blocks[i].ids`. */
static bool storage_path_step(const ASTNode *node)
{
    return node != NULL && (node->type == AST_MEMBER_ACCESS || node->type == AST_ARRAY_ACCESS);
}

/* A checked `xs.Slice(start, len)` is a view of its receiver's storage: a
 * path through it is an alias of that storage. */
static ASTNode *storage_view_receiver(const ASTNode *node, const StorageWalk *walk)
{
    return node != NULL && node->type == AST_CALL
        ? semantic_param_publication_slice_view_receiver(walk->lookup->ctx, node) : NULL;
}

static bool storage_path_value_read(const ASTNode *node, const SlotFunctionLookup *lookup);
static int storage_path_root(const ASTNode *node, const StorageWalk *walk);

/* A call's result holds what an argument names when the callee may return
 * it: a routine whose formal's summary returns it, or a builtin or
 * constructor whose result keeps or depends on it. Clone's result is new
 * storage, as is a typed Map query's (MapKeys copies every key). A String
 * or copy-only result or argument holds nothing. */
static int storage_result_root(const ASTNode *call, const StorageWalk *walk)
{
    if (storage_path_value_read(call, walk->lookup) ||
        semantic_typed_map_query_result_independent(walk->lookup->ctx, call)) return STORAGE_ROOT_NONE;
    int root = STORAGE_ROOT_NONE;
    for (size_t i = 0; i < ast_call_arg_count(call) && root != STORAGE_ROOT_ALIAS; i++) {
        int held = storage_path_root(ast_call_argument(call, i), walk);
        bool depends = true, retains = true;
        if (held == STORAGE_ROOT_NONE || storage_path_value_read(ast_call_argument(call, i), walk->lookup) ||
            (semantic_param_publication_callee(walk->lookup->ctx, call, i) != NULL
                ? (function_param_flow_summary_for_call(walk->lookup, call, i) & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0
                : semantic_param_publication_checked_result_relation(walk->lookup->ctx, call, i, &depends, &retains) &&
                    !depends && !retains)) continue;
        root = held == STORAGE_ROOT_ALIAS ? STORAGE_ROOT_ALIAS : STORAGE_ROOT_RESULT;
    }
    return root;
}

static int storage_path_root(const ASTNode *node, const StorageWalk *walk)
{
    FuncParam *param = ast_func_param(walk->origin->function_decl, walk->origin->param_index);
    uint32_t id = param == NULL ? 0 : ast_func_param_stable_id(param);
    bool view = false;
    for (;;) {
        if (storage_path_step(node))
            node = node->type == AST_MEMBER_ACCESS ? ast_member_object(node) : ast_array_access_array(node);
        else if (storage_view_receiver(node, walk) != NULL) { node = storage_view_receiver(node, walk); view = true; }
        else break;
    }
    if (node != NULL && node->type == AST_CALL) {
        int held = storage_result_root(node, walk);
        return view && held == STORAGE_ROOT_RESULT ? STORAGE_ROOT_ALIAS : held;
    }
    uint32_t binding = node != NULL && node->type == AST_IDENTIFIER ? ast_identifier_binding_syntax_id(node) : 0;
    if (binding != 0 && binding == id) return view ? STORAGE_ROOT_ALIAS : STORAGE_ROOT_FORMAL;
    for (size_t i = 0; binding != 0 && i < walk->alias_count; i++)
        if (walk->alias_ids[i] == binding) return STORAGE_ROOT_ALIAS;
    return STORAGE_ROOT_NONE;
}

/* A path of the formal or any use of an alias. An Array formal by itself
 * is a descriptor, proved by the descriptor rules instead. */
static int storage_path_at(const ASTNode *node, const StorageWalk *walk)
{
    int root = storage_path_root(node, walk);
    return root == STORAGE_ROOT_FORMAL && !walk->paths && !storage_path_step(node) ? STORAGE_ROOT_NONE : root;
}

/* A value that holds no container storage, a String for one, read through
 * a path carries none of the formal's. */
static bool storage_path_value_read(const ASTNode *node, const SlotFunctionLookup *lookup)
{
    SemanticPublishedValue value = semantic_param_publication_value(lookup->ctx, node);
    return value.ok && !value.meta && semantic_ownership_type_holds_no_container(value.type, lookup->ctx);
}

static unsigned storage_call_unproved(ASTNode *call, StorageWalk *walk);
static unsigned storage_match_unproved(ASTNode *match, StorageWalk *walk);

/* Index expressions and a view's bounds along a path are ordinary uses; an
 * index step reads an element of the storage. A call result the path starts
 * from is proved as any other call. */
static unsigned storage_path_indices(ASTNode *node, StorageWalk *walk)
{
    unsigned bad = 0;
    for (ASTNode *viewed; (viewed = storage_view_receiver(node, walk)) != NULL || storage_path_step(node);) {
        if (viewed != NULL) {
            for (size_t i = 0; i < ast_call_arg_count(node); i++) {
                bad |= array_storage_use_unproved(ast_call_argument(node, i), walk);
                if (bad == FUNCTION_PARAM_ARRAY_ALL_UNPROVED) return bad;
            }
            node = viewed;
            continue;
        }
        if (node->type == AST_MEMBER_ACCESS) { node = ast_member_object(node); continue; }
        *walk->payload_read = true;
        bad |= array_storage_use_unproved(ast_array_access_index(node), walk);
        if (bad == FUNCTION_PARAM_ARRAY_ALL_UNPROVED) return bad;
        node = ast_array_access_array(node);
    }
    return node != NULL && node->type == AST_CALL ? bad | storage_call_unproved(node, walk) : bad;
}

/* A path handed to a routine keeps its storage when the routine's formal
 * keeps what it receives. A builtin writer stores through its receiver, and
 * through this copy that store changes storage the caller's descriptor
 * still names. Any other builtin keeps its storage when it only borrows the
 * argument for the call, or keeps nothing and returns new storage, as Clone
 * of an Array of Strings or scalars does, or a String or copy-only value.
 * An alias's escapes are not in the formal's summary, so a routine may only
 * read an alias or return it, a result the walk follows as the alias. */
static unsigned storage_path_call_argument_unproved(ASTNode *call, size_t ordinal,
    const SlotFunctionLookup *lookup, bool alias)
{
    SemanticContext *ctx = lookup->ctx;
    if (!semantic_param_publication_checked_argument(ctx, call, ordinal))
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    ASTNode *decl = semantic_param_publication_callee(ctx, call, ordinal);
    size_t formal = 0;
    if (decl != NULL)
        return !semantic_param_publication_formal_for_actual(ctx, call, ordinal, &formal) ||
            (alias && (function_param_flow_summary_for_call(lookup, call, ordinal) &
                ~(SLOT_PARAM_SUMMARY_READ | SLOT_PARAM_SUMMARY_RETURN_ESCAPE)) != 0)
            ? FUNCTION_PARAM_ARRAY_ALL_UNPROVED : function_param_flow_array_preservation_failures(ctx, decl, formal);
    size_t stored = 0;
    uint32_t kind = 0;
    if (ordinal == 0 && (semantic_param_publication_checked_array_store(ctx, call, &stored) ||
            semantic_param_publication_checked_slot_write(ctx, call) ||
            (ast_call_semantic_callee_builtin_kind(call, &kind) &&
             kind == (uint32_t)BUILTIN_ARRAY_PUSH_OWNED_STRING)))
        return FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED;
    if (semantic_typed_map_query_receiver_read(ctx, call, ordinal) ||
        semantic_param_publication_argument_borrowed_for_call(ctx, call, ordinal)) return 0;
    /* A result that depends on the path, as a constructor's or UnwrapOption's
     * does, holds nothing else: the walk follows that result as a path. */
    bool depends = true, retains = true;
    return semantic_param_publication_checked_result_relation(ctx, call, ordinal, &depends, &retains) &&
        !retains ? 0 : FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
}

static unsigned
array_storage_call_argument_unproved(ASTNode *call, size_t ordinal,
    const SlotFunctionLookup *lookup)
{
    ASTNode *callee = ast_call_callee(call);
    uint32_t identity = ast_call_semantic_callee_decl_id(call);
    if (!semantic_param_publication_checked_argument(lookup->ctx, call, ordinal))
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    if (semantic_stdlib_array_argument_preserves_storage(call, ordinal))
        return semantic_stdlib_array_argument_preserves_elements(call, ordinal, lookup->ctx)
            ? 0 : FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED;
    if (identity == 0 || ast_call_semantic_callee_value_binding_id(call) != 0
        || callee == NULL || callee->type != AST_IDENTIFIER)
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    ASTNode *decl = slot_analyzer_find_function_decl(lookup, ast_identifier_name(callee));
    if (decl == NULL || ast_node_stable_id(decl) != identity)
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    return function_param_flow_array_preservation_failures(lookup->ctx, decl, ordinal);
}

/* Iterating a path reads its elements. A loop variable that is not a value
 * aliases the element it holds. */
static unsigned storage_iteration_unproved(ASTNode *loop, StorageWalk *walk)
{
    ASTNode *items = ast_for_iterable(loop);
    if (storage_path_root(items, walk) == STORAGE_ROOT_NONE) return array_storage_use_unproved(items, walk);
    *walk->payload_read = true;
    unsigned bad = storage_path_indices(items, walk);
    SemanticPublishedValue value = semantic_param_publication_value(walk->lookup->ctx, items);
    const Type *element = value.ok && !value.meta ? type_get_constructed_arg(value.type, 0) : NULL;
    if (bad == FUNCTION_PARAM_ARRAY_ALL_UNPROVED || element == NULL) return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    bool shares = !type_equals(element, TYPE_STRING) &&
        semantic_classify_ownership_type(element, walk->lookup->ctx) != OWNERSHIP_TYPE_COPY_ONLY;
    return !shares || storage_alias_add(walk, ast_node_stable_id(loop), ast_for_variable(loop))
        ? bad : FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
}

static unsigned
array_storage_use_unproved(ASTNode *node, StorageWalk *walk)
{
    if (node == NULL) return 0;
    unsigned bad = 0;
    int root = storage_path_at(node, walk);
    /* A path read in any other place copies what it names: a String or
     * copy-only value shares nothing, anything else aliases the storage. */
    if (root != STORAGE_ROOT_NONE)
        return storage_path_value_read(node, walk->lookup)
            ? storage_path_indices(node, walk) : FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
#define USE(child) array_storage_use_unproved((child), walk)
#define FORMAL(child) array_storage_formal((child), walk)
#define ADD(child) do { bad |= USE(child); if (bad == FUNCTION_PARAM_ARRAY_ALL_UNPROVED) return bad; } while (0)
#define EACH(count, child) do { for (size_t i = 0; i < (count); i++) { ADD(child); } return bad; } while (0)
    switch (node->type) {
    case AST_IDENTIFIER:
        /* A stale or shadowed spelling is not a proof of this formal, and
         * an unbound spelling of an alias is not a proof of the alias. */
        if (ast_identifier_name(node) != NULL && ((walk->origin->param_name != NULL
            && strcmp(ast_identifier_name(node), walk->origin->param_name) == 0) ||
            (ast_identifier_binding_syntax_id(node) == 0 && storage_alias_named(ast_identifier_name(node), walk))))
            return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
        return FORMAL(node) ? FUNCTION_PARAM_ARRAY_ALL_UNPROVED : 0;
    case AST_NUMBER: case AST_STRING: case AST_BOOLEAN: case AST_BREAK: case AST_CONTINUE:
        return 0;
    case AST_BLOCK:
        EACH(ast_block_statement_count(node), ast_block_statement(node, i));
    case AST_LET_DECL: {
        /* A local that takes a path holds the caller's storage. */
        ASTNode *value = ast_let_initializer(node);
        if (storage_path_at(value, walk) == STORAGE_ROOT_NONE || storage_path_value_read(value, walk->lookup))
            return USE(value);
        bad = storage_path_indices(value, walk);
        return bad == FUNCTION_PARAM_ARRAY_ALL_UNPROVED ||
            !storage_alias_add(walk, ast_node_stable_id(node), ast_let_name(node)) ? FUNCTION_PARAM_ARRAY_ALL_UNPROVED : bad;
    }
    case AST_IF_STMT:
        ADD(ast_if_condition(node)); ADD(ast_if_then_branch(node)); ADD(ast_if_else_branch(node)); return bad;
    case AST_WHILE_LOOP: ADD(ast_while_condition(node)); ADD(ast_while_body(node)); return bad;
    case AST_FOR_LOOP:
        ADD(ast_for_range_start(node)); ADD(ast_for_range_end(node));
        bad |= storage_iteration_unproved(node, walk);
        if (bad == FUNCTION_PARAM_ARRAY_ALL_UNPROVED) return bad;
        ADD(ast_for_body(node)); return bad;
    case AST_MATCH_STMT:
        if (storage_path_at(ast_match_subject(node), walk) != STORAGE_ROOT_NONE &&
            !storage_path_value_read(ast_match_subject(node), walk->lookup))
            return storage_match_unproved(node, walk);
        ADD(ast_match_subject(node)); ADD(ast_match_default_body(node));
        EACH(ast_match_case_count(node), ast_match_case_at(node, i));
    case AST_MATCH_CASE:
        ADD(ast_match_case_pattern(node)); ADD(ast_match_case_guard(node)); ADD(ast_match_case_body(node)); return bad;
    case AST_ASSIGNMENT: {
        ASTNode *target = ast_assignment_target(node);
        root = storage_path_at(target, walk);
        if (root == STORAGE_ROOT_NONE) { ADD(target); ADD(ast_assignment_value(node)); return bad; }
        /* An `inout` formal is the caller's place: any write through it
         * changes the caller's value. */
        if (walk->paths && ast_func_param(walk->origin->function_decl, walk->origin->param_index)->mode ==
                PARAM_MODE_MUT_REF) return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
        /* A bare formal or alias that takes another value holding the
         * formal stays what it was, as in `build = Add(build, row)`. */
        ASTNode *value = ast_assignment_value(node);
        if (!storage_path_step(target) && storage_path_at(value, walk) != STORAGE_ROOT_NONE &&
            !storage_path_value_read(value, walk->lookup))
            return storage_path_indices(value, walk);
        ADD(value);
        bad |= storage_path_indices(target, walk);
        if (bad == FUNCTION_PARAM_ARRAY_ALL_UNPROVED) return bad;
        /* Writing an Array formal's own element changes its elements only.
         * A field of a by-value copy changes only the copy. Any other
         * target below an index step is in the storage the copy shares. */
        if (!walk->paths && target->type == AST_ARRAY_ACCESS && FORMAL(ast_array_access_array(target)))
            return bad | FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED;
        for (const ASTNode *step = target; storage_path_step(step);
             step = step->type == AST_MEMBER_ACCESS ? ast_member_object(step) : ast_array_access_array(step))
            if (step->type == AST_ARRAY_ACCESS) return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
        return bad;
    }
    case AST_BINARY: ADD(ast_binary_left(node)); ADD(ast_binary_right(node)); return bad;
    case AST_UNARY: return USE(ast_unary_operand(node));
    case AST_CAST: return USE(ast_cast_operand(node));
    case AST_TYPE_TEST: return USE(ast_type_test_operand(node));
    case AST_LET_DESTRUCTURE: return USE(ast_let_destructure_initializer(node));
    case AST_RETURN:
        /* The summary's RETURN bit carries a returned path of the formal, or
         * a call result that holds one; the caller's loan proof follows the
         * result. The summary does not follow an alias, so a returned alias
         * is a getter's only when the summary returns the formal anyway. */
        root = storage_path_root(ast_return_value(node), walk);
        if (walk->paths && (root == STORAGE_ROOT_FORMAL || root == STORAGE_ROOT_RESULT ||
                (root == STORAGE_ROOT_ALIAS && walk->returns)))
            return storage_path_indices(ast_return_value(node), walk);
        return USE(ast_return_value(node));
    case AST_MEMBER_ACCESS: return USE(ast_member_object(node));
    case AST_ARRAY_ACCESS:
        if (FORMAL(ast_array_access_array(node))) *walk->payload_read = true;
        else { ADD(ast_array_access_array(node)); }
        ADD(ast_array_access_index(node)); return bad;
    case AST_ARRAY_LITERAL: EACH(ast_array_literal_count(node), ast_array_literal_element(node, i));
    case AST_TUPLE_LITERAL: EACH(ast_tuple_literal_count(node), ast_tuple_literal_element(node, i));
    case AST_SET_LITERAL: EACH(ast_set_literal_count(node), ast_set_literal_element(node, i));
    case AST_MAP_LITERAL:
        for (size_t i = 0; i < ast_map_literal_count(node); i++) {
            ADD(ast_map_literal_key(node, i)); ADD(ast_map_literal_value(node, i));
        }
        return bad;
    case AST_CALL: return storage_call_unproved(node, walk);
    default: return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    }
#undef USE
#undef FORMAL
#undef ADD
#undef EACH
}

/* Matching a value that holds the formal binds parts of it: a payload
 * binding aliases the storage it holds, as a loop variable does. A pattern
 * names nothing of the formal; its guard and body are uses. */
static unsigned storage_match_unproved(ASTNode *match, StorageWalk *walk)
{
    unsigned bad = storage_path_indices(ast_match_subject(match), walk);
    for (size_t c = 0; c < ast_match_case_count(match) && bad != FUNCTION_PARAM_ARRAY_ALL_UNPROVED; c++) {
        ASTNode *arm = ast_match_case_at(match, c);
        if (arm == NULL || arm->type != AST_MATCH_CASE) return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
        for (size_t p = 0; p < ast_match_case_pattern_count(arm); p++) {
            ASTNode *pattern = ast_match_case_pattern_at(arm, p);
            bool payload = pattern != NULL && pattern->type == AST_CALL;
            for (size_t a = 0; pattern != NULL && a < (payload ? ast_call_arg_count(pattern) : 1); a++) {
                ASTNode *binding = payload ? ast_call_argument(pattern, a) : pattern;
                uint32_t id = binding != NULL && binding->type == AST_IDENTIFIER ? ast_node_stable_id(binding) : 0;
                bool binds = id != 0 && ast_identifier_binding_syntax_id(binding) == id;
                /* A unit variant or a literal binds nothing. */
                if (!binds && !payload && (binding->type == AST_IDENTIFIER || binding->type == AST_MEMBER_ACCESS ||
                        binding->type == AST_NUMBER || binding->type == AST_STRING || binding->type == AST_BOOLEAN)) continue;
                if (!binds || !storage_alias_add(walk, id, ast_identifier_name(binding)))
                    return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
            }
        }
        bad |= array_storage_use_unproved(ast_match_case_guard(arm), walk);
        if (bad != FUNCTION_PARAM_ARRAY_ALL_UNPROVED) bad |= array_storage_use_unproved(ast_match_case_body(arm), walk);
    }
    return bad == FUNCTION_PARAM_ARRAY_ALL_UNPROVED ? bad : bad | array_storage_use_unproved(ast_match_default_body(match), walk);
}

/* A path, or a result that holds one, handed to a call. */
static unsigned storage_call_unproved(ASTNode *call, StorageWalk *walk)
{
    unsigned bad = array_storage_use_unproved(ast_call_callee(call), walk);
    for (size_t i = 0; i < ast_call_arg_count(call) && bad != FUNCTION_PARAM_ARRAY_ALL_UNPROVED; i++) {
        ASTNode *arg = ast_call_argument(call, i);
        int root = storage_path_at(arg, walk);
        if (root != STORAGE_ROOT_NONE) {
            bad |= storage_path_value_read(arg, walk->lookup)
                ? storage_path_indices(arg, walk)
                : storage_path_call_argument_unproved(call, i, walk->lookup, root == STORAGE_ROOT_ALIAS) |
                    storage_path_indices(arg, walk);
        } else bad |= array_storage_formal(arg, walk) ? array_storage_call_argument_unproved(call, i, walk->lookup)
            : array_storage_use_unproved(arg, walk);
    }
    return bad;
}

/* `roots` are the statements that name the formal, in the order of
 * `statements`. A statement that names only an alias is walked too. */
unsigned
function_param_array_storage_unproved_in_program_points(
    ASTNode *const *statements, size_t statement_count, ASTNode *const *roots, size_t count,
    const SlotSummaryOrigin *origin, const SlotFunctionLookup *lookup, unsigned summary, bool *payload_read)
{
    if (payload_read == NULL) return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    *payload_read = false;
    FuncParam *param = origin != NULL
        ? ast_func_param(origin->function_decl, origin->param_index) : NULL;
    StorageWalk walk = {origin, lookup, payload_read, false,
        (summary & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0, NULL, NULL, 0, 0};
    if (lookup == NULL || lookup->ctx == NULL || param == NULL
        || !array_storage_formal_kind(origin->function_decl, origin->param_index, &walk.paths)
        || ast_func_body(origin->function_decl) == NULL || origin->function_decl->is_async_decl
        || origin->param_name == NULL
        || ast_func_param_stable_id(param) == 0 || (roots == NULL && count != 0)
        || (statements == NULL && statement_count != 0))
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    unsigned bad = 0;
    size_t next = 0;
    for (size_t s = 0; s < statement_count && bad != FUNCTION_PARAM_ARRAY_ALL_UNPROVED; s++) {
        bool named = next < count && roots[next] == statements[s];
        next += named;
        if (!named && (walk.alias_count == 0 ||
                !ast_contains_identifier_ref(statements[s], storage_alias_named, &walk))) continue;
        bad |= statements[s] == NULL ? FUNCTION_PARAM_ARRAY_ALL_UNPROVED
            : array_storage_use_unproved(statements[s], &walk);
    }
    free(walk.alias_ids);
    free(walk.alias_names);
    return next == count ? bad : FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
}

static unsigned
array_preservation_bits(const FunctionParamFlowSummaryEntry *entry)
{
    return (entry->array_storage_unproved ? FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED : 0)
        | (entry->array_elements_unproved ? FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED : 0);
}

/* A settled entry's verdict, read without demanding a summary. Every entry
 * is walked in the first pass, and the walk refuses a formal that is not an
 * Array storage formal. */
unsigned
function_param_flow_array_preservation_settled(FunctionParamFlowSummaryStore *store, size_t index)
{
    const FunctionParamFlowSummaryEntry *entry = &store->entries[index];
    if (entry->state != FUNCTION_PARAM_FLOW_COMPLETE || entry->analysis_unsupported
        || !entry->output_dependencies_ready || !function_param_flow_output_current(store, index, false))
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    return array_preservation_bits(entry);
}

unsigned
function_param_flow_array_preservation_failures(SemanticContext *ctx,
    ASTNode *function_decl, size_t param_index)
{
    FuncParam *param = ast_func_param(function_decl, param_index);
    bool paths = false;
    if (ctx == NULL || function_decl == NULL || param == NULL
        || ast_func_body(function_decl) == NULL || function_decl->is_async_decl
        || !array_storage_formal_kind(function_decl, param_index, &paths))
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    (void)function_param_flow_summary_for_param(ctx, function_decl, param_index);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    size_t index = function_param_flow_find(store, ast_node_stable_id(function_decl), param_index);
    if (index == SIZE_MAX || store->entries[index].function_decl != function_decl)
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    return store->solving ? array_preservation_bits(&store->entries[index])
        : function_param_flow_array_preservation_settled(store, index);
}

bool
function_param_flow_preserves_array_storage(SemanticContext *ctx, ASTNode *decl, size_t ordinal)
{
    return (function_param_flow_array_preservation_failures(ctx, decl, ordinal)
        & FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED) == 0;
}

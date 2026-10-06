/* Descriptor preservation transfer over the demanded owner's indexed roots.
 * Plain element reads/writes are not descriptor stores. Unknown execution,
 * rebinding, forwarding to an owner, or retaining a descriptor fails closed. */
#include "function_param_flow_summary_internal.h"
#include "type_checker_builtins_stdlib_collections_internal.h"
#include <string.h>

static bool
array_storage_formal(const ASTNode *node, const SlotSummaryOrigin *origin)
{
    return node != NULL && node->type == AST_IDENTIFIER
        && ast_identifier_name(node) != NULL
        && strcmp(ast_identifier_name(node), origin->param_name) == 0;
}

static bool array_storage_use_unproved(ASTNode *node,
    const SlotSummaryOrigin *origin, const SlotFunctionLookup *lookup);

static bool
array_storage_call_argument_unproved(ASTNode *call, size_t ordinal,
    const SlotSummaryOrigin *origin, const SlotFunctionLookup *lookup)
{
    ASTNode *callee = ast_call_callee(call);
    ASTNode *decl;
    uint32_t identity = ast_call_semantic_callee_decl_id(call);
    if (semantic_stdlib_array_argument_preserves_storage(call, ordinal))
        return false;
    if (identity == 0 || ast_call_semantic_callee_value_binding_id(call) != 0
        || callee == NULL || callee->type != AST_IDENTIFIER)
        return true;
    decl = slot_analyzer_find_function_decl(lookup, ast_identifier_name(callee));
    if (decl == NULL || ast_node_stable_id(decl) != identity)
        return true;
    (void)origin;
    return !function_param_flow_preserves_array_storage(lookup->ctx, decl, ordinal);
}

static bool
array_storage_use_unproved(ASTNode *node, const SlotSummaryOrigin *origin,
                           const SlotFunctionLookup *lookup)
{
    if (node == NULL)
        return false;
#define UNPROVED(child) array_storage_use_unproved((child), origin, lookup)
    switch (node->type) {
    case AST_IDENTIFIER:
        return array_storage_formal(node, origin);
    case AST_NUMBER:
    case AST_STRING:
    case AST_BOOLEAN:
    case AST_BREAK:
    case AST_CONTINUE:
        return false;
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++)
            if (UNPROVED(ast_block_statement(node, i))) return true;
        return false;
    case AST_LET_DECL:
        return UNPROVED(ast_let_initializer(node));
    case AST_IF_STMT:
        return UNPROVED(ast_if_condition(node))
            || UNPROVED(ast_if_then_branch(node)) || UNPROVED(ast_if_else_branch(node));
    case AST_WHILE_LOOP:
        return UNPROVED(ast_while_condition(node)) || UNPROVED(ast_while_body(node));
    case AST_FOR_LOOP:
        return UNPROVED(ast_for_range_start(node)) || UNPROVED(ast_for_range_end(node))
            || UNPROVED(ast_for_body(node));
    case AST_MATCH_STMT:
        if (UNPROVED(ast_match_subject(node)) || UNPROVED(ast_match_default_body(node)))
            return true;
        for (size_t i = 0; i < ast_match_case_count(node); i++)
            if (UNPROVED(ast_match_case_at(node, i))) return true;
        return false;
    case AST_MATCH_CASE:
        return UNPROVED(ast_match_case_pattern(node))
            || UNPROVED(ast_match_case_guard(node)) || UNPROVED(ast_match_case_body(node));
    case AST_ASSIGNMENT:
        return UNPROVED(ast_assignment_target(node)) || UNPROVED(ast_assignment_value(node));
    case AST_BINARY:
        return UNPROVED(ast_binary_left(node)) || UNPROVED(ast_binary_right(node));
    case AST_UNARY:
        return UNPROVED(ast_unary_operand(node));
    case AST_CAST:
        return UNPROVED(ast_cast_operand(node));
    case AST_TYPE_TEST:
        return UNPROVED(ast_type_test_operand(node));
    case AST_LET_DESTRUCTURE:
        return UNPROVED(ast_let_destructure_initializer(node));
    case AST_RETURN:
        return UNPROVED(ast_return_value(node));
    case AST_MEMBER_ACCESS:
        return UNPROVED(ast_member_object(node));
    case AST_ARRAY_ACCESS:
        return (!array_storage_formal(ast_array_access_array(node), origin)
                && UNPROVED(ast_array_access_array(node)))
            || UNPROVED(ast_array_access_index(node));
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < node->data.array_literal.count; i++)
            if (UNPROVED(node->data.array_literal.elements[i])) return true;
        return false;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < node->data.tuple_literal.count; i++)
            if (UNPROVED(node->data.tuple_literal.elements[i])) return true;
        return false;
    case AST_SET_LITERAL:
        /* `{}` is also an empty statement body, as in `case None: {}`. */
        for (size_t i = 0; i < ast_set_literal_count(node); i++)
            if (UNPROVED(ast_set_literal_element(node, i))) return true;
        return false;
    case AST_MAP_LITERAL:
        for (size_t i = 0; i < ast_map_literal_count(node); i++)
            if (UNPROVED(ast_map_literal_key(node, i)) || UNPROVED(ast_map_literal_value(node, i)))
                return true;
        return false;
    case AST_CALL:
        if (UNPROVED(ast_call_callee(node))) return true;
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            ASTNode *arg = ast_call_argument(node, i);
            if (array_storage_formal(arg, origin)) {
                if (array_storage_call_argument_unproved(node, i, origin, lookup)) return true;
            } else if (UNPROVED(arg)) return true;
        }
        return false;
    default:
        /* Includes deferred/parallel/async/capture and unmodelled forms.
         * A missing transfer must never become permission by omission. */
        return true;
    }
#undef UNPROVED
}

bool
function_param_array_storage_unproved_in_program_points(
    ASTNode *const *roots, size_t count, const SlotSummaryOrigin *origin,
    const SlotFunctionLookup *lookup)
{
    FuncParam *param = origin != NULL
        ? ast_func_param(origin->function_decl, origin->param_index) : NULL;
    if (lookup == NULL || lookup->ctx == NULL || param == NULL || param->type == NULL
        || ast_func_body(origin->function_decl) == NULL || origin->function_decl->is_async_decl
        || ast_type_name(param->type) == NULL || strcmp(ast_type_name(param->type), "Array") != 0
        || param->mode == PARAM_MODE_OWN || origin->param_name == NULL
        || (roots == NULL && count != 0))
        return true;
    for (size_t i = 0; i < count; i++)
        if (roots[i] == NULL || array_storage_use_unproved(roots[i], origin, lookup))
            return true;
    return false;
}

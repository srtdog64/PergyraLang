/*
 * Expression operator and indexed-access type checkers.
 *
 * Kept out of type_checker_expr.h so the expression dispatcher stays
 * readable while operator overload and array literal rules remain together.
 */

#include <stdio.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_ownership_diag_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

static const char *
operator_overload_suffix(PgyTokenType op)
{
    switch (op) {
    case TOKEN_PLUS:          return "add";
    case TOKEN_MINUS:         return "sub";
    case TOKEN_STAR:          return "mul";
    case TOKEN_SLASH:         return "div";
    case TOKEN_PERCENT:       return "mod";
    case TOKEN_EQUAL:         return "eq";
    case TOKEN_NOT_EQUAL:     return "ne";
    case TOKEN_LESS:          return "lt";
    case TOKEN_LESS_EQUAL:    return "le";
    case TOKEN_GREATER:       return "gt";
    case TOKEN_GREATER_EQUAL: return "ge";
    default:                  return NULL;
    }
}

static bool
operator_method_name_matches(PgyTokenType op, const char *name)
{
    static const struct {
        PgyTokenType op;
        const char *names[10];
    } aliases[] = {
        { TOKEN_PLUS, { "Add", "add", "OperatorAdd", "operator_add", NULL } },
        { TOKEN_MINUS, { "Sub", "sub", "Subtract", "subtract",
                         "OperatorSub", "operator_sub", NULL } },
        { TOKEN_STAR, { "Mul", "mul", "Multiply", "multiply",
                        "OperatorMul", "operator_mul", NULL } },
        { TOKEN_SLASH, { "Div", "div", "Divide", "divide",
                         "OperatorDiv", "operator_div", NULL } },
        { TOKEN_PERCENT, { "Mod", "mod", "Modulo", "modulo",
                           "OperatorMod", "operator_mod", NULL } },
        { TOKEN_EQUAL, { "Eq", "eq", "Equal", "equal", "Equals", "equals",
                         "OperatorEq", "operator_eq", NULL } },
        { TOKEN_NOT_EQUAL, { "Ne", "ne", "NotEqual", "notEqual",
                             "NotEquals", "notEquals",
                             "OperatorNe", "operator_ne", NULL } },
        { TOKEN_LESS, { "Lt", "lt", "LessThan", "lessThan",
                        "OperatorLt", "operator_lt", NULL } },
        { TOKEN_LESS_EQUAL, { "Le", "le", "LessEqual", "lessEqual",
                              "LessThanOrEqual", "lessThanOrEqual",
                              "OperatorLe", "operator_le", NULL } },
        { TOKEN_GREATER, { "Gt", "gt", "GreaterThan", "greaterThan",
                           "OperatorGt", "operator_gt", NULL } },
        { TOKEN_GREATER_EQUAL, { "Ge", "ge", "GreaterEqual", "greaterEqual",
                                 "GreaterThanOrEqual", "greaterThanOrEqual",
                                 "OperatorGe", "operator_ge", NULL } },
    };

    if (name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (aliases[i].op != op)
            continue;
        for (size_t j = 0; aliases[i].names[j] != NULL; j++) {
            if (strcmp(aliases[i].names[j], name) == 0)
                return true;
        }
        break;
    }

    return false;
}

static ASTNode *
find_role_operator_method(ASTNode *role, SemanticContext *ctx, PgyTokenType op,
                          int depth)
{
    if (role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < ast_role_impl_count(role); i++) {
        ASTNode *impl = ast_role_impl(role, i);
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++) {
            ASTNode *method = ast_impl_ability_method(impl, j);
            const char *method_name = ast_declaration_name(method);
            if (method != NULL && method->type == AST_FUNC_DECL
                && operator_method_name_matches(op, method_name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < ast_role_include_count(role); i++) {
        ASTNode *inc = ast_role_include(role, i);
        const char *role_name = ast_include_role_name(inc);
        if (role_name == NULL)
            continue;
        ASTNode *included = semantic_find_role_decl_by_name(ctx, role_name);
        ASTNode *method = find_role_operator_method(included, ctx, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

static Type *
type_check_role_operator_overload(ASTNode *expr, SemanticContext *ctx,
                                  Type *left, Type *right)
{
    if (ctx == NULL || left == NULL || left->name == NULL)
        return NULL;

    ASTNode *stmt = NULL;
    while ((stmt = semantic_find_next_role_decl_for_type_name(
                ctx, left->name, stmt)) != NULL) {
        ASTNode *method = find_role_operator_method(
            stmt, ctx, ast_binary_operator(expr).type, 0);
        if (method == NULL)
            continue;

        FuncParam *rhs_param = NULL;
        size_t rhs_param_count = 0;
        for (size_t j = 0; j < ast_func_param_count(method); j++) {
            FuncParam *p = ast_func_param(method, j);
            if (p != NULL && !(p->type == NULL && strcmp(p->name, "self") == 0)) {
                rhs_param = p;
                rhs_param_count++;
            }
        }
        if (rhs_param_count != 1)
            continue;

        Type *rhs_type = TYPE_INT;
        if (rhs_param != NULL && rhs_param->type != NULL)
            rhs_type = type_check_func_resolve_param_type(rhs_param, ctx);
        if (!type_is_assignable(right, rhs_type))
            continue;

        if (ast_func_return_type(method) != NULL)
            return type_check_func_resolve_return_type(method, ctx);
        return TYPE_VOID;
    }

    return NULL;
}

static Type *
type_check_operator_overload(ASTNode *expr, SemanticContext *ctx,
                             Type *left, Type *right)
{
    const char *suffix = operator_overload_suffix(ast_binary_operator(expr).type);
    if (suffix == NULL || left == NULL || right == NULL || left->name == NULL)
        return NULL;

    char fn_name[256];
    snprintf(fn_name, sizeof(fn_name), "operator_%s_%s", suffix, left->name);

    Symbol *sym = scope_lookup(ctx->scope, fn_name);
    if (sym == NULL || sym->kind != SYMBOL_FUNCTION
        || sym->type == NULL || sym->type->kind != TYPE_KIND_FUNCTION)
        return NULL;

    if (type_function_param_count(sym->type) != 2)
        return NULL;

    Type *lhs = type_function_param_type(sym->type, 0);
    Type *rhs = type_function_param_type(sym->type, 1);
    if (!type_is_assignable(left, lhs) || !type_is_assignable(right, rhs))
        return NULL;

    sym->is_used = true;
    return type_function_return_type(sym->type);
}

static Type *
expr_ops_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

Type *
type_check_binary(ASTNode *expr, SemanticContext *ctx)
{
    Type *left  = expr_ops_normalize_type(
        type_check_expression(ast_binary_left(expr),  ctx));
    Type *right = expr_ops_normalize_type(
        type_check_expression(ast_binary_right(expr), ctx));

    if (type_is_slot_handle(left) && type_slot_inner_type(left) != NULL)
        left = type_slot_inner_type(left);
    if (type_is_slot_handle(right) && type_slot_inner_type(right) != NULL)
        right = type_slot_inner_type(right);

    Type *overloaded = type_check_operator_overload(expr, ctx, left, right);
    if (overloaded == NULL)
        overloaded = type_check_role_operator_overload(expr, ctx, left, right);
    if (overloaded != NULL)
        return overloaded;

    if (left == TYPE_UNKNOWN || right == TYPE_UNKNOWN) {
        PgyTokenType op = ast_binary_operator(expr).type;
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS     || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER  || op == TOKEN_GREATER_EQUAL
            || op == TOKEN_AND      || op == TOKEN_OR)
            return TYPE_BOOL;
        return (left != TYPE_UNKNOWN) ? left : right;
    }

    PgyTokenType op = ast_binary_operator(expr).type;
    if (op == TOKEN_COALESCE) {
        Type *inner;
        if (!type_is_constructed_named(left, "Option")) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BINOP_TYPE_MISMATCH,
                PGY_CAUSE_BINOP_OPERAND_TYPES,
                PGY_FIX_ALIGN_OPERAND_TYPES_OR_OVERLOAD,
                expr,
                "Option coalescing requires Option<T> on the left, got '%s'",
                type_name_or_unknown(left));
            return TYPE_UNKNOWN;
        }
        inner = expr_ops_normalize_type(type_get_constructed_arg(left, 0));
        if (!type_is_assignable(right, inner)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BINOP_TYPE_MISMATCH,
                PGY_CAUSE_BINOP_OPERAND_TYPES,
                PGY_FIX_ALIGN_OPERAND_TYPES_OR_OVERLOAD,
                expr,
                "Option coalescing fallback type '%s' is not assignable to '%s'",
                type_name_or_unknown(right), type_name_or_unknown(inner));
            return TYPE_UNKNOWN;
        }
        return inner;
    }

    if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
        || op == TOKEN_LESS     || op == TOKEN_LESS_EQUAL
        || op == TOKEN_GREATER  || op == TOKEN_GREATER_EQUAL) {
        if (!type_equals(left, right)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BINOP_TYPE_MISMATCH,
                PGY_CAUSE_BINOP_OPERAND_TYPES,
                PGY_FIX_ALIGN_OPERAND_TYPES_OR_OVERLOAD,
                expr,
                "Cannot compare '%s' and '%s'",
                type_name_or_unknown(left), type_name_or_unknown(right));
        }
        return TYPE_BOOL;
    }

    if (op == TOKEN_AND || op == TOKEN_OR) {
        if (!type_equals(left, TYPE_BOOL) || !type_equals(right, TYPE_BOOL)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BINOP_TYPE_MISMATCH,
                PGY_CAUSE_BINOP_OPERAND_TYPES,
                PGY_FIX_ALIGN_OPERAND_TYPES_OR_OVERLOAD,
                expr,
                "Logical operator requires Bool operands, got '%s' and '%s'",
                type_name_or_unknown(left), type_name_or_unknown(right));
        }
        return TYPE_BOOL;
    }

    if (!type_equals(left, right)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BINOP_TYPE_MISMATCH,
            PGY_CAUSE_BINOP_OPERAND_TYPES,
            PGY_FIX_ALIGN_OPERAND_TYPES_OR_OVERLOAD,
            expr,
            "Type mismatch in binary operation: '%s' and '%s'",
            type_name_or_unknown(left), type_name_or_unknown(right));
        return TYPE_UNKNOWN;
    }

    return left;
}

Type *
type_check_unary(ASTNode *expr, SemanticContext *ctx)
{
    PgyTokenType op = ast_unary_operator(expr).type;
    if (op == TOKEN_REFLECT) {
        ASTNode *reflect_target = ast_unary_operand(expr);
        if (reflect_target == NULL
            || reflect_target->type != AST_IDENTIFIER) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNOP_TYPE_MISMATCH,
                PGY_CAUSE_UNARY_OPERATOR_OPERAND,
                PGY_FIX_CHECK_INTENT_STEP_LOWERING, expr,
                "'reflect' currently supports only a type name; richer targets "
                "are not lowered yet (docs/reflect_operator_design.md)");
            return TYPE_UNKNOWN;
        }

        /*
         * Compile-time fold: rewrite `reflect TypeName` in place into the type
         * name as a String literal, so later lowering and both backends see a
         * plain constant and reflect leaves no runtime trace.
         */
        const char *reflect_name = ast_identifier_name(reflect_target);
        ast_morph_to_string(expr, reflect_name);
        return TYPE_PROJECTION;
    }

    Type *operand = expr_ops_normalize_type(
        type_check_expression(ast_unary_operand(expr), ctx));

    if (op == TOKEN_NOT) {
        if (!type_equals(operand, TYPE_BOOL)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNOP_TYPE_MISMATCH,
                PGY_CAUSE_UNARY_OPERATOR_OPERAND, PGY_FIX_ALIGN_OPERAND_TYPE,
                expr,
                "'!' operator requires Bool, got '%s'",
                type_name_or_unknown(operand));
        }
        return TYPE_BOOL;
    }

    if (op == TOKEN_MINUS) {
        if (!type_equals(operand, TYPE_INT)
            && !type_equals(operand, TYPE_LONG)
            && !type_equals(operand, TYPE_FLOAT)
            && !type_equals(operand, TYPE_DOUBLE)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNOP_TYPE_MISMATCH,
                PGY_CAUSE_UNARY_OPERATOR_OPERAND, PGY_FIX_ALIGN_OPERAND_TYPE,
                expr,
                "Unary '-' requires numeric type, got '%s'",
                type_name_or_unknown(operand));
        }
        return operand;
    }

    if (op == TOKEN_QUESTION) {
        if (!type_is_constructed_named(operand, "Result")
            && !type_is_constructed_named(operand, "Option")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNOP_TYPE_MISMATCH,
                PGY_CAUSE_UNARY_OPERATOR_OPERAND, PGY_FIX_ALIGN_OPERAND_TYPE,
                expr,
                "'?' operator requires Result<T>, Result<T, E>, or Option<T>, got '%s'",
                type_name_or_unknown(operand));
            return TYPE_UNKNOWN;
        }
        return expr_ops_normalize_type(type_get_constructed_arg(operand, 0));
    }

    return operand;
}

/* reflect projection field folding (expr_ops_projection_member) lives in
 * type_checker_reflect.c to keep this file under its size cap. */

/* Sequence (Array/Slice/List/Queue) constructors whose element nesting we
 * track for the nested-array support boundary. */
static bool
expr_ops_type_is_sequence_like(const Type *type)
{
    return type_is_constructed_named(type, "Array")
        || type_is_constructed_named(type, "Slice")
        || type_is_constructed_named(type, "List")
        || type_is_constructed_named(type, "Queue");
}

/* The scalar element types nested arrays are monomorphized for on both
 * backends (PgyArray_Array_<T>). Anything else fails closed. */
static bool
expr_ops_type_is_supported_nested_scalar(Type *type)
{
    return type_equals(type, TYPE_INT)
        || type_equals(type, TYPE_LONG)
        || type_equals(type, TYPE_FLOAT)
        || type_equals(type, TYPE_DOUBLE)
        || type_equals(type, TYPE_BOOL)
        || type_equals(type, TYPE_STRING);
}

/* Fail-closed guard for nested sequences. `elem_type` is the element type of a
 * sequence literal/annotation. Returns true (and emits a diagnostic) when the
 * nesting is beyond the supported one level (Array<Array<scalar>>): either the
 * inner element is itself a sequence (depth >= 3) or the supported one-level
 * case has an unsupported inner scalar. Unknown element types are allowed so
 * inference failures surface their own diagnostics rather than this one. */
static bool
expr_ops_reject_unsupported_nested_sequence(ASTNode *node, Type *elem_type,
                                            SemanticContext *ctx)
{
    Type *inner;

    if (elem_type == NULL || type_equals(elem_type, TYPE_UNKNOWN))
        return false;
    if (!expr_ops_type_is_sequence_like(elem_type))
        return false;

    inner = expr_ops_normalize_type(type_get_constructed_arg(elem_type, 0));
    if (inner != NULL && type_equals(inner, TYPE_UNKNOWN))
        return false;

    if (inner == NULL || expr_ops_type_is_sequence_like(inner)
        || !expr_ops_type_is_supported_nested_scalar(inner)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
            PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
            node,
            "nested array nesting deeper than two levels (or with this element "
            "type '%s') is not yet supported - use a flatter representation",
            type_name_or_unknown(elem_type));
        return true;
    }
    return false;
}

Type *
type_check_array_literal(ASTNode *expr, SemanticContext *ctx)
{
    /* A `[...]` literal is a sequence: its concrete constructor comes from the
     * binding annotation (Array by default, List/Queue when so declared). */
    Type *seq_ctor = TYPE_ARRAY;
    Type *expected_seq = ctx != NULL ? ctx->expected_collection_type : NULL;
    if (expected_seq != NULL) {
        if (type_is_constructed_named(expected_seq, "List"))
            seq_ctor = TYPE_LIST;
        else if (type_is_constructed_named(expected_seq, "Queue"))
            seq_ctor = TYPE_QUEUE;
    }

    if (ast_array_literal_count(expr) == 0) {
        /* An empty `[]` carries no element type; take the annotation directly
         * when it is a sequence, else a sequence of Unknown. */
        if (expected_seq != NULL
            && (type_is_constructed_named(expected_seq, "Array")
                || type_is_constructed_named(expected_seq, "List")
                || type_is_constructed_named(expected_seq, "Queue"))) {
            Type *expected_elem = expr_ops_normalize_type(
                type_get_constructed_arg(expected_seq, 0));
            if (expr_ops_reject_unsupported_nested_sequence(expr, expected_elem,
                    ctx))
                return TYPE_UNKNOWN;
            return expected_seq;
        }
        return wrap_constructed(seq_ctor, TYPE_UNKNOWN);
    }

    Type *elem_type = type_check_expression(ast_array_literal_element(expr, 0), ctx);
    reject_borrowed_array_literal_store(
        ast_array_literal_element(expr, 0), elem_type, ctx);
    if (elem_type == NULL)
        elem_type = TYPE_UNKNOWN;
    if (type_equals(elem_type, TYPE_VOID)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
            PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
            ast_array_literal_element(expr, 0),
            "Void expression cannot be stored as an array literal element; split the side effect before constructing the array");
        elem_type = TYPE_UNKNOWN;
    }

    for (size_t i = 1; i < ast_array_literal_count(expr); i++) {
        Type *next = type_check_expression(ast_array_literal_element(expr, i), ctx);
        reject_borrowed_array_literal_store(
            ast_array_literal_element(expr, i), next, ctx);
        if (next == NULL)
            next = TYPE_UNKNOWN;
        if (type_equals(next, TYPE_VOID)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
                PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
                ast_array_literal_element(expr, i),
                "Void expression cannot be stored as an array literal element; split the side effect before constructing the array");
            next = TYPE_UNKNOWN;
        }
        if (!type_is_assignable(next, elem_type) && !type_is_assignable(elem_type, next)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
                PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
                ast_array_literal_element(expr, i),
                "Array literal element type mismatch: expected '%s', got '%s'",
                type_name_or_unknown(elem_type),
                type_name_or_unknown(next));
            elem_type = TYPE_UNKNOWN;
        }
    }

    if (expr_ops_reject_unsupported_nested_sequence(expr, elem_type, ctx))
        return TYPE_UNKNOWN;

    return wrap_constructed(seq_ctor, elem_type);
}

Type *
type_check_set_literal(ASTNode *expr, SemanticContext *ctx)
{
    /* An empty `{}` carries no element type; defer to the binding annotation
     * by reporting Unknown, which is assignable to any Set<T> (mirrors the
     * empty-map rule). */
    if (ast_set_literal_count(expr) == 0)
        return TYPE_UNKNOWN;

    Type *elem_type = type_check_expression(ast_set_literal_element(expr, 0), ctx);
    if (elem_type == NULL)
        elem_type = TYPE_UNKNOWN;
    if (type_equals(elem_type, TYPE_VOID)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
            PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
            ast_set_literal_element(expr, 0),
            "Void expression cannot be stored as a set literal element; split the side effect before constructing the set");
        elem_type = TYPE_UNKNOWN;
    }

    for (size_t i = 1; i < ast_set_literal_count(expr); i++) {
        Type *next = type_check_expression(ast_set_literal_element(expr, i), ctx);
        if (next == NULL)
            next = TYPE_UNKNOWN;
        if (!type_is_assignable(next, elem_type) && !type_is_assignable(elem_type, next)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
                PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
                ast_set_literal_element(expr, i),
                "Set literal element type mismatch: expected '%s', got '%s'",
                type_name_or_unknown(elem_type),
                type_name_or_unknown(next));
            elem_type = TYPE_UNKNOWN;
        }
    }

    return wrap_constructed(TYPE_SET, elem_type);
}

static void
map_literal_unify_entry(SemanticContext *ctx, ASTNode *expr, size_t i,
                        Type **key_type, Type **value_type,
                        Type *k, Type *v)
{
    if (k == NULL)
        k = TYPE_UNKNOWN;
    if (v == NULL)
        v = TYPE_UNKNOWN;
    if (i == 0) {
        *key_type = k;
        *value_type = v;
        return;
    }
    if (!type_is_assignable(k, *key_type) && !type_is_assignable(*key_type, k)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
            PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES, ast_map_literal_key(expr, i),
            "Map literal key type mismatch: expected '%s', got '%s'",
            type_name_or_unknown(*key_type), type_name_or_unknown(k));
        *key_type = TYPE_UNKNOWN;
    }
    if (!type_is_assignable(v, *value_type)
        && !type_is_assignable(*value_type, v)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
            PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES, ast_map_literal_value(expr, i),
            "Map literal value type mismatch: expected '%s', got '%s'",
            type_name_or_unknown(*value_type), type_name_or_unknown(v));
        *value_type = TYPE_UNKNOWN;
    }
}

Type *
type_check_map_literal(ASTNode *expr, SemanticContext *ctx)
{
    size_t n = ast_map_literal_count(expr);
    Type *key_type = TYPE_UNKNOWN;
    Type *value_type = TYPE_UNKNOWN;
    Type *args[2];

    /* An empty `{}` carries no entry types; defer to the binding annotation
     * by reporting Unknown, which is assignable to any HashMap<K, V>. */
    if (n == 0)
        return TYPE_UNKNOWN;

    for (size_t i = 0; i < n; i++) {
        Type *k = expr_ops_normalize_type(
            type_check_expression(ast_map_literal_key(expr, i), ctx));
        Type *v = expr_ops_normalize_type(
            type_check_expression(ast_map_literal_value(expr, i), ctx));
        map_literal_unify_entry(ctx, expr, i, &key_type, &value_type, k, v);
    }
    args[0] = key_type;
    args[1] = value_type;
    return type_create_constructed(TYPE_HASHMAP, args, 2);
}

Type *
type_check_array_access(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *array_node = ast_array_access_array(expr);
    ASTNode *index_node = ast_array_access_index(expr);
    Type *object_type = expr_ops_normalize_type(
        type_check_expression(array_node, ctx));
    Type *index_type  = expr_ops_normalize_type(
        type_check_expression(index_node, ctx));

    if (!type_equals(index_type, TYPE_INT)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_ACCESS_INDEX_NON_INT, PGY_FIX_USE_INT_INDEX,
            index_node,
            "Array index must be Int, got '%s'",
            type_name_or_unknown(index_type));
        return TYPE_UNKNOWN;
    }

    if (type_is_constructed_named(object_type, "Array")
        || type_is_constructed_named(object_type, "Slice")) {
        return expr_ops_normalize_type(type_get_constructed_arg(object_type, 0));
    }

    semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_ARRAY_ACCESS_TARGET_NOT_INDEXABLE, PGY_FIX_USE_ARRAY_OR_SLICE,
        array_node,
        "Index access requires Array<T> or Slice<T>, got '%s'",
        type_name_or_unknown(object_type));
    return TYPE_UNKNOWN;
}

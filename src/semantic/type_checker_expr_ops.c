/*
 * Expression operator and indexed-access type checkers.
 *
 * Kept out of type_checker_expr.h so the expression dispatcher stays
 * readable while operator overload and array literal rules remain together.
 */

#include <stdio.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_ownership_diag_internal.h"
#include "diag_codes.h"

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

static Type *
operator_expr_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_type_ref_or_materialize(ctx, type_ref);
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
find_role_decl_in_program(ASTNode *program, const char *role_name)
{
    if (program == NULL || program->type != AST_PROGRAM || role_name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt != NULL && stmt->type == AST_ROLE_DECL
            && stmt->data.role_decl.name != NULL
            && strcmp(stmt->data.role_decl.name, role_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_role_operator_method(ASTNode *role, ASTNode *program, PgyTokenType op,
                          int depth)
{
    if (role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method != NULL && method->type == AST_FUNC_DECL
                && operator_method_name_matches(op, method->data.func_decl.name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *inc = role->data.role_decl.includes[i];
        ASTNode *included = find_role_decl_in_program(program,
            inc->data.include_stmt.role_name);
        ASTNode *method = find_role_operator_method(included, program, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

static Type *
type_check_role_operator_overload(ASTNode *expr, SemanticContext *ctx,
                                  Type *left, Type *right)
{
    if (ctx->program_root == NULL || left == NULL || left->name == NULL)
        return NULL;

    ASTNode *program = ctx->program_root;
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL
            || strcmp(stmt->data.role_decl.for_type->data.type.name, left->name) != 0) {
            continue;
        }

        ASTNode *method = find_role_operator_method(
            stmt, ctx->program_root, expr->data.binary.op.type, 0);
        if (method == NULL)
            continue;

        FuncParam *rhs_param = NULL;
        size_t rhs_param_count = 0;
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && !(p->type == NULL && strcmp(p->name, "self") == 0)) {
                rhs_param = p;
                rhs_param_count++;
            }
        }
        if (rhs_param_count != 1)
            continue;

        Type *rhs_type = TYPE_INT;
        if (rhs_param != NULL && rhs_param->type != NULL)
            rhs_type = operator_expr_resolve_type_ref(rhs_param->type, ctx);
        if (!type_is_assignable(right, rhs_type))
            continue;

        if (method->data.func_decl.return_type != NULL)
            return operator_expr_resolve_type_ref(
                method->data.func_decl.return_type, ctx);
        return TYPE_VOID;
    }

    return NULL;
}

static Type *
type_check_operator_overload(ASTNode *expr, SemanticContext *ctx,
                             Type *left, Type *right)
{
    const char *suffix = operator_overload_suffix(expr->data.binary.op.type);
    if (suffix == NULL || left == NULL || right == NULL || left->name == NULL)
        return NULL;

    char fn_name[256];
    snprintf(fn_name, sizeof(fn_name), "operator_%s_%s", suffix, left->name);

    Symbol *sym = scope_lookup(ctx->scope, fn_name);
    if (sym == NULL || sym->kind != SYMBOL_FUNCTION
        || sym->type == NULL || sym->type->kind != TYPE_KIND_FUNCTION)
        return NULL;

    if (sym->type->data.function.param_count != 2)
        return NULL;

    Type *lhs = sym->type->data.function.param_types[0];
    Type *rhs = sym->type->data.function.param_types[1];
    if (!type_is_assignable(left, lhs) || !type_is_assignable(right, rhs))
        return NULL;

    sym->is_used = true;
    return sym->type->data.function.return_type;
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
        type_check_expression(expr->data.binary.left,  ctx));
    Type *right = expr_ops_normalize_type(
        type_check_expression(expr->data.binary.right, ctx));

    if (type_is_slot_handle(left) && left->data.slot.inner_type != NULL)
        left = left->data.slot.inner_type;
    if (type_is_slot_handle(right) && right->data.slot.inner_type != NULL)
        right = right->data.slot.inner_type;

    Type *overloaded = type_check_operator_overload(expr, ctx, left, right);
    if (overloaded == NULL)
        overloaded = type_check_role_operator_overload(expr, ctx, left, right);
    if (overloaded != NULL)
        return overloaded;

    if (left == TYPE_UNKNOWN || right == TYPE_UNKNOWN) {
        PgyTokenType op = expr->data.binary.op.type;
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS     || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER  || op == TOKEN_GREATER_EQUAL)
            return TYPE_BOOL;
        return (left != TYPE_UNKNOWN) ? left : right;
    }

    PgyTokenType op = expr->data.binary.op.type;
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
    Type *operand = expr_ops_normalize_type(
        type_check_expression(expr->data.unary.operand, ctx));

    PgyTokenType op = expr->data.unary.op.type;
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
            && !type_equals(operand, TYPE_FLOAT)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNOP_TYPE_MISMATCH,
                PGY_CAUSE_UNARY_OPERATOR_OPERAND, PGY_FIX_ALIGN_OPERAND_TYPE,
                expr,
                "Unary '-' requires numeric type, got '%s'",
                type_name_or_unknown(operand));
        }
        return operand;
    }

    if (op == TOKEN_QUESTION) {
        if (!type_is_constructed_named(operand, "Result")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNOP_TYPE_MISMATCH,
                PGY_CAUSE_UNARY_OPERATOR_OPERAND, PGY_FIX_ALIGN_OPERAND_TYPE,
                expr,
                "'?' operator requires Result<T> or Result<T, E>, got '%s'",
                type_name_or_unknown(operand));
            return TYPE_UNKNOWN;
        }
        return expr_ops_normalize_type(type_get_constructed_arg(operand, 0));
    }

    return operand;
}

Type *
type_check_array_literal(ASTNode *expr, SemanticContext *ctx)
{
    if (expr->data.array_literal.count == 0)
        return wrap_constructed(TYPE_ARRAY, TYPE_UNKNOWN);

    Type *elem_type = type_check_expression(expr->data.array_literal.elements[0], ctx);
    reject_borrowed_array_literal_store(
        expr->data.array_literal.elements[0], elem_type, ctx);
    if (elem_type == NULL)
        elem_type = TYPE_UNKNOWN;

    for (size_t i = 1; i < expr->data.array_literal.count; i++) {
        Type *next = type_check_expression(expr->data.array_literal.elements[i], ctx);
        reject_borrowed_array_literal_store(
            expr->data.array_literal.elements[i], next, ctx);
        if (!type_is_assignable(next, elem_type) && !type_is_assignable(elem_type, next)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
                PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
                expr->data.array_literal.elements[i],
                "Array literal element type mismatch: expected '%s', got '%s'",
                elem_type->name, next->name);
            elem_type = TYPE_UNKNOWN;
        }
    }

    return wrap_constructed(TYPE_ARRAY, elem_type);
}

Type *
type_check_array_access(ASTNode *expr, SemanticContext *ctx)
{
    Type *object_type = expr_ops_normalize_type(
        type_check_expression(expr->data.array_access.array, ctx));
    Type *index_type  = expr_ops_normalize_type(
        type_check_expression(expr->data.array_access.index, ctx));

    if (!type_equals(index_type, TYPE_INT)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_ACCESS_INDEX_NON_INT, PGY_FIX_USE_INT_INDEX,
            expr->data.array_access.index,
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
        expr->data.array_access.array,
        "Index access requires Array<T> or Slice<T>, got '%s'",
        type_name_or_unknown(object_type));
    return TYPE_UNKNOWN;
}

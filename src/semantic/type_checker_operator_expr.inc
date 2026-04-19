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
            rhs_type = resolve_type_node(rhs_param->type, ctx);
        if (!type_is_assignable(right, rhs_type))
            continue;

        if (method->data.func_decl.return_type != NULL)
            return resolve_type_node(method->data.func_decl.return_type, ctx);
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

static void
reject_borrowed_array_literal_store(ASTNode *value_expr,
                                    const Type *stored_value_type,
                                    SemanticContext *ctx)
{
    const char *borrowed_name;
    char *source_path;
    const char *consumer_name;
    const char *value_label;
    const char *provenance_label;
    const char *snapshot_label;
    const char *transfer_label;

    if (value_expr == NULL || stored_value_type == NULL || ctx == NULL)
        return;

    borrowed_name = semantic_borrowed_boundary_root_name(value_expr, ctx);
    if (borrowed_name == NULL)
        return;

    source_path = semantic_assignment_target_path(value_expr);
    consumer_name =
        ctx->current_function_decl != NULL
        && ctx->current_function_decl->data.func_decl.name != NULL
            ? ctx->current_function_decl->data.func_decl.name
            : "<anonymous>";
    value_label = "boundary value";
    provenance_label = "boundary provenance";
    snapshot_label = "copied/value/projection result";
    transfer_label = "transfer";

    if (type_is_movable_resource_handle(stored_value_type)) {
        value_label = "movable resource";
        provenance_label = "movable-resource provenance";
        snapshot_label = "copied/value/projection result";
    } else if (type_is_subject_type(stored_value_type, ctx)) {
        value_label = "subject";
        provenance_label = "subject provenance";
        snapshot_label = "projection/object/tobject/value result";
    } else if (!type_requires_boundary_borrow_tracking(stored_value_type, ctx)) {
        free(source_path);
        return;
    }

    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", value_expr,
        "Borrowed ref %s '%s' cannot escape through array literal store%s%s%s.\n"
        "Reason:\n"
        "- consumer path is function '%s'\n"
        "- '%s' entered this function as a borrowed 'ref' %s\n"
        "- '%s' is derived from that borrowed %s\n"
        "- array literal storage inserts values into array state that may outlive the current call boundary\n"
        "- storing the borrow would create a longer-lived ownership alias the compiler cannot keep boundary-safe\n"
        "Fix:\n"
        "- store a %s instead\n"
        "- or change the current parameter to 'own' if %s into array storage is intended",
        value_label,
        borrowed_name,
        source_path != NULL ? " from '" : "",
        source_path != NULL ? source_path : "",
        source_path != NULL ? "'" : "",
        consumer_name,
        borrowed_name,
        value_label,
        source_path != NULL ? source_path : borrowed_name,
        provenance_label,
        snapshot_label,
        transfer_label);
    free(source_path);
}

static Type *
type_check_array_literal(ASTNode *expr, SemanticContext *ctx)
{
    if (expr->data.array_literal.count == 0)
        return wrap_constructed(TYPE_ARRAY, TYPE_INT);

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
            semantic_error(ctx, expr->data.array_literal.elements[i],
                "Array literal element type mismatch: expected '%s', got '%s'",
                elem_type->name, next->name);
            elem_type = TYPE_UNKNOWN;
        }
    }

    return wrap_constructed(TYPE_ARRAY, elem_type);
}

/* -----------------------------------------------------------------
 * Expression type checker
 * ----------------------------------------------------------------- */

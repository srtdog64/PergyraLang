/* C backend MIR CFG control helpers.
 *
 * These helpers keep explicit CFG containers out of the generic AST statement
 * and expression emitters. MIR owns the control-flow edges; C lowering only
 * renders the loop/match branch conditions and edge-local maintenance.
 */

static bool
transpiler_mir_stmt_is_cfg_container(ASTNode *node)
{
    if (node == NULL)
        return false;
    return node->type == AST_WITH_STMT
        || node->type == AST_IF_STMT
        || node->type == AST_WHILE_LOOP
        || node->type == AST_FOR_LOOP
        || node->type == AST_SELECT_STMT
        || node->type == AST_MATCH_STMT;
}

static bool
transpiler_mir_emit_for_loop_init_inst(CodeBuf *buf,
                                       const MIRInstruction *inst,
                                       TranspilerCtx *ctx,
                                       const TranspilerSSANameMap *ssa_map)
{
    char *start;
    const char *variable;

    if (buf == NULL || inst == NULL || ctx == NULL)
        return true;
    if (inst->kind != MIR_INST_LOOP_INIT)
        return true;
    if (inst->ast == NULL || inst->ast->type != AST_FOR_LOOP)
        return false;
    variable = inst->arg0;
    if (variable == NULL)
        return true;
    if (inst->ast->data.for_loop.iterable != NULL) {
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "size_t _pgy_idx_%s = 0;\n", variable);
        return true;
    }

    start = emit_expression_with_ssa_map(inst->expr0, ctx, ssa_map);
    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "int32_t %s = %s;\n",
                  variable,
                  start != NULL ? start : "0");
    register_typed_var(ctx, variable, "Int");
    free(start);
    return true;
}

static const char *
transpiler_mir_for_in_length_field(const char *collection_type)
{
    if (collection_type != NULL
        && (strncmp(collection_type, "Array<", 6) == 0
            || strncmp(collection_type, "Slice<", 6) == 0)) {
        return "length";
    }
    return "count";
}

static const char *
transpiler_mir_for_in_element_type(TranspilerCtx *ctx,
                                   ASTNode *iterable,
                                   const char **collection_type_out)
{
    const char *collection_type;

    if (collection_type_out != NULL)
        *collection_type_out = NULL;
    if (ctx == NULL || iterable == NULL)
        return NULL;

    collection_type = infer_expression_type_name(ctx, iterable);
    if (collection_type_out != NULL)
        *collection_type_out = collection_type;
    if (collection_type == NULL)
        return NULL;
    if (strncmp(collection_type, "Array<", 6) != 0
        && strncmp(collection_type, "Slice<", 6) != 0
        && strncmp(collection_type, "List<", 5) != 0) {
        return NULL;
    }
    return pergyra_type_to_c(slot_inner_type_name(collection_type));
}

static char *
transpiler_mir_render_for_loop_condition_inst(const MIRInstruction *inst,
                                              TranspilerCtx *ctx,
                                              const TranspilerSSANameMap *ssa_map)
{
    char *end;
    char *cond;
    const char *variable;

    if (inst == NULL || ctx == NULL)
        return NULL;
    if (inst->ast == NULL || inst->ast->type != AST_FOR_LOOP)
        return NULL;
    variable = inst->arg0;
    if (variable == NULL)
        return NULL;
    if (inst->ast->data.for_loop.iterable != NULL) {
        const char *collection_type = NULL;
        const char *length_field;
        char *collection;
        (void)transpiler_mir_for_in_element_type(ctx, inst->expr0, &collection_type);
        length_field = transpiler_mir_for_in_length_field(collection_type);
        collection = emit_expression_with_ssa_map(inst->expr0, ctx, ssa_map);
        cond = strdup_fmt("_pgy_idx_%s < %s.%s",
            variable,
            collection != NULL ? collection : "0",
            length_field);
        free(collection);
        return cond;
    }

    end = emit_expression_with_ssa_map(inst->expr1, ctx, ssa_map);
    cond = strdup_fmt("%s < %s", variable, end != NULL ? end : "0");
    free(end);
    return cond;
}

static const MIRInstruction *
transpiler_mir_find_incoming_for_in_branch(const MIRRoutine *routine,
                                           const MIRBasicBlock *block)
{
    size_t target_id = SIZE_MAX;

    if (routine == NULL || block == NULL)
        return NULL;
    for (size_t i = 0; i < routine->block_count; i++) {
        if (&routine->blocks[i] == block) {
            target_id = i;
            break;
        }
    }
    if (target_id == SIZE_MAX)
        return NULL;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *pred = &routine->blocks[i];
        if (!pred->has_succ_true || pred->succ_true != target_id)
            continue;
        for (size_t j = 0; j < pred->instruction_count; j++) {
            const MIRInstruction *inst = &pred->instructions[j];
            if (inst->kind == MIR_INST_BRANCH
                && inst->ast != NULL
                && inst->ast->type == AST_FOR_LOOP
                && inst->ast->data.for_loop.iterable != NULL) {
                return inst;
            }
        }
    }
    return NULL;
}

static bool
transpiler_mir_emit_for_in_body_binding(CodeBuf *buf,
                                        const MIRRoutine *routine,
                                        const MIRBasicBlock *block,
                                        TranspilerCtx *ctx,
                                        const TranspilerSSANameMap *ssa_map)
{
    const MIRInstruction *branch_inst;
    const char *variable;
    const char *collection_type = NULL;
    const char *element_type;
    char *collection;

    if (buf == NULL || routine == NULL || block == NULL || ctx == NULL)
        return true;

    branch_inst = transpiler_mir_find_incoming_for_in_branch(routine, block);
    if (branch_inst == NULL)
        return true;
    variable = branch_inst->arg0;
    if (variable == NULL)
        return true;

    element_type = transpiler_mir_for_in_element_type(
        ctx, branch_inst->expr0, &collection_type);
    if (element_type == NULL)
        return false;

    collection = emit_expression_with_ssa_map(branch_inst->expr0, ctx, ssa_map);
    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "%s %s = %s.data[_pgy_idx_%s];\n",
        element_type,
        variable,
        collection != NULL ? collection : "0",
        variable);
    register_typed_var(ctx, variable, slot_inner_type_name(collection_type));
    free(collection);
    return true;
}

static const MIRInstruction *
transpiler_mir_find_loop_branch_inst(const MIRBasicBlock *block)
{
    if (block == NULL)
        return NULL;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_BRANCH
            && inst->ast != NULL
            && inst->ast->type == AST_FOR_LOOP) {
            return inst;
        }
    }
    return NULL;
}

static bool
transpiler_mir_emit_loop_backedge_increment(CodeBuf *buf,
                                            TranspilerCtx *ctx,
                                            const MIRRoutine *routine,
                                            const MIRBasicBlock *block)
{
    const MIRBasicBlock *target;
    const MIRInstruction *branch_inst;
    const char *variable;

    if (buf == NULL || ctx == NULL || routine == NULL || block == NULL)
        return true;
    if (!block->has_succ_true || block->succ_true >= routine->block_count)
        return true;
    if (block->id <= block->succ_true)
        return true;

    target = &routine->blocks[block->succ_true];
    if (target == block)
        return true;

    branch_inst = transpiler_mir_find_loop_branch_inst(target);
    if (branch_inst == NULL)
        return true;
    variable = branch_inst->arg0;
    if (variable == NULL)
        return true;

    write_indent_to(buf, ctx->indent);
    if (branch_inst->ast != NULL
        && branch_inst->ast->type == AST_FOR_LOOP
        && branch_inst->ast->data.for_loop.iterable != NULL) {
        codebuf_write(buf, "_pgy_idx_%s = _pgy_idx_%s + 1;\n",
            variable,
            variable);
    } else {
        codebuf_write(buf, "%s = %s + 1;\n", variable, variable);
    }
    return true;
}

static ASTNode *
transpiler_mir_find_match_subject_for_case(ASTNode *node, ASTNode *case_node)
{
    if (node == NULL || case_node == NULL)
        return NULL;

    if (node->type == AST_MATCH_STMT) {
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
            if (node->data.match_stmt.cases[i] == case_node)
                return node->data.match_stmt.subject;
        }
        if (node->data.match_stmt.default_body != NULL) {
            ASTNode *found = transpiler_mir_find_match_subject_for_case(
                node->data.match_stmt.default_body, case_node);
            if (found != NULL)
                return found;
        }
    }

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            ASTNode *found = transpiler_mir_find_match_subject_for_case(
                node->data.block.statements[i], case_node);
            if (found != NULL)
                return found;
        }
        break;
    case AST_IF_STMT: {
        ASTNode *found = transpiler_mir_find_match_subject_for_case(
            node->data.if_stmt.then_branch, case_node);
        if (found != NULL)
            return found;
        return transpiler_mir_find_match_subject_for_case(
            node->data.if_stmt.else_branch, case_node);
    }
    case AST_FOR_LOOP:
        return transpiler_mir_find_match_subject_for_case(
            node->data.for_loop.body, case_node);
    case AST_WHILE_LOOP:
        return transpiler_mir_find_match_subject_for_case(
            node->data.while_loop.body, case_node);
    case AST_WITH_STMT:
        return transpiler_mir_find_match_subject_for_case(
            node->data.with_stmt.body, case_node);
    default:
        break;
    }
    return NULL;
}

static bool
transpiler_mir_is_option_destructor(ASTNode *pat,
                                    const char **kind,
                                    const char **binding)
{
    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = pat->data.identifier.name;
        if (name != NULL && strcmp(name, "None") == 0) {
            if (kind != NULL)
                *kind = "None";
            return true;
        }
        return false;
    }

    if (pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;

    if (strcmp(name, "None") == 0 && pat->data.call.arg_count == 0) {
        if (kind != NULL)
            *kind = "None";
        return true;
    }
    if (strcmp(name, "Some") == 0 && pat->data.call.arg_count == 1) {
        if (kind != NULL)
            *kind = "Some";
        if (binding != NULL
            && pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }
    return false;
}

static bool
transpiler_mir_is_result_destructor(ASTNode *pat,
                                    const char **kind,
                                    const char **binding)
{
    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    if (pat == NULL || pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;
    if ((strcmp(name, "Ok") == 0 || strcmp(name, "Err") == 0)
        && pat->data.call.arg_count == 1) {
        if (kind != NULL)
            *kind = name;
        if (binding != NULL
            && pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }
    return false;
}

static void
transpiler_mir_emit_match_payload_binding(CodeBuf *buf,
                                          TranspilerCtx *ctx,
                                          const char *subject,
                                          const char *kind,
                                          const char *binding)
{
    const char *field;

    if (buf == NULL || ctx == NULL || subject == NULL
        || kind == NULL || binding == NULL) {
        return;
    }

    if (strcmp(kind, "Some") == 0)
        field = "value";
    else if (strcmp(kind, "Ok") == 0)
        field = "ok";
    else if (strcmp(kind, "Err") == 0)
        field = "err";
    else
        return;

    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "__typeof__((%s).%s) %s = (%s).%s;\n",
                  subject, field, binding, subject, field);
}

static char *
transpiler_mir_render_match_case_condition(ASTNode *func_decl,
                                           ASTNode *case_node,
                                           TranspilerCtx *ctx,
                                           const TranspilerSSANameMap *ssa_map)
{
    ASTNode *subject_node;
    char *subject;
    char *cond = NULL;
    char *guard = NULL;

    if (func_decl == NULL || case_node == NULL || ctx == NULL
        || func_decl->type != AST_FUNC_DECL || case_node->type != AST_MATCH_CASE) {
        return NULL;
    }

    subject_node = transpiler_mir_find_match_subject_for_case(
        func_decl->data.func_decl.body, case_node);
    if (subject_node == NULL)
        return NULL;

    subject = emit_expression_with_ssa_map(subject_node, ctx, ssa_map);
    if (subject == NULL)
        return NULL;

    if (case_node->data.match_case.patterns != NULL
        && case_node->data.match_case.pattern_count > 1) {
        for (size_t i = 0; i < case_node->data.match_case.pattern_count; i++) {
            char *pat = emit_expression_with_ssa_map(
                case_node->data.match_case.patterns[i], ctx, ssa_map);
            char *next = NULL;
            if (pat == NULL)
                continue;
            next = cond == NULL
                ? strdup_fmt("(%s == %s)", subject, pat)
                : strdup_fmt("(%s || (%s == %s))", cond, subject, pat);
            free(cond);
            free(pat);
            cond = next;
        }
    } else if (case_node->data.match_case.pattern != NULL) {
        const char *kind = NULL;
        const char *binding = NULL;
        if (transpiler_mir_is_option_destructor(
                case_node->data.match_case.pattern, &kind, &binding)) {
            const char *tag = strcmp(kind, "Some") == 0
                ? "PgyOptionSome" : "PgyOptionNone";
            transpiler_mir_emit_match_payload_binding(ctx->out, ctx,
                                                      subject, kind, binding);
            cond = strdup_fmt("(%s).tag == %s", subject, tag);
        } else if (transpiler_mir_is_result_destructor(
                       case_node->data.match_case.pattern, &kind, &binding)) {
            const char *tag = strcmp(kind, "Ok") == 0
                ? "PgyResultOk" : "PgyResultErr";
            transpiler_mir_emit_match_payload_binding(ctx->out, ctx,
                                                      subject, kind, binding);
            cond = strdup_fmt("(%s).tag == %s", subject, tag);
        } else {
            char *pat = emit_expression_with_ssa_map(
                case_node->data.match_case.pattern, ctx, ssa_map);
            if (pat != NULL)
                cond = strdup_fmt("%s == %s", subject, pat);
            free(pat);
        }
    }

    if (case_node->data.match_case.guard != NULL) {
        guard = emit_expression_with_ssa_map(case_node->data.match_case.guard,
                                             ctx, ssa_map);
        if (guard != NULL && cond != NULL) {
            char *with_guard = strdup_fmt("(%s) && (%s)", cond, guard);
            free(cond);
            cond = with_guard;
        }
    }

    free(guard);
    free(subject);
    return cond;
}

static char *
transpiler_mir_render_branch_condition(ASTNode *func_decl,
                                       const MIRInstruction *inst,
                                       TranspilerCtx *ctx,
                                       const TranspilerSSANameMap *ssa_map)
{
    ASTNode *condition = inst != NULL ? inst->ast : NULL;
    if (condition == NULL)
        return pergyra_strdup("true");
    if (condition->type == AST_FOR_LOOP)
        return transpiler_mir_render_for_loop_condition_inst(inst, ctx, ssa_map);
    if (condition->type == AST_MATCH_CASE)
        return transpiler_mir_render_match_case_condition(func_decl, condition,
                                                         ctx, ssa_map);
    return emit_expression_with_ssa_map(condition, ctx, ssa_map);
}

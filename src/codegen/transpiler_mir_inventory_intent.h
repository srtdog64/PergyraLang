static void
emit_func_forward_decl_named(ASTNode *node, const char *emitted_name,
                             CodeBuf *buf, TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    g_type_render_ctx = ctx;
    ensure_type_specializations_from_ast(ctx, node->data.func_decl.return_type);
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = NULL;
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p->type != NULL)
            ensure_type_specializations_from_ast(ctx, p->type);
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (pt == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine parameter type for forward declaration '%s' at argument %llu",
                name != NULL ? name : "<function>",
                (unsigned long long) i);
            if (params_sig != NULL)
                codebuf_destroy(params_sig);
            free(header_decl);
            g_type_render_ctx = saved_render_ctx;
            return;
        }
        if (i > 0)
            codebuf_write(params_sig, ", ");
        if (p->type != NULL)
            type_name = render_type_name(p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            const char *inner = slot_inner_type_name(type_name);
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE) {
            decl = pergyra_ast_typed_declarator(p->type, p->name);
            codebuf_write(params_sig, "%s", decl);
        } else if (p->name != NULL && strcmp(p->name, "self") != 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(type_name);
    }
    header_decl = pergyra_func_signature_declarator(node->data.func_decl.return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(buf, "%s;\n", header_decl);
    free(header_decl);
    codebuf_destroy(params_sig);
    g_type_render_ctx = saved_render_ctx;
}

static void
emit_func_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    emit_func_forward_decl_named(node, node->data.func_decl.name, buf, ctx);
}

static const MIRRoutine *
transpiler_find_mir_function(const TranspilerCtx *ctx, const ASTNode *func_decl)
{
    TranspilerMIRRoutineInventory inventory;

    if (ctx == NULL || ctx->mir == NULL || func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.name == NULL) {
        return NULL;
    }

    const char *target = func_decl->data.func_decl.name;
    transpiler_active_routine_inventory(ctx, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine =
            transpiler_routine_inventory_get(&inventory, i);
        if (routine == NULL)
            continue;
        if (routine->kind != MIR_SCOPE_FUNCTION)
            continue;
        if (routine->name == NULL)
            continue;
        /* Exact match */
        if (strcmp(routine->name, target) == 0)
            return routine;
        /* Specialized name: e.g. "Identity_Int" matches "Identity" */
        size_t name_len = strlen(target);
        if (strncmp(routine->name, target, name_len) == 0
            && (routine->name[name_len] == '_' || routine->name[name_len] == '\0'))
            return routine;
    }

    return NULL;
}

static const MIRRoutine *
transpiler_find_mir_intent(const TranspilerCtx *ctx, const ASTNode *intent_decl)
{
    TranspilerMIRRoutineInventory inventory;

    if (ctx == NULL || ctx->mir == NULL || intent_decl == NULL
        || intent_decl->type != AST_INTENT_DECL
        || intent_decl->data.intent_decl.name == NULL) {
        return NULL;
    }

    transpiler_active_routine_inventory(ctx, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine =
            transpiler_routine_inventory_get(&inventory, i);
        if (routine == NULL)
            continue;
        if (routine->kind != MIR_SCOPE_INTENT
            || routine->name == NULL
            || strcmp(routine->name, intent_decl->data.intent_decl.name) != 0) {
            continue;
        }
        /* Match by name only - pointer comparison may fail across different AST instances */
        return routine;
    }

    return NULL;
}

static const char *
transpiler_find_mir_intent_meta_arg(const MIRRoutine *routine,
                                    const char *step_name,
                                    const char *inst_name)
{
    if (routine == NULL || inst_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            if (!mir_instruction_is_intent_stmt(inst, inst_name))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            return payload;
        }
    }
    return NULL;
}

#include "transpiler_mir_inventory_intent_collect.h"

static size_t
transpiler_collect_mir_intent_steps(const MIRRoutine *routine, ASTNode ***steps_out)
{
    ASTNode **steps = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (steps_out != NULL)
        *steps_out = NULL;
    if (routine == NULL || steps_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode **grown;

            if (inst->ast == NULL
                || !inst->has_source_location
                || inst->source_ast_type != AST_INTENT_STEP) {
                continue;
            }
            if (!mir_instruction_is_intent_stmt(inst, "IntentStep"))
                continue;
            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                grown = realloc(steps, new_capacity * sizeof(ASTNode *));
                if (grown == NULL) {
                    free(steps);
                    return 0;
                }
                steps = grown;
                capacity = new_capacity;
            }
            steps[count++] = inst->ast;
        }
    }

    *steps_out = steps;
    return count;
}

static size_t
transpiler_collect_mir_intent_step_names(const MIRRoutine *routine, const char ***names_out)
{
    const char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (names_out != NULL)
        *names_out = NULL;
    if (routine == NULL || names_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (mir_instruction_intent_step_name(inst) == NULL)
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                grown = realloc((void *)names, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)names);
                    return 0;
                }
                names = grown;
                capacity = new_capacity;
            }
            names[count++] = mir_instruction_intent_step_name(inst);
        }
    }

    *names_out = names;
    return count;
}

static ASTNode *
transpiler_find_mir_intent_check_expr(const MIRRoutine *routine,
                                      const char *step_name,
                                      const char *phase_name)
{
    if (routine == NULL || phase_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->ast == NULL)
                continue;
            if (!mir_instruction_is_intent_stmt(inst, "IntentCheck"))
                continue;
            if (!mir_instruction_intent_phase_matches(inst, phase_name))
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            return inst->ast;
        }
    }
    return NULL;
}

static size_t
transpiler_collect_mir_intent_eval_exprs(const MIRRoutine *routine,
                                         const char *step_name,
                                         const char *phase_name,
                                         ASTNode ***exprs_out)
{
    ASTNode **exprs = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (exprs_out != NULL)
        *exprs_out = NULL;
    if (routine == NULL || phase_name == NULL || exprs_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode **grown;

            if (inst->ast == NULL)
                continue;
            if (!mir_instruction_is_intent_stmt(inst, "IntentEval"))
                continue;
            if (!mir_instruction_intent_phase_matches(inst, phase_name))
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc(exprs, new_capacity * sizeof(ASTNode *));
                if (grown == NULL) {
                    free(exprs);
                    return 0;
                }
                exprs = grown;
                capacity = new_capacity;
            }
            exprs[count++] = inst->ast;
        }
    }

    *exprs_out = exprs;
    return count;
}

static ASTNode *
transpiler_find_mir_intent_eval_expr(const MIRRoutine *routine,
                                     const char *step_name,
                                     const char *phase_name)
{
    ASTNode **exprs = NULL;
    ASTNode *result = NULL;
    size_t count = transpiler_collect_mir_intent_eval_exprs(
        routine, step_name, phase_name, &exprs);
    if (count > 0)
        result = exprs[0];
    free(exprs);
    return result;
}

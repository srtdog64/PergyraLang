#include "transpiler_spawn_channel_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "../compiler/execution_lane.h"
#include "../compiler/verified_projection_plan.h"

#include "codegen_channel_runtime_abi.h"
#include "transpiler_context.h"
#include "transpiler_channel_type_query.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_future_type_query.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_generic_specialization_emit.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_signature.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_symbols.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

static char *
transpiler_spawn_channel_emit_expr(TranspilerCtx *ctx,
                                   ASTNode *expr,
                                   const char *operation,
                                   const char *role)
{
    char *lowered = emit_expression(expr, ctx);
    if (lowered != NULL)
        return lowered;

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: %s could not lower %s expression",
        operation != NULL ? operation : "spawn/channel operation",
        role != NULL ? role : "operand");
    return NULL;
}

static bool
transpiler_spawn_channel_runtime_symbol(TranspilerCtx *ctx,
                                        const char *operation,
                                        const char *runtime_op,
                                        const char *inner,
                                        char *out,
                                        size_t out_size)
{
    if (pgy_lane_channel_runtime_name(out, out_size, runtime_op, inner))
        return true;

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "C backend: %s runtime function name is too long",
        operation != NULL ? operation : "channel operation");
    return false;
}

static bool
transpiler_spawn_reject_worker_storage(TranspilerCtx *ctx,
                                       const char *function_name,
                                       size_t arg_index,
                                       const char *type_name)
{
    const char *kind =
        codegen_worker_boundary_storage_kind_from_type_name(type_name, true);

    if (kind == NULL)
        return false;

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_BORROW_ESCAPE,
        PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
        "C backend: spawn argument %llu for '%s' cannot transport %s<T> storage across a worker boundary; semantic/CFG should reject this path before lowering",
        (unsigned long long)(arg_index + 1),
        function_name != NULL ? function_name : "<function>",
        kind);
    return true;
}

static const char *
transpiler_spawn_lane_symbol(PgyExecutionLane lane)
{
    switch (lane)
    {
        case PGY_LANE_REJECT:            return "PGY_LANE_REJECT";
        case PGY_LANE_INLINE:            return "PGY_LANE_INLINE";
        case PGY_LANE_PINNED_ZONE:       return "PGY_LANE_PINNED_ZONE";
        case PGY_LANE_BLOCKING_POOL:     return "PGY_LANE_BLOCKING_POOL";
        case PGY_LANE_LOCAL_ASYNC:       return "PGY_LANE_LOCAL_ASYNC";
        case PGY_LANE_WORKER_POOL:       return "PGY_LANE_WORKER_POOL";
        case PGY_LANE_MOVABLE_SCHEDULER: return "PGY_LANE_MOVABLE_SCHEDULER";
    }
    return "PGY_LANE_REJECT";
}

char *
emit_spawn_expr(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *target = ast_spawn_function(node);
    ASTNode *call = NULL;
    ASTNode *callee = NULL;
    const char *function_name = NULL;
    const char *emitted_function_name = NULL;
    ASTNode *decl = NULL;
    size_t arg_count = 0;
    int wrapper_id = ++ctx->tmp_counter;
    const char *wrapper_name = transpiler_scratch_fmt(
        ctx, "pgy_spawn_wrapper_%d", wrapper_id);
    const char *args_type_name = NULL;
    const char *return_type_name = infer_spawn_return_type_name_scratch(
        ctx, node);
    char return_c_type_buf[256];
    const char *return_c_type = NULL;
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;
    const MIRRoutine *callee_routine = NULL;
    bool callee_has_mir_signature = false;
    bool callee_is_generic_func = false;
    bool callee_is_extern_func = false;
    bool allow_ast_compat = false;

    if (transpiler_require_type_name_c_type_copy(ctx, return_type_name,
            "spawn return metadata", return_c_type_buf,
            sizeof(return_c_type_buf))) {
        return_c_type = return_c_type_buf;
    }

    if (target == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C spawn expression requires a target expression");
        return NULL;
    }

    if (target->type == AST_CALL) {
        call = target;
        callee = ast_call_callee(target);
        arg_count = ast_call_arg_count(target);
    } else {
        callee = target;
    }

    if (callee != NULL && callee->type == AST_IDENTIFIER)
        function_name = ast_identifier_name(callee);
    if (function_name == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported spawn target at line %d", target->line);
        return NULL;
    }
    if (return_type_name == NULL || return_type_name[0] == '\0'
        || strcmp(return_type_name, "Unknown") == 0
        || return_c_type == NULL || return_c_type[0] == '\0'
        || strcmp(return_c_type, "Unknown") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C spawn expression requires concrete Future<T> return metadata for '%s'",
            function_name);
        return NULL;
    }

    decl = find_function_decl(ctx, function_name);
    emitted_function_name = function_name;
    callee_is_extern_func = transpiler_decl_is_extern_function(ctx, decl);
    if (decl != NULL && decl->type == AST_FUNC_DECL)
        callee_routine = transpiler_find_mir_function(ctx, decl);
    callee_is_generic_func =
        transpiler_mir_or_ast_function_is_generic(callee_routine, decl);
    if (call != NULL && callee_is_generic_func
        && transpiler_infer_generic_call_bindings(ctx, decl, call, bindings,
            &binding_count)) {
        const char *specialized = ensure_generic_specialization(ctx, decl, call);
        if (specialized != NULL)
            emitted_function_name = specialized;
    }
    if (!callee_is_generic_func && !callee_is_extern_func
        && decl != NULL && decl->type == AST_FUNC_DECL
        && transpiler_active_has_mir(ctx)) {
        if (callee_routine == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing spawn routine for '%s'",
                function_name != NULL ? function_name : "<function>");
            return NULL;
        }
        if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
                callee_routine, decl,
                TRANSPILER_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES,
                "MIR-only C path missing spawn signature metadata for '%s'",
                NULL,
                "MIR-only C path missing spawn parameter type-name metadata for '%s'")) {
            return NULL;
        }
        callee_has_mir_signature = true;
    }
    allow_ast_compat = decl != NULL
        && decl->type == AST_FUNC_DECL
        && (callee_is_generic_func
            || callee_is_extern_func);
    if (arg_count > 0)
        args_type_name = transpiler_scratch_fmt(ctx,
                                                "PgySpawnArgs_%d",
                                                wrapper_id);

    if (args_type_name != NULL) {
        codebuf_write(ctx->decls, "\ntypedef struct {\n");
        for (size_t i = 0; i < arg_count; i++) {
            char arg_type_buf[256];
            const char *arg_type = NULL;
            FuncParam *param = NULL;
            const char *param_type_name = NULL;
            if (callee_has_mir_signature) {
                if (i < transpiler_mir_routine_param_count(callee_routine)) {
                    param = transpiler_mir_routine_param(callee_routine, i);
                    param_type_name =
                        transpiler_mir_routine_param_type_name(
                            callee_routine, i);
                }
            } else if (allow_ast_compat) {
                param = ast_func_param(decl, i);
            }
            if (param_type_name != NULL) {
                if (transpiler_spawn_reject_worker_storage(ctx,
                        function_name, i, param_type_name)) {
                    return NULL;
                }
                if (transpiler_require_type_name_c_type_copy(ctx,
                        param_type_name,
                        "spawn wrapper MIR argument",
                        arg_type_buf,
                        sizeof(arg_type_buf))) {
                    arg_type = arg_type_buf;
                }
            } else if (param != NULL && param->type != NULL) {
                if (binding_count > 0) {
                    char *bound_type =
                        transpiler_render_type_name_with_bindings(ctx,
                        param->type, bindings, binding_count);
                    if (bound_type == NULL || bound_type[0] == '\0'
                        || strcmp(bound_type, "Unknown") == 0) {
                        transpiler_set_backend_error_with_hints(ctx,
                            PGY_CODE_C_TYPE_UNSUPPORTED,
                            PGY_CAUSE_C_TYPE_UNSUPPORTED,
                            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                            "C spawn wrapper argument %llu requires concrete parameter metadata for call '%s'",
                            (unsigned long long)i,
                            function_name != NULL ? function_name : "<function>");
                        free(bound_type);
                        return NULL;
                    }
                    if (transpiler_spawn_reject_worker_storage(ctx,
                            function_name, i, bound_type)) {
                        free(bound_type);
                        return NULL;
                    }
                    if (!transpiler_require_type_name_c_type_copy(ctx,
                            bound_type,
                            "spawn wrapper generic argument",
                            arg_type_buf,
                            sizeof(arg_type_buf))) {
                        transpiler_set_backend_error_with_hints(ctx,
                            PGY_CODE_C_TYPE_UNSUPPORTED,
                            PGY_CAUSE_C_TYPE_UNSUPPORTED,
                            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                            "C spawn wrapper argument %llu requires concrete parameter metadata for call '%s'",
                            (unsigned long long)i,
                            function_name != NULL ? function_name : "<function>");
                        free(bound_type);
                        return NULL;
                    }
                    arg_type = arg_type_buf;
                    codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
                    free(bound_type);
                    continue;
                }
                char *rendered_param_type =
                    render_type_name_in_ctx(ctx, param->type);
                if (transpiler_spawn_reject_worker_storage(ctx,
                        function_name, i, rendered_param_type)) {
                    free(rendered_param_type);
                    return NULL;
                }
                free(rendered_param_type);
                if (pergyra_ast_type_to_c_copy_in_ctx(ctx,
                        param->type,
                        arg_type_buf,
                        sizeof(arg_type_buf))) {
                    arg_type = arg_type_buf;
                }
            } else if (call != NULL) {
                const char *inferred_arg_type = infer_expression_type_name(
                    ctx, ast_call_argument(call, i));
                if (transpiler_spawn_reject_worker_storage(ctx,
                        function_name, i, inferred_arg_type)) {
                    return NULL;
                }
                if (inferred_arg_type != NULL
                    && transpiler_require_type_name_c_type_copy(ctx,
                        inferred_arg_type,
                        "spawn wrapper inferred argument",
                        arg_type_buf,
                        sizeof(arg_type_buf))) {
                    arg_type = arg_type_buf;
                }
            }
            if (arg_type == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine spawn wrapper argument type for call '%s' at argument %llu",
                    function_name != NULL ? function_name : "<function>",
                    (unsigned long long) i);
                return NULL;
            }
            if (arg_type[0] == '\0' || strcmp(arg_type, "Unknown") == 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C spawn wrapper argument %llu requires concrete C type metadata for call '%s'",
                    (unsigned long long)i,
                    function_name != NULL ? function_name : "<function>");
                return NULL;
            }
            codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
        }
        codebuf_write(ctx->decls, "} %s;\n", args_type_name);
    }

    codebuf_write(ctx->decls, "static void *%s(void *raw);\n", wrapper_name);
    codebuf_write(ctx->wrappers, "\nstatic void *%s(void *raw)\n{\n", wrapper_name);
    if (args_type_name != NULL) {
        codebuf_write(ctx->wrappers, "    %s *args = (%s *)raw;\n",
            args_type_name, args_type_name);
    } else {
        codebuf_write(ctx->wrappers, "    (void)raw;\n");
    }

    if (strcmp(return_type_name, "Void") == 0) {
        codebuf_write(ctx->wrappers, "    %s(", emitted_function_name);
    } else {
        codebuf_write(ctx->wrappers,
            "    %s *result = (%s *)malloc(sizeof(%s));\n",
            return_c_type, return_c_type, return_c_type);
        codebuf_write(ctx->wrappers,
            "    if (result == NULL) {\n"
            "        PGY_PANIC(\"spawn result allocation failed\");\n"
            "    }\n"
            "    *result = %s(",
            emitted_function_name);
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (i > 0)
            codebuf_write(ctx->wrappers, ", ");
        codebuf_write(ctx->wrappers, "args->arg%zu", i);
    }
    codebuf_write(ctx->wrappers, ");\n");

    if (args_type_name != NULL)
        codebuf_write(ctx->wrappers, "    free(args);\n");

    if (strcmp(return_type_name, "Void") == 0)
        codebuf_write(ctx->wrappers, "    return NULL;\n");
    else
        codebuf_write(ctx->wrappers, "    return result;\n");
    codebuf_write(ctx->wrappers, "}\n");

    /* SEA: spawn lowering consumes the verified ExecutionLane fact carried
       from AIR (the spawn-lane plan). The backend does not recover the lane
       from source spelling; the lane scheduler facade owns the executor
       mapping. A spawn site the plan does not cover is fail-closed. */
    PgyExecutionLane spawn_lane = PGY_LANE_REJECT;
    if (!pgy_verified_spawn_lane_plan_lookup(ctx->spawn_lane_plan, node,
            &spawn_lane)) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C backend: spawn at line %d has no verified execution-lane fact in the AIR spawn-lane plan",
            node->line);
        return NULL;
    }

    CodeBuf *expr = codebuf_create();
    if (expr == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C spawn expression could not allocate wrapper call buffer");
        return NULL;
    }

    {
        const char *lane_symbol = transpiler_spawn_lane_symbol(spawn_lane);
        if (args_type_name == NULL) {
            codebuf_write(expr,
                "({ PgyTaskHandle _pgy_spawn_h = pgy_lane_spawn_dispatch(%s, %s, NULL); "
                "if (_pgy_spawn_h.task == NULL) { PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \"spawn task creation failed\"); } "
                "_pgy_spawn_h; })",
                lane_symbol, wrapper_name);
        } else {
            codebuf_write(expr,
                "({ %s *_pgy_args = (%s *)malloc(sizeof(%s)); "
                "if (_pgy_args == NULL) { PGY_PANIC(\"spawn arg allocation failed\"); } ",
                args_type_name, args_type_name, args_type_name);
            for (size_t i = 0; i < arg_count; i++) {
                char *arg = transpiler_spawn_channel_emit_expr(ctx,
                    ast_call_argument(call, i), "spawn", "argument");
                if (arg == NULL) {
                    codebuf_destroy(expr);
                    return NULL;
                }
                codebuf_write(expr, "_pgy_args->arg%zu = %s; ", i, arg);
                free(arg);
            }
            codebuf_write(expr,
                "PgyTaskHandle _pgy_spawn_h = pgy_lane_spawn_dispatch(%s, %s, _pgy_args); "
                "if (_pgy_spawn_h.task == NULL) { free(_pgy_args); PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \"spawn task creation failed\"); } "
                "_pgy_spawn_h; })",
                lane_symbol, wrapper_name);
        }
    }

    char *result = pergyra_strdup(expr->data);
    codebuf_destroy(expr);
    return result;
}

char *
emit_channel_send(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *channel = ast_channel_send_channel(node);
    ASTNode *value = ast_channel_send_value(node);
    if (!transpiler_channel_expr_is_c_lvalue(channel)) {
        transpiler_set_channel_lvalue_error(ctx, "channel send");
        return NULL;
    }

    char *ch  = transpiler_emit_channel_lvalue_expr(ctx, channel);
    char *val = transpiler_spawn_channel_emit_expr(ctx,
        value, "channel send", "value");
    if (val == NULL) {
        free(ch);
        return NULL;
    }
    char inner_buf[128];
    const char *inner = transpiler_require_channel_inner_type(
        ctx, channel, "channel send", inner_buf, sizeof(inner_buf));
    char runtime_fn[128];

    if (inner == NULL) {
        free(ch);
        free(val);
        return NULL;
    }
    if (!transpiler_spawn_channel_runtime_symbol(ctx, "channel send",
            "send", inner, runtime_fn, sizeof(runtime_fn))) {
        free(ch);
        free(val);
        return NULL;
    }

    char *result = strdup_fmt(
        "%s(PGY_LANE_PINNED_ZONE, &%s, %s)",
        runtime_fn, ch, val);
    free(ch);
    free(val);
    return result;
}

char *
emit_channel_recv(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *channel = ast_channel_recv_channel(node);
    if (!transpiler_channel_expr_is_c_lvalue(channel)) {
        transpiler_set_channel_lvalue_error(ctx, "channel receive");
        return NULL;
    }

    char *ch = transpiler_emit_channel_lvalue_expr(ctx, channel);
    char inner_buf[128];
    const char *inner = transpiler_require_channel_inner_type(
        ctx, channel, "channel receive", inner_buf, sizeof(inner_buf));
    char runtime_fn[128];

    if (inner == NULL) {
        free(ch);
        return NULL;
    }
    if (!transpiler_spawn_channel_runtime_symbol(ctx, "channel receive",
            "recv_val", inner, runtime_fn, sizeof(runtime_fn))) {
        free(ch);
        return NULL;
    }

    char *result = strdup_fmt(
        "%s(PGY_LANE_PINNED_ZONE, &%s)",
        runtime_fn, ch);
    free(ch);
    return result;
}

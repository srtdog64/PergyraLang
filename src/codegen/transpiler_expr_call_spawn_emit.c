#include "transpiler_expr_call_spawn_emit.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "../semantic/builtin_kind.h"
#include "../semantic/diag_codes.h"

#include "transpiler_call_constructor_result_emit.h"
#include "transpiler_call_result_option_builtin_emit.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_event_builtin_emit.h"
#include "transpiler_expr_builtin_dispatch.h"
#include "transpiler_expr_call_member_emit.h"
#include "transpiler_expr_call_user_emit.h"
#include "transpiler_expr_stdlib_builtin.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"

#include <stdlib.h>

static char *
emit_call_resolved(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode    *callee = ast_call_callee(call);
    BuiltinKind bk     = BUILTIN_NOT_BUILTIN;

    if (callee->type == AST_IDENTIFIER) {
        const char *callee_name = ast_identifier_name(callee);
        /* Only this call site's admitted binding can select a local; C also
         * pre-registers locals that are not yet in source scope. */
        if (ast_call_semantic_callee_value_binding_id(call) != 0)
            return emit_call_user_function(call, callee, ctx);
        /* Semantic chose a declaration (the host's method or a program
         * function) over a builtin or stdlib operation of the same
         * spelling (docs/205 R7). */
        if (ast_call_semantic_callee_declared_callable(call))
            return emit_call_user_function(call, callee, ctx);
        bk = builtin_resolve(callee_name);
        if ((bk == BUILTIN_BOX || bk == BUILTIN_RC_NEW)
            && transpiler_projection_nominal_decl_exists_local(
                ctx, callee_name)) {
            bk = BUILTIN_NOT_BUILTIN;
        }
    }

    bool handled = false;
    char *result = emit_call_builtin_dispatch(call, bk, ctx, &handled);
    if (handled)
        return result;

    result = emit_call_domain_constructor(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_result_option_builtin(call, callee, ctx, &handled);
    if (handled)
        return result;
    result = emit_call_stdlib_builtin(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_event_builtin(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_member_style(call, callee, ctx);
    if (result != NULL)
        return result;
    return emit_call_user_function(call, callee, ctx);
}

/* Pergyra evaluates call arguments left to right. C leaves the order
 * unspecified and GCC evaluates right to left, so a builtin or stdlib call
 * with two or more arguments, one of them more than a literal or a local
 * read, binds every argument that can cause or observe an effect to a
 * temporary in source order, as ordered user calls and binary operands
 * already do (docs/205 §11). The builtin emitter reads each temporary through
 * emit_expression. Literals stay in place; so do non-scalar reads, whose
 * storage the emitter may take the address of. */
static bool
emit_call_arg_is_literal(const ASTNode *arg)
{
    return arg != NULL && (arg->type == AST_NUMBER || arg->type == AST_STRING
        || arg->type == AST_BOOLEAN);
}

static bool
emit_call_arg_is_read(const ASTNode *arg)
{
    return arg != NULL && (arg->type == AST_IDENTIFIER
        || (arg->type == AST_MEMBER_ACCESS && ast_member_object(arg) != NULL
            && ast_member_object(arg)->type == AST_IDENTIFIER));
}

static bool
emit_call_arg_is_scalar(TranspilerCtx *ctx, ASTNode *arg)
{
    static const char *const scalars[] = {
        "Int", "Long", "Float", "Double", "Bool", "String",
    };
    const char *type = transpiler_expr_infer_type_name(ctx, arg);
    for (size_t i = 0; type != NULL && i < sizeof(scalars) / sizeof(scalars[0]); i++) {
        if (strcmp(type, scalars[i]) == 0)
            return true;
    }
    return false;
}

static bool
emit_call_args_need_order(ASTNode *call)
{
    size_t non_literal = 0;
    bool effectful = false;
    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
        ASTNode *arg = ast_call_argument(call, i);
        if (!emit_call_arg_is_literal(arg))
            non_literal++;
        if (!emit_call_arg_is_literal(arg) && !emit_call_arg_is_read(arg))
            effectful = true;
    }
    return non_literal >= 2 && effectful;
}

const char *
transpiler_ordered_argument_name(TranspilerCtx *ctx, const ASTNode *node)
{
    for (size_t i = ctx != NULL ? ctx->ordered_arg_count : 0; i > 0; i--) {
        if (ctx->ordered_args[i - 1].node == node) {
            ctx->ordered_args[i - 1].uses++;
            return ctx->ordered_args[i - 1].name;
        }
    }
    return NULL;
}

static char *
emit_call_ordered(ASTNode *call, TranspilerCtx *ctx)
{
    size_t base = ctx->ordered_arg_count;
    size_t count = ast_call_arg_count(call);
    size_t bound = 0;
    size_t bound_index[64];
    char bound_name[64][sizeof(ctx->ordered_args[0].name)];
    CodeBuf *prefix = codebuf_create();
    char *inner = NULL;
    char *result = NULL;

    if (prefix == NULL)
        return NULL;
    for (size_t i = 0; i < count; i++) {
        ASTNode *arg = ast_call_argument(call, i);
        char *text;
        if (emit_call_arg_is_literal(arg)
            || (emit_call_arg_is_read(arg) && !emit_call_arg_is_scalar(ctx, arg)))
            continue;
        if (bound >= 64 || base + bound >= 64) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: builtin call nests more than 64 ordered arguments");
            goto done;
        }
        text = emit_expression(arg, ctx);
        if (text == NULL)
            goto done;
        snprintf(bound_name[bound], sizeof(bound_name[bound]),
            "__pgy_seq_%u_%zu", (unsigned)ast_node_stable_id(call), i);
        codebuf_write(prefix, "__auto_type %s = (%s); ", bound_name[bound], text);
        free(text);
        bound_index[bound++] = i;
    }
    for (size_t b = 0; b < bound; b++) {
        ctx->ordered_args[base + b].node = ast_call_argument(call, bound_index[b]);
        memcpy(ctx->ordered_args[base + b].name, bound_name[b],
            sizeof(ctx->ordered_args[base + b].name));
        ctx->ordered_args[base + b].uses = 0;
    }
    ctx->ordered_arg_count = base + bound;
    inner = emit_call_resolved(call, ctx);
    for (size_t b = 0; inner != NULL && b < bound; b++) {
        if (ctx->ordered_args[base + b].uses == 0) {
            /* The emitter reached this argument some other way, so its text
             * would run twice; refuse instead of reordering silently. */
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: call '%s' did not read argument %zu through its ordered temporary",
                ast_call_callee(call)->type == AST_IDENTIFIER
                    ? ast_identifier_name(ast_call_callee(call)) : "<call>",
                bound_index[b] + 1);
            free(inner);
            inner = NULL;
        }
    }
    ctx->ordered_arg_count = base;
    if (inner == NULL)
        goto done;
    result = bound == 0 ? inner
        : strdup_fmt("({ %s%s; })", prefix->data != NULL ? prefix->data : "", inner);
    if (result != inner)
        free(inner);
done:
    ctx->ordered_arg_count = base;
    codebuf_destroy(prefix);
    return result;
}

char *
emit_call(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *callee = ast_call_callee(call);

    if (callee != NULL && callee->type == AST_IDENTIFIER
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && !ast_call_semantic_callee_declared_callable(call)
        && emit_call_args_need_order(call))
        return emit_call_ordered(call, ctx);
    return emit_call_resolved(call, ctx);
}

/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "transpiler.h"
#include "../common/string_compat.h"
#include "../semantic/type_checker.h"

/* -----------------------------------------------------------------
 * CodeBuf
 * ----------------------------------------------------------------- */

#define CODEBUF_INITIAL_CAP 4096

CodeBuf *
codebuf_create(void)
{
    CodeBuf *b = calloc(1, sizeof(CodeBuf));
    if (b == NULL)
        return NULL;
    b->data = malloc(CODEBUF_INITIAL_CAP);
    if (b->data == NULL) {
        free(b);
        return NULL;
    }
    b->data[0] = '\0';
    b->cap     = CODEBUF_INITIAL_CAP;
    b->len     = 0;
    return b;
}

void
codebuf_destroy(CodeBuf *buf)
{
    if (buf == NULL)
        return;
    free(buf->data);
    free(buf);
}

void
codebuf_write(CodeBuf *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    /* Measure required space */
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);

    if (needed < 0) {
        va_end(ap);
        return;
    }

    /* Grow if needed */
    while (buf->len + (size_t)needed + 1 > buf->cap) {
        size_t new_cap = buf->cap * 2;
        char  *grown   = realloc(buf->data, new_cap);
        if (grown == NULL) {
            va_end(ap);
            return;
        }
        buf->data = grown;
        buf->cap  = new_cap;
    }

    vsnprintf(buf->data + buf->len, buf->cap - buf->len, fmt, ap);
    buf->len += (size_t)needed;
    va_end(ap);
}

void
codebuf_write_raw(CodeBuf *buf, const char *s, size_t n)
{
    while (buf->len + n + 1 > buf->cap) {
        size_t new_cap = buf->cap * 2;
        char  *grown   = realloc(buf->data, new_cap);
        if (grown == NULL)
            return;
        buf->data = grown;
        buf->cap  = new_cap;
    }
    memcpy(buf->data + buf->len, s, n);
    buf->len += n;
    buf->data[buf->len] = '\0';
}

bool
codebuf_dump_file(const CodeBuf *buf, const char *path)
{
    FILE *f = fopen(path, "w");
    if (f == NULL)
        return false;
    fwrite(buf->data, 1, buf->len, f);
    fclose(f);
    return true;
}

/* -----------------------------------------------------------------
 * TranspilerCtx
 * ----------------------------------------------------------------- */

TranspilerCtx *
transpiler_ctx_create(void)
{
    TranspilerCtx *ctx = calloc(1, sizeof(TranspilerCtx));
    if (ctx == NULL)
        return NULL;
    ctx->out   = codebuf_create();
    ctx->decls = codebuf_create();
    ctx->helpers = codebuf_create();
    if (ctx->out == NULL || ctx->decls == NULL || ctx->helpers == NULL) {
        codebuf_destroy(ctx->out);
        codebuf_destroy(ctx->decls);
        codebuf_destroy(ctx->helpers);
        free(ctx);
        return NULL;
    }
    return ctx;
}

void
transpiler_ctx_destroy(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return;
    codebuf_destroy(ctx->out);
    codebuf_destroy(ctx->decls);
    codebuf_destroy(ctx->helpers);
    free(ctx);
}

/* -----------------------------------------------------------------
 * Indent helper
 * ----------------------------------------------------------------- */

static void
write_indent(TranspilerCtx *ctx)
{
    for (int i = 0; i < ctx->indent; i++)
        codebuf_write(ctx->out, "    ");
}

static void
write_indent_to(CodeBuf *buf, int indent)
{
    for (int i = 0; i < indent; i++)
        codebuf_write(buf, "    ");
}

/* -----------------------------------------------------------------
 * Slot variable tracking
 * ----------------------------------------------------------------- */

static void
register_slot_var(TranspilerCtx *ctx, const char *name,
                  const char *inner_type, bool is_secure)
{
    if (ctx->slot_var_count >= MAX_SLOT_VARS)
        return;
    SlotVarEntry *e = &ctx->slot_vars[ctx->slot_var_count++];
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    strncpy(e->inner_type, inner_type, sizeof(e->inner_type) - 1);
    e->inner_type[sizeof(e->inner_type) - 1] = '\0';
    e->is_secure = is_secure;
}

static const char *
lookup_slot_type(TranspilerCtx *ctx, const char *var_name)
{
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0)
            return ctx->slot_vars[i].inner_type;
    }
    return "Int"; /* fallback */
}

static bool
lookup_slot_is_secure(TranspilerCtx *ctx, const char *var_name)
{
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0)
            return ctx->slot_vars[i].is_secure;
    }
    return false;
}

static void
register_typed_var(TranspilerCtx *ctx, const char *name, const char *type_name)
{
    if (ctx->typed_var_count >= MAX_SLOT_VARS || name == NULL || type_name == NULL)
        return;
    TypedVarEntry *e = &ctx->typed_vars[ctx->typed_var_count++];
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    strncpy(e->type_name, type_name, sizeof(e->type_name) - 1);
    e->type_name[sizeof(e->type_name) - 1] = '\0';
}

static const char *
lookup_typed_var(TranspilerCtx *ctx, const char *var_name)
{
    for (int i = 0; i < ctx->typed_var_count; i++) {
        if (strcmp(ctx->typed_vars[i].name, var_name) == 0)
            return ctx->typed_vars[i].type_name;
    }
    return NULL;
}

static ASTNode *
find_role_decl(TranspilerCtx *ctx, const char *role_name)
{
    if (ctx == NULL || ctx->program == NULL || ctx->program->type != AST_PROGRAM)
        return NULL;

    for (size_t i = 0; i < ctx->program->data.program.count; i++) {
        ASTNode *stmt = ctx->program->data.program.statements[i];
        if (stmt->type == AST_ROLE_DECL
            && stmt->data.role_decl.name != NULL
            && strcmp(stmt->data.role_decl.name, role_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_function_decl(TranspilerCtx *ctx, const char *function_name)
{
    if (ctx == NULL || ctx->program == NULL || ctx->program->type != AST_PROGRAM)
        return NULL;

    for (size_t i = 0; i < ctx->program->data.program.count; i++) {
        ASTNode *stmt = ctx->program->data.program.statements[i];
        if (stmt->type == AST_FUNC_DECL
            && stmt->data.func_decl.name != NULL
            && strcmp(stmt->data.func_decl.name, function_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static bool
role_has_ability(ASTNode *role, const char *ability_name)
{
    if (role == NULL || role->type != AST_ROLE_DECL || ability_name == NULL)
        return false;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl->type == AST_IMPL_ABILITY
            && impl->data.impl_ability.ability_name != NULL
            && strcmp(impl->data.impl_ability.ability_name, ability_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
role_has_method(ASTNode *role, const char *method_name)
{
    if (role == NULL || role->type != AST_ROLE_DECL || method_name == NULL)
        return false;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method->type == AST_FUNC_DECL
                && method->data.func_decl.name != NULL
                && strcmp(method->data.func_decl.name, method_name) == 0) {
                return true;
            }
        }
    }

    return false;
}

/* -----------------------------------------------------------------
 * Type mapping
 * ----------------------------------------------------------------- */

const char *
pergyra_primitive_to_c(const char *name)
{
    if (strcmp(name, "Int")    == 0) return "int32_t";
    if (strcmp(name, "Long")   == 0) return "int64_t";
    if (strcmp(name, "Float")  == 0) return "float";
    if (strcmp(name, "Double") == 0) return "double";
    if (strcmp(name, "Bool")   == 0) return "bool";
    if (strcmp(name, "String") == 0) return "char*";
    if (strcmp(name, "Void")   == 0) return "void";
    return name; /* user-defined type — pass through */
}

/*
 * slot_inner_type_name("Slot<Int>")       → "Int"
 * slot_inner_type_name("SecureSlot<Int>") → "Int"
 */
const char *
slot_inner_type_name(const char *slot_type_name)
{
    const char *open = strchr(slot_type_name, '<');
    if (open == NULL)
        return slot_type_name;
    open++; /* skip '<' */

    /* Trim trailing '>' — return a static buffer (fine for our uses) */
    static char buf[128];
    const char *close = strrchr(slot_type_name, '>');
    if (close == NULL || close <= open)
        return "Int";

    size_t len = (size_t)(close - open);
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, open, len);
    buf[len] = '\0';
    return buf;
}

const char *
pergyra_type_to_c(const char *name)
{
    if (strcmp(name, "Allocator") == 0)
        return "PgyAllocator";
    if (strncmp(name, "Future<", 7) == 0)
        return "PgyTaskHandle";
    if (strncmp(name, "Channel<", 8) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyChannel_%s", inner);
        return buf;
    }
    if (strncmp(name, "Weak<", 5) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyWeak_%s", inner);
        return buf;
    }
    if (strncmp(name, "Rc<", 3) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyRc_%s", inner);
        return buf;
    }
    if (strncmp(name, "Box<Array<", 10) == 0) {
        static char buf[128];
        const char *inner = name + 10;
        const char *close = strstr(inner, ">>");
        size_t len = close != NULL ? (size_t)(close - inner) : strlen(inner);
        if (len > 63) len = 63;
        char inner_buf[64];
        memcpy(inner_buf, inner, len);
        inner_buf[len] = '\0';
        snprintf(buf, sizeof(buf), "PgyBoxArray_%s", inner_buf);
        return buf;
    }
    if (strncmp(name, "Box<", 4) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyBox_%s", inner);
        return buf;
    }
    if (strncmp(name, "Slice<", 6) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlice_%s", inner);
        return buf;
    }
    if (strncmp(name, "Array<", 6) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyArray_%s", inner);
        return buf;
    }
    if (strncmp(name, "SecureSlot<", 11) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySecureSlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "Slot<", 5) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    return pergyra_primitive_to_c(name);
}

static void
append_type_name(CodeBuf *buf, ASTNode *type_node)
{
    if (type_node == NULL
        || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        codebuf_write(buf, "Int");
        return;
    }

    codebuf_write(buf, "%s", type_node->data.type.name);
    if (type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0) {
        codebuf_write(buf, "<");
        for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
            GenericParam *param = type_node->data.type.generic_args->params[i];
            if (i > 0)
                codebuf_write(buf, ", ");
            if (param != NULL && param->constraint != NULL) {
                append_type_name(buf, param->constraint);
            } else if (param != NULL && param->name != NULL) {
                codebuf_write(buf, "%s", param->name);
            } else {
                codebuf_write(buf, "Int");
            }
        }
        codebuf_write(buf, ">");
    }
}

static char *strdup_fmt(const char *fmt, ...);

static char *
render_type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return pergyra_strdup("Int");

    if (type_node->type == AST_CHANNEL_TYPE) {
        char *inner = render_type_name(type_node->data.channel_type.element_type);
        char *result = strdup_fmt("Channel<%s>", inner);
        free(inner);
        return result;
    }

    if (type_node->type == AST_FUTURE_TYPE) {
        char *inner = render_type_name(type_node->data.future_type.value_type);
        char *result = strdup_fmt("Future<%s>", inner);
        free(inner);
        return result;
    }

    CodeBuf *buf = codebuf_create();
    if (buf == NULL)
        return pergyra_strdup("Int");
    append_type_name(buf, type_node);
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static const char *
infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL)
        return "Int";

    switch (expr->type) {
    case AST_NUMBER:
        return "Int";
    case AST_STRING:
        return "String";
    case AST_BOOLEAN:
        return "Bool";
    case AST_IDENTIFIER: {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        return type_name != NULL ? type_name : "Int";
    }
    case AST_CHANNEL_RECV: {
        ASTNode *channel = expr->data.channel_recv.channel;
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *type_name = lookup_typed_var(ctx, channel->data.identifier.name);
            if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                return slot_inner_type_name(type_name);
        }
        return "Int";
    }
    default:
        return "Int";
    }
}

static char *
infer_spawn_return_type_name(TranspilerCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = spawn_expr != NULL ? spawn_expr->data.spawn_expr.function : NULL;
    const char *function_name = NULL;

    if (target == NULL)
        return pergyra_strdup("Void");

    if (target->type == AST_CALL
        && target->data.call.callee != NULL
        && target->data.call.callee->type == AST_IDENTIFIER) {
        function_name = target->data.call.callee->data.identifier.name;
    } else if (target->type == AST_IDENTIFIER) {
        function_name = target->data.identifier.name;
    } else if (target->type == AST_FUNC_DECL) {
        if (target->data.func_decl.return_type != NULL)
            return render_type_name(target->data.func_decl.return_type);
        return pergyra_strdup("Void");
    }

    if (function_name == NULL)
        return pergyra_strdup("Void");

    ASTNode *decl = find_function_decl(ctx, function_name);
    if (decl != NULL && decl->data.func_decl.return_type != NULL)
        return render_type_name(decl->data.func_decl.return_type);

    return pergyra_strdup("Void");
}

static const char *
lookup_future_inner_type(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL)
        return "Void";

    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Future<", 7) == 0)
            return slot_inner_type_name(type_name);
    }

    if (expr->type == AST_SPAWN_EXPR) {
        char *inner = infer_spawn_return_type_name(ctx, expr);
        static char buf[128];
        snprintf(buf, sizeof(buf), "%s", inner);
        free(inner);
        return buf;
    }

    return "Void";
}

static const char *
pergyra_ast_type_to_c(ASTNode *type_node)
{
    static char mapped[128];
    if (type_node == NULL)
        return "void";

    char *type_name = render_type_name(type_node);
    snprintf(mapped, sizeof(mapped), "%s", pergyra_type_to_c(type_name));
    free(type_name);
    return mapped;
}

/* -----------------------------------------------------------------
 * Expression emitters — return heap-allocated C expression string
 * ----------------------------------------------------------------- */

static char *
strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0)
        return pergyra_strdup("");

    char *s = malloc((size_t)n + 1);
    if (s == NULL)
        return pergyra_strdup("");

    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

/* -----------------------------------------------------------------
 * Built-in call emitters
 * ----------------------------------------------------------------- */

char *
emit_builtin_claim_slot(ASTNode *call, TranspilerCtx *ctx)
{
    /*
     * ClaimSlot<T>() is handled in let_decl where the type is known.
     * If encountered standalone, emit a comment.
     */
    (void)call;
    (void)ctx;
    return pergyra_strdup("/* ClaimSlot */");
}

char *
emit_builtin_write(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 2)
        return pergyra_strdup("/* Write: missing args */");

    /* Resolve slot inner type from tracking table */
    const char *inner = "Int";
    ASTNode *slot_arg = call->data.call.arguments[0];
    if (slot_arg->type == AST_IDENTIFIER)
        inner = lookup_slot_type(ctx, slot_arg->data.identifier.name);

    char *slot_expr  = emit_expression(slot_arg, ctx);
    char *value_expr = emit_expression(call->data.call.arguments[1], ctx);

    char *result;
    if (call->data.call.arg_count >= 3) {
        /* SecureSlot: Write(slot, value, token) */
        char *token_expr = emit_expression(call->data.call.arguments[2], ctx);
        result = strdup_fmt(
            "pgy_secure_write_%s(&%s, %s, &%s)",
            inner, slot_expr, value_expr, token_expr);
        free(token_expr);
    } else {
        /* Plain slot: Write(slot, value) */
        result = strdup_fmt(
            "pgy_write_%s(&%s, %s)",
            inner, slot_expr, value_expr);
    }

    free(slot_expr);
    free(value_expr);
    return result;
}

char *
emit_builtin_read(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 1)
        return pergyra_strdup("/* Read: missing args */");

    /* Resolve slot inner type from tracking table */
    const char *inner = "Int";
    ASTNode *slot_arg = call->data.call.arguments[0];
    if (slot_arg->type == AST_IDENTIFIER)
        inner = lookup_slot_type(ctx, slot_arg->data.identifier.name);

    char *slot_expr = emit_expression(slot_arg, ctx);
    char *result;

    if (call->data.call.arg_count >= 2) {
        char *token_expr = emit_expression(call->data.call.arguments[1], ctx);
        result = strdup_fmt(
            "pgy_secure_read_%s(&%s, &%s)",
            inner, slot_expr, token_expr);
        free(token_expr);
    } else {
        result = strdup_fmt("pgy_read_%s(&%s)", inner, slot_expr);
    }

    free(slot_expr);
    return result;
}

char *
emit_builtin_release(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 1)
        return pergyra_strdup("/* Release: missing args */");

    /* Resolve slot inner type from tracking table */
    const char *inner = "Int";
    ASTNode *slot_arg = call->data.call.arguments[0];
    if (slot_arg->type == AST_IDENTIFIER)
        inner = lookup_slot_type(ctx, slot_arg->data.identifier.name);

    char *slot_expr = emit_expression(slot_arg, ctx);
    char *result;

    if (call->data.call.arg_count >= 2) {
        char *token_expr = emit_expression(call->data.call.arguments[1], ctx);
        result = strdup_fmt(
            "pgy_secure_release_%s(&%s, &%s)",
            inner, slot_expr, token_expr);
        free(token_expr);
    } else {
        result = strdup_fmt("pgy_release_%s(&%s)", inner, slot_expr);
    }

    free(slot_expr);
    return result;
}

char *
emit_builtin_log(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count == 0)
        return pergyra_strdup("printf(\"\\n\")");

    if (call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_log(%s)", arg);
        free(arg);
        return result;
    }

    /* Multi-arg Log: emit each argument with pgy_log() */
    CodeBuf *buf = codebuf_create();
    codebuf_write(buf, "do { ");
    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        char *arg = emit_expression(call->data.call.arguments[i], ctx);
        codebuf_write(buf, "pgy_log(%s); ", arg);
        free(arg);
    }
    codebuf_write(buf, "} while(0)");
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static const char *
lookup_wrapped_inner_type(TranspilerCtx *ctx, ASTNode *arg, const char *wrapper)
{
    if (arg != NULL && arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, arg->data.identifier.name);
        size_t wrapper_len = strlen(wrapper);
        if (type_name != NULL && strncmp(type_name, wrapper, wrapper_len) == 0
            && type_name[wrapper_len] == '<') {
            return slot_inner_type_name(type_name);
        }
    }
    return "Int";
}

char *
emit_builtin_rc(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *arg = call->data.call.arg_count > 0 ? call->data.call.arguments[0] : NULL;

    switch (kind) {
    case BUILTIN_RC_NEW:
        if (call->data.call.arg_count != 1)
            return pergyra_strdup("/* RcNew: invalid args */");
        if (arg->type == AST_NUMBER) inner = "Int";
        else if (arg->type == AST_STRING) inner = "String";
        else if (arg->type == AST_BOOLEAN) inner = "Bool";
        else if (arg->type == AST_IDENTIFIER) {
            const char *arg_type = lookup_typed_var(ctx, arg->data.identifier.name);
            if (arg_type != NULL)
                inner = arg_type;
        }
        break;
    case BUILTIN_RC_CLONE:
    case BUILTIN_RC_DROP:
    case BUILTIN_RC_GET:
    case BUILTIN_RC_DOWNGRADE:
        inner = lookup_wrapped_inner_type(ctx, arg, "Rc");
        break;
    case BUILTIN_WEAK_UPGRADE:
    case BUILTIN_WEAK_DROP:
        inner = lookup_wrapped_inner_type(ctx, arg, "Weak");
        break;
    default:
        break;
    }

    if (kind == BUILTIN_RC_NEW) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_new_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_CLONE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_clone_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_GET) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("(*pgy_rc_get_%s(&%s))", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_DOWNGRADE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_downgrade_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_WEAK_UPGRADE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_weak_upgrade_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_WEAK_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_weak_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    return pergyra_strdup("/* unsupported rc builtin */");
}

char *
emit_builtin_allocator(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    switch (kind) {
    case BUILTIN_ALLOCATOR_SYSTEM:
        return pergyra_strdup("pgy_allocator_system()");
    case BUILTIN_ALLOCATOR_TRACING:
        return pergyra_strdup("pgy_allocator_tracing()");
    case BUILTIN_ALLOCATOR_DEBUG:
        return pergyra_strdup("pgy_allocator_debug()");
    case BUILTIN_ALLOCATOR_POOL:
        if (call->data.call.arg_count != 1)
            return pergyra_strdup("/* AllocatorPool: invalid args */");
        {
            char *cap = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("pgy_allocator_pool(%s)", cap);
            free(cap);
            return result;
        }
    default:
        return pergyra_strdup("/* unsupported allocator builtin */");
    }
}

/* -----------------------------------------------------------------
 * Token operator helpers
 * ----------------------------------------------------------------- */

static const char *
binary_op_to_c(TokenType op)
{
    switch (op) {
    case TOKEN_PLUS:          return "+";
    case TOKEN_MINUS:         return "-";
    case TOKEN_STAR:          return "*";
    case TOKEN_SLASH:         return "/";
    case TOKEN_PERCENT:       return "%";
    case TOKEN_EQUAL:         return "==";
    case TOKEN_NOT_EQUAL:     return "!=";
    case TOKEN_LESS:          return "<";
    case TOKEN_LESS_EQUAL:    return "<=";
    case TOKEN_GREATER:       return ">";
    case TOKEN_GREATER_EQUAL: return ">=";
    case TOKEN_AND:           return "&&";
    case TOKEN_OR:            return "||";
    default:                  return "?";
    }
}

/* -----------------------------------------------------------------
 * Expression emitters
 * ----------------------------------------------------------------- */

char *
emit_binary(ASTNode *expr, TranspilerCtx *ctx)
{
    char *left  = emit_expression(expr->data.binary.left,  ctx);
    char *right = emit_expression(expr->data.binary.right, ctx);
    const char *op = binary_op_to_c(expr->data.binary.op.type);
    char *result = strdup_fmt("(%s %s %s)", left, op, right);
    free(left);
    free(right);
    return result;
}

char *
emit_unary(ASTNode *expr, TranspilerCtx *ctx)
{
    char *operand = emit_expression(expr->data.unary.operand, ctx);
    const char *op = (expr->data.unary.op.type == TOKEN_NOT) ? "!" : "-";
    char *result = strdup_fmt("(%s%s)", op, operand);
    free(operand);
    return result;
}

char *
emit_call(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode    *callee = call->data.call.callee;
    BuiltinKind bk     = BUILTIN_NOT_BUILTIN;

    if (callee->type == AST_IDENTIFIER) {
        bk = builtin_resolve(callee->data.identifier.name);
    }

    switch (bk) {
    case BUILTIN_CLAIM_SLOT:
    case BUILTIN_CLAIM_SECURE_SLOT:
        return emit_builtin_claim_slot(call, ctx);
    case BUILTIN_WRITE:
        return emit_builtin_write(call, ctx);
    case BUILTIN_READ:
        return emit_builtin_read(call, ctx);
    case BUILTIN_RELEASE:
        return emit_builtin_release(call, ctx);
    case BUILTIN_LOG:
        return emit_builtin_log(call, ctx);
    case BUILTIN_RC_NEW:
    case BUILTIN_RC_CLONE:
    case BUILTIN_RC_DROP:
    case BUILTIN_RC_DOWNGRADE:
    case BUILTIN_RC_GET:
    case BUILTIN_WEAK_UPGRADE:
    case BUILTIN_WEAK_DROP:
        return emit_builtin_rc(call, bk, ctx);
    case BUILTIN_ALLOCATOR_SYSTEM:
    case BUILTIN_ALLOCATOR_TRACING:
    case BUILTIN_ALLOCATOR_DEBUG:
    case BUILTIN_ALLOCATOR_POOL:
        return emit_builtin_allocator(call, bk, ctx);
    default:
        break;
    }

    /* Method-call style slot operations: slot.Write(val), slot.Read(), slot.Release() */
    if (callee->type == AST_MEMBER_ACCESS) {
        const char *method = callee->data.member.name;
        ASTNode *obj = callee->data.member.object;
        bool is_slot_method = (strcmp(method, "Write") == 0
                            || strcmp(method, "Read") == 0
                            || strcmp(method, "Release") == 0);

        if (is_slot_method && obj->type == AST_IDENTIFIER) {
            const char *inner = lookup_slot_type(ctx, obj->data.identifier.name);
            bool is_secure = lookup_slot_is_secure(ctx, obj->data.identifier.name);
            char *obj_expr = emit_expression(obj, ctx);

            if (strcmp(method, "Write") == 0 && call->data.call.arg_count >= 1) {
                char *val_expr = emit_expression(call->data.call.arguments[0], ctx);
                char *result;
                if (is_secure && call->data.call.arg_count >= 2) {
                    char *tok = emit_expression(call->data.call.arguments[1], ctx);
                    result = strdup_fmt("pgy_secure_write_%s(&%s, %s, &%s)",
                                        inner, obj_expr, val_expr, tok);
                    free(tok);
                } else if (is_secure) {
                    result = strdup_fmt("pgy_secure_write_%s(&%s, %s, &%s_token)",
                                        inner, obj_expr, val_expr, obj_expr);
                } else {
                    result = strdup_fmt("pgy_write_%s(&%s, %s)",
                                        inner, obj_expr, val_expr);
                }
                free(val_expr);
                free(obj_expr);
                return result;
            } else if (strcmp(method, "Read") == 0) {
                char *result;
                if (is_secure) {
                    result = strdup_fmt("pgy_secure_read_%s(&%s, &%s_token)",
                                        inner, obj_expr, obj_expr);
                } else {
                    result = strdup_fmt("pgy_read_%s(&%s)", inner, obj_expr);
                }
                free(obj_expr);
                return result;
            } else if (strcmp(method, "Release") == 0) {
                char *result;
                if (is_secure) {
                    result = strdup_fmt("pgy_secure_release_%s(&%s, &%s_token)",
                                        inner, obj_expr, obj_expr);
                } else {
                    result = strdup_fmt("pgy_release_%s(&%s)", inner, obj_expr);
                }
                free(obj_expr);
                return result;
            }
            free(obj_expr);
        }
    }

    /* User function call */
    char *callee_str = emit_expression(callee, ctx);

    /* Build argument list */
    CodeBuf *args_buf = codebuf_create();
    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        char *arg = emit_expression(call->data.call.arguments[i], ctx);
        if (i > 0)
            codebuf_write(args_buf, ", ");
        codebuf_write(args_buf, "%s", arg);
        free(arg);
    }

    char *result = strdup_fmt("%s(%s)", callee_str, args_buf->data);
    free(callee_str);
    codebuf_destroy(args_buf);
    return result;
}

char *
emit_spawn_expr(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *target = node->data.spawn_expr.function;
    ASTNode *call = NULL;
    ASTNode *callee = NULL;
    const char *function_name = NULL;
    ASTNode *decl = NULL;
    size_t arg_count = 0;
    int wrapper_id = ++ctx->tmp_counter;
    char *wrapper_name = strdup_fmt("pgy_spawn_wrapper_%d", wrapper_id);
    char *args_type_name = NULL;
    char *return_type_name = infer_spawn_return_type_name(ctx, node);
    char *return_c_type = pergyra_strdup(pergyra_type_to_c(return_type_name));

    if (target == NULL) {
        free(wrapper_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("pgy_spawn(NULL, NULL)");
    }

    if (target->type == AST_CALL) {
        call = target;
        callee = target->data.call.callee;
        arg_count = target->data.call.arg_count;
    } else {
        callee = target;
    }

    if (callee != NULL && callee->type == AST_IDENTIFIER)
        function_name = callee->data.identifier.name;
    if (function_name == NULL) {
        free(wrapper_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("/* unsupported spawn target */");
    }

    decl = find_function_decl(ctx, function_name);
    if (arg_count > 0)
        args_type_name = strdup_fmt("PgySpawnArgs_%d", wrapper_id);

    if (args_type_name != NULL) {
        codebuf_write(ctx->decls, "\ntypedef struct {\n");
        for (size_t i = 0; i < arg_count; i++) {
            const char *arg_type = "int32_t";
            if (decl != NULL && i < decl->data.func_decl.param_count
                && decl->data.func_decl.params[i] != NULL
                && decl->data.func_decl.params[i]->type != NULL) {
                arg_type = pergyra_ast_type_to_c(decl->data.func_decl.params[i]->type);
            } else if (call != NULL) {
                arg_type = pergyra_type_to_c(infer_expression_type_name(
                    ctx, call->data.call.arguments[i]));
            }
            codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
        }
        codebuf_write(ctx->decls, "} %s;\n", args_type_name);
    }

    codebuf_write(ctx->decls, "static void *%s(void *raw);\n", wrapper_name);
    codebuf_write(ctx->helpers, "\nstatic void *%s(void *raw)\n{\n", wrapper_name);
    if (args_type_name != NULL) {
        codebuf_write(ctx->helpers, "    %s *args = (%s *)raw;\n",
            args_type_name, args_type_name);
    } else {
        codebuf_write(ctx->helpers, "    (void)raw;\n");
    }

    if (strcmp(return_type_name, "Void") == 0) {
        codebuf_write(ctx->helpers, "    %s(", function_name);
    } else {
        codebuf_write(ctx->helpers,
            "    %s *result = (%s *)malloc(sizeof(%s));\n",
            return_c_type, return_c_type, return_c_type);
        codebuf_write(ctx->helpers,
            "    if (result == NULL) {\n"
            "        PGY_PANIC(\"spawn result allocation failed\");\n"
            "    }\n"
            "    *result = %s(",
            function_name);
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (i > 0)
            codebuf_write(ctx->helpers, ", ");
        codebuf_write(ctx->helpers, "args->arg%zu", i);
    }
    codebuf_write(ctx->helpers, ");\n");

    if (args_type_name != NULL)
        codebuf_write(ctx->helpers, "    free(args);\n");

    if (strcmp(return_type_name, "Void") == 0)
        codebuf_write(ctx->helpers, "    return NULL;\n");
    else
        codebuf_write(ctx->helpers, "    return result;\n");
    codebuf_write(ctx->helpers, "}\n");

    CodeBuf *expr = codebuf_create();
    if (expr == NULL) {
        free(wrapper_name);
        free(args_type_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("/* spawn alloc failed */");
    }

    if (args_type_name == NULL) {
        codebuf_write(expr, "pgy_spawn(%s, NULL)", wrapper_name);
    } else {
        codebuf_write(expr,
            "({ %s *_pgy_args = (%s *)malloc(sizeof(%s)); "
            "if (_pgy_args == NULL) { PGY_PANIC(\"spawn arg allocation failed\"); } ",
            args_type_name, args_type_name, args_type_name);
        for (size_t i = 0; i < arg_count; i++) {
            char *arg = emit_expression(call->data.call.arguments[i], ctx);
            codebuf_write(expr, "_pgy_args->arg%zu = %s; ", i, arg);
            free(arg);
        }
        codebuf_write(expr, "pgy_spawn(%s, _pgy_args); })", wrapper_name);
    }

    char *result = pergyra_strdup(expr->data);
    codebuf_destroy(expr);
    free(wrapper_name);
    free(args_type_name);
    free(return_type_name);
    free(return_c_type);
    return result;
}

char *
emit_channel_send(ASTNode *node, TranspilerCtx *ctx)
{
    char *ch  = emit_expression(node->data.channel_send.channel, ctx);
    char *val = emit_expression(node->data.channel_send.value, ctx);
    const char *inner = "Int";

    if (node->data.channel_send.channel != NULL
        && node->data.channel_send.channel->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            node->data.channel_send.channel->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
            inner = slot_inner_type_name(type_name);
    }

    char *result = strdup_fmt("pgy_channel_send_%s(&%s, %s)", inner, ch, val);
    free(ch);
    free(val);
    return result;
}

char *
emit_channel_recv(ASTNode *node, TranspilerCtx *ctx)
{
    char *ch = emit_expression(node->data.channel_recv.channel, ctx);
    const char *inner = "Int";

    if (node->data.channel_recv.channel != NULL
        && node->data.channel_recv.channel->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            node->data.channel_recv.channel->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
            inner = slot_inner_type_name(type_name);
    }

    char *result = strdup_fmt("pgy_channel_recv_val_%s(&%s)", inner, ch);
    free(ch);
    return result;
}

char *
emit_expression(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return pergyra_strdup("0");

    switch (node->type) {
    case AST_NUMBER:
        if (node->data.number.value == (int64_t)node->data.number.value)
            return strdup_fmt("%lld", (long long)(int64_t)node->data.number.value);
        return strdup_fmt("%g", node->data.number.value);

    case AST_STRING:
        return strdup_fmt("\"%s\"", node->data.string.value);

    case AST_BOOLEAN:
        return pergyra_strdup(node->data.boolean.value ? "true" : "false");

    case AST_IDENTIFIER: {
        const char *id_name = node->data.identifier.name;
        /* Inside parallel wrapper: captured outer variables are accessed
         * through the context struct pointer.  (*_pctx->x) yields the
         * value, and &(*_pctx->x) collapses to _pctx->x (a pointer). */
        if (ctx->in_parallel_wrapper) {
            for (int i = 0; i < ctx->par_capture_slot_end; i++) {
                if (strcmp(ctx->slot_vars[i].name, id_name) == 0)
                    return strdup_fmt("(*_pctx->%s)", id_name);
            }
            for (int i = 0; i < ctx->par_capture_typed_end; i++) {
                if (strcmp(ctx->typed_vars[i].name, id_name) == 0)
                    return strdup_fmt("(*_pctx->%s)", id_name);
            }
        }
        return pergyra_strdup(id_name);
    }

    case AST_BINARY:
        return emit_binary(node, ctx);

    case AST_UNARY:
        return emit_unary(node, ctx);

    case AST_CALL:
        return emit_call(node, ctx);

    case AST_MEMBER_ACCESS: {
        char *obj = emit_expression(node->data.member.object, ctx);
        char *result = strdup_fmt("%s.%s", obj, node->data.member.name);
        free(obj);
        return result;
    }

    case AST_ARRAY_ACCESS: {
        char *array = emit_expression(node->data.array_access.array, ctx);
        char *index = emit_expression(node->data.array_access.index, ctx);
        char *result = strdup_fmt("%s[%s]", array, index);
        free(array);
        free(index);
        return result;
    }

    case AST_ASSIGNMENT: {
        char *target = emit_expression(node->data.assignment.target, ctx);
        char *value  = emit_expression(node->data.assignment.value,  ctx);
        char *result = strdup_fmt("%s = %s", target, value);
        free(target);
        free(value);
        return result;
    }

    case AST_AWAIT_EXPR:
        {
            char *expr = emit_expression(node->data.await_expr.expression, ctx);
            const char *inner = lookup_future_inner_type(ctx,
                node->data.await_expr.expression);
            char *result;
            if (strcmp(inner, "Void") == 0) {
                result = strdup_fmt("pgy_await_void(%s)", expr);
            } else {
                result = strdup_fmt("pgy_await_take(%s, %s)",
                    expr, pergyra_type_to_c(inner));
            }
            free(expr);
            return result;
        }

    case AST_SPAWN_EXPR:
        return emit_spawn_expr(node, ctx);

    case AST_CHANNEL_SEND:
        return emit_channel_send(node, ctx);

    case AST_CHANNEL_RECV:
        return emit_channel_recv(node, ctx);

    case AST_EVENT_INVOKE: {
        char *event = emit_expression(node->data.event_invoke.event, ctx);
        CodeBuf *args = codebuf_create();
        for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
            char *arg = emit_expression(node->data.event_invoke.arguments[i], ctx);
            if (i > 0)
                codebuf_write(args, ", ");
            codebuf_write(args, "%s", arg);
            free(arg);
        }
        char *result = strdup_fmt("%s_INVOKE(&%s%s%s)",
                                  event,
                                  event,
                                  args->len > 0 ? ", " : "",
                                  args->data);
        free(event);
        codebuf_destroy(args);
        return result;
    }

    case AST_CONTEXT_ACCESS:
        if (node->data.context_access.role_slot_name != NULL) {
            return strdup_fmt("self->%s", node->data.context_access.role_slot_name);
        }
        return pergyra_strdup("self");

    case AST_PARTY_INSTANCE: {
        CodeBuf *assignments = codebuf_create();
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
            char *value = emit_expression(node->data.party_instance.assignments[i].value, ctx);
            if (i > 0)
                codebuf_write(assignments, ", ");
            codebuf_write(assignments, ".%s = %s",
                          node->data.party_instance.assignments[i].slot_name,
                          value);
            free(value);
        }
        char *result = strdup_fmt("(%s){%s}",
                                  node->data.party_instance.party_type,
                                  assignments->data);
        codebuf_destroy(assignments);
        return result;
    }

    case AST_LAMBDA_EXPR:
        return emit_lambda_expr(node, ctx);

    default:
        return pergyra_strdup("/* unsupported expr */");
    }
}

/* Forward declarations for emitters defined later */
void emit_ability_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_role_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_party_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_systemic_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_world_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_actor_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_select_stmt(ASTNode *node, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Let declaration emitter
 * ----------------------------------------------------------------- */

void
emit_let_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode    *init = node->data.let_decl.initializer;
    ASTNode    *ann  = node->data.let_decl.type;
    char       *ann_type_name = ann != NULL ? render_type_name(ann) : NULL;

    /* Detect ClaimSlot / ClaimSecureSlot */
    bool is_slot        = false;
    bool is_secure_slot = false;
    const char *slot_inner = "Int"; /* default */

    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        if (strcmp(callee_name, "ClaimSlot") == 0) {
            is_slot   = true;
        } else if (strcmp(callee_name, "ClaimSecureSlot") == 0) {
            is_slot        = true;
            is_secure_slot = true;
        }
    }

    if (is_slot) {
        /*
         * Resolve inner type from annotation.
         * If not annotated, default to Int.
         */
        if (ann != NULL) {
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                slot_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                slot_inner = slot_inner_type_name(ann->data.type.name);
            }
        }

        register_slot_var(ctx, name, slot_inner, is_secure_slot);

        write_indent(ctx);
        if (is_secure_slot) {
            codebuf_write(ctx->out,
                "PgyToken_%s %s_token;\n", slot_inner, name);
            write_indent(ctx);
            codebuf_write(ctx->out,
                "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
                slot_inner, name, slot_inner, name);
        } else {
            codebuf_write(ctx->out,
                "PgySlot_%s %s = pgy_claim_%s();\n",
                slot_inner, name, slot_inner);
        }
        if (ann_type_name != NULL)
            register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "BoxArray") == 0) {
        const char *inner = "Int";
        if (ann_type_name != NULL && strncmp(ann_type_name, "Box<Array<", 10) == 0) {
            inner = ann_type_name + 10;
            const char *close = strstr(inner, ">>");
            static char inner_buf[64];
            size_t len = close != NULL ? (size_t)(close - inner) : strlen(inner);
            if (len >= sizeof(inner_buf))
                len = sizeof(inner_buf) - 1;
            memcpy(inner_buf, inner, len);
            inner_buf[len] = '\0';
            inner = inner_buf;
        }

        char *capacity = (init->data.call.arg_count > 0)
                         ? emit_expression(init->data.call.arguments[0], ctx)
                         : pergyra_strdup("0");
        char *allocator = (init->data.call.arg_count > 1)
                          ? emit_expression(init->data.call.arguments[1], ctx)
                          : pergyra_strdup("NULL");
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyBoxArray_%s %s = pgy_box_array_new_%s(%s, %s);\n",
            inner, name, inner, capacity, allocator);
        register_typed_var(ctx, name,
            ann_type_name != NULL ? ann_type_name : "Box<Array<Int>>");
        free(capacity);
        free(allocator);
        free(ann_type_name);
        return;
    }

    /* Detect Box<T> - Type inference from Box(value) */
    bool is_box = false;
    const char *box_inner = "Int";
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        if (strcmp(callee_name, "Box") == 0) {
            is_box = true;
            /* Infer type from annotation or argument */
            if (ann != NULL && ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                box_inner = ann->data.type.generic_args->params[0]->name;
            } else if (init->data.call.arg_count > 0) {
                /* Infer from argument type */
                ASTNode *arg = init->data.call.arguments[0];
                if (arg->type == AST_NUMBER) box_inner = "Int";
                else if (arg->type == AST_STRING) box_inner = "String";
                else if (arg->type == AST_BOOLEAN) box_inner = "Bool";
            }
        }
        /* Rc<T> - Reference counted box */
        else if (strcmp(callee_name, "Rc") == 0) {
            is_box = true;
            if (ann != NULL && ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                box_inner = ann->data.type.generic_args->params[0]->name;
            } else if (init->data.call.arg_count > 0) {
                ASTNode *arg = init->data.call.arguments[0];
                if (arg->type == AST_NUMBER) box_inner = "Int";
                else if (arg->type == AST_STRING) box_inner = "String";
                else if (arg->type == AST_BOOLEAN) box_inner = "Bool";
            }
        }
    }

    if (is_box) {
        write_indent(ctx);
        codebuf_write(ctx->out, "PgyBox_%s %s = pgy_box_new_%s(", 
                      box_inner, name, box_inner);
        if (init->data.call.arg_count > 0) {
            char *arg = emit_expression(init->data.call.arguments[0], ctx);
            codebuf_write(ctx->out, "%s", arg);
            free(arg);
        }
        codebuf_write(ctx->out, ");\n");
        register_typed_var(ctx, name,
            ann_type_name != NULL ? ann_type_name : "Box<Int>");
        free(ann_type_name);
        return;
    }

    if (ann_type_name != NULL && strncmp(ann_type_name, "Channel<", 8) == 0) {
        const char *inner = slot_inner_type_name(ann_type_name);
        char *capacity = pergyra_strdup("16");

        if (init != NULL && init->type == AST_CALL
            && init->data.call.arg_count > 0) {
            free(capacity);
            capacity = emit_expression(init->data.call.arguments[0], ctx);
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "PgyChannel_%s %s;\n", inner, name);
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_channel_init_%s(&%s, %s);\n",
            inner, name, capacity);
        register_typed_var(ctx, name, ann_type_name);
        free(capacity);
        free(ann_type_name);
        return;
    }

    /* Normal variable with type inference */
    const char *c_type = "int32_t"; /* fallback */
    if (ann != NULL) {
        c_type = pergyra_ast_type_to_c(ann);
    } else if (init != NULL) {
        /* Type inference from initializer */
        if (init->type == AST_NUMBER)  c_type = "int32_t";
        else if (init->type == AST_STRING)  c_type = "char*";
        else if (init->type == AST_BOOLEAN) c_type = "bool";
        else if (init->type == AST_SPAWN_EXPR) c_type = "PgyTaskHandle";
        else if (init->type == AST_CHANNEL_RECV) {
            c_type = pergyra_type_to_c(infer_expression_type_name(ctx, init));
        }
        else if (init->type == AST_CALL) {
            /* Infer from call return type - default to int for now */
            c_type = "int32_t";
        }
    }

    write_indent(ctx);
    if (init != NULL) {
        char *init_expr = emit_expression(init, ctx);
        codebuf_write(ctx->out, "%s %s = %s;\n", c_type, name, init_expr);
        free(init_expr);
    } else {
        codebuf_write(ctx->out, "%s %s = 0;\n", c_type, name);
    }

    if (ann_type_name != NULL) {
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
    } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        char *future_type = infer_spawn_return_type_name(ctx, init);
        char *wrapped = strdup_fmt("Future<%s>", future_type);
        register_typed_var(ctx, name, wrapped);
        free(future_type);
        free(wrapped);
    } else if (init != NULL && init->type == AST_CHANNEL_RECV) {
        const char *inner = infer_expression_type_name(ctx, init);
        register_typed_var(ctx, name, inner);
    }
}

/* -----------------------------------------------------------------
 * Function declaration emitter
 * ----------------------------------------------------------------- */

static void
emit_func_forward_decl(ASTNode *node, CodeBuf *buf)
{
    const char *name = node->data.func_decl.name;
    const char *ret_type = "void";
    if (node->data.func_decl.return_type != NULL)
        ret_type = pergyra_ast_type_to_c(node->data.func_decl.return_type);

    codebuf_write(buf, "%s %s(", ret_type, name);
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = "int32_t";
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (i > 0)
            codebuf_write(buf, ", ");
        codebuf_write(buf, "%s %s", pt, p->name);
    }
    codebuf_write(buf, ");\n");
}

void
emit_func_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.func_decl.name;

    const char *ret_type = "void";
    if (node->data.func_decl.return_type != NULL) {
        ret_type = pergyra_ast_type_to_c(node->data.func_decl.return_type);
    }

    codebuf_write(ctx->out, "\n%s\n%s(", ret_type, name);
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = "int32_t";
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (i > 0) codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s %s", pt, p->name);
    }
    codebuf_write(ctx->out, ")\n");
    codebuf_write(ctx->out, "{\n");

    ctx->indent++;
    if (node->data.func_decl.body != NULL)
        emit_block(node->data.func_decl.body, ctx);
    ctx->indent--;

    codebuf_write(ctx->out, "}\n");
}

void
emit_extern_block(ASTNode *node, TranspilerCtx *ctx)
{
    codebuf_write(ctx->out, "\n/* extern \"%s\" */\n",
                  node->data.extern_block.abi != NULL
                    ? node->data.extern_block.abi : "");

    for (size_t i = 0; i < node->data.extern_block.count; i++) {
        ASTNode *decl = node->data.extern_block.declarations[i];
        if (decl == NULL || decl->type != AST_FUNC_DECL)
            continue;

        const char *name = decl->data.func_decl.name;
        const char *ret_type = "void";
        if (decl->data.func_decl.return_type != NULL) {
            ret_type = pergyra_ast_type_to_c(decl->data.func_decl.return_type);
        }

        codebuf_write(ctx->out, "%s %s(", ret_type, name);

        for (size_t j = 0; j < decl->data.func_decl.param_count; j++) {
            FuncParam *p = decl->data.func_decl.params[j];
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            if (j > 0) {
                codebuf_write(ctx->out, ", ");
            }
            codebuf_write(ctx->out, "%s %s", pt, p->name);
        }

        codebuf_write(ctx->out, ");\n");
    }
}

/* -----------------------------------------------------------------
 * Class declaration emitter
 * ----------------------------------------------------------------- */

void
emit_class_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.class_decl.name;

    codebuf_write(ctx->out, "\ntypedef struct %s\n{\n", name);

    /* Fields */
    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *f = node->data.class_decl.fields[i];
        const char *ft = "int32_t";
        if (f->type != NULL)
            ft = pergyra_ast_type_to_c(f->type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, f->name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods become free functions: RetType ClassName_MethodName(...) */
    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type    = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");

        ctx->indent++;
        if (method->data.func_decl.body != NULL)
            emit_block(method->data.func_decl.body, ctx);
        ctx->indent--;

        codebuf_write(ctx->out, "}\n");
    }
}

/* -----------------------------------------------------------------
 * with slot<T> as s { }
 * ----------------------------------------------------------------- */

void
emit_with_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias = node->data.with_stmt.alias;
    bool is_secure    = node->data.with_stmt.is_secure;

    const char *inner = "Int";
    if (node->data.with_stmt.slot_type != NULL)
        inner = node->data.with_stmt.slot_type->data.type.name;

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    /* Register alias in slot variable table */
    register_slot_var(ctx, alias, inner, is_secure);

    if (is_secure) {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyToken_%s %s_token;\n", inner, alias);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
            inner, alias, inner, alias);
    } else {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySlot_%s %s = pgy_claim_%s();\n",
            inner, alias, inner);
    }

    if (node->data.with_stmt.body != NULL)
        emit_block(node->data.with_stmt.body, ctx);

    /* Auto-release */
    write_indent(ctx);
    if (is_secure) {
        codebuf_write(ctx->out,
            "pgy_secure_release_%s(&%s, &%s_token);\n",
            inner, alias, alias);
    } else {
        codebuf_write(ctx->out,
            "pgy_release_%s(&%s);\n", inner, alias);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

/* -----------------------------------------------------------------
 * Parallel block
 * ----------------------------------------------------------------- */

void
emit_parallel_block(ASTNode *node, TranspilerCtx *ctx)
{
    size_t count = node->data.parallel.task_count;
    if (count == 0)
        return;

    unsigned int pid = ctx->parallel_id++;

    /* ---------------------------------------------------------------
     * 1) Generate a context struct that holds pointers to all local
     *    variables currently in scope (slots + non-duplicate typed vars).
     *    Wrapper functions access outer variables through this struct.
     * --------------------------------------------------------------- */
    int n_slots  = ctx->slot_var_count;
    int n_typed  = ctx->typed_var_count;

    /* Helper: check if a typed_var is already in slot_vars (avoid dupes) */
    #define IS_SLOT_DUP(name_str) ({ \
        bool _dup = false; \
        for (int _j = 0; _j < n_slots; _j++) { \
            if (strcmp(ctx->slot_vars[_j].name, (name_str)) == 0) \
                { _dup = true; break; } \
        } _dup; })

    /* Count non-duplicate typed vars */
    int n_unique_typed = 0;
    for (int i = 0; i < n_typed; i++) {
        if (!IS_SLOT_DUP(ctx->typed_vars[i].name))
            n_unique_typed++;
    }

    bool has_captures = (n_slots > 0 || n_unique_typed > 0);

    if (has_captures) {
        codebuf_write(ctx->helpers,
            "typedef struct {\n");
        for (int i = 0; i < n_slots; i++) {
            codebuf_write(ctx->helpers,
                "    PgySlot_%s *%s;\n",
                ctx->slot_vars[i].inner_type,
                ctx->slot_vars[i].name);
        }
        for (int i = 0; i < n_typed; i++) {
            if (IS_SLOT_DUP(ctx->typed_vars[i].name))
                continue;
            const char *c_type = pergyra_type_to_c(ctx->typed_vars[i].type_name);
            codebuf_write(ctx->helpers,
                "    %s *%s;\n", c_type, ctx->typed_vars[i].name);
        }
        codebuf_write(ctx->helpers,
            "} _pgy_par_ctx_%u;\n\n", pid);
    }

    /* ---------------------------------------------------------------
     * 2) Generate static wrapper functions for each task.
     *    Variable references inside the wrapper go through _pctx->.
     * --------------------------------------------------------------- */
    for (size_t i = 0; i < count; i++) {
        codebuf_write(ctx->helpers,
            "static void *_pgy_par_%zu_%u(void *_arg) {\n",
            i, pid);
        if (has_captures) {
            codebuf_write(ctx->helpers,
                "    _pgy_par_ctx_%u *_pctx = "
                "(_pgy_par_ctx_%u *)_arg;\n",
                pid, pid);
        } else {
            codebuf_write(ctx->helpers, "    (void)_arg;\n");
        }

        /* Redirect output to helpers and set parallel-capture mode */
        CodeBuf *saved = ctx->out;
        int saved_indent = ctx->indent;
        bool saved_in_pw = ctx->in_parallel_wrapper;
        int saved_slot_end  = ctx->par_capture_slot_end;
        int saved_typed_end = ctx->par_capture_typed_end;

        ctx->out = ctx->helpers;
        ctx->indent = 1;
        ctx->in_parallel_wrapper  = true;
        ctx->par_capture_slot_end  = n_slots;
        ctx->par_capture_typed_end = n_typed;

        emit_statement(node->data.parallel.tasks[i], ctx);

        ctx->out = saved;
        ctx->indent = saved_indent;
        ctx->in_parallel_wrapper  = saved_in_pw;
        ctx->par_capture_slot_end  = saved_slot_end;
        ctx->par_capture_typed_end = saved_typed_end;

        codebuf_write(ctx->helpers,
            "    return NULL;\n"
            "}\n\n");
    }

    /* ---------------------------------------------------------------
     * 3) Emit context initialization + spawn + await at call site.
     * --------------------------------------------------------------- */
    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    if (has_captures) {
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_par_ctx_%u _pctx%u = { ", pid, pid);
        bool first = true;
        for (int i = 0; i < n_slots; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", ctx->slot_vars[i].name);
            first = false;
        }
        for (int i = 0; i < n_typed; i++) {
            if (IS_SLOT_DUP(ctx->typed_vars[i].name))
                continue;
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", ctx->typed_vars[i].name);
            first = false;
        }
        codebuf_write(ctx->out, " };\n");
    }

    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        if (has_captures) {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_spawn(_pgy_par_%zu_%u, &_pctx%u);\n",
                i, i, pid, pid);
        } else {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_spawn(_pgy_par_%zu_%u, NULL);\n",
                i, i, pid);
        }
    }
    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_await(_ph_%zu);\n", i);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    #undef IS_SLOT_DUP
}

/* -----------------------------------------------------------------
 * Control flow
 * ----------------------------------------------------------------- */

void
emit_if_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(node->data.if_stmt.condition, ctx);
    write_indent(ctx);
    codebuf_write(ctx->out, "if (%s)\n", cond);
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.if_stmt.then_branch != NULL)
        emit_block(node->data.if_stmt.then_branch, ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    if (node->data.if_stmt.else_branch != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_statement(node->data.if_stmt.else_branch, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_for_loop(ASTNode *node, TranspilerCtx *ctx)
{
    char *start = emit_expression(node->data.for_loop.range_start, ctx);
    char *end   = emit_expression(node->data.for_loop.range_end,   ctx);
    const char *var = node->data.for_loop.variable;

    write_indent(ctx);
    codebuf_write(ctx->out,
        "for (int32_t %s = %s; %s < %s; %s++)\n",
        var, start, var, end, var);
    free(start);
    free(end);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.for_loop.body != NULL)
        emit_block(node->data.for_loop.body, ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

void
emit_while_loop(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(node->data.while_loop.condition, ctx);
    write_indent(ctx);
    codebuf_write(ctx->out, "while (%s)\n", cond);
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.while_loop.body != NULL)
        emit_block(node->data.while_loop.body, ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

void
emit_match_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    char *subj = emit_expression(node->data.match_stmt.subject, ctx);
    int tmp_id = ctx->tmp_counter++;

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "int32_t __match_%d = %s;\n", tmp_id, subj);
    free(subj);

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        char *pat = emit_expression(mc->data.match_case.pattern, ctx);

        write_indent(ctx);
        if (i == 0)
            codebuf_write(ctx->out, "if (__match_%d == %s", tmp_id, pat);
        else
            codebuf_write(ctx->out, "else if (__match_%d == %s", tmp_id, pat);
        free(pat);

        if (mc->data.match_case.guard != NULL) {
            char *guard = emit_expression(mc->data.match_case.guard, ctx);
            codebuf_write(ctx->out, " && %s", guard);
            free(guard);
        }
        codebuf_write(ctx->out, ")\n");

        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_block(mc->data.match_case.body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    if (node->data.match_stmt.default_body != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_block(node->data.match_stmt.default_body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

void
emit_return_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    write_indent(ctx);
    if (node->data.return_stmt.value != NULL) {
        char *val = emit_expression(node->data.return_stmt.value, ctx);
        codebuf_write(ctx->out, "return %s;\n", val);
        free(val);
    } else {
        codebuf_write(ctx->out, "return;\n");
    }
}

/* -----------------------------------------------------------------
 * Statement dispatcher
 * ----------------------------------------------------------------- */

void
emit_statement(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_LET_DECL:
        emit_let_decl(node, ctx);
        break;
    case AST_FUNC_DECL:
        emit_func_decl(node, ctx);
        break;
    case AST_CLASS_DECL:
        emit_class_decl(node, ctx);
        break;
    case AST_EXTERN_BLOCK:
        emit_extern_block(node, ctx);
        break;
    case AST_ABILITY_DECL:
        emit_ability_decl(node, ctx);
        break;
    case AST_ROLE_DECL:
        emit_role_decl(node, ctx);
        break;
    case AST_PARTY_DECL:
        emit_party_decl(node, ctx);
        break;
    case AST_SYSTEMIC_DECL:
        emit_systemic_decl(node, ctx);
        break;
    case AST_WORLD_DECL:
        emit_world_decl(node, ctx);
        break;
    case AST_EVENT_DECL:
        emit_event_decl(node, ctx);
        break;
    case AST_EVENT_SUBSCRIBE:
        emit_event_subscribe(node, ctx);
        break;
    case AST_EVENT_UNSUBSCRIBE:
        emit_event_unsubscribe(node, ctx);
        break;
    case AST_IF_STMT:
        emit_if_stmt(node, ctx);
        break;
    case AST_FOR_LOOP:
        emit_for_loop(node, ctx);
        break;
    case AST_WHILE_LOOP:
        emit_while_loop(node, ctx);
        break;
    case AST_MATCH_STMT:
        emit_match_stmt(node, ctx);
        break;
    case AST_RETURN:
        emit_return_stmt(node, ctx);
        break;
    case AST_WITH_STMT:
        emit_with_stmt(node, ctx);
        break;
    case AST_PARALLEL_BLOCK:
        emit_parallel_block(node, ctx);
        break;
    case AST_BLOCK:
        emit_block(node, ctx);
        break;
    case AST_LAMBDA_EXPR:
        {
            char *expr = emit_expression(node, ctx);
            if (expr != NULL && expr[0] != '\0') {
                write_indent(ctx);
                codebuf_write(ctx->out, "%s;\n", expr);
            }
            free(expr);
            break;
        }
    case AST_ACTOR_DECL:
        emit_actor_decl(node, ctx);
        break;
    case AST_SELECT_STMT:
        emit_select_stmt(node, ctx);
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++)
            emit_statement(node->data.async_block.statements[i], ctx);
        break;
    default: {
        /* Expression statement (including event invoke) */
        char *expr = emit_expression(node, ctx);
        if (expr != NULL && expr[0] != '\0') {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s;\n", expr);
        }
        free(expr);
        break;
    }
    }
}

void
emit_block(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    if (node->type == AST_BLOCK) {
        for (size_t i = 0; i < node->data.block.count; i++)
            emit_statement(node->data.block.statements[i], ctx);
    } else {
        emit_statement(node, ctx);
    }
}

/* -----------------------------------------------------------------
 * Program emitter
 * ----------------------------------------------------------------- */

void
emit_program(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL || node->type != AST_PROGRAM)
        return;

    ctx->program = node;

    /* File header */
    codebuf_write(ctx->out,
        "/*\n"
        " * Generated by the Pergyra compiler C backend\n"
        " * Do not edit manually.\n"
        " */\n"
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include \"pgy_runtime.h\"\n"
        "#include \"pgy_parallel.h\"\n"
        "#include \"pgy_channel.h\"\n\n");

    /*
     * Multi-pass strategy for valid C output:
     *   Pass 1 — ability declarations (vtable typedefs)
     *   Pass 2 — class declarations (type completeness)
     *   Pass 3 — role declarations (vtable instances + methods)
     *   Pass 4 — function declarations (file scope)
     *   Pass 5 — remaining top-level statements → wrapped in main()
     */

    /* Pass 1: abilities (vtable typedefs) */
    for (size_t i = 0; i < node->data.program.count; i++) {
        ASTNode *stmt = node->data.program.statements[i];
        if (stmt->type == AST_ABILITY_DECL)
            emit_ability_decl(stmt, ctx);
    }

    /* Pass 2: classes */
    for (size_t i = 0; i < node->data.program.count; i++) {
        ASTNode *stmt = node->data.program.statements[i];
        if (stmt->type == AST_CLASS_DECL)
            emit_class_decl(stmt, ctx);
    }

    /* Pass 2.5: extern declarations */
    for (size_t i = 0; i < node->data.program.count; i++) {
        ASTNode *stmt = node->data.program.statements[i];
        if (stmt->type == AST_EXTERN_BLOCK)
            emit_extern_block(stmt, ctx);
    }

    /* Pass 3: roles (vtable instances + free functions) */
    for (size_t i = 0; i < node->data.program.count; i++) {
        ASTNode *stmt = node->data.program.statements[i];
        if (stmt->type == AST_ROLE_DECL)
            emit_role_decl(stmt, ctx);
    }

    /* Pass 3.5: parties (struct + methods) */
    for (size_t i = 0; i < node->data.program.count; i++) {
        ASTNode *stmt = node->data.program.statements[i];
        if (stmt->type == AST_PARTY_DECL)
            emit_party_decl(stmt, ctx);
    }

    /* Pass 3.7: systemics (struct + methods) */
    for (size_t i = 0; i < node->data.program.count; i++) {
        ASTNode *stmt = node->data.program.statements[i];
        if (stmt->type == AST_SYSTEMIC_DECL)
            emit_systemic_decl(stmt, ctx);
    }

    /* Pass 3.9: worlds (struct + methods) */
    for (size_t i = 0; i < node->data.program.count; i++) {
        ASTNode *stmt = node->data.program.statements[i];
        if (stmt->type == AST_WORLD_DECL)
            emit_world_decl(stmt, ctx);
    }

    /* Pass 3.95: actors (struct + methods) */
    for (size_t i = 0; i < node->data.program.count; i++) {
        ASTNode *stmt = node->data.program.statements[i];
        if (stmt->type == AST_ACTOR_DECL)
            emit_actor_decl(stmt, ctx);
    }

    for (size_t i = 0; i < node->data.program.count; i++) {
        ASTNode *stmt = node->data.program.statements[i];
        if (stmt->type == AST_FUNC_DECL)
            emit_func_forward_decl(stmt, ctx->decls);
    }
    if (ctx->decls->len > 0) {
        codebuf_write(ctx->out, "\n");
        codebuf_write_raw(ctx->out, ctx->decls->data, ctx->decls->len);
    }

    /* Pass 4: functions — emit in two sub-passes so that helpers
     * (parallel context structs, wrapper functions) generated during
     * function emission are available.  First pass: emit all functions
     * into a temporary buffer. */
    {
        CodeBuf *func_buf = codebuf_create();
        CodeBuf *saved_out = ctx->out;
        ctx->out = func_buf;
        for (size_t i = 0; i < node->data.program.count; i++) {
            ASTNode *stmt = node->data.program.statements[i];
            if (stmt->type == AST_FUNC_DECL)
                emit_func_decl(stmt, ctx);
        }
        ctx->out = saved_out;

        /* Emit helpers (parallel context structs + wrappers) first */
        if (ctx->helpers->len > 0) {
            codebuf_write(ctx->out, "\n");
            codebuf_write_raw(ctx->out, ctx->helpers->data, ctx->helpers->len);
        }

        /* Then emit the function bodies */
        if (func_buf->len > 0) {
            codebuf_write_raw(ctx->out, func_buf->data, func_buf->len);
        }
        codebuf_destroy(func_buf);
    }

    /* Check if a Main() function exists */
    bool has_main_func = false;
    for (size_t i = 0; i < node->data.program.count; i++) {
        ASTNode *stmt = node->data.program.statements[i];
        if (stmt->type == AST_FUNC_DECL
            && strcmp(stmt->data.func_decl.name, "Main") == 0) {
            has_main_func = true;
            break;
        }
    }

    /* Collect top-level statements (non-class, non-func) */
    bool has_toplevel = false;
    for (size_t i = 0; i < node->data.program.count; i++) {
        ASTNode *stmt = node->data.program.statements[i];
        if (stmt->type != AST_CLASS_DECL && stmt->type != AST_FUNC_DECL
                && stmt->type != AST_EXTERN_BLOCK
                && stmt->type != AST_ABILITY_DECL && stmt->type != AST_ROLE_DECL
                && stmt->type != AST_PARTY_DECL
                && stmt->type != AST_SYSTEMIC_DECL
                && stmt->type != AST_WORLD_DECL
                && stmt->type != AST_ACTOR_DECL) {
            has_toplevel = true;
            break;
        }
    }

    /* Generate int main(void) { ... } */
    if (has_toplevel || has_main_func) {
        codebuf_write(ctx->out, "\nint\nmain(void)\n{\n");
        ctx->indent++;

        /* Initialize runtime */
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_pool_init(0);\n\n");

        /* Emit top-level statements inside main() */
        for (size_t i = 0; i < node->data.program.count; i++) {
            ASTNode *stmt = node->data.program.statements[i];
            if (stmt->type != AST_CLASS_DECL && stmt->type != AST_FUNC_DECL
                && stmt->type != AST_EXTERN_BLOCK
                && stmt->type != AST_ABILITY_DECL && stmt->type != AST_ROLE_DECL
                && stmt->type != AST_PARTY_DECL
                && stmt->type != AST_SYSTEMIC_DECL
                && stmt->type != AST_WORLD_DECL
                && stmt->type != AST_ACTOR_DECL)
                emit_statement(stmt, ctx);
        }

        /* If Main() exists and no top-level statements, call it */
        if (has_main_func) {
            write_indent(ctx);
            codebuf_write(ctx->out, "Main();\n");
        }

        /* Shutdown runtime */
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_pool_shutdown();\n");

        write_indent(ctx);
        codebuf_write(ctx->out, "return 0;\n");
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

/* -----------------------------------------------------------------
 * Main entry point
 * ----------------------------------------------------------------- */

TranspileResult *
transpile(ASTNode *ast, const char *output_path)
{
    TranspileResult *result = calloc(1, sizeof(TranspileResult));
    if (result == NULL)
        return NULL;

    TranspilerCtx *ctx = transpiler_ctx_create();
    if (ctx == NULL) {
        result->success       = false;
        result->error_message = pergyra_strdup("Out of memory");
        return result;
    }

    emit_program(ast, ctx);

    if (output_path != NULL) {
        if (!codebuf_dump_file(ctx->out, output_path)) {
            result->success       = false;
            result->error_message = strdup_fmt(
                "Cannot write output file: %s", output_path);
            transpiler_ctx_destroy(ctx);
            return result;
        }
    }

    result->success = true;
    transpiler_ctx_destroy(ctx);
    return result;
}

void
transpile_result_destroy(TranspileResult *res)
{
    if (res == NULL)
        return;
    free(res->error_message);
    free(res);
}

/* =================================================================
 * Role/Ability system emitters
 * ================================================================= */

static void
emit_role_method_impl(const char *role_name, ASTNode *method, TranspilerCtx *ctx)
{
    const char *method_name = method->data.func_decl.name;
    const char *ret_type = "void";
    if (method->data.func_decl.return_type != NULL)
        ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

    codebuf_write(ctx->out, "\nstatic %s\n%s_%s(void *self",
                  ret_type, role_name, method_name);

    for (size_t k = 0; k < method->data.func_decl.param_count; k++) {
        FuncParam *p = method->data.func_decl.params[k];
        const char *pt = "int32_t";
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        codebuf_write(ctx->out, ", %s %s", pt, p->name);
    }
    codebuf_write(ctx->out, ")\n{\n");

    ctx->indent++;
    if (method->data.func_decl.body != NULL)
        emit_block(method->data.func_decl.body, ctx);
    ctx->indent--;

    codebuf_write(ctx->out, "}\n");
}

static void
emit_role_vtable_instance(const char *role_name, ASTNode *impl, TranspilerCtx *ctx)
{
    const char *ability_name = impl->data.impl_ability.ability_name;
    if (ability_name == NULL || impl->data.impl_ability.method_count == 0)
        return;

    codebuf_write(ctx->out,
        "\nstatic const %s_vtable %s_%s_vtable_instance = {\n",
        ability_name, role_name, ability_name);

    for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
        ASTNode *method = impl->data.impl_ability.methods[j];
        if (method->type != AST_FUNC_DECL)
            continue;
        codebuf_write(ctx->out, "    .%s = %s_%s,\n",
                      method->data.func_decl.name,
                      role_name, method->data.func_decl.name);
    }

    codebuf_write(ctx->out, "};\n");
}

static void
emit_included_role_impls(ASTNode *role, TranspilerCtx *ctx)
{
    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *include_stmt = role->data.role_decl.includes[i];
        ASTNode *included_role = find_role_decl(ctx, include_stmt->data.include_stmt.role_name);

        if (included_role == NULL) {
            codebuf_write(ctx->out, "/* unresolved include role: %s */\n",
                          include_stmt->data.include_stmt.role_name);
            continue;
        }

        for (size_t j = 0; j < included_role->data.role_decl.impl_count; j++) {
            ASTNode *impl = included_role->data.role_decl.impl_abilities[j];
            if (impl->type != AST_IMPL_ABILITY)
                continue;

            if (role_has_ability(role, impl->data.impl_ability.ability_name))
                continue;

            for (size_t k = 0; k < impl->data.impl_ability.method_count; k++) {
                ASTNode *method = impl->data.impl_ability.methods[k];
                if (method->type != AST_FUNC_DECL)
                    continue;
                if (role_has_method(role, method->data.func_decl.name))
                    continue;
                emit_role_method_impl(role->data.role_decl.name, method, ctx);
            }

            emit_role_vtable_instance(role->data.role_decl.name, impl, ctx);
        }
    }
}

/*
 * Ability → vtable struct typedef
 *
 *   typedef struct {
 *       RetType (*MethodName)(void* self, ParamType p1, ...);
 *       ...
 *   } AbilityName_vtable;
 */
void
emit_ability_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.ability_decl.name;

    codebuf_write(ctx->out, "\n/* Ability: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct\n{\n");

    for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
        ASTNode *method = node->data.ability_decl.methods[i];
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "    %s (*%s)(void *self", ret_type, method_name);

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ");\n");
    }

    codebuf_write(ctx->out, "} %s_vtable;\n", name);
}

/*
 * Role → vtable instance + free functions
 *
 *   static RetType RoleName_MethodName(void* self, ...) { body }
 *   static const AbilityName_vtable RoleName_AbilityName_vtable_instance = {
 *       .MethodName = RoleName_MethodName,
 *   };
 */
void
emit_role_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.role_decl.name;

    codebuf_write(ctx->out, "\n/* Role: %s */\n", name);
    emit_included_role_impls(node, ctx);

    for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
        ASTNode *impl = node->data.role_decl.impl_abilities[i];

        if (impl->type == AST_IMPL_ABILITY) {
            for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method->type != AST_FUNC_DECL)
                    continue;
                emit_role_method_impl(name, method, ctx);
            }

            emit_role_vtable_instance(name, impl, ctx);

        } else if (impl->type == AST_OVERRIDE_FUNC) {
            ASTNode *func = impl->data.override_func.func_decl;
            if (func == NULL || func->type != AST_FUNC_DECL)
                continue;

            const char *method_name = func->data.func_decl.name;
            const char *ret_type = "void";
            if (func->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(func->data.func_decl.return_type);

            codebuf_write(ctx->out, "\nstatic %s\n%s_%s(void *self",
                          ret_type, name, method_name);

            for (size_t k = 0; k < func->data.func_decl.param_count; k++) {
                FuncParam *p = func->data.func_decl.params[k];
                const char *pt = "int32_t";
                if (p->type != NULL)
                    pt = pergyra_ast_type_to_c(p->type);
                codebuf_write(ctx->out, ", %s %s", pt, p->name);
            }
            codebuf_write(ctx->out, ")\n{\n");

            ctx->indent++;
            if (func->data.func_decl.body != NULL)
                emit_block(func->data.func_decl.body, ctx);
            ctx->indent--;

            codebuf_write(ctx->out, "}\n");
        }
    }
}

/* =================================================================
 * Party system emitters
 * ================================================================= */

/*
 * Party → C struct with role slot pointers + shared fields
 * Party methods → free functions
 */
void
emit_party_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.party_decl.name;

    codebuf_write(ctx->out, "\n/* Party: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Role slots as void* + vtable pointer */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];
        const char *slot_name = rs->data.role_slot.slot_name;
        codebuf_write(ctx->out, "    void *%s;\n", slot_name);
        /* Emit vtable pointers for each required ability */
        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab = rs->data.role_slot.required_abilities[j];
            if (ab != NULL && ab->data.type.name != NULL) {
                codebuf_write(ctx->out,
                    "    const %s_vtable *%s_%s_vt;\n",
                    ab->data.type.name, slot_name, ab->data.type.name);
            }
        }
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
        ASTNode *shared = node->data.party_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods as free functions */
    for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
        ASTNode *method = node->data.party_decl.methods[i];
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");

        ctx->indent++;
        if (method->data.func_decl.body != NULL)
            emit_block(method->data.func_decl.body, ctx);
        ctx->indent--;

        codebuf_write(ctx->out, "}\n");
    }
}

/* =================================================================
 * Systemic/World system emitters
 * ================================================================= */

void
emit_systemic_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.systemic_decl.name;

    codebuf_write(ctx->out, "\n/* Systemic: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Party slots */
    for (size_t i = 0; i < node->data.systemic_decl.party_count; i++) {
        ASTNode *ps = node->data.systemic_decl.party_slots[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            ps->data.systemic_slot.party_type,
            ps->data.systemic_slot.slot_name);
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.systemic_decl.shared_count; i++) {
        ASTNode *shared = node->data.systemic_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods */
    for (size_t i = 0; i < node->data.systemic_decl.method_count; i++) {
        ASTNode *method = node->data.systemic_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        ctx->indent++;
        if (method->data.func_decl.body != NULL)
            emit_block(method->data.func_decl.body, ctx);
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_world_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.world_decl.name;

    codebuf_write(ctx->out, "\n/* World: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Systemic instances */
    for (size_t i = 0; i < node->data.world_decl.systemic_count; i++) {
        ASTNode *ws = node->data.world_decl.systemics[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            ws->data.world_systemic.systemic_type,
            ws->data.world_systemic.slot_name);
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.world_decl.shared_count; i++) {
        ASTNode *shared = node->data.world_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods */
    for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
        ASTNode *method = node->data.world_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        ctx->indent++;
        if (method->data.func_decl.body != NULL)
            emit_block(method->data.func_decl.body, ctx);
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

/* =================================================================
 * Async system emitters
 * ================================================================= */

void
emit_actor_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.actor_decl.name;

    codebuf_write(ctx->out, "\n/* Actor: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Fields */
    for (size_t i = 0; i < node->data.actor_decl.field_count; i++) {
        ClassField *f = node->data.actor_decl.fields[i];
        const char *ft = "int32_t";
        if (f->type != NULL)
            ft = pergyra_ast_type_to_c(f->type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, f->name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods (actor methods are emitted as free functions) */
    for (size_t i = 0; i < node->data.actor_decl.method_count; i++) {
        ASTNode *method = node->data.actor_decl.methods[i];
        if (method == NULL) continue;

        const char *method_name = method->data.async_func_decl.name;
        const char *ret_type = "void";
        if (method->data.async_func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.async_func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.async_func_decl.param_count; j++) {
            FuncParam *p = method->data.async_func_decl.params[j];
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        ctx->indent++;
        if (method->data.async_func_decl.body != NULL)
            emit_block(method->data.async_func_decl.body, ctx);
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_select_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    codebuf_write(ctx->out, "\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "/* select */\n");

    /* MVP: emit cases as sequential if-else chain checking channel readiness */
    for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
        ASTNode *c = node->data.select_stmt.cases[i];
        write_indent(ctx);
        if (i == 0)
            codebuf_write(ctx->out, "if (1) { /* select case %zu */\n", i);
        else
            codebuf_write(ctx->out, "} else if (1) { /* select case %zu */\n", i);
        ctx->indent++;
        if (c) emit_statement(c, ctx);
        ctx->indent--;
    }

    if (node->data.select_stmt.default_case) {
        write_indent(ctx);
        codebuf_write(ctx->out, "} else { /* default */\n");
        ctx->indent++;
        emit_statement(node->data.select_stmt.default_case, ctx);
        ctx->indent--;
    }

    if (node->data.select_stmt.case_count > 0 || node->data.select_stmt.default_case) {
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}

/* =================================================================
 * Event system emitters
 * ================================================================= */

void
emit_event_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.event_decl.name;
    
    /* Generate event handler type typedef */
    codebuf_write(ctx->out, "\n/* Event: %s */\n", name);
    
    /* Build parameter types string */
    codebuf_write(ctx->out, "typedef void (*%s_Handler)(", name);
    
    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        ASTNode *param = node->data.event_decl.params[i];
        const char *pt = "void*";
        if (param->data.let_decl.type != NULL) {
            pt = pergyra_ast_type_to_c(param->data.let_decl.type);
        }
        if (i > 0) codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s %s", pt, param->data.let_decl.name);
    }
    
    codebuf_write(ctx->out, ");\n");
    
    /* Generate event struct with handler array */
    codebuf_write(ctx->out, "typedef struct {\n");
    codebuf_write(ctx->out, "    %s_Handler handlers[PGY_EVENT_MAX_HANDLERS];\n", name);
    codebuf_write(ctx->out, "    void* contexts[PGY_EVENT_MAX_HANDLERS];\n");
    codebuf_write(ctx->out, "    size_t count;\n");
    codebuf_write(ctx->out, "    bool is_invoking;\n");
    codebuf_write(ctx->out, "    bool pending_changes;\n");
    codebuf_write(ctx->out, "} %s;\n", name);
    
    /* Generate inline init function */
    codebuf_write(ctx->out, "static inline void %s_INIT(%s* e) {\n", name, name);
    codebuf_write(ctx->out, "    memset(e, 0, sizeof(*e));\n");
    codebuf_write(ctx->out, "}\n");
    
    /* Generate subscribe function */
    codebuf_write(ctx->out, "static inline void %s_SUBSCRIBE(%s* e, %s_Handler h) {\n", name, name, name);
    codebuf_write(ctx->out, "    if (e->count < PGY_EVENT_MAX_HANDLERS) {\n");
    codebuf_write(ctx->out, "        e->handlers[e->count++] = h;\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "}\n");
    
    /* Generate unsubscribe function */
    codebuf_write(ctx->out, "static inline void %s_UNSUBSCRIBE(%s* e, %s_Handler h) {\n", name, name, name);
    codebuf_write(ctx->out, "    for (size_t i = 0; i < e->count; i++) {\n");
    codebuf_write(ctx->out, "        if (e->handlers[i] == h) {\n");
    codebuf_write(ctx->out, "            for (size_t j = i; j < e->count - 1; j++) {\n");
    codebuf_write(ctx->out, "                e->handlers[j] = e->handlers[j + 1];\n");
    codebuf_write(ctx->out, "            }\n");
    codebuf_write(ctx->out, "            e->count--;\n");
    codebuf_write(ctx->out, "            break;\n");
    codebuf_write(ctx->out, "        }\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "}\n");
    
    /* Generate invoke function */
    codebuf_write(ctx->out, "static inline void %s_INVOKE(%s* e", name, name);
    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        ASTNode *param = node->data.event_decl.params[i];
        const char *pt = "void*";
        if (param->data.let_decl.type != NULL) {
            pt = pergyra_ast_type_to_c(param->data.let_decl.type);
        }
        codebuf_write(ctx->out, ", %s %s", pt, param->data.let_decl.name);
    }
    codebuf_write(ctx->out, ") {\n");
    codebuf_write(ctx->out, "    e->is_invoking = true;\n");
    codebuf_write(ctx->out, "    for (size_t i = 0; i < e->count; i++) {\n");
    codebuf_write(ctx->out, "        e->handlers[i](");
    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        if (i > 0) codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s", node->data.event_decl.params[i]->data.let_decl.name);
    }
    codebuf_write(ctx->out, ");\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "    e->is_invoking = false;\n");
    codebuf_write(ctx->out, "}\n");
}

void
emit_event_subscribe(ASTNode *node, TranspilerCtx *ctx)
{
    char *event_expr = emit_expression(node->data.event_op.event, ctx);
    char *handler_expr = emit_expression(node->data.event_op.handler, ctx);
    
    write_indent(ctx);
    codebuf_write(ctx->out, "%s_SUBSCRIBE(&%s, %s);\n", 
                  event_expr, event_expr, handler_expr);
    
    free(event_expr);
    free(handler_expr);
}

void
emit_event_unsubscribe(ASTNode *node, TranspilerCtx *ctx)
{
    char *event_expr = emit_expression(node->data.event_op.event, ctx);
    char *handler_expr = emit_expression(node->data.event_op.handler, ctx);
    
    write_indent(ctx);
    codebuf_write(ctx->out, "%s_UNSUBSCRIBE(&%s, %s);\n", 
                  event_expr, event_expr, handler_expr);
    
    free(event_expr);
    free(handler_expr);
}

void
emit_event_invoke(ASTNode *node, TranspilerCtx *ctx)
{
    /* Event invoke is handled in emit_call for function-style invocation */
    (void)node;
    (void)ctx;
}

char *
emit_lambda_expr(ASTNode *node, TranspilerCtx *ctx)
{
    int lambda_id = ++ctx->tmp_counter;
    const char *return_type = "int32_t";

    if (node->data.lambda_expr.return_type != NULL) {
        return_type = pergyra_ast_type_to_c(node->data.lambda_expr.return_type);
    } else if (node->data.lambda_expr.body != NULL
               && node->data.lambda_expr.body->type == AST_BLOCK) {
        return_type = "void";
    }

    char *lambda_name = strdup_fmt("pgy_lambda_%d", lambda_id);

    codebuf_write(ctx->decls, "\nstatic %s %s(",
                  return_type, lambda_name);
    for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
        ASTNode *param = node->data.lambda_expr.params[i];
        if (i > 0)
            codebuf_write(ctx->decls, ", ");
        codebuf_write(ctx->decls, "int32_t %s", param->data.identifier.name);
    }
    codebuf_write(ctx->decls, ");\n");

    codebuf_write(ctx->helpers, "\nstatic %s %s(",
                  return_type, lambda_name);
    for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
        ASTNode *param = node->data.lambda_expr.params[i];
        if (i > 0)
            codebuf_write(ctx->helpers, ", ");
        codebuf_write(ctx->helpers, "int32_t %s", param->data.identifier.name);
    }
    codebuf_write(ctx->helpers, ")\n{\n");

    if (node->data.lambda_expr.body != NULL
        && node->data.lambda_expr.body->type == AST_BLOCK) {
        CodeBuf *saved_out = ctx->out;
        int saved_indent = ctx->indent;
        ctx->out = ctx->helpers;
        ctx->indent = 1;
        emit_block(node->data.lambda_expr.body, ctx);
        ctx->indent = saved_indent;
        ctx->out = saved_out;
    } else if (node->data.lambda_expr.body != NULL) {
        char *expr = emit_expression(node->data.lambda_expr.body, ctx);
        write_indent_to(ctx->helpers, 1);
        codebuf_write(ctx->helpers, "return %s;\n", expr);
        free(expr);
    }

    codebuf_write(ctx->helpers, "}\n");
    return lambda_name;
}

void
emit_include_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *included_role = node->data.include_stmt.role_name;

    codebuf_write(ctx->out, "/* include %s */\n", included_role);
    if (find_role_decl(ctx, included_role) == NULL) {
        codebuf_write(ctx->out, "/* unresolved include %s */\n", included_role);
    }
}

void
emit_impl_ability(ASTNode *node, TranspilerCtx *ctx)
{
    const char *ability_name = node->data.impl_ability.ability_name;
    
    codebuf_write(ctx->out, "/* Impl ability: %s */\n", ability_name);
    
    /* This is handled within emit_role_decl */
    (void)ctx;
}

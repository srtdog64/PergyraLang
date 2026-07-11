#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"

static Type *
nominal_builtin_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static bool
type_is_rc_weak_beta_payload(Type *type)
{
    return type_equals(type, TYPE_INT)
        || type_equals(type, TYPE_LONG)
        || type_equals(type, TYPE_FLOAT)
        || type_equals(type, TYPE_DOUBLE)
        || type_equals(type, TYPE_BOOL)
        || type_equals(type, TYPE_STRING);
}

static bool
require_rc_weak_beta_payload(ASTNode *node, SemanticContext *ctx,
                             const char *builtin_name, Type *payload)
{
    if (payload == NULL || payload == TYPE_UNKNOWN)
        return false;
    if (type_is_rc_weak_beta_payload(payload))
        return true;

    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
        PGY_FIX_MATCH_BUILTIN_SIGNATURE,
        node,
        "%s beta-stable shared ownership supports only Int, Long, Float, Double, Bool, or String payloads; got '%s'",
        builtin_name,
        type_name_or_unknown(payload));
    return false;
}

static Type *
type_check_rc_new(ASTNode *call, SemanticContext *ctx)
{
    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcNew requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *payload = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (!require_rc_weak_beta_payload(call, ctx, "RcNew", payload))
        return TYPE_UNKNOWN;
    return wrap_constructed(TYPE_RC, payload);
}

static Type *
type_check_rc_clone(ASTNode *call, SemanticContext *ctx)
{
    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcClone requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcClone requires Rc<T>, got '%s'", type_name_or_unknown(rc_type));
        return TYPE_UNKNOWN;
    }
    if (!require_rc_weak_beta_payload(call, ctx, "RcClone",
            type_get_constructed_arg(rc_type, 0)))
        return TYPE_UNKNOWN;
    return rc_type;
}

static Type *
type_check_rc_get(ASTNode *call, SemanticContext *ctx)
{
    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcGet requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcGet requires Rc<T>, got '%s'", type_name_or_unknown(rc_type));
        return TYPE_UNKNOWN;
    }
    Type *payload = nominal_builtin_normalize_type(
        type_get_constructed_arg(rc_type, 0));
    if (!require_rc_weak_beta_payload(call, ctx, "RcGet", payload))
        return TYPE_UNKNOWN;
    return payload;
}

static Type *
type_check_rc_downgrade(ASTNode *call, SemanticContext *ctx)
{
    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcDowngrade requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcDowngrade requires Rc<T>, got '%s'", type_name_or_unknown(rc_type));
        return TYPE_UNKNOWN;
    }
    Type *payload = nominal_builtin_normalize_type(
        type_get_constructed_arg(rc_type, 0));
    if (!require_rc_weak_beta_payload(call, ctx, "RcDowngrade", payload))
        return TYPE_UNKNOWN;
    return wrap_constructed(TYPE_WEAK, payload);
}

static Type *
type_check_weak_upgrade(ASTNode *call, SemanticContext *ctx)
{
    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "WeakUpgrade requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *weak_type = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (!type_is_constructed_named(weak_type, "Weak")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "WeakUpgrade requires Weak<T>, got '%s'",
            type_name_or_unknown(weak_type));
        return TYPE_UNKNOWN;
    }
    Type *payload = nominal_builtin_normalize_type(
        type_get_constructed_arg(weak_type, 0));
    if (!require_rc_weak_beta_payload(call, ctx, "WeakUpgrade", payload))
        return TYPE_UNKNOWN;
    return wrap_constructed(TYPE_RC, payload);
}

static Type *
type_check_weak_drop(ASTNode *call, SemanticContext *ctx)
{
    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "WeakDrop requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *weak_type = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (!type_is_constructed_named(weak_type, "Weak")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "WeakDrop requires Weak<T>, got '%s'", type_name_or_unknown(weak_type));
        return TYPE_UNKNOWN;
    }
    if (!require_rc_weak_beta_payload(call, ctx, "WeakDrop",
            type_get_constructed_arg(weak_type, 0)))
        return TYPE_UNKNOWN;
    return TYPE_VOID;
}

static Type *
type_check_allocator_builtin(ASTNode *call, SemanticContext *ctx,
                             bool requires_capacity)
{
    if ((!requires_capacity && ast_call_arg_count(call) != 0)
        || (requires_capacity && ast_call_arg_count(call) != 1)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call,
            requires_capacity
                ? "AllocatorPool requires exactly 1 capacity argument"
                : "Allocator constructor takes no arguments");
        return TYPE_UNKNOWN;
    }

    if (requires_capacity) {
        Type *cap_type = nominal_builtin_normalize_type(
            type_check_expression(ast_call_argument(call, 0), ctx));
        if (!type_equals(cap_type, TYPE_INT) && !type_equals(cap_type, TYPE_LONG)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_BUILTIN_CAPACITY_NON_INTEGER,
                PGY_FIX_USE_INT_OR_LONG_CAPACITY,
                ast_call_argument(call, 0),
                "AllocatorPool capacity must be Int or Long, got '%s'",
                type_name_or_unknown(cap_type));
            return TYPE_UNKNOWN;
        }
    }

    semantic_record_effect(ctx, EFFECT_ALLOC);
    return TYPE_ALLOCATOR;
}

static Type *
type_check_allocator_destroy(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *arg;
    Type *arg_type;

    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call, "AllocatorDestroy requires exactly 1 Allocator argument");
        return TYPE_UNKNOWN;
    }

    arg = ast_call_argument(call, 0);
    if (arg == NULL || arg->type != AST_IDENTIFIER
        || ast_identifier_name(arg) == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call, "AllocatorDestroy requires a named Allocator local");
        return TYPE_UNKNOWN;
    }

    arg_type = nominal_builtin_normalize_type(type_check_expression(arg, ctx));
    if (!type_equals(arg_type, TYPE_ALLOCATOR)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            arg, "AllocatorDestroy argument must be Allocator, got '%s'",
            type_name_or_unknown(arg_type));
        return TYPE_UNKNOWN;
    }

    return TYPE_VOID;
}

static bool
text_builder_require_named_type(ASTNode *call, SemanticContext *ctx,
                                size_t arg_index, Type *expected,
                                const char *operation, Symbol **symbol_out)
{
    ASTNode *arg = ast_call_argument(call, arg_index);
    Type *actual;

    if (symbol_out != NULL)
        *symbol_out = NULL;
    if (arg == NULL || arg->type != AST_IDENTIFIER
        || ast_identifier_name(arg) == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "%s requires a named %s local at argument %llu",
            operation, type_name_or_unknown(expected),
            (unsigned long long)(arg_index + 1));
        return false;
    }
    actual = nominal_builtin_normalize_type(type_check_expression(arg, ctx));
    if (!type_equals(actual, expected)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg,
            "%s argument %llu must be %s, got '%s'", operation,
            (unsigned long long)(arg_index + 1), type_name_or_unknown(expected),
            type_name_or_unknown(actual));
        return false;
    }
    if (symbol_out != NULL)
        *symbol_out = scope_lookup(ctx->scope, ast_identifier_name(arg));
    return !ctx->has_error;
}

static Type *
type_check_text_builder_builtin(ASTNode *call, SemanticContext *ctx,
                                BuiltinKind kind)
{
    const char *operation = kind == BUILTIN_TEXT_BUILDER_NEW
        ? "TextBuilderNew"
        : kind == BUILTIN_TEXT_BUILDER_APPEND
            ? "TextBuilderAppend"
            : kind == BUILTIN_TEXT_BUILDER_FINISH
                ? "TextBuilderFinish" : "TextBuilderDrop";
    size_t expected_count = (kind == BUILTIN_TEXT_BUILDER_NEW
        || kind == BUILTIN_TEXT_BUILDER_DROP) ? 1 : 2;
    Symbol *builder_symbol = NULL;

    if (ast_call_arg_count(call) != expected_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "%s requires exactly %llu arguments", operation,
            (unsigned long long)expected_count);
        return TYPE_UNKNOWN;
    }

    if (kind == BUILTIN_TEXT_BUILDER_NEW) {
        Type *capacity_type;
        if (ctx->scope == NULL || ctx->scope->kind != SCOPE_FUNCTION) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
                "TextBuilderNew is limited to a function's top-level owner scope until MIR cleanup facts land");
            return TYPE_UNKNOWN;
        }
        capacity_type = nominal_builtin_normalize_type(
            type_check_expression(ast_call_argument(call, 0), ctx));
        if (!type_equals(capacity_type, TYPE_INT)
            && !type_equals(capacity_type, TYPE_LONG)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_BUILTIN_CAPACITY_NON_INTEGER,
                PGY_FIX_USE_INT_OR_LONG_CAPACITY,
                ast_call_argument(call, 0),
                "TextBuilderNew capacity must be Int or Long, got '%s'",
                type_name_or_unknown(capacity_type));
            return TYPE_UNKNOWN;
        }
        semantic_record_effect(ctx, EFFECT_ALLOC);
        return TYPE_TEXT_BUILDER;
    }

    if (!text_builder_require_named_type(call, ctx, 0, TYPE_TEXT_BUILDER,
            operation, &builder_symbol))
        return TYPE_UNKNOWN;
    if (builder_symbol == NULL || builder_symbol->is_consumed) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED,
            PGY_CAUSE_MOVE_FROM_RELEASED,
            PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE,
            ast_call_argument(call, 0),
            "%s requires a live TextBuilder owner", operation);
        return TYPE_UNKNOWN;
    }
    if (builder_symbol->is_parameter
        || scope_lookup_current(ctx->scope, builder_symbol->name)
            != builder_symbol) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            ast_call_argument(call, 0),
            "%s requires a TextBuilder local in its declaration scope; parameter and nested-scope access is not yet ownership-safe",
            operation);
        return TYPE_UNKNOWN;
    }

    if (kind == BUILTIN_TEXT_BUILDER_APPEND) {
        Type *text_type = nominal_builtin_normalize_type(
            type_check_expression(ast_call_argument(call, 1), ctx));
        if (!type_equals(text_type, TYPE_STRING)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                ast_call_argument(call, 1),
                "TextBuilderAppend text must be String, got '%s'",
                type_name_or_unknown(text_type));
            return TYPE_UNKNOWN;
        }
        semantic_record_effect(ctx, EFFECT_ALLOC);
        return TYPE_VOID;
    }

    if (kind == BUILTIN_TEXT_BUILDER_FINISH
        && !text_builder_require_named_type(call, ctx, 1, TYPE_ALLOCATOR,
            operation, NULL))
        return TYPE_UNKNOWN;

    builder_symbol->is_consumed = true;
    semantic_record_body_summary(ctx, BODY_SUMMARY_DROPS_RESOURCE);
    if (kind == BUILTIN_TEXT_BUILDER_FINISH)
        semantic_record_effect(ctx, EFFECT_ALLOC);
    return kind == BUILTIN_TEXT_BUILDER_FINISH ? TYPE_STRING : TYPE_VOID;
}

static Type *
type_check_box_builtin(ASTNode *call, SemanticContext *ctx)
{
    Type *payload;

    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "Box requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    payload = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (semantic_reject_active_slot_owner_escape(
            ast_call_argument(call, 0), ctx, "box", "Box")) {
        return TYPE_UNKNOWN;
    }
    if (type_is_resource_handle(payload)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            ast_call_argument(call, 0),
            "Box<T> beta-stable payloads cannot be resource handles; got '%s'.\n"
            "Reason:\n"
            "- resource handles already carry ownership, lifecycle, and runtime anchor contracts\n"
            "- boxing them would create a second storage owner the current CFG/ABI layer cannot prove\n"
            "Fix:\n"
            "- box a copied value or passive class/object payload instead\n"
            "- or keep the resource handle in its original owning binding",
            type_name_or_unknown(payload));
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_BOX, payload);
}

static Type *
type_check_box_get(ASTNode *call, SemanticContext *ctx)
{
    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxGet requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }

    Type *box_type = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxGet requires Box<T>, got '%s'",
            type_name_or_unknown(box_type));
        return TYPE_UNKNOWN;
    }
    return nominal_builtin_normalize_type(type_get_constructed_arg(box_type, 0));
}

static Type *
type_check_box_set(ASTNode *call, SemanticContext *ctx)
{
    if (ast_call_arg_count(call) != 2) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxSet requires exactly 2 arguments");
        return TYPE_UNKNOWN;
    }

    Type *box_type = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxSet requires Box<T>, got '%s'",
            type_name_or_unknown(box_type));
        return TYPE_UNKNOWN;
    }

    Type *inner = nominal_builtin_normalize_type(
        type_get_constructed_arg(box_type, 0));
    Type *value_type = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 1), ctx));
    if (semantic_reject_active_slot_owner_escape(
            ast_call_argument(call, 1), ctx, "box", "BoxSet")) {
        return TYPE_UNKNOWN;
    }
    if (type_is_resource_handle(value_type)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            ast_call_argument(call, 1),
            "BoxSet beta-stable payloads cannot be resource handles; got '%s'.\n"
            "Reason:\n"
            "- resource handles already carry ownership, lifecycle, and runtime anchor contracts\n"
            "- storing them in Box<T> would create a second storage owner the current CFG/ABI layer cannot prove\n"
            "Fix:\n"
            "- store a copied value or passive class/object payload instead\n"
            "- or keep the resource handle in its original owning binding",
            type_name_or_unknown(value_type));
        return TYPE_UNKNOWN;
    }
    require_assignable(value_type, inner, ast_call_argument(call, 1), ctx);
    return TYPE_VOID;
}

static Type *
type_check_box_drop(ASTNode *call, SemanticContext *ctx)
{
    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxDrop requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }

    Type *box_type = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxDrop requires Box<T>, got '%s'",
            type_name_or_unknown(box_type));
        return TYPE_UNKNOWN;
    }
    return TYPE_VOID;
}

static Type *
type_check_box_is_valid(ASTNode *call, SemanticContext *ctx)
{
    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxIsValid requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }

    Type *box_type = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxIsValid requires Box<T>, got '%s'",
            type_name_or_unknown(box_type));
        return TYPE_UNKNOWN;
    }
    return TYPE_BOOL;
}

static Type *
type_check_box_array_builtin(ASTNode *call, SemanticContext *ctx)
{
    if (ast_call_arg_count(call) < 1 || ast_call_arg_count(call) > 2) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "BoxArray requires capacity and optional allocator");
        return TYPE_UNKNOWN;
    }

    Type *cap_type = nominal_builtin_normalize_type(
        type_check_expression(ast_call_argument(call, 0), ctx));
    if (!type_equals(cap_type, TYPE_INT) && !type_equals(cap_type, TYPE_LONG)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, ast_call_argument(call, 0),
            "BoxArray capacity must be Int or Long, got '%s'",
            type_name_or_unknown(cap_type));
        return TYPE_UNKNOWN;
    }

    if (ast_call_arg_count(call) == 2) {
        Type *alloc_type = nominal_builtin_normalize_type(
            type_check_expression(ast_call_argument(call, 1), ctx));
        if (!type_equals(alloc_type, TYPE_ALLOCATOR)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, ast_call_argument(call, 1),
                "BoxArray allocator must be Allocator, got '%s'",
                type_name_or_unknown(alloc_type));
            return TYPE_UNKNOWN;
        }
    }

    return TYPE_UNKNOWN;
}

Type *
type_check_nominal_ownership_builtin(ASTNode *call,
                                     BuiltinKind kind,
                                     SemanticContext *ctx,
                                     bool *handled_out)
{
    if (handled_out != NULL)
        *handled_out = true;

    switch (kind) {
    case BUILTIN_RC_NEW:
        return type_check_rc_new(call, ctx);
    case BUILTIN_RC_CLONE:
        return type_check_rc_clone(call, ctx);
    case BUILTIN_RC_DROP:
        (void)type_check_rc_clone(call, ctx);
        return TYPE_VOID;
    case BUILTIN_RC_DOWNGRADE:
        return type_check_rc_downgrade(call, ctx);
    case BUILTIN_RC_GET:
        return type_check_rc_get(call, ctx);
    case BUILTIN_WEAK_UPGRADE:
        return type_check_weak_upgrade(call, ctx);
    case BUILTIN_WEAK_DROP:
        return type_check_weak_drop(call, ctx);
    case BUILTIN_ALLOCATOR_SYSTEM:
    case BUILTIN_ALLOCATOR_TRACING:
    case BUILTIN_ALLOCATOR_DEBUG:
    case BUILTIN_ALLOCATOR_SCRATCH:
    case BUILTIN_ALLOCATOR_RESULT:
    case BUILTIN_ALLOCATOR_PERSISTENT:
        return type_check_allocator_builtin(call, ctx, false);
    case BUILTIN_ALLOCATOR_DESTROY:
        return type_check_allocator_destroy(call, ctx);
    case BUILTIN_ALLOCATOR_POOL:
        return type_check_allocator_builtin(call, ctx, true);
    case BUILTIN_TEXT_BUILDER_NEW:
    case BUILTIN_TEXT_BUILDER_APPEND:
    case BUILTIN_TEXT_BUILDER_FINISH:
    case BUILTIN_TEXT_BUILDER_DROP:
        return type_check_text_builder_builtin(call, ctx, kind);
    case BUILTIN_BOX:
        return type_check_box_builtin(call, ctx);
    case BUILTIN_BOX_GET:
        return type_check_box_get(call, ctx);
    case BUILTIN_BOX_SET:
        return type_check_box_set(call, ctx);
    case BUILTIN_BOX_DROP:
        return type_check_box_drop(call, ctx);
    case BUILTIN_BOX_IS_VALID:
        return type_check_box_is_valid(call, ctx);
    case BUILTIN_BOX_ARRAY:
        return type_check_box_array_builtin(call, ctx);
    default:
        if (handled_out != NULL)
            *handled_out = false;
        return TYPE_UNKNOWN;
    }
}

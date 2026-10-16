#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "type_checker_builtins_ownership_nominal_internal.h"

static bool
text_builder_require_named_type(ASTNode *call, SemanticContext *ctx,
                                size_t arg_index, Type *expected,
                                const char *operation, Symbol **symbol_out)
{
    ASTNode *arg = ast_call_argument(call, arg_index);
    Type *actual;
    size_t diagnostic_start;

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
    diagnostic_start = ctx->diagnostic_count;
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
    for (size_t i = diagnostic_start; i < ctx->diagnostic_count; i++) {
        Diagnostic *diagnostic = ctx->diagnostics[i];
        if (diagnostic != NULL && diagnostic->level == DIAG_ERROR)
            return false;
    }
    if (symbol_out != NULL)
        *symbol_out = scope_lookup(ctx->scope, ast_identifier_name(arg));
    return true;
}

Type *
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
    /* Same spelling as the self-host builtin signature display. */
    const char *signature = kind == BUILTIN_TEXT_BUILDER_NEW
        ? "TextBuilderNew(Int) -> TextBuilder"
        : kind == BUILTIN_TEXT_BUILDER_APPEND
            ? "TextBuilderAppend(TextBuilder, String) -> Void"
            : kind == BUILTIN_TEXT_BUILDER_FINISH
                ? "TextBuilderFinish(TextBuilder, Allocator) -> String"
                : "TextBuilderDrop(TextBuilder) -> Void";
    Symbol *builder_symbol = NULL;

    if (ast_call_arg_count(call) != expected_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "%s requires exactly %llu arguments. Declared signature: %s",
            operation, (unsigned long long)expected_count, signature);
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
    bool owner_in_current_function = false;
    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->parent) {
        if (scope_lookup_current(scope, builder_symbol->name)
            == builder_symbol) {
            owner_in_current_function = true;
            break;
        }
        if (scope->kind == SCOPE_FUNCTION)
            break;
    }
    bool requires_declaration_scope =
        kind == BUILTIN_TEXT_BUILDER_FINISH
        || kind == BUILTIN_TEXT_BUILDER_DROP;
    if (builder_symbol->is_parameter
        || !owner_in_current_function
        || (requires_declaration_scope
            && scope_lookup_current(ctx->scope, builder_symbol->name)
                != builder_symbol)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            ast_call_argument(call, 0),
            "%s requires a TextBuilder local owned by the current function%s",
            operation,
            requires_declaration_scope
                ? " and consumed in its declaration scope"
                : "");
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

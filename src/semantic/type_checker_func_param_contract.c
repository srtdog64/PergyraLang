#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"
#include "diag_codes.h"

#include <string.h>

/*
 * C reserved words that are NOT Pergyra keywords, so they parse as legal
 * identifiers -- but the bootstrap C backend emits function and parameter
 * names raw, so they would produce broken C. The self-hosted compiler
 * already escapes these (src/self_hosted/compiler/symbol_table_owner.pgy,
 * CompilerSymbolCReservedWord -- the SoT twin of this list; keep them in
 * lockstep). The bootstrap rejects instead of renaming: renaming would
 * have to be threaded through every emission site of both backends,
 * while rejection is a single fail-closed surface rule (docs/189 C11).
 * Let-bound locals are exempt by construction -- they are SSA-renamed to
 * _pgy_ssa_* on emission, and the corpus legitimately uses names like
 * `let double`.
 */
static bool
identifier_is_c_reserved_word(const char *name)
{
    static const char *const words[] = {
        "auto", "break", "case", "char", "const", "continue", "default",
        "do", "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "inline", "int", "long", "register", "restrict", "return",
        "short", "signed", "sizeof", "static", "struct", "switch",
        "typedef", "union", "unsigned", "void", "volatile", "while",
    };

    if (name == NULL)
        return false;
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        if (strcmp(name, words[i]) == 0)
            return true;
    }
    return false;
}

void
type_check_func_validate_identifier_hygiene(ASTNode *node,
                                            SemanticContext *ctx,
                                            const char *role,
                                            const char *name)
{
    if (!identifier_is_c_reserved_word(name))
        return;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_IDENTIFIER_C_RESERVED,
        PGY_FIX_RENAME_IDENTIFIER,
        node,
        "%s name '%s' is a C reserved word and cannot cross the C emission boundary.\n"
        "Reason:\n"
        "- function and parameter names are emitted verbatim into the C backend's output, where '%s' is a keyword\n"
        "Fix:\n"
        "- rename the %s (e.g. '%s_value')",
        role != NULL ? role : "identifier",
        name, name,
        role != NULL ? role : "identifier",
        name);
}

void
type_check_func_validate_return_boundary(ASTNode *node,
                                         SemanticContext *ctx,
                                         Type *return_type)
{
    if (semantic_type_is_future_handle(return_type)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_TASK_LIFECYCLE,
            PGY_CAUSE_TASK_LIFECYCLE,
            PGY_FIX_AWAIT_TASK_BEFORE_EXIT,
            ast_func_return_type(node),
            "Future cannot cross a return boundary in the beta structured-spawn contract.\n"
            "Reason:\n"
            "- returning a completion handle moves its join obligation beyond the declaring scope\n"
            "- beta transfer is explicit only through an 'own Future<T>' parameter\n"
            "Fix:\n"
            "- await the Future and return its completed value\n"
            "- or pass the Future to a named function through an own parameter");
        return;
    }
    if (type_is_constructed_named(return_type, "Channel")) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ANCHORED_HANDLE_RETURN_BOUNDARY,
            PGY_FIX_RETURN_PROJECTION_OR_KEEP_LOCAL,
            ast_func_return_type(node),
            "Channel cannot cross a return boundary.\n"
            "Reason:\n"
            "- returning a channel copies its descriptor (buffer pointer + cursors), and aliased descriptors drift so deliveries silently split (docs/189 C12)\n"
            "Fix:\n"
            "- declare the channel at the use site and pass values through it\n"
            "- or send the produced values over a channel the caller owns");
        return;
    }
    if (!type_is_builtin_owner_handle(return_type))
        return;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_ANCHORED_HANDLE_RETURN_BOUNDARY,
        PGY_FIX_RETURN_PROJECTION_OR_KEEP_LOCAL,
        ast_func_return_type(node),
        "TextBuilder cannot cross a return boundary in the bounded owner rung; return the finished String instead");
}

void
type_check_func_validate_param_boundary(ASTNode *node,
                                        SemanticContext *ctx,
                                        const char *func_name,
                                        FuncParam *param,
                                        Type *param_type)
{
    if (param == NULL)
        return;

    type_check_func_validate_identifier_hygiene(node, ctx, "parameter",
                                                param->name);

    /* Channel<T> descriptors are by-value structs with interior cursors: a
     * parameter silently copies {buffer, head, count} and the copies drift
     * (duplicate/split deliveries even in serial code). Final copy edge of
     * the class -- let/mut/return/field/capture are already closed
     * (docs/189 C12, board WO-RT-6). */
    if (type_is_constructed_named(param_type, "Channel")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_PARAM_MODE_UNSUPPORTED_BOUNDARY_TYPE,
            PGY_FIX_USE_BOUNDARY_VISIBLE_TYPE_OR_DROP_QUALIFIER,
            node,
            "Channel parameters are not supported.\n"
            "Reason:\n"
            "- passing a channel copies its descriptor (buffer pointer + cursors), so the copies drift and deliveries silently split\n"
            "Fix:\n"
            "- declare the channel at the use site and send/recv on that one variable\n"
            "- pass the values that travel through the channel instead of the channel itself");
        return;
    }

    if (type_is_builtin_owner_handle(param_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_PARAM_MODE_UNSUPPORTED_BOUNDARY_TYPE,
            PGY_FIX_USE_BOUNDARY_VISIBLE_TYPE_OR_DROP_QUALIFIER,
            node,
            "TextBuilder parameters are not supported by the bounded owner rung.\n"
            "Reason:\n"
            "- parameter carriage would expose an unproved copy, borrow, or transfer boundary\n"
            "Fix:\n"
            "- create and consume the TextBuilder inside one function owner scope\n"
            "- pass the finished String across the function boundary instead");
        return;
    }

    if (semantic_type_is_future_handle(param_type)
        && param->mode != PARAM_MODE_OWN) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_TASK_LIFECYCLE,
            PGY_CAUSE_TASK_LIFECYCLE,
            PGY_FIX_AWAIT_TASK_BEFORE_EXIT,
            node,
            "Future parameters require explicit 'own'.\n"
            "Reason:\n"
            "- await frees the completion handle, so default/ref carriage would alias one runtime handle\n"
            "- the caller must transfer its join obligation to exactly one callee\n"
            "Fix:\n"
            "- declare 'own %s: %s' and await or transfer it on every callee path",
            param->name != NULL ? param->name : "task",
            type_name_or_unknown(param_type));
        return;
    }

    if (param->mode != PARAM_MODE_DEFAULT
        && semantic_classify_ownership_type(param_type, ctx)
            == OWNERSHIP_TYPE_COPY_ONLY) {
        return;
    }

    if (param->mode != PARAM_MODE_DEFAULT
        && !type_is_anchored_resource_handle(param_type)
        && !type_is_general_boundary_type(param_type, ctx)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_PARAM_MODE_UNSUPPORTED_BOUNDARY_TYPE,
            PGY_FIX_USE_BOUNDARY_VISIBLE_TYPE_OR_DROP_QUALIFIER,
            node,
            "'%s' parameter mode requires a boundary-visible type at function boundaries.\n"
            "Reason:\n"
            "- value is parameter '%s'\n"
            "- ownership mode is '%s'\n"
            "- consumer path is function '%s'\n"
            "- type '%s' is not a copy-visible value, boundary-tracked aggregate, subject identity, or slot handle (movable)\n"
            "- own/ref only changes boundary semantics when the parameter carries ownership-relevant state across the call\n"
            "Fix:\n"
            "- remove '%s' and pass it as an ordinary value\n"
            "- or change the parameter type to a boundary-visible value / subject / slot handle",
            param->mode == PARAM_MODE_OWN ? "own" : "ref",
            param->name != NULL ? param->name : "<param>",
            param->mode == PARAM_MODE_OWN ? "own" : "ref",
            func_name != NULL ? func_name : "<anonymous>",
            param_type != NULL && param_type->name != NULL
                ? param_type->name : "<type>",
            param->mode == PARAM_MODE_OWN ? "own" : "ref");
    }

    if (type_is_anchored_resource_handle(param_type)
        && param->mode == PARAM_MODE_DEFAULT) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_SLOT_PARAM_QUALIFIER_MISSING,
            PGY_FIX_ANNOTATE_SLOT_PARAM_QUALIFIER,
            node,
            "Slot handle (anchored) parameters require explicit 'own' or 'ref'.\n"
            "Reason:\n"
            "- slot handles (anchored) must declare whether the boundary borrows or transfers ownership\n"
            "- implicit parameter passing would hide that boundary contract\n"
            "Fix:\n"
            "- mark the parameter as 'ref' for borrowing\n"
            "- or mark it as 'own' for transfer");
    }
}

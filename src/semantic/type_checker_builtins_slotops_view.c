#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"

static Type *
slotops_view_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static Type *
type_check_view_source_type(ASTNode *arg, SemanticContext *ctx)
{
    if (arg != NULL && arg->type == AST_IDENTIFIER
        && ast_identifier_name(arg) != NULL) {
        Symbol *sym = scope_lookup(ctx->scope, ast_identifier_name(arg));
        if (sym != NULL && sym->kind == SYMBOL_SLOT && sym->type != NULL) {
            sym->is_used = true;
            return sym->type;
        }
    }

    return slotops_view_normalize_type(type_check_expression(arg, ctx));
}

static bool
reject_qubit_view(ASTNode *call, Type *slot_type, const char *view_name,
                  SemanticContext *ctx)
{
    if (!type_is_qubit(slot_type))
        return false;

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_PIN_QUBIT_REJECT,
        PGY_CAUSE_PIN_QUBIT_REJECT,
        PGY_FIX_DO_NOT_PIN_QUBIT,
        ast_call_argument(call, 0),
        "%s cannot pin QubitSlot resources.\n"
        "Reason:\n"
        "- QubitSlot has a movable quantum state machine, not a stable resource-boundary lease\n"
        "- pinning it would bypass measurement/entanglement lifecycle checks\n"
        "Fix:\n"
        "- use the quantum operations directly\n"
        "- or convert to a classical value before creating a view",
        view_name);
    return true;
}

static bool
require_owned_slot_view_source(ASTNode *call, Type *slot_type,
                               const char *view_name, SemanticContext *ctx)
{
    if (type_is_owned_slot_handle(slot_type))
        return true;

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_BUILTIN_SLOT_TYPE_REQUIRED,
        PGY_FIX_PASS_OWNING_SLOT,
        ast_call_argument(call, 0),
        "%s requires owning Slot<T>, got '%s'",
        view_name,
        slot_type != NULL ? slot_type->name : "<null>");
    return false;
}

static bool
require_secure_pin_paired_token(ASTNode *call, Type *slot_type,
                                const char *view_name, SemanticContext *ctx)
{
    ASTNode *src_arg;
    Symbol *slot_sym;
    Symbol *token_sym;
    const char *slot_name;
    const char *paired_token_name;

    if (!type_slot_is_secure(slot_type))
        return true;

    src_arg = ast_call_argument(call, 0);
    if (src_arg == NULL || src_arg->type != AST_IDENTIFIER
        || ast_identifier_name(src_arg) == NULL)
        return true;

    slot_name = ast_identifier_name(src_arg);
    slot_sym = scope_lookup(ctx->scope, slot_name);
    if (slot_sym == NULL || slot_sym->kind != SYMBOL_SLOT)
        return true;

    paired_token_name = slot_sym->slot_info.paired_token_name;
    if (paired_token_name == NULL || paired_token_name[0] == '\0') {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_PIN_TOKEN_INVALID,
            PGY_CAUSE_PIN_TOKEN_INVALID,
            PGY_FIX_PROVIDE_VALID_PIN_TOKEN,
            src_arg,
            "%s of SecureSlot '%s' requires a paired capability token.\n"
            "Reason:\n"
            "- SecureSlot pinning is a capability lease; the runtime ABI rejects "
              "pin without a valid token for the requested read/write mode\n"
            "- this SecureSlot has no paired token name recorded at the pin site\n"
            "Fix:\n"
            "- bind the SecureSlot through ClaimSecureSlot so its paired token is "
              "produced in the same scope\n"
            "- or thread the SecureSlot and its token together to this pin site",
            view_name, slot_name);
        return false;
    }

    token_sym = scope_lookup(ctx->scope, paired_token_name);
    if (token_sym == NULL || token_sym->kind != SYMBOL_TOKEN) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_PIN_TOKEN_INVALID,
            PGY_CAUSE_PIN_TOKEN_INVALID,
            PGY_FIX_PROVIDE_VALID_PIN_TOKEN,
            src_arg,
            "%s of SecureSlot '%s' has no reachable paired token '%s'.\n"
            "Reason:\n"
            "- SecureSlot pinning is a capability lease and the runtime ABI rejects "
              "pin without a valid token for the requested read/write mode\n"
            "- the paired token symbol expected at this scope is unavailable; "
              "it may have been moved out, released, or never declared\n"
            "Fix:\n"
            "- pin the SecureSlot in the same scope where its paired token is "
              "reachable\n"
            "- or rebind the paired token before pinning",
            view_name, slot_name, paired_token_name);
        return false;
    }

    return true;
}

Type *
type_check_view_read(ASTNode *call, SemanticContext *ctx)
{
    Type *slot_type;

    if (!check_call_arity(call, 1, "ViewRead", ctx))
        return TYPE_UNKNOWN;

    slot_type = type_check_view_source_type(ast_call_argument(call, 0),
                                            ctx);
    if (reject_qubit_view(call, slot_type, "ViewRead", ctx))
        return TYPE_UNKNOWN;
    if (!require_owned_slot_view_source(call, slot_type, "ViewRead", ctx))
        return TYPE_UNKNOWN;
    if (!require_secure_pin_paired_token(call, slot_type, "ViewRead", ctx))
        return TYPE_UNKNOWN;
    return type_create_read_view(type_slot_inner_type(slot_type));
}

Type *
type_check_view_write(ASTNode *call, SemanticContext *ctx)
{
    Type *slot_type;

    if (!check_call_arity(call, 1, "ViewWrite", ctx))
        return TYPE_UNKNOWN;

    slot_type = type_check_view_source_type(ast_call_argument(call, 0),
                                            ctx);
    if (reject_qubit_view(call, slot_type, "ViewWrite", ctx))
        return TYPE_UNKNOWN;
    if (!require_owned_slot_view_source(call, slot_type, "ViewWrite", ctx))
        return TYPE_UNKNOWN;
    if (!require_secure_pin_paired_token(call, slot_type, "ViewWrite", ctx))
        return TYPE_UNKNOWN;
    return type_create_write_view(type_slot_inner_type(slot_type));
}

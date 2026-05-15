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
    return type_create_write_view(type_slot_inner_type(slot_type));
}

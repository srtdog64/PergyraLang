#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"
#include "diag_codes.h"

void
type_check_func_validate_param_boundary(ASTNode *node,
                                        SemanticContext *ctx,
                                        const char *func_name,
                                        FuncParam *param,
                                        Type *param_type)
{
    if (param == NULL)
        return;

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

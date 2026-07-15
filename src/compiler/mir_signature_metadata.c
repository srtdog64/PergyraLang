#include "mir_signature_metadata.h"

#include <stdint.h>
#include <stdlib.h>

#include "mir_decl_headers.h"
#include "mir_type_helpers.h"
#include "../parser/ast_api.h"
#include "../common/string_compat.h"

MIRParamCarriage
mir_param_carriage_from_source_mode(ParamMode mode)
{
    switch (mode) {
    case PARAM_MODE_REF:
        return MIR_PARAM_CARRIAGE_READONLY_REF;
    case PARAM_MODE_MUT_REF:
        return MIR_PARAM_CARRIAGE_VALUE_RESULT;
    case PARAM_MODE_OWN:
        return MIR_PARAM_CARRIAGE_OWNER_HANDLE;
    case PARAM_MODE_DEFAULT:
    default:
        return MIR_PARAM_CARRIAGE_VALUE;
    }
}

/* Row 607: free a callable signature descriptor. */
static void
mir_callable_sig_clear(MIRCallableSig *sig)
{
    if (sig == NULL)
        return;
    free(sig->return_type_name);
    if (sig->param_type_names != NULL) {
        for (size_t i = 0; i < sig->param_count; i++)
            free(sig->param_type_names[i]);
        free(sig->param_type_names);
    }
    sig->is_callable = false;
    sig->return_type_name = NULL;
    sig->param_type_names = NULL;
    sig->param_count = 0;
}

/*
 * Row 607: populate a callable descriptor from an EventHandler AST type node.
 * Best-effort and lossless-or-nothing: if any nested return/param type cannot
 * be rendered losslessly (e.g. a nested EventHandler, which mir_render_type_name
 * leaves absent), the descriptor is cleared and is_callable stays false so the
 * caller keeps the retained-AST carrier rather than emitting a lossy signature.
 */
static void
mir_callable_sig_build(const ASTNode *type_node, MIRCallableSig *out)
{
    size_t param_count;

    if (out == NULL || type_node == NULL
        || type_node->type != AST_EVENT_HANDLER_TYPE)
        return;

    ASTNode *ret = ast_event_handler_return_type((ASTNode *)type_node);
    if (ret != NULL) {
        out->return_type_name = mir_capture_type_name(ret, NULL);
        if (out->return_type_name == NULL) {
            mir_callable_sig_clear(out);
            return;
        }
    }
    param_count = ast_event_handler_param_count((ASTNode *)type_node);
    if (param_count > 0) {
        out->param_type_names = calloc(param_count, sizeof(char *));
        if (out->param_type_names == NULL) {
            mir_callable_sig_clear(out);
            return;
        }
        for (size_t i = 0; i < param_count; i++) {
            ASTNode *pt =
                ast_event_handler_param_type((ASTNode *)type_node, i);
            if (pt != NULL) {
                out->param_type_names[i] = mir_capture_type_name(pt, NULL);
                if (out->param_type_names[i] == NULL) {
                    mir_callable_sig_clear(out);
                    return;
                }
            }
        }
    }
    out->param_count = param_count;
    out->is_callable = true;
}

void
mir_routine_signature_metadata_clear(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    if (routine->param_type_names != NULL) {
        for (size_t i = 0; i < routine->param_count; i++)
            free(routine->param_type_names[i]);
    }
    free(routine->param_type_names);
    routine->param_type_names = NULL;
    free(routine->param_abi_facts);
    routine->param_abi_facts = NULL;
    free(routine->return_type_name);
    routine->return_type_name = NULL;
    if (routine->generic_param_names != NULL) {
        for (size_t i = 0; i < routine->generic_param_count; i++)
            free(routine->generic_param_names[i]);
        free(routine->generic_param_names);
        routine->generic_param_names = NULL;
    }
    if (routine->param_callable_sigs != NULL) {
        for (size_t i = 0; i < routine->param_count; i++)
            mir_callable_sig_clear(&routine->param_callable_sigs[i]);
        free(routine->param_callable_sigs);
        routine->param_callable_sigs = NULL;
    }
    mir_callable_sig_clear(&routine->return_callable_sig);
}

bool
mir_routine_signature_metadata_capture(const MIRProgram *program,
                                       MIRRoutine *routine)
{
    if (routine == NULL || !routine->has_signature)
        return true;

    if (routine->generic_param_count > 0) {
        GenericParams *generic_params = routine->ast != NULL
            ? ast_declaration_generic_params(routine->ast)
            : NULL;
        if (generic_params == NULL
            || ast_generic_param_count(generic_params)
                != routine->generic_param_count
            || routine->generic_param_count > SIZE_MAX / sizeof(char *)) {
            return false;
        }
        routine->generic_param_names = calloc(
            routine->generic_param_count, sizeof(char *));
        if (routine->generic_param_names == NULL)
            return false;
        for (size_t i = 0; i < routine->generic_param_count; i++) {
            GenericParam *param = ast_generic_param_at(generic_params, i);
            const char *name = ast_generic_param_name(param);
            if (name == NULL)
                return false;
            routine->generic_param_names[i] = pergyra_strdup(name);
            if (routine->generic_param_names[i] == NULL)
                return false;
        }
    }

    if (routine->param_count > 0) {
        if (routine->param_count > SIZE_MAX / sizeof(char *)
            || routine->param_count > SIZE_MAX / sizeof(MIRCallableSig))
            return false;
        routine->param_type_names = calloc(routine->param_count,
            sizeof(char *));
        if (routine->param_type_names == NULL)
            return false;
        routine->param_callable_sigs = calloc(routine->param_count,
            sizeof(MIRCallableSig));
        if (routine->param_callable_sigs == NULL)
            return false;
        routine->param_abi_facts = calloc(routine->param_count,
            sizeof(MIRParamAbiFact));
        if (routine->param_abi_facts == NULL)
            return false;
        for (size_t i = 0; i < routine->param_count; i++) {
            FuncParam *param =
                routine->params != NULL ? routine->params[i] : NULL;
            ParamMode mode = param != NULL
                ? param->mode
                : PARAM_MODE_DEFAULT;
            routine->param_abi_facts[i].carriage =
                mir_param_carriage_from_source_mode(mode);
            if (param != NULL && param->type != NULL) {
                routine->param_type_names[i] =
                    mir_capture_type_name(param->type, NULL);
                if (routine->param_abi_facts[i].carriage
                        == MIR_PARAM_CARRIAGE_READONLY_REF
                    && routine->param_type_names[i] != NULL) {
                    const MIRDeclHeader *header = mir_find_decl_header(
                        program, routine->param_type_names[i]);
                    routine->param_abi_facts[i].pass_indirect =
                        header != NULL
                        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
                            == AST_CLASS_DECL;
                }
                /* Row 607: when the rendered name is absent because the param
                   is an EventHandler, carry its shape losslessly in MIR. */
                if (routine->param_type_names[i] == NULL)
                    mir_callable_sig_build(param->type,
                        &routine->param_callable_sigs[i]);
            }
        }
    }
    if (routine->return_type != NULL) {
        routine->return_type_name =
            mir_capture_type_name(routine->return_type, NULL);
        if (routine->return_type_name == NULL)
            mir_callable_sig_build(routine->return_type,
                &routine->return_callable_sig);
    } else if (routine->ast != NULL && routine->ast->type == AST_FUNC_DECL
               && ast_func_semantic_return_type_name(routine->ast) != NULL) {
        routine->return_type_name =
            mir_capture_type_name(NULL,
                ast_func_semantic_return_type_name(routine->ast));
    }
    return true;
}

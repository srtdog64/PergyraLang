#include "mir_signature_metadata.h"

#include <stdint.h>
#include <stdlib.h>

#include "mir_type_helpers.h"
#include "../parser/ast_api.h"

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
        out->return_type_name = mir_render_type_name(ret);
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
                out->param_type_names[i] = mir_render_type_name(pt);
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
mir_routine_signature_type_names_clear(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    if (routine->param_type_names != NULL) {
        for (size_t i = 0; i < routine->param_count; i++)
            free(routine->param_type_names[i]);
    }
    free(routine->param_type_names);
    routine->param_type_names = NULL;
    free(routine->return_type_name);
    routine->return_type_name = NULL;
    if (routine->param_callable_sigs != NULL) {
        for (size_t i = 0; i < routine->param_count; i++)
            mir_callable_sig_clear(&routine->param_callable_sigs[i]);
        free(routine->param_callable_sigs);
        routine->param_callable_sigs = NULL;
    }
    mir_callable_sig_clear(&routine->return_callable_sig);
}

bool
mir_routine_signature_type_names_capture(MIRRoutine *routine)
{
    if (routine == NULL || !routine->has_signature)
        return true;

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
        for (size_t i = 0; i < routine->param_count; i++) {
            FuncParam *param =
                routine->params != NULL ? routine->params[i] : NULL;
            if (param != NULL && param->type != NULL) {
                routine->param_type_names[i] =
                    mir_capture_type_name(param->type, NULL);
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

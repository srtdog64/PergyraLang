#include "transpiler_intent_emit_metadata_helpers.h"

#include "intent_binding_metadata_view.h"

#include <stdlib.h>
#include <string.h>

void
transpiler_free_intent_emit_metadata(ASTNode **mir_steps,
                                     const char **participant_aliases,
                                     const char **participant_types,
                                     const char **value_aliases,
                                     const char **value_types,
                                     IntentBindingMetadataView *bindings,
                                     const char **mir_step_names)
{
    free(mir_steps);
    free((void *)participant_aliases);
    free((void *)participant_types);
    free((void *)value_aliases);
    free((void *)value_types);
    intent_binding_metadata_view_dispose(bindings);
    free((void *)mir_step_names);
}

ASTNode *
transpiler_find_intent_step_source_by_name(ASTNode *intent,
                                           const char *step_name)
{
    ASTNode **steps;
    size_t step_count;

    if (intent == NULL || intent->type != AST_INTENT_DECL || step_name == NULL)
        return NULL;
    steps = ast_intent_decl_steps(intent, &step_count);
    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP
            || ast_intent_step_name(step) == NULL) {
            continue;
        }
        if (strcmp(ast_intent_step_name(step), step_name) == 0)
            return step;
    }
    return NULL;
}

ASTNode **
transpiler_build_mir_intent_step_sources(ASTNode *intent,
                                         const char **step_names,
                                         size_t step_count)
{
    ASTNode **steps;

    if (step_count == 0 || step_names == NULL)
        return NULL;
    steps = calloc(step_count, sizeof(ASTNode *));
    if (steps == NULL)
        return NULL;
    for (size_t i = 0; i < step_count; i++)
        steps[i] = transpiler_find_intent_step_source_by_name(intent, step_names[i]);
    return steps;
}

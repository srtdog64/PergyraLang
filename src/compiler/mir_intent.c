#include "mir_intent.h"
#include "mir_intent_step_emit.h"

#include "dir.h"
#include "mir_type_helpers.h"

#include <stdlib.h>
#include <string.h>

#include "../common/arena.h"
#include "../common/string_compat.h"
#include "parser/ast_api.h"

static const DIRIntentInfo *
mir_intent_dir_info(const DIRProgram *dir, const ASTNode *intent)
{
    uint32_t intent_id;

    if (dir == NULL || intent == NULL || intent->type != AST_INTENT_DECL)
        return NULL;
    intent_id = ast_node_stable_id(intent);
    if (intent_id == 0)
        return NULL;
    for (size_t i = 0; i < dir->intent_count; i++) {
        const DIRIntentInfo *info = &dir->intents[i];
        if (info->node_id < dir->node_count
            && dir->nodes[info->node_id].source_syntax_id == intent_id) {
            return info;
        }
    }
    return NULL;
}

static const char *
mir_intent_mode_from_rir_policy(const MIRRoutine *routine)
{
    const char *mode = NULL;
    size_t count = 0;

    if (routine == NULL || routine->rir_scope == NULL)
        return NULL;
    for (size_t i = 0; i < rir_scope_fact_count(routine->rir_scope); i++) {
        const RIRFact *fact = rir_scope_fact_at(routine->rir_scope, i);
        if (fact == NULL || fact->kind != RIR_FACT_INTENT_POLICY
            || fact->name == NULL || strcmp(fact->name, "concurrency") != 0) {
            continue;
        }
        count++;
        mode = fact->arg0;
    }
    if (count != 1 || mode == NULL
        || (strcmp(mode, "exclusive") != 0
            && strcmp(mode, "concurrent") != 0)) {
        return NULL;
    }
    return mode;
}

static const DIRIntentInfo *
mir_intent_dir_info_for_routine(const DIRProgram *dir,
                                const MIRRoutine *routine)
{
    if (dir == NULL || routine == NULL || routine->source_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < dir->intent_count; i++) {
        const DIRIntentInfo *info = &dir->intents[i];
        if (info->node_id < dir->node_count
            && dir->nodes[info->node_id].source_syntax_id
                == routine->source_syntax_id) {
            return info;
        }
    }
    return NULL;
}

bool
mir_intent_capture_signature(MIRRoutine *routine,
                             const DIRProgram *dir,
                             char **error_message)
{
    const DIRIntentInfo *info;

    if (routine == NULL || routine->kind != MIR_SCOPE_INTENT)
        return true;
    info = mir_intent_dir_info_for_routine(dir, routine);
    if (info == NULL || info->return_type_name == NULL
        || info->return_type_name[0] == '\0') {
        if (error_message != NULL) {
            *error_message = pergyra_strdup(
                "typed/legacy intent MIR signature has no exact DIR return fact");
        }
        return false;
    }
    routine->return_type_name = pergyra_strdup(info->return_type_name);
    if (routine->return_type_name == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    routine->has_signature = true;
    return true;
}

static bool
mir_append_intent_participants(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *intent)
{
    ASTNode **involves_nodes;
    size_t involve_count;

    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        ASTNode *subject_type;
        const char *alias = NULL;
        const char *type_name = NULL;

        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        alias = ast_intent_involves_alias(involves);
        subject_type = ast_intent_involves_subject_type(involves);
        if (subject_type != NULL && subject_type->type == AST_TYPE) {
            type_name = ast_type_name(subject_type);
        }
        if (alias == NULL || type_name == NULL)
            continue;
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentParticipant",
                                    routine->name,
                                    alias,
                                    type_name,
                                    involves)) {
            return false;
        }
    }
    return true;
}

static const char *
mir_intent_binding_type_name(MIRRoutine *routine, ASTNode *binding)
{
    if (routine == NULL || binding == NULL)
        return NULL;
    if (binding->type == AST_INTENT_INVOLVES) {
        ASTNode *subject_type = ast_intent_involves_subject_type(binding);
        if (subject_type != NULL && subject_type->type == AST_TYPE)
            return ast_type_name(subject_type);
        return NULL;
    }
    if (binding->type == AST_INTENT_VALUE) {
        ASTNode *value_type = ast_intent_value_type(binding);
        if (value_type != NULL) {
            char *rendered = mir_render_type_name(value_type);
            if (rendered != NULL) {
                const char *type_name = pgy_arena_strdup(&routine->scratch,
                                                         rendered);
                free(rendered);
                return type_name;
            }
        }
    }
    return NULL;
}

static const char *
mir_intent_binding_alias(ASTNode *binding)
{
    if (binding == NULL)
        return NULL;
    if (binding->type == AST_INTENT_INVOLVES)
        return ast_intent_involves_alias(binding);
    if (binding->type == AST_INTENT_VALUE)
        return ast_intent_value_alias(binding);
    return NULL;
}

static const char *
mir_intent_binding_kind(ASTNode *binding)
{
    if (binding == NULL)
        return NULL;
    if (binding->type == AST_INTENT_INVOLVES)
        return "participant";
    if (binding->type == AST_INTENT_VALUE)
        return "value";
    return NULL;
}

static bool
mir_append_intent_binding(MIRRoutine *routine,
                          MIRBasicBlock *block,
                          ASTNode *binding)
{
    const char *kind = mir_intent_binding_kind(binding);
    const char *alias = mir_intent_binding_alias(binding);
    const char *type_name = mir_intent_binding_type_name(routine, binding);

    if (kind == NULL || alias == NULL || type_name == NULL)
        return true;
    return mir_append_intent_stmt(routine,
                                  block,
                                  "IntentBinding",
                                  kind,
                                  alias,
                                  type_name,
                                  binding);
}

static bool
mir_append_intent_bindings(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *intent)
{
    ASTNode **bindings;
    size_t binding_count;

    bindings = ast_intent_decl_bindings(intent, &binding_count);
    if (binding_count > 0) {
        for (size_t i = 0; i < binding_count; i++) {
            if (!mir_append_intent_binding(routine, block, bindings[i]))
                return false;
        }
        return true;
    }

    {
        ASTNode **involves_nodes;
        size_t involve_count;
        ASTNode **values;
        size_t value_count;

        involves_nodes = ast_intent_decl_involves(intent, &involve_count);
        for (size_t i = 0; i < involve_count; i++) {
            if (!mir_append_intent_binding(routine, block, involves_nodes[i]))
                return false;
        }
        values = ast_intent_decl_values(intent, &value_count);
        for (size_t i = 0; i < value_count; i++) {
            if (!mir_append_intent_binding(routine, block, values[i]))
                return false;
        }
    }
    return true;
}

static bool
mir_append_intent_values(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *intent)
{
    ASTNode **values;
    size_t value_count;

    values = ast_intent_decl_values(intent, &value_count);
    for (size_t i = 0; i < value_count; i++) {
        ASTNode *value = values[i];
        ASTNode *value_type;
        const char *alias = NULL;
        const char *type_name = NULL;

        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;
        alias = ast_intent_value_alias(value);
        value_type = ast_intent_value_type(value);
        if (value_type != NULL) {
            char *rendered = mir_render_type_name(value_type);
            if (rendered != NULL) {
                type_name = pgy_arena_strdup(&routine->scratch, rendered);
                free(rendered);
            }
        }
        if (alias == NULL || type_name == NULL)
            continue;
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentValue",
                                    routine->name,
                                    alias,
                                    type_name,
                                    value)) {
            return false;
        }
    }
    return true;
}

static bool
mir_append_intent_decl_contracts(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *intent)
{
    const char *mode;
    ASTNode *priority_expr;
    ASTNode *success_expr;

    if (routine == NULL || block == NULL || intent == NULL)
        return false;

    mode = mir_intent_mode_from_rir_policy(routine);
    if (mode == NULL
        || !mir_append_intent_stmt(routine,
                                   block,
                                   "IntentMode",
                                   routine->name,
                                   mode,
                                   routine->name,
                                   intent)) {
        return false;
    }

    priority_expr = ast_intent_decl_priority_expr(intent);
    if (priority_expr != NULL
        && !mir_append_intent_stmt(routine,
                                   block,
                                   "IntentEval",
                                   routine->name,
                                   "priority",
                                   routine->name,
                                   priority_expr)) {
        return false;
    }

    success_expr = ast_intent_decl_success_expr(intent);
    if (success_expr != NULL
        && !mir_append_intent_stmt(routine,
                                   block,
                                   "IntentCheck",
                                   routine->name,
                                   "success",
                                   routine->name,
                                   success_expr)) {
        return false;
    }

    return true;
}

bool
mir_append_intent_step_instructions(MIRRoutine *routine,
                                    MIRBasicBlock *block,
                                    const DIRProgram *dir)
{
    ASTNode *intent;
    const DIRIntentInfo *dir_intent;

    if (routine == NULL || block == NULL || routine->hir_routine == NULL)
        return false;
    if (routine->hir_routine->ast == NULL || routine->hir_routine->ast->type != AST_INTENT_DECL)
        return true;

    intent = routine->hir_routine->ast;
    dir_intent = mir_intent_dir_info(dir, intent);
    if (!mir_append_intent_bindings(routine, block, intent))
        return false;
    if (!mir_append_intent_participants(routine, block, intent))
        return false;
    if (!mir_append_intent_values(routine, block, intent))
        return false;
    if (!mir_append_intent_decl_contracts(routine, block, intent))
        return false;

    {
        ASTNode **steps;
        size_t step_count;

        steps = ast_intent_decl_steps(intent, &step_count);
        for (size_t i = 0; i < step_count; i++) {
            if (!mir_append_intent_step_facts(
                    routine, block, dir_intent, steps[i], i)) {
                return false;
            }
        }
    }
    return true;
}

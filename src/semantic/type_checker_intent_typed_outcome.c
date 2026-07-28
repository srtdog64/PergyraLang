#include "type_checker_internal.h"
#include "diag_codes.h"

#include <stdint.h>
#include <string.h>

static ASTNode *
intent_typed_find_step(const ASTNode *intent,
                       const char *step_name,
                       size_t *index_out)
{
    size_t step_count = 0;
    ASTNode **steps = ast_intent_decl_steps(intent, &step_count);

    if (index_out != NULL)
        *index_out = SIZE_MAX;
    if (step_name == NULL)
        return NULL;
    for (size_t i = 0; i < step_count; i++) {
        const char *candidate = ast_intent_step_name(steps[i]);
        if (candidate != NULL && strcmp(candidate, step_name) == 0) {
            if (index_out != NULL)
                *index_out = i;
            return steps[i];
        }
    }
    return NULL;
}

static bool
intent_typed_report_invalid(SemanticContext *ctx,
                            ASTNode *site,
                            const char *message,
                            const char *detail)
{
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_INTENT_STEP_INVALID,
        PGY_CAUSE_INTENT_STEP,
        PGY_FIX_CHECK_INTENT_STEP_LOWERING,
        site,
        message,
        detail != NULL ? detail : "<unknown>");
    return false;
}

bool
intent_typed_validate_topology(ASTNode *intent, SemanticContext *ctx)
{
    size_t step_count = 0;
    ASTNode **steps = ast_intent_decl_steps(intent, &step_count);
    bool typed = ast_intent_decl_has_typed_result(intent);

    if (!typed) {
        for (size_t i = 0; i < step_count; i++) {
            if (ast_intent_step_predecessor_name(steps[i]) != NULL
                || ast_intent_step_success_variant_name(steps[i]) != NULL
                || ast_intent_step_failure_variant_name(steps[i]) != NULL) {
                return intent_typed_report_invalid(ctx, steps[i],
                    "Legacy Bool intent step '%s' cannot declare typed predecessor/outcome branches.\n"
                    "Fix:\n"
                    "- add an explicit intent return type and labeled terminals\n"
                    "- or remove 'after', step success, and step failure clauses",
                    ast_intent_step_name(steps[i]));
            }
        }
        return true;
    }

    if (step_count == 0) {
        return intent_typed_report_invalid(ctx, intent,
            "Typed intent '%s' requires at least one executable step.",
            ast_intent_decl_name(intent));
    }
    if (ast_intent_decl_success_expr(intent) != NULL
        || ast_intent_decl_failure_expr(intent) != NULL) {
        return intent_typed_report_invalid(ctx, intent,
            "Typed intent '%s' cannot use legacy unlabeled Bool terminals.",
            ast_intent_decl_name(intent));
    }

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = steps[i];
        const char *predecessor = ast_intent_step_predecessor_name(step);
        size_t predecessor_index = SIZE_MAX;
        ASTNode *predecessor_step = NULL;

        if (i == 0 && predecessor != NULL) {
            return intent_typed_report_invalid(ctx, step,
                "Typed intent first step '%s' cannot name a predecessor.",
                ast_intent_step_name(step));
        }
        if (i > 0 && predecessor == NULL) {
            return intent_typed_report_invalid(ctx, step,
                "Typed intent step '%s' requires explicit 'after <Step>' topology.",
                ast_intent_step_name(step));
        }
        if (predecessor != NULL) {
            predecessor_step = intent_typed_find_step(
                intent, predecessor, &predecessor_index);
            if (predecessor_step == NULL || predecessor_index + 1 != i
                || ast_node_stable_id(predecessor_step) == 0
                || !ast_intent_step_set_predecessor_syntax_id(
                    step, ast_node_stable_id(predecessor_step))) {
                return intent_typed_report_invalid(ctx, step,
                    "Typed intent step '%s' predecessor must name the immediately preceding admitted step with stable identity.",
                    ast_intent_step_name(step));
            }
        }
        if (ast_intent_step_outcome_binding_name(step) == NULL
            || ast_intent_step_success_variant_name(step) == NULL
            || ast_intent_step_success_payload_name(step) == NULL
            || ast_intent_step_failure_variant_name(step) == NULL
            || ast_intent_step_failure_payload_name(step) == NULL) {
            return intent_typed_report_invalid(ctx, step,
                "Typed intent step '%s' requires one bound action outcome and exact success/failure payload patterns.",
                ast_intent_step_name(step));
        }
    }

    {
        const char *success_step_name =
            ast_intent_decl_success_terminal_step(intent);
        size_t success_index = SIZE_MAX;
        ASTNode *success_step = intent_typed_find_step(
            intent, success_step_name, &success_index);
        if (success_step == NULL || success_index + 1 != step_count
            || ast_intent_decl_success_terminal_expr(intent) == NULL
            || ast_node_stable_id(success_step) == 0
            || !ast_intent_decl_set_success_terminal_step_syntax_id(
                intent, ast_node_stable_id(success_step))) {
            return intent_typed_report_invalid(ctx, intent,
                "Typed intent '%s' requires one labeled success terminal for an existing step.",
                ast_intent_decl_name(intent));
        }
        for (size_t i = 0; i < step_count; i++) {
            if (ast_intent_step_predecessor_syntax_id(steps[i])
                == ast_node_stable_id(success_step)) {
                return intent_typed_report_invalid(ctx, intent,
                    "Typed intent success terminal step '%s' must be a leaf, not a predecessor.",
                    success_step_name);
            }
        }
    }

    if (ast_intent_decl_failure_terminal_count(intent) != step_count) {
        return intent_typed_report_invalid(ctx, intent,
            "Typed intent '%s' requires one labeled failure terminal for every step.",
            ast_intent_decl_name(intent));
    }
    for (size_t i = 0;
         i < ast_intent_decl_failure_terminal_count(intent);
         i++) {
        const char *step_name =
            ast_intent_decl_failure_terminal_step(intent, i);
        ASTNode *step = intent_typed_find_step(intent, step_name, NULL);
        if (step == NULL
            || ast_intent_decl_failure_terminal_expr(intent, i) == NULL
            || ast_node_stable_id(step) == 0
            || !ast_intent_decl_set_failure_terminal_step_syntax_id(
                intent, i, ast_node_stable_id(step))) {
            return intent_typed_report_invalid(ctx, intent,
                "Typed intent failure terminal references unknown step '%s'.",
                step_name);
        }
    }
    for (size_t step_index = 0; step_index < step_count; step_index++) {
        uint32_t step_syntax_id = ast_node_stable_id(steps[step_index]);
        size_t matching_terminal_count = 0;

        for (size_t terminal_index = 0;
             terminal_index < ast_intent_decl_failure_terminal_count(intent);
             terminal_index++) {
            if (ast_intent_decl_failure_terminal_step_syntax_id(
                    intent, terminal_index) == step_syntax_id) {
                matching_terminal_count++;
            }
        }
        if (matching_terminal_count != 1) {
            return intent_typed_report_invalid(ctx, intent,
                "Typed intent step '%s' requires exactly one matching failure terminal.",
                ast_intent_step_name(steps[step_index]));
        }
    }
    return true;
}

static bool
intent_typed_find_variant(ASTNode *enum_decl,
                          const char *variant_name,
                          size_t *variant_index_out)
{
    size_t variant_count = 0;
    char **variants = ast_enum_variants(enum_decl, &variant_count);

    for (size_t i = 0; i < variant_count; i++) {
        if (variants[i] != NULL && variant_name != NULL
            && strcmp(variants[i], variant_name) == 0) {
            *variant_index_out = i;
            return true;
        }
    }
    return false;
}

static Type *
intent_typed_resolve_tobject_variant(ASTNode *step,
                                     ASTNode *enum_decl,
                                     const char *variant_name,
                                     size_t *variant_index_out,
                                     SemanticContext *ctx)
{
    ASTNode *payload_type_ast;
    Type *payload_type;
    ASTNode *payload_decl;

    if (!intent_typed_find_variant(
            enum_decl, variant_name, variant_index_out)
        || ast_enum_variant_param_count(enum_decl, *variant_index_out) != 1) {
        intent_typed_report_invalid(ctx, step,
            "Intent step outcome branch '%s' must name one exact enum variant with one payload.",
            variant_name);
        return NULL;
    }
    payload_type_ast = ast_enum_variant_param(
        enum_decl, *variant_index_out, 0);
    payload_type = intent_normalize_type(
        intent_resolve_type_ref(payload_type_ast, ctx));
    payload_decl = payload_type != NULL && payload_type->name != NULL
        ? semantic_find_class_decl_by_name(ctx, payload_type->name)
        : NULL;
    if (payload_type == TYPE_UNKNOWN || payload_type->name == NULL
        || payload_decl == NULL
        || ast_class_nominal_kind(payload_decl) != NOMINAL_DECL_TOBJECT) {
        intent_typed_report_invalid(ctx, step,
            "Intent step outcome branch '%s' payload must be an exact tobject type.",
            variant_name);
        return NULL;
    }
    return payload_type;
}

bool
intent_typed_resolve_step_branches(ASTNode *step,
                                   Type *outcome_type,
                                   SemanticContext *ctx,
                                   Type **success_payload_out,
                                   Type **failure_payload_out)
{
    ASTNode *enum_decl;
    Type *success_payload;
    Type *failure_payload;
    size_t success_index = SIZE_MAX;
    size_t failure_index = SIZE_MAX;
    uint32_t enum_syntax_id;

    if (success_payload_out != NULL)
        *success_payload_out = NULL;
    if (failure_payload_out != NULL)
        *failure_payload_out = NULL;
    if (outcome_type == NULL || outcome_type == TYPE_UNKNOWN
        || outcome_type->name == NULL) {
        return intent_typed_report_invalid(ctx, step,
            "Typed intent step '%s' action outcome must resolve to an enum type.",
            ast_intent_step_name(step));
    }
    enum_decl = semantic_find_enum_decl_by_name(ctx, outcome_type->name);
    if (enum_decl == NULL || ast_node_stable_id(enum_decl) == 0) {
        return intent_typed_report_invalid(ctx, step,
            "Typed intent step '%s' action outcome must name an enum declaration with stable identity.",
            ast_intent_step_name(step));
    }
    success_payload = intent_typed_resolve_tobject_variant(
        step, enum_decl, ast_intent_step_success_variant_name(step),
        &success_index, ctx);
    failure_payload = intent_typed_resolve_tobject_variant(
        step, enum_decl, ast_intent_step_failure_variant_name(step),
        &failure_index, ctx);
    if (success_payload == NULL || failure_payload == NULL
        || success_index == failure_index) {
        return false;
    }

    enum_syntax_id = ast_node_stable_id(enum_decl);
    if (!ast_intent_step_set_outcome_branch_resolution_copy(
            step, true, outcome_type->name, enum_syntax_id,
            success_index, success_payload->name)
        || !ast_intent_step_set_outcome_branch_resolution_copy(
            step, false, outcome_type->name, enum_syntax_id,
            failure_index, failure_payload->name)) {
        semantic_error(ctx, step,
            "Out of memory while sealing typed intent outcome branches");
        return false;
    }
    if (success_payload_out != NULL)
        *success_payload_out = success_payload;
    if (failure_payload_out != NULL)
        *failure_payload_out = failure_payload;
    return true;
}

bool
intent_typed_declare_payload_binding(ASTNode *step,
                                     bool success_branch,
                                     Type *payload_type,
                                     SemanticContext *ctx)
{
    const char *name = success_branch
        ? ast_intent_step_success_payload_name(step)
        : ast_intent_step_failure_payload_name(step);
    Symbol *binding;

    if (name == NULL || name[0] == '\0' || payload_type == NULL
        || payload_type == TYPE_UNKNOWN) {
        return false;
    }
    if (scope_lookup(ctx->scope, name) != NULL) {
        return intent_typed_report_invalid(ctx, step,
            "Typed intent payload binding '%s' conflicts with an existing visible binding.",
            name);
    }
    binding = symbol_create_variable(
        name, payload_type, step->line, step->column);
    if (binding == NULL || !scope_declare(ctx->scope, binding)) {
        symbol_destroy(binding);
        semantic_error(ctx, step,
            "Could not declare typed intent payload binding '%s'", name);
        return false;
    }
    binding->is_mut_binding = false;
    return true;
}

ASTNode *
intent_typed_step_for_failure_terminal(ASTNode *intent, size_t index)
{
    return intent_typed_find_step(
        intent, ast_intent_decl_failure_terminal_step(intent, index), NULL);
}

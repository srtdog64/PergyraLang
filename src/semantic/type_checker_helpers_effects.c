#include "type_checker_internal.h"
#include "../common/string_compat.h"
#include "callable_contract_vocabulary.h"

#include <ctype.h>
#include <string.h>

static uint32_t
parse_effect_word(const char *word)
{
    const PgyCallableContractWordSpec *spec;

    if (word == NULL || *word == '\0')
        return EFFECT_NONE;
    spec = pgy_callable_contract_vocabulary_find(
        PGY_CALLABLE_CONTRACT_AXIS_EFFECT, word);
    if (spec != NULL)
        return spec->mask;
    return UINT32_MAX;
}

static bool
effect_separator(char c)
{
    return c == ',' || c == '|' || c == ' '
        || c == '\t' || c == '\r' || c == '\n';
}

static void
append_effect_name(char *buf, size_t buf_size, const char *name, bool *first)
{
    if (buf == NULL || buf_size == 0 || name == NULL || first == NULL)
        return;

    if (!*first)
        pergyra_str_append(buf, buf_size, ", ");
    pergyra_str_append(buf, buf_size, name);
    *first = false;
}

void
effect_mask_to_string(uint32_t mask, char *buf, size_t buf_size)
{
    bool first = true;

    if (buf == NULL || buf_size == 0)
        return;

    buf[0] = '\0';
    mask = type_effect_mask_closure(mask);
    if (mask == EFFECT_NONE) {
        pergyra_str_append(buf, buf_size, "local");
        return;
    }

    for (size_t i = 0; i < pgy_callable_contract_vocabulary_axis_count(
             PGY_CALLABLE_CONTRACT_AXIS_EFFECT); i++) {
        const PgyCallableContractWordSpec *spec =
            pgy_callable_contract_vocabulary_at_rank(
                PGY_CALLABLE_CONTRACT_AXIS_EFFECT, i);
        if (spec != NULL && spec->mask != 0 &&
            type_effect_mask_has(mask, spec->mask)) {
            append_effect_name(buf, buf_size, spec->spelling, &first);
        }
    }
}

static uint32_t
effects_from_structured_comment(StructuredComment *comment,
                                SemanticContext *ctx,
                                const ASTNode *site)
{
    uint32_t mask = EFFECT_NONE;
    char token[64];

    for (StructuredComment *block = comment; block != NULL; block = block->next) {
        for (size_t i = 0; i < block->tag_count; i++) {
            DocTag *tag = block->tags[i];
            const char *cursor;
            if (tag == NULL || tag->type != DOC_TAG_EFFECTS || tag->content == NULL)
                continue;

            cursor = tag->content;
            while (*cursor != '\0') {
                size_t len = 0;
                while (effect_separator(*cursor))
                    cursor++;
                while (cursor[len] != '\0' && !effect_separator(cursor[len]))
                    len++;
                if (len == 0)
                    break;
                if (len >= sizeof(token))
                    len = sizeof(token) - 1;
                for (size_t j = 0; j < len; j++)
                    token[j] = (char)tolower((unsigned char)cursor[j]);
                token[len] = '\0';

                uint32_t effect = parse_effect_word(token);
                if (effect == UINT32_MAX) {
                    semantic_warning(ctx, site,
                        "Unknown effect tag '%s' in structured comment; expected a registered callable effect",
                        token);
                } else {
                    mask |= effect;
                }
                cursor += len;
            }
        }
    }

    return mask;
}

static bool
structured_comment_has_effects_tag(StructuredComment *comment)
{
    for (StructuredComment *block = comment; block != NULL; block = block->next) {
        for (size_t i = 0; i < block->tag_count; i++) {
            DocTag *tag = block->tags[i];
            if (tag != NULL && tag->type == DOC_TAG_EFFECTS)
                return true;
        }
    }
    return false;
}

uint32_t
declared_effects_from_function_node(ASTNode *node, SemanticContext *ctx,
                                    bool *has_contract_out)
{
    bool has_doc_contract;
    bool has_sig_contract;
    uint32_t doc_effects;
    uint32_t sig_effects;

    if (node == NULL)
        return EFFECT_NONE;

    has_doc_contract =
        structured_comment_has_effects_tag(ast_func_doc_comment(node));
    has_sig_contract = ast_func_has_effects_clause(node);
    doc_effects = effects_from_structured_comment(
        ast_func_doc_comment(node), ctx, node);
    sig_effects = ast_func_declared_effects(node);

    if (has_contract_out != NULL)
        *has_contract_out = has_doc_contract || has_sig_contract;

    return type_effect_mask_closure(doc_effects | sig_effects);
}

void
semantic_record_effect(SemanticContext *ctx, uint32_t effect_mask)
{
    if (ctx == NULL || !ctx->tracking_function_effects)
        return;
    ctx->current_function_effects =
        type_effect_mask_join(ctx->current_function_effects, effect_mask);
    if (effect_mask != EFFECT_NONE)
        semantic_record_body_summary(ctx, BODY_SUMMARY_EFFECTS);
}

/*
 * Record a fine-grained capability (PGY_CAP_* bit) the current code exercises.
 * Capabilities are a pure-union refinement of effects: `program_capabilities`
 * always accumulates (top-level statements included) so the program manifest is
 * complete; `current_function_capabilities` accumulates only inside a function
 * body so the per-function `declared >= used` check (with caps) has a precise,
 * interprocedurally-propagated used set. Unlike effects there is no closure or
 * conflict lattice -- a capability is simply used or not.
 */
void
semantic_record_capability(SemanticContext *ctx, uint32_t capability_mask)
{
    if (ctx == NULL || capability_mask == 0u)
        return;
    ctx->program_capabilities |= capability_mask;
    if (ctx->tracking_function_effects)
        ctx->current_function_capabilities |= capability_mask;
}

void
semantic_record_body_summary(SemanticContext *ctx, uint32_t summary_mask)
{
    if (ctx == NULL || !ctx->tracking_function_effects)
        return;
    ctx->current_function_body_summary |= summary_mask;
}

void
semantic_record_callee_body_summary(SemanticContext *ctx,
                                    const Type *callee_type)
{
    uint32_t summary;
    uint32_t transitive_mask =
        BODY_SUMMARY_MAY_ESCAPE_REF
        | BODY_SUMMARY_DROPS_RESOURCE
        | BODY_SUMMARY_EFFECTS
        | BODY_SUMMARY_REQUIRES_ZONE
        | BODY_SUMMARY_CAUSES_EFFECT
        | BODY_SUMMARY_SPAWNS_TASK
        | BODY_SUMMARY_SENDS_CHANNEL;

    if (ctx == NULL || callee_type == NULL)
        return;
    summary = type_function_body_summary(callee_type) & transitive_mask;
    if (summary != BODY_SUMMARY_NONE)
        semantic_record_body_summary(ctx, summary);
}

void
semantic_record_callable_decl_summary(SemanticContext *ctx,
                                      ASTNode *callable_decl,
                                      const Type *callable_type,
                                      uint32_t declared_effects)
{
    size_t param_count;
    bool has_callable_summary;
    uint32_t callable_summary;

    if (ctx == NULL || callable_decl == NULL
        || callable_decl->type != AST_FUNC_DECL) {
        return;
    }

    has_callable_summary = callable_type != NULL
        && callable_type->kind == TYPE_KIND_FUNCTION
        && type_function_has_body_summary(callable_type);
    callable_summary = has_callable_summary
        ? type_function_body_summary(callable_type)
        : BODY_SUMMARY_NONE;

    if (has_callable_summary) {
        semantic_record_body_summary(ctx,
            callable_summary & (BODY_SUMMARY_EFFECTS
                                | BODY_SUMMARY_REQUIRES_ZONE
                                | BODY_SUMMARY_CAUSES_EFFECT));
    } else {
        if (declared_effects != EFFECT_NONE)
            semantic_record_body_summary(ctx, BODY_SUMMARY_EFFECTS);
        if (ast_func_within_zone(callable_decl) != NULL)
            semantic_record_body_summary(ctx, BODY_SUMMARY_REQUIRES_ZONE);
        if (ast_func_causes_effect(callable_decl) != NULL)
            semantic_record_body_summary(ctx, BODY_SUMMARY_CAUSES_EFFECT);
    }

    param_count = callable_type != NULL
        && callable_type->kind == TYPE_KIND_FUNCTION
        ? type_function_param_count(callable_type)
        : ast_func_param_count(callable_decl);
    for (size_t i = 0; i < param_count; i++) {
        ParamMode mode = callable_type != NULL
            && callable_type->kind == TYPE_KIND_FUNCTION
            ? type_function_param_mode(callable_type, i)
            : PARAM_MODE_DEFAULT;
        if (callable_type == NULL || callable_type->kind != TYPE_KIND_FUNCTION) {
            FuncParam *param = ast_func_param(callable_decl, i);
            mode = param != NULL ? param->mode : PARAM_MODE_DEFAULT;
        }
        if (mode == PARAM_MODE_OWN)
            semantic_record_body_summary(ctx, BODY_SUMMARY_MOVES_PARAM);
        else if (mode == PARAM_MODE_REF)
            semantic_record_body_summary(ctx, BODY_SUMMARY_BORROWS_PARAM);
    }
}

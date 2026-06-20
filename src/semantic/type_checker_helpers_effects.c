#include "type_checker_internal.h"
#include "../common/string_compat.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    uint32_t mask;
} EffectWordSpec;

static const EffectWordSpec kEffectWordSpecs[] = {
    {"alloc", EFFECT_ALLOC},
    {"authority", EFFECT_AUTHORITY},
    {"collapse", EFFECT_COLLAPSE},
    {"io", EFFECT_IO},
    {"local", EFFECT_NONE},
    {"nondeterministic", EFFECT_NONDETERMINISTIC},
    {"remote", EFFECT_REMOTE},
    {"secure", EFFECT_SECURE},
};

static int
effect_word_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const EffectWordSpec *spec = (const EffectWordSpec *)entry;
    return strcmp(name, spec->name);
}

static uint32_t
parse_effect_word(const char *word)
{
    const EffectWordSpec *spec;

    if (word == NULL || *word == '\0')
        return EFFECT_NONE;
    spec = (const EffectWordSpec *)bsearch(
        word,
        kEffectWordSpecs,
        sizeof(kEffectWordSpecs) / sizeof(kEffectWordSpecs[0]),
        sizeof(kEffectWordSpecs[0]),
        effect_word_spec_compare);
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

    if (type_effect_mask_has(mask, EFFECT_SECURE))
        append_effect_name(buf, buf_size, "secure", &first);
    if (type_effect_mask_has(mask, EFFECT_REMOTE))
        append_effect_name(buf, buf_size, "remote", &first);
    if (type_effect_mask_has(mask, EFFECT_NONDETERMINISTIC))
        append_effect_name(buf, buf_size, "nondeterministic", &first);
    if (type_effect_mask_has(mask, EFFECT_COLLAPSE))
        append_effect_name(buf, buf_size, "collapse", &first);
    if (type_effect_mask_has(mask, EFFECT_UNSAFE))
        append_effect_name(buf, buf_size, "unsafe", &first);
    if (type_effect_mask_has(mask, EFFECT_IO))
        append_effect_name(buf, buf_size, "io", &first);
    if (type_effect_mask_has(mask, EFFECT_ALLOC))
        append_effect_name(buf, buf_size, "alloc", &first);
    if (type_effect_mask_has(mask, EFFECT_AUTHORITY))
        append_effect_name(buf, buf_size, "authority", &first);
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
                        "Unknown effect tag '%s' in structured comment; expected secure, remote, nondeterministic, collapse, or local",
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
                                | BODY_SUMMARY_REQUIRES_ZONE));
    } else {
        if (declared_effects != EFFECT_NONE)
            semantic_record_body_summary(ctx, BODY_SUMMARY_EFFECTS);
        if (ast_func_within_zone(callable_decl) != NULL)
            semantic_record_body_summary(ctx, BODY_SUMMARY_REQUIRES_ZONE);
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

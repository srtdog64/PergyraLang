#include "transpiler_mir_match_pattern_emit.h"

#include <stdint.h>
#include <stdio.h>

#include "../parser/ast_api.h"
#include "codegen_match_variant_policy.h"

void
transpiler_mir_match_binding_name(uint32_t case_stable_id,
                                  const char *binding,
                                  char *buf,
                                  size_t buf_size)
{
    if (buf == NULL || buf_size == 0)
        return;
    buf[0] = '\0';
    if (binding == NULL) {
        snprintf(buf, buf_size, "_pgy_match_payload");
        return;
    }
    if (case_stable_id == 0) {
        snprintf(buf, buf_size, "_pgy_match_%s", binding);
        return;
    }
    snprintf(buf, buf_size, "_pgy_match_%s_%u", binding, case_stable_id);
}

bool
transpiler_mir_is_option_destructor(ASTNode *pat,
                                    const char **kind,
                                    const char **binding)
{
    ASTNode *callee;
    ASTNode *payload;
    size_t arg_count;

    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = ast_identifier_name(pat);
        PgyCodegenMatchVariantKind variant =
            pgy_codegen_match_variant_lookup(name);
        if (variant == PGY_MATCH_VARIANT_NONE_CTOR) {
            if (kind != NULL)
                *kind = pgy_codegen_match_variant_name(variant);
            return true;
        }
        return false;
    }

    callee = ast_call_callee(pat);
    arg_count = ast_call_arg_count(pat);
    if (pat->type != AST_CALL
        || callee == NULL
        || callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = ast_identifier_name(callee);
    PgyCodegenMatchVariantKind variant =
        pgy_codegen_match_variant_lookup(name);
    if (name == NULL)
        return false;

    if (variant == PGY_MATCH_VARIANT_NONE_CTOR && arg_count == 0) {
        if (kind != NULL)
            *kind = pgy_codegen_match_variant_name(variant);
        return true;
    }
    if (variant == PGY_MATCH_VARIANT_SOME && arg_count == 1) {
        if (kind != NULL)
            *kind = pgy_codegen_match_variant_name(variant);
        payload = ast_call_argument(pat, 0);
        if (binding != NULL
            && payload != NULL
            && payload->type == AST_IDENTIFIER) {
            *binding = ast_identifier_name(payload);
        }
        return true;
    }
    return false;
}

bool
transpiler_mir_is_result_destructor(ASTNode *pat,
                                    const char **kind,
                                    const char **binding)
{
    ASTNode *callee;
    ASTNode *payload;
    size_t arg_count;

    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    if (pat == NULL || pat->type != AST_CALL
        || ast_call_callee(pat) == NULL
        || ast_call_callee(pat)->type != AST_IDENTIFIER) {
        return false;
    }
    callee = ast_call_callee(pat);
    arg_count = ast_call_arg_count(pat);

    const char *name = ast_identifier_name(callee);
    PgyCodegenMatchVariantKind variant =
        pgy_codegen_match_variant_lookup(name);
    if (name == NULL)
        return false;
    if (pgy_codegen_match_variant_is_result(variant)
        && arg_count == 1) {
        if (kind != NULL)
            *kind = pgy_codegen_match_variant_name(variant);
        payload = ast_call_argument(pat, 0);
        if (binding != NULL
            && payload != NULL
            && payload->type == AST_IDENTIFIER) {
            *binding = ast_identifier_name(payload);
        }
        return true;
    }
    return false;
}

const char *
transpiler_mir_match_payload_field(const char *kind)
{
    return pgy_codegen_match_variant_c_payload_field(
        pgy_codegen_match_variant_lookup(kind));
}

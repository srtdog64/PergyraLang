#include "diagnostic_layer.h"

#include <stdbool.h>
#include <string.h>

static bool
diag_tag_contains(const char *text, const char *needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

static bool
diag_tag_starts_with(const char *text, const char *prefix)
{
    size_t prefix_len;

    if (text == NULL || prefix == NULL)
        return false;

    prefix_len = strlen(prefix);
    return strncmp(text, prefix, prefix_len) == 0;
}

static bool
diag_tag_equals(const char *text, const char *expected)
{
    return text != NULL && expected != NULL && strcmp(text, expected) == 0;
}

static bool
diag_tag_contains_any(const char *text, const char *const *needles,
                      size_t needle_count)
{
    for (size_t i = 0; i < needle_count; i++) {
        if (diag_tag_contains(text, needles[i]))
            return true;
    }
    return false;
}

#define DIAG_ARRAY_LEN(values) (sizeof(values) / sizeof((values)[0]))

const char *
diagnostic_layer_name(DiagnosticLayer layer)
{
    switch (layer) {
    case DIAG_LAYER_SYNTAX:
        return "syntax";
    case DIAG_LAYER_TYPE:
        return "type";
    case DIAG_LAYER_RESOURCE:
        return "resource";
    case DIAG_LAYER_CONCURRENCY:
        return "concurrency";
    case DIAG_LAYER_DOMAIN:
        return "domain";
    case DIAG_LAYER_BACKEND:
        return "backend";
    case DIAG_LAYER_DRIVER:
        return "driver";
    case DIAG_LAYER_UNKNOWN:
    default:
        return "unknown";
    }
}

DiagnosticLayer
diagnostic_layer_from_tags(const char *stage,
                           const char *cause_ir,
                           const char *code)
{
    static const char *const syntax_code_terms[] = {"_PARSE_", "_LEX_"};
    static const char *const resource_terms[] = {
        "slot", "pin", "borrow", "move", "release", "ownership", "resource",
        "defer", "view",
    };
    static const char *const resource_code_terms[] = {
        "_SLOT_", "_PIN_", "_BORROW_", "_OWNERSHIP_", "_RESOURCE_",
    };
    static const char *const concurrency_terms[] = {
        "parallel", "channel", "select", "spawn", "async", "await", "cancel",
    };
    static const char *const concurrency_code_terms[] = {
        "_PARALLEL_", "_CHANNEL_", "_ASYNC_", "_AWAIT_", "_CANCEL_",
    };
    static const char *const domain_terms[] = {
        "intent", "zone", "world", "role", "ability", "authority",
        "projection", "effect", "relation", "contract",
    };
    static const char *const domain_code_terms[] = {
        "_INTENT_", "_ZONE_", "_WORLD_", "_ROLE_", "_ABILITY_",
        "_AUTHORITY_", "_PROJECTION_", "_EFFECT_", "_RELATION_", "_CONTRACT_",
    };
    static const char *const backend_code_terms[] = {
        "_LLVM_", "_MIR_", "_CODEGEN_", "PGY_AIR_",
    };
    static const char *const semantic_terms[] = {
        "cfg", "control", "type", "generic", "assign", "infer",
        "predicate", "builtin",
    };
    static const char *const semantic_code_terms[] = {
        "_TYPE_", "_GENERIC_", "_ASSIGN_", "_INFER_", "_BUILTIN_",
        "PGY_SEM_",
    };

    if (diag_tag_starts_with(cause_ir, "lexer:")
        || diag_tag_starts_with(cause_ir, "lex:")
        || diag_tag_starts_with(cause_ir, "parser:")
        || diag_tag_starts_with(cause_ir, "parse:")
        || diag_tag_contains_any(code, syntax_code_terms,
                                 DIAG_ARRAY_LEN(syntax_code_terms))
        || diag_tag_equals(stage, "parse")
        || diag_tag_equals(stage, "lex")) {
        return DIAG_LAYER_SYNTAX;
    }

    if (diag_tag_starts_with(cause_ir, "driver:")
        || diag_tag_contains(code, "_DRIVER_")
        || diag_tag_equals(stage, "driver")) {
        return DIAG_LAYER_DRIVER;
    }

    if (diag_tag_contains_any(cause_ir, resource_terms,
                              DIAG_ARRAY_LEN(resource_terms))
        || diag_tag_contains_any(code, resource_code_terms,
                                 DIAG_ARRAY_LEN(resource_code_terms))) {
        return DIAG_LAYER_RESOURCE;
    }

    if (diag_tag_contains_any(cause_ir, concurrency_terms,
                              DIAG_ARRAY_LEN(concurrency_terms))
        || diag_tag_contains_any(code, concurrency_code_terms,
                                 DIAG_ARRAY_LEN(concurrency_code_terms))) {
        return DIAG_LAYER_CONCURRENCY;
    }

    if (diag_tag_equals(cause_ir, "semantic:assignability_check")) {
        return DIAG_LAYER_TYPE;
    }

    if (diag_tag_contains_any(cause_ir, domain_terms,
                              DIAG_ARRAY_LEN(domain_terms))
        || diag_tag_contains_any(code, domain_code_terms,
                                 DIAG_ARRAY_LEN(domain_code_terms))) {
        return DIAG_LAYER_DOMAIN;
    }

    if (diag_tag_starts_with(cause_ir, "llvm:")
        || diag_tag_starts_with(cause_ir, "mir:")
        || diag_tag_starts_with(cause_ir, "c:")
        || diag_tag_contains_any(code, backend_code_terms,
                                 DIAG_ARRAY_LEN(backend_code_terms))
        || diag_tag_equals(stage, "llvm_codegen")
        || diag_tag_equals(stage, "c_codegen")
        || diag_tag_equals(stage, "mir_validation")
        || diag_tag_equals(stage, "air_verify")) {
        return DIAG_LAYER_BACKEND;
    }

    if (diag_tag_starts_with(cause_ir, "semantic:")
        || diag_tag_contains_any(cause_ir, semantic_terms,
                                 DIAG_ARRAY_LEN(semantic_terms))
        || diag_tag_contains_any(code, semantic_code_terms,
                                 DIAG_ARRAY_LEN(semantic_code_terms))) {
        return DIAG_LAYER_TYPE;
    }

    return DIAG_LAYER_UNKNOWN;
}

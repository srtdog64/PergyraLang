#ifndef PERGYRA_CODEGEN_MATCH_VARIANT_POLICY_H
#define PERGYRA_CODEGEN_MATCH_VARIANT_POLICY_H

#include <stdbool.h>

typedef enum PgyCodegenMatchVariantKind {
    PGY_MATCH_VARIANT_NONE = 0,
    PGY_MATCH_VARIANT_ERR,
    PGY_MATCH_VARIANT_NONE_CTOR,
    PGY_MATCH_VARIANT_OK,
    PGY_MATCH_VARIANT_SOME,
} PgyCodegenMatchVariantKind;

PgyCodegenMatchVariantKind pgy_codegen_match_variant_lookup(const char *name);
const char *pgy_codegen_match_variant_name(PgyCodegenMatchVariantKind kind);

bool pgy_codegen_match_variant_is_option(PgyCodegenMatchVariantKind kind);
bool pgy_codegen_match_variant_is_result(PgyCodegenMatchVariantKind kind);
bool pgy_codegen_match_variant_is_builtin(PgyCodegenMatchVariantKind kind);

const char *pgy_codegen_match_variant_c_option_tag(PgyCodegenMatchVariantKind kind);
const char *pgy_codegen_match_variant_c_result_tag(PgyCodegenMatchVariantKind kind);
const char *pgy_codegen_match_variant_c_payload_field(PgyCodegenMatchVariantKind kind);

unsigned pgy_codegen_match_variant_llvm_tag(PgyCodegenMatchVariantKind kind);
unsigned pgy_codegen_match_variant_result_payload_index(PgyCodegenMatchVariantKind kind);

#endif /* PERGYRA_CODEGEN_MATCH_VARIANT_POLICY_H */

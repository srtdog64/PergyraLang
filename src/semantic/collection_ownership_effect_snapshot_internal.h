/* Private storage of the one immutable collection evidence owner. */
#ifndef PGY_COLLECTION_OWNERSHIP_EFFECT_SNAPSHOT_INTERNAL_H
#define PGY_COLLECTION_OWNERSHIP_EFFECT_SNAPSHOT_INTERNAL_H
#include "collection_ownership_fact.h"
#include "indexed_string_field_lineage_internal.h"
#include "indexed_string_nonowner_risk_internal.h"
#include "indexed_string_definition_recipe.h"
#include "indexed_string_permanent_element.h"

struct PgyCollectionEffectSnapshot {
    const ASTNode *program;
    uint32_t program_id;
    PgyCollectionEffectCaller *callers;
    FunctionParamFlowIndexClaims **claims;
    PgyCollectionExpectedEffect *effects;
    IndexedStringFieldRecipe **source_uses;
    IndexedStringNonownerRisk **nonowner_risks;
    IndexedStringDefinitionRecipe **string_definitions;
    IndexedStringPermanentElementRecipe **permanent_elements;
    SemanticNominalResultSnapshot **nominals;
    size_t caller_count, effect_count, sealed_callers, sealed_effects;
    size_t source_use_count, sealed_source_uses, nominal_count;
    size_t nonowner_risk_count, sealed_nonowner_risks;
    size_t string_definition_count, sealed_string_definitions;
    size_t permanent_element_count, sealed_permanent_elements;
    size_t refs;
    bool owns_source_payloads;
};
#endif

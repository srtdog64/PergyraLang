#ifndef PERGYRA_TYPE_CHECKER_RESOLUTION_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_RESOLUTION_INTERNAL_H

#include "type_checker.h"

void semantic_type_resolution_record_type_ref_dependency(
    SemanticContext *ctx,
    const ASTNode *consumer_site,
    const char *consumer_name,
    const ASTNode *provider_type_ref,
    const char *reason);
void semantic_type_resolution_collect_type_refs(
    ASTNode *type_node,
    SemanticContext *ctx,
    const ASTNode *consumer_site,
    const char *consumer_name,
    const char *reason);
void semantic_type_resolution_record_resolved_type(SemanticContext *ctx,
                                                   ASTNode *type_node,
                                                   Type *resolved_type);
void semantic_type_resolution_record_owned_resolved_type(SemanticContext *ctx,
                                                         ASTNode *type_node,
                                                         Type *resolved_type);
Type *semantic_type_resolution_lookup_metadata_type_ref(SemanticContext *ctx,
                                                        ASTNode *type_node);
Type *semantic_type_resolution_lookup_metadata_name_or_alias(
    SemanticContext *ctx,
    const char *name);
Type *semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
    SemanticContext *ctx,
    const char *name,
    ASTNode *site);
Type *semantic_type_resolution_metadata_alias_type(SemanticContext *ctx,
                                                  ASTNode *type_node);
Type *semantic_type_resolution_metadata_builtin_singleton(const char *name);
Type *semantic_type_resolution_metadata_named_builtin_or_shell_singleton(
    const char *name);
bool semantic_type_resolution_metadata_type_ref_has_no_generic_args(
    const ASTNode *type_node);
bool semantic_type_resolution_metadata_stable_builtin_shell_arity(
    const char *name,
    size_t *out_min,
    size_t *out_max);
Type *semantic_type_resolution_metadata_stable_constructed_shell(
    const char *name,
    size_t argc);
bool semantic_type_resolution_metadata_stable_slot_like_shell(const char *name);
bool semantic_type_resolution_metadata_name_is_shadowed_class(
    SemanticContext *ctx,
    const char *name);
bool semantic_type_resolution_reject_invalid_stable_shell_arity(
    SemanticContext *ctx,
    ASTNode *type_node);
bool semantic_type_resolution_reject_invalid_stable_constructed_type(
    SemanticContext *ctx,
    ASTNode *type_node);
bool semantic_type_resolution_reject_unknown_bare_named_type(
    SemanticContext *ctx,
    ASTNode *type_node);
void semantic_type_resolution_free_owned_type(Type *type);
void semantic_type_resolution_free_metadata(SemanticContext *ctx);
void semantic_type_resolution_try_record_stable_constructed_type(
    SemanticContext *ctx,
    ASTNode *type_node);
void semantic_type_resolution_collect_generic_contract_inventory(
    GenericParams *gp,
    WhereClause *wc,
    SemanticContext *ctx,
    const ASTNode *owner,
    const char *owner_kind,
    const char *owner_name);
void semantic_type_resolution_record_string_dependency(
    SemanticContext *ctx,
    const ASTNode *consumer_site,
    const char *consumer_name,
    const char *provider_name,
    const char *reason);
void semantic_type_resolution_precollect_required_abilities(
    ASTNode **ability_refs,
    size_t ability_count,
    SemanticContext *ctx,
    const ASTNode *owner,
    const char *consumer_name,
    const char *reason);
void semantic_type_resolution_precollect_body_type_refs(ASTNode *stmt,
                                                        SemanticContext *ctx,
                                                        const ASTNode *owner,
                                                        const char *owner_name);
void semantic_type_resolution_precollect_action_contract(
    ASTNode *method,
    SemanticContext *ctx,
    const char *owner_name_hint);
void semantic_type_resolution_precollect_ability_inventory(ASTNode *ability_decl,
                                                           SemanticContext *ctx);
void semantic_type_resolution_precollect_role_inventory(ASTNode *role_decl,
                                                        SemanticContext *ctx);
void semantic_type_resolution_precollect_class_inventory(ASTNode *class_decl,
                                                         SemanticContext *ctx);
void semantic_type_resolution_precollect_party_inventory(ASTNode *party_decl,
                                                         SemanticContext *ctx);
void semantic_type_resolution_precollect_roster_inventory(ASTNode *roster_decl,
                                                          SemanticContext *ctx);
void semantic_type_resolution_precollect_world_inventory(ASTNode *world_decl,
                                                         SemanticContext *ctx);
void semantic_type_resolution_precollect_zone_inventory(ASTNode *zone_decl,
                                                        SemanticContext *ctx);
void semantic_type_resolution_precollect_zone_command_inventory(
    ASTNode *zone_decl,
    SemanticContext *ctx);
void semantic_type_resolution_precollect_zone_state_authority_inventory(
    ASTNode *zone_decl,
    SemanticContext *ctx);
void semantic_type_resolution_precollect_zone_refresh_projection_map(
    ASTNode *zone_decl,
    ASTNode *refresh,
    SemanticContext *ctx,
    const char *consumer_label);
void semantic_type_resolution_precollect_intent_inventory(ASTNode *intent_decl,
                                                          SemanticContext *ctx);
void semantic_maybe_print_type_resolution_stats(SemanticContext *ctx);
void semantic_type_resolution_precollect_relation_inventory(ASTNode *relation_decl,
                                                            SemanticContext *ctx);
void semantic_type_resolution_precollect_effect_inventory(ASTNode *effect_decl,
                                                          SemanticContext *ctx);
void semantic_type_resolution_register_top_level_decl(ASTNode *stmt,
                                                      SemanticContext *ctx);
void semantic_type_resolution_register_local_contract_node(SemanticContext *ctx,
                                                           const ASTNode *site,
                                                           const char *label);
void semantic_type_resolution_record_local_contract_dependency(
    SemanticContext *ctx,
    const ASTNode *consumer_site,
    const char *consumer_label,
    const ASTNode *provider_site,
    const char *provider_label,
    const char *reason);
char *semantic_type_resolution_world_zone_slot_label(ASTNode *world_decl,
                                                     const char *slot_name);
char *semantic_type_resolution_world_state_label(ASTNode *world_decl,
                                                 const char *state_name);
char *semantic_type_resolution_zone_slot_label(ASTNode *zone_decl,
                                               const char *slot_name);
char *semantic_type_resolution_zone_layer_label(ASTNode *zone_decl,
                                                const char *slot_name);
char *semantic_type_resolution_zone_state_label(ASTNode *zone_decl,
                                                const char *state_name);
char *semantic_type_resolution_projection_path_label(ASTNode *zone_decl,
                                                     const char *target_slot_name,
                                                     const char *source_slot_name,
                                                     const char *target_field_name,
                                                     const char *source_field_name);
char *semantic_type_resolution_projection_slot_field_label(ASTNode *zone_decl,
                                                           const char *slot_name,
                                                           const char *field_path);
ASTNode *semantic_type_resolution_projection_source_decl(ASTNode *zone_decl,
                                                         const char *slot_name,
                                                         SemanticContext *ctx);
int resolve_projection_source_field_path(ASTNode *program_root,
                                         ASTNode *source_decl,
                                         const char *field_name,
                                         SemanticContext *ctx,
                                         const char **path_out,
                                         Type **field_type_out);
int semantic_resolve_projection_source_field_path(SemanticContext *ctx,
                                                  ASTNode *source_decl,
                                                  const char *field_name,
                                                  const char **path_out,
                                                  Type **field_type_out);
void semantic_type_resolution_precollect_event_inventory(ASTNode *event_decl,
                                                         SemanticContext *ctx);
void semantic_type_resolution_precollect_enum_inventory(ASTNode *enum_decl,
                                                        SemanticContext *ctx);

#endif

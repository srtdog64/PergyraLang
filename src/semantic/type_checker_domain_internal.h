#ifndef PGY_TYPE_CHECKER_DOMAIN_INTERNAL_H
#define PGY_TYPE_CHECKER_DOMAIN_INTERNAL_H

#include "type_checker.h"

size_t count_subject_domain_slots(ASTNode **slots, size_t slot_count);
size_t count_object_domain_slots(ASTNode **slots, size_t slot_count);
size_t count_bindable_domain_slots(ASTNode **slots,
                                   size_t slot_count,
                                   ASTNode **refreshes,
                                   size_t refresh_count);
bool type_check_projection_contract(ASTNode **slots,
                                    size_t slot_count,
                                    const char *owner_label,
                                    const char *owner_name,
                                    ASTNode *site,
                                    const char *object_slot_name,
                                    const char *source_slot_name,
                                    SemanticContext *ctx,
                                    const char *action_name);
bool type_check_overlay_decl_common(ASTNode *node,
                                    SemanticContext *ctx,
                                    const char *name,
                                    SymbolKind kind,
                                    ASTNode **shared_fields,
                                    size_t shared_count,
                                    ASTNode **methods,
                                    size_t method_count,
                                    const char *kind_name);
bool type_check_domain_slots(ASTNode **slots,
                             size_t slot_count,
                             SemanticContext *ctx,
                             const char *kind_name);
bool type_check_domain_slot_initializers(ASTNode **slots,
                                         size_t slot_count,
                                         SemanticContext *ctx,
                                         const char *kind_name);
ASTNode *find_zone_effect_slot(ASTNode *zone, const char *slot_name);
ASTNode *find_zone_relation_slot(ASTNode *zone, const char *slot_name);
ASTNode *find_zone_state(ASTNode *zone, const char *state_name);
bool resolve_zone_effect_state(ASTNode *zone,
                               ASTNode *site,
                               const char *state_name,
                               SemanticContext *ctx,
                               const char *action_name,
                               const char **effect_slot_name,
                               const char **target_slot_name);
bool resolve_zone_relation_state(ASTNode *zone,
                                 ASTNode *site,
                                 const char *state_name,
                                 SemanticContext *ctx,
                                 const char *action_name,
                                 const char **relation_slot_name,
                                 const char **left_slot_name,
                                 const char **right_slot_name);
bool type_check_zone_participant_authority(ASTNode *zone,
                                           ASTNode *site,
                                           const char *participant_slot_name,
                                           SemanticContext *ctx,
                                           const char *action_name);
void type_check_zone_lifecycle_authority_presence(ASTNode *zone,
                                                  ASTNode *site,
                                                  const char *participant_slot_name,
                                                  SemanticContext *ctx,
                                                  const char *action_name,
                                                  const char *lifecycle_kind,
                                                  const char *primary_slot_name,
                                                  const char *secondary_slot_name);
bool type_check_zone_projection_contract(ASTNode *zone,
                                         ASTNode *site,
                                         const char *object_slot_name,
                                         const char *source_slot_name,
                                         SemanticContext *ctx,
                                         const char *action_name);
bool type_check_zone_effect_contract(ASTNode *zone,
                                     ASTNode *apply_like,
                                     const char *effect_slot_name,
                                     const char *target_slot_name,
                                     SemanticContext *ctx,
                                     const char *action_name);
bool type_check_zone_relation_contract(ASTNode *zone,
                                       ASTNode *link_like,
                                       const char *relation_slot_name,
                                       const char *left_slot_name,
                                       const char *right_slot_name,
                                       SemanticContext *ctx,
                                       const char *action_name);
void type_check_zone_authorities(ASTNode *zone, SemanticContext *ctx);
void type_check_zone_layer_slots(ASTNode *zone, SemanticContext *ctx);
void type_check_zone_state_aliases(ASTNode *zone, SemanticContext *ctx);
size_t type_check_zone_shape_warnings(ASTNode *zone, SemanticContext *ctx);
void type_check_zone_projection_rules(ASTNode *zone, SemanticContext *ctx);
void semantic_validate_action_func_contract(ASTNode *node,
                                            SemanticContext *ctx,
                                            ASTNode *enclosing_nominal,
                                            const char *name,
                                            bool is_action);

#endif /* PGY_TYPE_CHECKER_DOMAIN_INTERNAL_H */

#ifndef PERGYRA_TYPE_CHECKER_RESOLUTION_GRAPH_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_RESOLUTION_GRAPH_INTERNAL_H

#include "type_checker.h"

Type *semantic_stage_resolve_type_quiet(ASTNode *type_node,
                                        SemanticContext *ctx,
                                        const ASTNode *consumer_site,
                                        const char *consumer_name,
                                        const char *reason);
Type *semantic_stage_resolve_alias_target_quiet(ASTNode *alias_decl,
                                                SemanticContext *ctx);
void semantic_stage_type_alias_decl(ASTNode *decl, SemanticContext *ctx);
ASTNode *semantic_stage_named_decl_quiet(SemanticContext *ctx,
                                         ASTNodeType decl_type,
                                         const char *provider_name);
void semantic_stage_required_abilities(ASTNode **ability_refs,
                                       size_t ability_count,
                                       SemanticContext *ctx,
                                       const ASTNode *owner,
                                       const char *consumer_name,
                                       const char *reason);
void semantic_stage_generic_contract_nodes(GenericParams *gp,
                                           WhereClause *wc,
                                           SemanticContext *ctx,
                                           ASTNode *owner,
                                           const char *kind_name,
                                           const char *owner_name);
void semantic_stage_function_signature(ASTNode *func_decl,
                                       SemanticContext *ctx,
                                       const char *fallback_name);
void semantic_stage_method_array(ASTNode **methods,
                                 size_t method_count,
                                 SemanticContext *ctx,
                                 const char *fallback_name);
void semantic_stage_event_signature(ASTNode *event_decl,
                                    SemanticContext *ctx);
void semantic_stage_class_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_stage_enum_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_stage_ability_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_stage_role_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_stage_party_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_stage_roster_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_stage_world_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_stage_intent_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_stage_relation_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_stage_effect_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_stage_zone_decl(ASTNode *decl, SemanticContext *ctx);
ASTNode *semantic_find_top_level_decl_by_label(ASTNode *program,
                                               const char *label,
                                               TypeResolutionNodeKind kind);
ASTNode *semantic_find_graph_host_decl(ASTNode *program,
                                       const char *label);
void semantic_stage_top_level_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_type_resolution_record_named_dependency(
    SemanticContext *ctx,
    const ASTNode *consumer_site,
    const char *consumer_name,
    TypeResolutionNodeKind provider_kind,
    const ASTNode *provider_site,
    const char *provider_name,
    const char *reason);
void semantic_type_resolution_precollect_program(ASTNode *program,
                                                 SemanticContext *ctx);
void semantic_run_type_resolution_worklist(ASTNode *program,
                                           SemanticContext *ctx,
                                           size_t *topo_order,
                                           size_t topo_count);
size_t type_resolution_intern_node(TypeResolutionGraph *graph,
                                   TypeResolutionNodeKind kind,
                                   const ASTNode *site,
                                   const char *label);
void type_resolution_add_edge(TypeResolutionGraph *graph,
                              size_t from,
                              size_t to,
                              const char *reason);
bool type_resolution_find_path(TypeResolutionGraph *graph,
                               size_t current,
                               size_t goal,
                               bool *visited,
                               size_t *path,
                               size_t *path_len,
                               size_t path_cap);
bool type_resolution_validate_graph(SemanticContext *ctx);
bool type_resolution_build_topo_order(TypeResolutionGraph *graph,
                                      size_t **out_order,
                                      size_t *out_count);
char *type_resolution_format_cycle(TypeResolutionGraph *graph,
                                   size_t *path,
                                   size_t path_len,
                                   size_t closing_node);

#endif /* PERGYRA_TYPE_CHECKER_RESOLUTION_GRAPH_INTERNAL_H */

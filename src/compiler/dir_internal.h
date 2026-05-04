#ifndef PERGYRA_DIR_INTERNAL_H
#define PERGYRA_DIR_INTERNAL_H

#include "dir.h"

#include <sys/types.h>

bool dir_failf(const char *fmt, ...);

ssize_t dir_find_node_by_name_kind(const DIRProgram *dir, const char *name, DIRNodeKind kind);
ssize_t dir_find_slot_node(const DIRProgram *dir,
                           DIRNodeKind kind,
                           const char *owner_name,
                           const char *slot_name);
ssize_t dir_find_any_node_by_name(const DIRProgram *dir, const char *name);
ssize_t dir_find_type_node_by_name(const DIRProgram *dir, const char *name);
ssize_t dir_find_ability_node_by_name(const DIRProgram *dir, const char *name);
ssize_t dir_find_role_node_by_name(const DIRProgram *dir, const char *name);
ssize_t dir_find_party_node_by_name(const DIRProgram *dir, const char *name);
ssize_t dir_find_roster_node_by_name(const DIRProgram *dir, const char *name);
ssize_t dir_find_zone_node_by_name(const DIRProgram *dir, const char *name);
ssize_t dir_find_effect_node_by_name(const DIRProgram *dir, const char *name);
ssize_t dir_find_relation_node_by_name(const DIRProgram *dir, const char *name);

bool dir_add_node(DIRProgram *dir, DIRNodeKind kind, const char *name, ASTNode *ast);
ssize_t dir_ensure_qualified_slot_node(DIRProgram *dir,
                                       DIRNodeKind kind,
                                       const char *owner_name,
                                       const char *slot_name,
                                       ASTNode *ast);
bool dir_add_named_edge(DIRProgram *dir,
                        DIREdgeKind kind,
                        size_t from_node_id,
                        size_t to_node_id,
                        const char *label,
                        const char *target_name);
const char *type_name(DIRProgram *dir, ASTNode *type_node);
bool dir_domain_slot_is_projection(ASTNode *slot);

bool dir_collect_nodes(DIRProgram *dir, ASTNode *program);
bool dir_collect_intent_info(DIRProgram *dir, size_t from_id, ASTNode *node);
bool dir_collect_zone_edges(DIRProgram *dir, size_t from_id, ASTNode *node);
bool dir_collect_relation_effect_slot_edges(DIRProgram *dir,
                                            size_t from_id,
                                            const char *owner_name,
                                            ASTNode **slots,
                                            size_t slot_count,
                                            ASTNode **refreshes,
                                            size_t refresh_count);
bool dir_collect_edges_and_intents(DIRProgram *dir, ASTNode *program);

#endif

/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_RIR_INTERNAL_H
#define PGY_RIR_INTERNAL_H

#include "rir.h"

extern ASTNode *g_rir_program_root;

char *rir_strdup_fmt(const char *fmt, ...);
ASTNode *rir_find_domain_slot_in_owner(ASTNode *owner, const char *slot_name);
bool append_scope(RIRProgram *rir, RIRScope scope);
void rir_free_flow_blocks(RIRScope *scope);
bool scope_add_fact(RIRScope *scope, RIRFact fact);
bool scope_add_op(RIRScope *scope, RIROp op);
bool scope_ensure_state_summary(RIRScope *scope, const RIRFact *fact);
RIRStateSummary *scope_find_state_summary(RIRScope *scope, const char *name);
const RIRFact *rir_scope_find_projection_fact(const RIRScope *scope,
                                              const char *name);
bool rir_normalize_scope_shared(RIRScope *scope);

const char *rir_type_name(ASTNode *type_node);
const char *rir_expr_name(ASTNode *node);
const char *rir_call_name(ASTNode *call);
const char *rir_rollback_policy_name(IntentRollbackPolicy policy);

bool add_resource_fact(RIRScope *scope,
                       const char *name,
                       ASTNode *type_node,
                       RIRResourceState state,
                       ASTNode *ast);
bool add_param_resource_fact(RIRScope *scope, const char *name, ASTNode *type_node, ASTNode *ast);
bool add_domain_slot_fact(RIRScope *scope, ASTNode *slot);
bool add_projection_fact(RIRScope *scope,
                         const char *target,
                         const char *source,
                         const char *mode,
                         RIRResourceState state,
                         RIRResourceKind kind,
                         ASTNode *ast);
bool add_named_resource_fact(RIRScope *scope,
                             const char *name,
                             const char *type_name_value,
                             RIRResourceKind kind,
                             RIRResourceState state,
                             ASTNode *ast);
bool add_authority_fact(RIRScope *scope, const char *participant, const char *ability, ASTNode *ast);
bool add_intent_policy_fact(RIRScope *scope, const char *key, const char *value, ASTNode *ast);
bool add_op(RIRScope *scope,
            RIROpKind kind,
            const char *subject,
            const char *arg0,
            const char *arg1,
            ASTNode *ast);

void rir_apply_op_to_summary(RIRScope *scope, RIRStateSummary *summary, const RIROp *op);
void rir_apply_op_to_state(RIRResourceKind resource_kind,
                           RIRResourceState *state,
                           bool *had_error,
                           RIROpKind op);

bool rir_walk_node(RIRScope *scope, ASTNode *node);
bool rir_walk_block_node(RIRScope *scope, ASTNode *node);
bool rir_collect_intent_scope(RIRProgram *rir, ASTNode *node);

#endif /* PGY_RIR_INTERNAL_H */

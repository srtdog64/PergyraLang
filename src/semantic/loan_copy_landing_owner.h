#ifndef PGY_LOAN_COPY_LANDING_OWNER_H
#define PGY_LOAN_COPY_LANDING_OWNER_H

#include <stdbool.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;

/* A loan whose owner changes while it lives is copied where it is made
 * (2026-10-20_escaping_loan_copies_where_it_fails): the getter call `call`
 * is selected for a next analysis to take as `Clone(call)`, when that Clone
 * copies the call's result fully and the call changes no storage it shares
 * with its caller. False only when the selection could not be recorded. */
bool semantic_loan_copy_landing_select(SemanticContext *ctx, const ASTNode *call);

/* The call whose result the builtin Clone `call` copies fully, and which
 * changes no storage it shares with its caller, so that nothing of what it
 * read stays shared; NULL when `call` is no such Clone of such a call. */
const ASTNode *semantic_loan_copy_landing_copied_call(SemanticContext *ctx, const ASTNode *call);

#endif

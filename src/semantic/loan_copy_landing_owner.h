#ifndef PGY_LOAN_COPY_LANDING_OWNER_H
#define PGY_LOAN_COPY_LANDING_OWNER_H

#include <stdbool.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;

/* A loan whose owner changes while it lives, or that leaves where no copy
 * can stand in, is copied where it is made
 * (2026-10-20_escaping_loan_copies_where_it_fails,
 * 2026-10-20_loan_copied_where_made_when_its_exit_cannot_be): the getter
 * call `call` is selected for a next analysis to take as `Clone(call)`, when
 * that Clone copies the call's result fully and the call changes no storage
 * it shares with its caller. False only when the selection could not be
 * recorded. */
bool semantic_loan_copy_landing_select(SemanticContext *ctx, const ASTNode *call);

/* A loan that leaves the routine is copied where it leaves: the expression
 * `carrier` whose value takes it out is selected for a next analysis to
 * take as `Clone(carrier)`, when that Clone copies it fully. False only
 * when the selection could not be recorded. */
bool semantic_loan_copy_landing_select_carrier(SemanticContext *ctx, const ASTNode *carrier);

/* The same for a String the type checker refuses where it leaves its owner,
 * before any value is published: the caller has checked `expression` is a
 * String, which Clone copies whole
 * (2026-10-22_string_crossing_copied_where_it_crosses). False only when the
 * selection could not be recorded. */
bool semantic_loan_copy_landing_select_string(SemanticContext *ctx, const ASTNode *expression);

/* String-copy discovery may continue after its own refusal only inside the
 * same routine, so one bounded analysis can collect that routine's remaining
 * exact landing identities. A later diagnostic or routine boundary closes
 * the tail immediately. */
bool semantic_loan_copy_landing_refusal_tail_current(const SemanticContext *ctx);
void semantic_loan_copy_landing_refusal_tail_seal(SemanticContext *ctx);

/* Whether a Clone of `carrier` copies its value fully, so that a copy can
 * stand in where the loan leaves. */
bool semantic_loan_copy_landing_carrier_copies(SemanticContext *ctx, const ASTNode *carrier);

/* The call whose result the builtin Clone `call` copies fully, and which
 * changes no storage it shares with its caller, so that nothing of what it
 * read stays shared; NULL when `call` is no such Clone of such a call. */
const ASTNode *semantic_loan_copy_landing_copied_call(SemanticContext *ctx, const ASTNode *call);

/* Whether the getter call `call` in `caller` is the argument of such a
 * Clone: its loan ends where it is made, so nothing of the owner it borrows
 * from is shared after the copy
 * (2026-10-22_ref_call_loan_copied_where_made). */
bool semantic_loan_copy_landing_ends_loan(SemanticContext *ctx, ASTNode *caller, const ASTNode *call);

#endif

#ifndef PGY_LOAN_COPY_LANDING_OWNER_H
#define PGY_LOAN_COPY_LANDING_OWNER_H

#include <stdbool.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Type Type;

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

/* Select an exact carrier whose current typed owner has already proved the
 * value type.  This is for facts that intentionally retire the carrier's
 * publication row when they move it, while retaining both the carrier's
 * stable identity and the moved binding's type. */
bool semantic_loan_copy_landing_select_typed_carrier(SemanticContext *ctx,
    const ASTNode *carrier, const Type *type);

/* The same for a String the type checker refuses where it leaves its owner,
 * before any value is published: the caller has checked `expression` is a
 * String, which Clone copies whole
 * (2026-10-22_string_crossing_copied_where_it_crosses). False only when the
 * selection could not be recorded. */
bool semantic_loan_copy_landing_select_string(SemanticContext *ctx, const ASTNode *expression);

/* The same for a by-value argument of `type` the type checker refuses beside
 * an `inout` argument naming the same variable, before any value is
 * published: selected only when Clone copies a value of `type` whole
 * (2026-10-23_by_value_argument_beside_its_inout_is_copied). False only when
 * the selection could not be recorded. */
bool semantic_loan_copy_landing_select_typed(SemanticContext *ctx, const ASTNode *expression, const Type *type);

/* Whether this exact carrier is already scheduled for a Clone in the next
 * semantic analysis.  Consumers use this only to avoid selecting a second,
 * dependent copy in the same failed analysis; it is not current admission. */
bool semantic_loan_copy_landing_selected(const SemanticContext *ctx,
    const ASTNode *carrier);

/* String-copy discovery may continue after its own refusal only inside the
 * same routine, so one bounded analysis can collect that routine's remaining
 * exact landing identities. A later diagnostic or routine boundary closes
 * the tail immediately. */
bool semantic_loan_copy_landing_refusal_tail_current(const SemanticContext *ctx);
void semantic_loan_copy_landing_refusal_tail_seal(SemanticContext *ctx);

/* Whether a Clone of `carrier` copies its value fully, so that a copy can
 * stand in where the loan leaves. */
bool semantic_loan_copy_landing_carrier_copies(SemanticContext *ctx, const ASTNode *carrier);

/* Whether `carrier` is the exact compiler-inserted Clone from an earlier
 * landing round and the current typed publication proves that Clone fully
 * independent. Such a carrier already satisfies the copy boundary; it must
 * never be selected again and wrapped in a nested Clone. */
bool semantic_loan_copy_landing_satisfied(SemanticContext *ctx,
    const ASTNode *carrier);

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

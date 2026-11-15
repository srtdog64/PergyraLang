#ifndef PERGYRA_MEMBER_ELEMENT_BORROW_CARRIER_OWNER_H
#define PERGYRA_MEMBER_ELEMENT_BORROW_CARRIER_OWNER_H

/* Carriers of a String element read through an aggregate's member.
 *
 * `table.names[0]` borrows the String storage `table.names` holds. A local
 * that takes that String keeps the borrow: bound to it directly
 * (`let picked = table.names[0]`), built into a constructor
 * (`Outer(.., table.names[0])`), returned by a call that hands the argument
 * back (`Same(table.names[0])`), aliased (`let other = picked`), or pushed
 * into a local Array (`ArrayPush(out, table.names[0])`). A later call that
 * takes `table` by `inout` and does not preserve the field's storage (its
 * sealed field preservation verdict: it may release or replace it) frees
 * what the carrier points at. A carrier read after that call is refused at
 * the call.
 *
 * Pass 2 records each carrier with the definition that filled it, the root
 * binding and top-level field it reads from, and the call whose result holds
 * the String only when that call returns the argument. The hazard is judged
 * after the publication seals, on the routine's program-point claims: the
 * definition finishes before the call, and the carrier is read after it. A
 * call the claims cannot place, or a callee with no summary, counts as
 * holding the String; a missing order proof is refused. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;

typedef struct MemberBorrowCarrierRow {
    const ASTNode *routine;
    const ASTNode *definition;
    const char *carrier_name;
    uint32_t carrier;
    uint32_t root;
    const char *field;
    /* A user call whose result holds the String only if the callee returns
     * the argument at `via_ordinal`; NULL when the value holds it outright. */
    const ASTNode *via_call;
    size_t via_ordinal;
} MemberBorrowCarrierRow;

/* A binding the routine passes to an inout formal; only such a root's
 * carriers are judged. */
typedef struct MemberBorrowInoutRoot {
    const ASTNode *routine;
    uint32_t root;
} MemberBorrowInoutRoot;

/* `argument` is passed to an inout formal. */
void semantic_member_borrow_carrier_record_inout_root(ASTNode *argument,
    SemanticContext *ctx);
/* `binding` takes `value` at `definition` (its let or an assignment). */
void semantic_member_borrow_carrier_record_binding(Symbol *binding,
    const ASTNode *definition, ASTNode *value, SemanticContext *ctx);
/* `call` stores `value` into the Array `receiver` names. */
void semantic_member_borrow_carrier_record_store(ASTNode *receiver,
    const ASTNode *call, ASTNode *value, SemanticContext *ctx);
/* After the publication seals: refuse each carrier read after a call that
 * may release or replace the field it borrows from. */
bool semantic_member_borrow_carrier_finalize(SemanticContext *ctx);

#endif

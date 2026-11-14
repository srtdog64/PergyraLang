#ifndef PERGYRA_CALL_ARGUMENT_MOVE_ORDER_OWNER_H
#define PERGYRA_CALL_ARGUMENT_MOVE_ORDER_OWNER_H

/* Move order inside one call's argument list.
 *
 * Every argument is evaluated before the callee runs. An argument that names
 * a binding (or a path rooted at one) hands the callee that binding's
 * storage; a later argument whose evaluation moves the same binding (an own
 * hand-off inside a nested call) releases that storage before the callee
 * reads it. `Matches(values, Consume(values))` read freed memory. The earlier
 * argument is refused; a binding the call itself takes by `own` is not
 * recorded, since a later read of it is an ordinary use after move.
 *
 * The callee also runs with every argument at once: a String read from a
 * binding's storage (`holder.tables.names[0]`, or a local bound to that
 * read) passed beside that binding as an inout or own argument is freed
 * when the callee releases that formal's storage (its summary carries
 * RELEASE). That String argument is refused. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;

typedef struct Type Type;

typedef struct {
    uint32_t  binding;
    ASTNode  *argument;
    size_t    ordinal;
    bool      reads;
} CallArgumentSiblingRow;

typedef struct {
    Symbol  **bindings;
    ASTNode **arguments;
    size_t    count;
    size_t    capacity;
    CallArgumentSiblingRow *siblings;
    size_t    sibling_count;
    size_t    sibling_capacity;
    ASTNode  *call;
} CallArgumentMoveFrame;

void semantic_call_argument_move_begin(CallArgumentMoveFrame *frame);
/* After `argument` and the call's own handling of it. */
bool semantic_call_argument_move_record(CallArgumentMoveFrame *frame,
    ASTNode *argument, SemanticContext *ctx);
/* After a later argument is typed: refuse each recorded earlier argument
 * whose binding that evaluation moved. */
bool semantic_call_argument_move_check(CallArgumentMoveFrame *frame,
    SemanticContext *ctx);
/* After argument `ordinal` typed `type` and passed in `param_mode` (a
 * ParamMode): its String reads and the binding an inout or own argument
 * hands the callee. */
bool semantic_call_argument_sibling_record(CallArgumentMoveFrame *frame,
    ASTNode *argument, size_t ordinal, const Type *type, int param_mode,
    SemanticContext *ctx);
/* After every argument: a String read from a sibling inout or own
 * argument's binding is judged after Pass 2, against that formal's summary. */
bool semantic_call_argument_sibling_check(CallArgumentMoveFrame *frame,
    ASTNode *call, SemanticContext *ctx);
/* After Pass 2: refuse each recorded String argument whose sibling's
 * formal releases what it holds. */
bool semantic_call_argument_sibling_finalize(SemanticContext *ctx);

/* Which formals release String storage they hold: a routine deep-drops
 * (ArrayDropOwnedStrings) storage rooted at its formal, or hands storage
 * rooted at its formal to an inout or own formal that releases it. Rows are
 * recorded in Pass 2 and closed over the forwarding edges after it. */
typedef struct FormalReleaseRow {
    ASTNode *routine;
    size_t formal;
    ASTNode *callee;         /* NULL: a direct release */
    size_t callee_formal;
    bool releases;
} FormalReleaseRow;
bool semantic_formal_release_record_drop(ASTNode *receiver, SemanticContext *ctx);
bool semantic_formal_release_record_argument(ASTNode *argument, ASTNode *callee,
    size_t callee_formal, int param_mode, SemanticContext *ctx);

typedef struct CallArgumentSiblingPending {
    ASTNode *call;
    uint32_t call_id;
    ASTNode *reader;
    ASTNode *mover;
    size_t mover_ordinal;
    /* reader names the argument itself: its storage is not exclusive. */
    bool storage_release;
} CallArgumentSiblingPending;
void semantic_call_argument_move_finish(CallArgumentMoveFrame *frame);

#endif

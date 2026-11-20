#ifndef PERGYRA_INDEXED_STRING_BORROW_OWNER_H
#define PERGYRA_INDEXED_STRING_BORROW_OWNER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;
typedef struct Scope Scope;
/* A String loan whose Array<String> storage source is not known: every
 * owned release of a String element retires it. Zero means no loan. */
#define INDEXED_STRING_BORROW_UNKNOWN_SOURCE UINT32_MAX
typedef struct Type Type;
/* Conditional local-risk exclusion; no String source or cleanup grant. */
bool semantic_indexed_string_nonowner_record(SemanticContext *ctx,
    const Symbol *candidate, ASTNode *value, ASTNode *sink, size_t actual_ordinal);
typedef enum {
    INDEXED_STRING_PUSH_NOT_APPLICABLE,
    INDEXED_STRING_PUSH_RECORDED,
    INDEXED_STRING_PUSH_FAILED
} IndexedStringPushRecord;
IndexedStringPushRecord semantic_indexed_string_record_private_push(
    SemanticContext *ctx, ASTNode *call, ASTNode *source,
    ASTNode *receiver, uint32_t risk_binding_id);
bool semantic_indexed_string_private_push_current(SemanticContext *ctx,
    const ASTNode *call);
bool semantic_indexed_string_prepare_private_push(SemanticContext *ctx, const ASTNode *call);
void semantic_indexed_string_private_push_destroy(void *receipt);
/* Query at an actual successfully typed String publication/binding seam.
 * Independence does not prove exact payload copying or allocation success. */
bool semantic_indexed_string_independent_result(const ASTNode *expression,
                                               const SemanticContext *ctx);
bool semantic_indexed_string_borrow_publish_source(ASTNode *expression,
    SemanticContext *ctx);
bool semantic_indexed_string_borrow_source_current(ASTNode *expression,
    SemanticContext *ctx, uint32_t *source_id);
bool semantic_indexed_string_field_record(SemanticContext *ctx,
    ASTNode *value, ASTNode *sink, size_t ordinal);
bool semantic_indexed_string_field_source_current(SemanticContext *ctx,
    ASTNode *value, uint32_t *source_id);
bool semantic_indexed_string_borrow_result_dependency_current(SemanticContext *ctx,
    ASTNode *value, const ASTNode **dependency);
bool semantic_indexed_string_borrow_finalize(SemanticContext *ctx);
bool semantic_indexed_string_borrow_prepare(SemanticContext *ctx);
bool semantic_indexed_string_borrow_storage_source_current(ASTNode *expression,
    SemanticContext *ctx, uint32_t *source_id);
/* A NULL callee covers shallow stores and builtin Array mutators, including
 * a non-NULL call site; named handoffs also consume the source relation. */
void semantic_indexed_string_borrow_call_argument(ASTNode *expression,
    ASTNode *call, ASTNode *callee, size_t ordinal, bool constructor,
    SemanticContext *ctx);
/* A return is a distinct retention boundary, not a call with a missing sink.
 * Mutable local Strings register that exact boundary with the definition
 * owner so all reaching replacements are proved before source zero is used. */
void semantic_indexed_string_borrow_return_value(ASTNode *expression,
    ASTNode *return_stmt, SemanticContext *ctx);
/* The String an ArrayPush stores. Where no owner proves that String alive
 * for the receiver, *copy_landing is set instead of a refusal: the push must
 * store an owned copy. Every other refusal is reported as for any argument. */
void semantic_indexed_string_borrow_push_argument(ASTNode *expression,
    ASTNode *call, SemanticContext *ctx, bool *copy_landing);
/* The String an ArraySet stores, checked as any argument. Where no owner
 * proves it alive, the refusal stands and *copy_wanted reports that an owned
 * copy could stand in for it. */
void semantic_indexed_string_borrow_set_argument(ASTNode *expression,
    ASTNode *call, SemanticContext *ctx, bool *copy_wanted);

bool semantic_indexed_string_borrow_initialize_binding(
    Symbol *binding, ASTNode *declaration, ASTNode *initializer, SemanticContext *ctx);
void semantic_indexed_string_borrow_record_assignment(
    Symbol *binding, ASTNode *value, ASTNode *assignment,
    SemanticContext *ctx);
void semantic_indexed_string_borrow_invalidate_deep_drop(
    ASTNode *receiver, SemanticContext *ctx);
/* An owned element replacement or pop retires one payload, as do ListSet and
 * ListRemove on a List<String>. Conservatively invalidate every local String
 * loan from that Array source and every loan of unknown source (which every
 * List loan is); a later use fails while direct reads used to compute the
 * replacement remain valid. */
void semantic_indexed_string_borrow_invalidate_owned_element(
    ASTNode *receiver, SemanticContext *ctx);
/* Loans across loop iterations (indexed_string_loop_loan_owner.c).
 * A for-in binding of String elements is the element read
 * `iterable[cursor]` and carries the loan that read carries: the Array<String>
 * iterable's storage source, or an unknown source for any other iterable.
 * Called where the binding is declared and again where each iteration binds
 * the next element, so a release in one iteration does not outlive it. */
void semantic_indexed_string_borrow_bind_iteration(
    Symbol *binding, ASTNode *iterable, SemanticContext *ctx);
/* String loans visible from `scope` (it and its enclosing scopes up to the
 * routine) whose element was retired; `visible` (optional) receives how many
 * String bindings are visible. A loop body that retires one more is checked
 * again, so a read before the release in the body meets it in the next pass;
 * each such pass retires at least one of the visible loans. A loop summary
 * does not carry them. */
size_t semantic_indexed_string_borrow_retired_loans(const Scope *scope,
    size_t *visible);
bool semantic_indexed_string_borrow_validate_use(
    Symbol *binding, ASTNode *use, SemanticContext *ctx);
/* Loans live across an inout call (indexed_string_call_release_owner.c).
 * A callee that frees a String element of its inout formal (its formal
 * release summary, call_argument_move_order_owner.h) retires the loans of
 * the actual's storage as a direct release does. That summary is final only
 * after Pass 2, so the call cannot retire them where it is typed. Pass 2
 * records, for each String local that may borrow from the actual (its exact
 * Array<String> source, or an unknown source), an edge naming the call; a
 * read of the local records the edges it has there. After Pass 2 a read
 * behind a call whose callee may free an element of that formal is retired:
 * refused for an exact source, copied at each reaching definition for an
 * unknown one where the definition owner can issue that copy, refused
 * otherwise.
 *
 * `argument` of `type` is passed to the inout formal at `actual` of `call`;
 * called after every argument of the call is typed. */
void semantic_indexed_string_borrow_record_call_release(ASTNode *argument,
    const Type *type, ASTNode *call, size_t actual, SemanticContext *ctx);
/* The read `use` of `binding`, which holds call edges (from validate_use). */
bool semantic_indexed_string_borrow_record_call_release_read(Symbol *binding,
    ASTNode *use, SemanticContext *ctx);
/* Branch join of one local's edges: `base` before the branches, `joined`
 * from the branches joined so far (UINT32_MAX: none yet), `incoming` from
 * the next one that falls through. Returns the joined edges, adding a join
 * edge when both branches added calls. */
uint32_t semantic_indexed_string_borrow_call_release_join(SemanticContext *ctx,
    uint32_t base, uint32_t joined, uint32_t incoming);
/* Loops: a loan read in a loop body before a call later in the body meets
 * that call in the next iteration. Mark the reads before the first body
 * pass; at each `continue`, record the edges each local of `outer` (the
 * scopes around the loop, up to the routine) holds there; after each pass,
 * join the edges those locals hold at the body's end, and those recorded at
 * a `continue`, into their reads since the mark. */
typedef struct IndexedStringCallReleaseHead {
    uint32_t binding_id, head;
} IndexedStringCallReleaseHead;
size_t semantic_indexed_string_borrow_call_release_loop_begin(const SemanticContext *ctx);
bool semantic_indexed_string_borrow_call_release_continue(SemanticContext *ctx,
    const Scope *outer, IndexedStringCallReleaseHead **heads, size_t *count,
    size_t *capacity);
void semantic_indexed_string_borrow_call_release_back_edge(SemanticContext *ctx,
    const Scope *outer, size_t reads_from,
    const IndexedStringCallReleaseHead *continues, size_t continue_count);
/* After Pass 2, in the receipts prepare phase: retire each read behind a
 * call whose callee may free an element of the storage it borrows. */
bool semantic_indexed_string_borrow_call_release_receipts(SemanticContext *ctx);
bool semantic_indexed_string_borrow_owner_transfer_allowed(
    ASTNode *receiver, SemanticContext *ctx);
bool semantic_indexed_string_borrow_deferred_release_current(SemanticContext *ctx,
    uint32_t binding_id, uint32_t source_id, bool *reserved);
bool semantic_indexed_string_borrow_array_binding_source(const Symbol *binding,
    uint32_t *source_id);
bool semantic_indexed_string_borrow_array_receiver_source(ASTNode *receiver,
    SemanticContext *ctx, bool *tracked, uint32_t *source_id);

#endif

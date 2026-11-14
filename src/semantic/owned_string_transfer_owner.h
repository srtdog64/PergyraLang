#ifndef PERGYRA_OWNED_STRING_TRANSFER_OWNER_H
#define PERGYRA_OWNED_STRING_TRANSFER_OWNER_H

/* Transfer of String storage to a new owner.
 *
 * An `own String` formal and an ArrayDropOwnedStrings of an Array<String>
 * literal both free the String storage they receive. The value handed over
 * must be an owned allocation (a Concat, StringJoin, scalar ToString, Clone
 * or other independent producer, an own formal, or a user call); a literal,
 * a borrowed formal, a field or element read, an allocator-owned result or a
 * local of unknown provenance is refused. A user call's result is judged
 * after Pass 2: refused when the owned-result examination disproves it,
 * admitted when proved or merely unproved (the native bootstrap boundary for
 * unknown provenance, which the self-host analyzer closes).
 *
 * The identity is the storage source (a SyntaxNodeId): a binding that owns
 * an allocation names itself, an alias (`let alias = text`) or a struct or
 * literal built from one binding shares that binding's source. A transfer
 * releases the source; every binding sharing it is refused at its next use.
 * The released bit of a String binding joins over branches with the String
 * invalidation snapshot; a loop body that releases is checked once more. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;

enum {
    OWNED_STRING_UNKNOWN = 0,
    OWNED_STRING_OWNED,
    OWNED_STRING_BORROWED,
    OWNED_STRING_PENDING
};

typedef struct OwnedStringPendingProof {
    ASTNode *site;
    const ASTNode *producer;
    uint32_t producer_id;
} OwnedStringPendingProof;

void semantic_owned_string_initialize_binding(Symbol *binding,
    ASTNode *initializer, SemanticContext *ctx);
void semantic_owned_string_record_assignment(Symbol *target, ASTNode *value,
    SemanticContext *ctx);
bool semantic_owned_string_validate_use(Symbol *binding, ASTNode *use,
    SemanticContext *ctx);
/* `value` is handed to an own String formal; `site` names the consumer. */
bool semantic_owned_string_transfer(ASTNode *value, const char *consumer,
    SemanticContext *ctx);
/* ArrayDropOwnedStrings of `receiver` whose element provenance the
 * collection fact does not know: an Array<String> literal binding transfers
 * each element; any other receiver is left to the collection fact. */
bool semantic_owned_string_drop_literal_elements(ASTNode *receiver,
    SemanticContext *ctx);
/* `target` (a field or element place) takes `value`: its root's parts may now
 * share storage or hold borrowed String elements. */
void semantic_owned_string_record_member_assignment(ASTNode *target,
    ASTNode *value, SemanticContext *ctx);
/* A plain push of `value` into the Array `receiver` names. */
void semantic_owned_string_record_push(ASTNode *receiver, ASTNode *value,
    SemanticContext *ctx);
/* The binding `argument` names holds parts that share storage or borrowed
 * String elements, so a routine releasing it frees them wrongly. */
bool semantic_owned_string_storage_unexclusive(ASTNode *argument,
    SemanticContext *ctx);
/* After Pass 2: every pending user-call producer has an owned result. */
bool semantic_owned_string_finalize(SemanticContext *ctx);

#endif

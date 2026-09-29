#ifndef PERGYRA_COLLECTION_OWNERSHIP_FACT_H
#define PERGYRA_COLLECTION_OWNERSHIP_FACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;
typedef struct Type Type;

/* Element-lifetime provenance for Array<String>.  This is deliberately
 * orthogonal to Slot and to the physical array descriptor: it records which
 * binding owns the pointed-to strings. */
typedef enum
{
    PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN = 0,
    PGY_STRING_ARRAY_BORROWED_ELEMENTS,
    PGY_STRING_ARRAY_OWNED_ELEMENTS,
    PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
} PgyStringArrayOwnership;

typedef enum
{
    PGY_COLLECTION_DISPOSITION_LIVE = 1,
    PGY_COLLECTION_DISPOSITION_RETIRED
} PgyCollectionDisposition;

typedef enum
{
    PGY_COLLECTION_ORIGIN_UNKNOWN = 0,
    PGY_COLLECTION_ORIGIN_BORROWED_LITERAL,
    PGY_COLLECTION_ORIGIN_MAP_KEYS,
    PGY_COLLECTION_ORIGIN_BINDING,
    PGY_COLLECTION_ORIGIN_EMPTY_LITERAL,
    PGY_COLLECTION_ORIGIN_MEMBER_MOVE,
    PGY_COLLECTION_ORIGIN_CLONE
} PgyCollectionOrigin;

typedef enum
{
    PGY_COLLECTION_EFFECT_NONE = 0,
    PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH,
    PGY_COLLECTION_EFFECT_SHALLOW_MUTATION,
    PGY_COLLECTION_EFFECT_DROP
} PgyCollectionOwnershipEffectKind;

/* Semantic-owned stable row.  Symbol pointers are permitted only as a
 * transient scope lookup while recording or locating this row; they are not
 * semantic identity and are never projected to HIR/MIR. */
typedef struct PgyCollectionOwnershipFact
{
    uint32_t function_syntax_id;
    uint32_t binding_syntax_id;
    uint32_t origin_syntax_id;
    uint32_t source_binding_syntax_id;
    PgyStringArrayOwnership element_ownership;
    PgyCollectionDisposition disposition;
    PgyCollectionOrigin origin;
} PgyCollectionOwnershipFact;

bool semantic_collection_ownership_initialize_binding(
    Symbol *binding,
    const ASTNode *initializer,
    const Type *binding_type,
    SemanticContext *ctx);

bool semantic_collection_reject_unsafe_owned_string_mutation(
    ASTNode *receiver,
    const char *operation,
    SemanticContext *ctx);

bool semantic_collection_reject_moved_member_use(
    ASTNode *member_access,
    SemanticContext *ctx);

bool semantic_collection_admit_owned_string_push(
    ASTNode *receiver,
    SemanticContext *ctx);

bool semantic_collection_restore_moved_member(
    ASTNode *target,
    ASTNode *value,
    SemanticContext *ctx);

bool semantic_collection_admit_owned_string_drop(
    ASTNode *receiver,
    SemanticContext *ctx);

bool semantic_collection_reject_unsafe_string_array_assignment(
    ASTNode *target,
    ASTNode *value,
    const Type *target_type,
    const Type *value_type,
    SemanticContext *ctx);

bool semantic_collection_record_call_effect(
    ASTNode *call,
    ASTNode *receiver,
    PgyCollectionOwnershipEffectKind kind,
    SemanticContext *ctx);

const PgyCollectionOwnershipFact *semantic_collection_ownership_fact_find(
    const SemanticContext *ctx,
    uint32_t function_syntax_id,
    uint32_t binding_syntax_id);

void pgy_collection_ownership_facts_destroy(
    PgyCollectionOwnershipFact *facts,
    size_t count);

#endif

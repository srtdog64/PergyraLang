#ifndef PGY_COLLECTION_OWNED_ELEMENT_REQUIREMENT_INTERNAL_H
#define PGY_COLLECTION_OWNED_ELEMENT_REQUIREMENT_INTERNAL_H

/* Private rows shared by the capture, validation and base requirement owners.
 * The store remains one program-owned fact; these declarations only split its
 * responsibilities into bounded translation units. */

#include "array_storage_element_lifetime_owner.h"
#include "collection_owned_element_requirement_owner.h"
#include "collection_ownership_fact.h"
#include "collection_ownership_fact_internal.h"
#include "function_param_publication_owner.h"

#include <stdint.h>

typedef struct {
    ASTNode *function;
    FuncParam *parameter;
    uint32_t function_id;
    uint32_t parameter_id;
    size_t formal_ordinal;
    ParamMode parameter_mode;
    unsigned requirements;
    bool identity_present;
} OwnedElementParameter;

typedef struct {
    size_t source;
    size_t target;
} OwnedElementForwardingEdge;

typedef struct {
    ASTNode *call;
    uint32_t call_id;
    ASTNode *callee_decl;
    uint32_t callee_id;
    size_t ordinal;
    size_t formal_ordinal;
    FuncParam *formal;
    uint32_t formal_id;
    ASTNode *formal_type;
    uint32_t formal_type_id;
    ASTNode *argument;
    uint32_t argument_id;
    const Type *argument_type;
    size_t target;
    ParamMode parameter_mode;
    PgyStringArrayOwnership element_ownership;
    bool exclusive_storage;
    uint32_t pending_binding_id;
    size_t pending_sequence;
    ArrayStorageReturnElementReceipt element_use;
    bool forwarded_formal;
    /* A local Array<String> may be the exact field carrier of a default or
     * inout aggregate parameter. Its element premise belongs to that
     * aggregate field path, not to an invented local ownership fact. Capture
     * the immutable path while the field is still moved out; restoration may
     * retire the local before the complete caller graph is sealed. */
    ASTNode *aggregate_member_function;
    const ASTNode *aggregate_member_source;
    uint32_t aggregate_member_function_id;
    uint32_t aggregate_member_formal_id;
    uint32_t aggregate_member_source_id;
    uint32_t aggregate_member_source_binding_id;
    uint32_t aggregate_member_leaf_field_id;
    uint64_t aggregate_member_source_birth_serial;
    uint32_t aggregate_member_field_ids[PGY_COLLECTION_MEMBER_PATH_LIMIT];
    size_t aggregate_member_formal_ordinal;
    size_t aggregate_member_field_count;
    size_t aggregate_member_pending_sequence;
    ParamMode aggregate_member_formal_mode;
    bool aggregate_member_premise;
    /* An exact field take is only provisionally exclusive during Pass 2.
     * Finalization must confirm that the selected own formal really retires
     * the detached backing, including when its body is declared later. */
    bool detached_member_requires_release;
} OwnedElementActualSnapshot;

/* A moved aggregate member can be observed before a later owned-element
 * mutation consumes its premise.  The read itself grants nothing: only a
 * later matching consumer marks the receipt required, and finalization then
 * proves that the exact selected formal had a read-only sealed summary. */
typedef struct {
    ASTNode *call;
    ASTNode *callee_decl;
    ASTNode *argument;
    ASTNode *source_function;
    const ASTNode *member_source;
    uint32_t call_id;
    uint32_t callee_id;
    uint32_t argument_id;
    uint32_t argument_binding_id;
    uint32_t target_formal_id;
    uint32_t source_function_id;
    uint32_t source_formal_id;
    uint32_t member_source_id;
    uint32_t root_binding_id;
    uint32_t leaf_field_id;
    uint64_t source_birth_serial;
    uint32_t field_ids[PGY_COLLECTION_MEMBER_PATH_LIMIT];
    size_t actual_ordinal;
    size_t target_formal_ordinal;
    size_t source_formal_ordinal;
    size_t field_count;
    ParamMode target_mode;
    ParamMode source_mode;
    bool required;
} OwnedElementMemberRead;

typedef struct {
    ASTNode *call;
    ASTNode *receiver;
    ASTNode *function;
    const ASTNode *root_value;
    const ASTNode *member_source;
    uint32_t call_id;
    uint32_t receiver_id;
    uint32_t receiver_binding_id;
    uint32_t function_id;
    uint32_t root_value_id;
    uint32_t formal_id;
    uint32_t member_source_id;
    uint32_t root_binding_id;
    uint32_t leaf_field_id;
    uint64_t source_birth_serial;
    size_t formal_ordinal;
    ParamMode formal_mode;
    bool local_result_root;
    bool direct_member_receiver;
    uint32_t field_ids[PGY_COLLECTION_MEMBER_PATH_LIMIT];
    size_t field_count;
} OwnedElementMemberPush;

typedef struct {
    ASTNode *call;
    ASTNode *callee;
    ASTNode *caller;
    ASTNode *argument;
    FuncParam *formal;
    uint32_t call_id;
    uint32_t callee_id;
    uint32_t caller_id;
    uint32_t argument_id;
    uint32_t formal_id;
    size_t argument_ordinal;
    size_t formal_ordinal;
    ParamMode formal_mode;
    /* An intent owns the call identity but has no routine definition graph. */
    bool caller_is_intent;
    const Type *argument_type;
} OwnedElementAggregateActual;

/* Return facts are captured while the value is still typed in its routine.
 * Final validation follows only these facts; it never re-scans a body. */
typedef struct {
    ASTNode *statement;
    ASTNode *value;
    ASTNode *function;
    uint32_t statement_id;
    uint32_t value_id;
    uint32_t value_binding_id;
    uint32_t function_id;
    const Type *value_type;
} OwnedElementAggregateReturn;

typedef enum {
    OWNED_ELEMENT_AGGREGATE_REQUIREMENT_VISITING,
    OWNED_ELEMENT_AGGREGATE_REQUIREMENT_PROVED,
    OWNED_ELEMENT_AGGREGATE_REQUIREMENT_REFUTED,
    OWNED_ELEMENT_AGGREGATE_REQUIREMENT_UNCACHED
} OwnedElementAggregateRequirementState;

/* Finalization seals every producer before validating member pushes.  A
 * requirement result is therefore stable for this store generation and may
 * be reused by its exact callable, formal and member-path identity. */
typedef struct {
    uint32_t function_id;
    uint32_t formal_id;
    uint32_t field_ids[PGY_COLLECTION_MEMBER_PATH_LIMIT];
    size_t field_count;
    OwnedElementAggregateRequirementState state;
} OwnedElementAggregateRequirementMemo;

typedef struct {
    const PublishedNode *publication;
    size_t next_plus_one;
} OwnedElementAggregateAssignmentIndexRow;

typedef struct {
    uint32_t function_id;
    uint32_t binding_id;
    size_t head_plus_one;
} OwnedElementAggregateAssignmentIndexBucket;

typedef struct {
    uint32_t function_id;
    size_t head_plus_one;
} OwnedElementAggregateReturnIndexBucket;

typedef struct {
    uint32_t function_id;
    uint32_t formal_id;
    size_t head_plus_one;
} OwnedElementAggregateActualIndexBucket;

typedef enum {
    OWNED_ELEMENT_AGGREGATE_PROOF_LOCAL,
    OWNED_ELEMENT_AGGREGATE_PROOF_CALL_RESULT
} OwnedElementAggregateProofKind;

typedef struct {
    OwnedElementAggregateProofKind kind;
    uint32_t function_id;
    uint32_t value_id;
    uint32_t call_context_id;
    uint32_t field_ids[PGY_COLLECTION_MEMBER_PATH_LIMIT];
    size_t field_count;
    OwnedElementAggregateRequirementState state;
    size_t cycle_root_plus_one;
    size_t hash_next_plus_one;
} OwnedElementAggregateProofMemo;

typedef enum {
    OWNED_ELEMENT_AGGREGATE_MEMO_ERROR,
    OWNED_ELEMENT_AGGREGATE_MEMO_NEW,
    OWNED_ELEMENT_AGGREGATE_MEMO_VISITING,
    OWNED_ELEMENT_AGGREGATE_MEMO_PROVED,
    OWNED_ELEMENT_AGGREGATE_MEMO_REFUTED
} OwnedElementAggregateMemoLookup;

struct CollectionOwnedElementRequirementStore {
    ASTNode *program;
    OwnedElementParameter *parameters;
    size_t parameter_count;
    size_t parameter_capacity;
    OwnedElementForwardingEdge *edges;
    size_t edge_count;
    size_t edge_capacity;
    OwnedElementActualSnapshot *actuals;
    size_t actual_count;
    size_t actual_capacity;
    OwnedElementMemberRead *member_reads;
    size_t member_read_count;
    size_t member_read_capacity;
    OwnedElementMemberPush *member_pushes;
    size_t member_push_count;
    size_t member_push_capacity;
    OwnedElementAggregateActual *aggregate_actuals;
    size_t aggregate_actual_count;
    size_t aggregate_actual_capacity;
    OwnedElementAggregateReturn *aggregate_returns;
    size_t aggregate_return_count;
    size_t aggregate_return_capacity;
    OwnedElementAggregateRequirementMemo *aggregate_requirement_memos;
    size_t aggregate_requirement_memo_count;
    size_t aggregate_requirement_memo_capacity;
    OwnedElementAggregateAssignmentIndexRow *aggregate_assignment_rows;
    size_t aggregate_assignment_count;
    OwnedElementAggregateAssignmentIndexBucket *aggregate_assignment_buckets;
    size_t aggregate_assignment_bucket_capacity;
    size_t *aggregate_return_next_plus_one;
    OwnedElementAggregateReturnIndexBucket *aggregate_return_buckets;
    size_t aggregate_return_bucket_capacity;
    size_t *aggregate_actual_next_plus_one;
    OwnedElementAggregateActualIndexBucket *aggregate_actual_buckets;
    size_t aggregate_actual_bucket_capacity;
    OwnedElementAggregateProofMemo *aggregate_proof_memos;
    size_t aggregate_proof_memo_count;
    size_t aggregate_proof_memo_capacity;
    size_t *aggregate_proof_memo_buckets;
    size_t aggregate_proof_memo_bucket_capacity;
    bool aggregate_graph_index_ready;
};

bool collection_owned_requirement_failure(
    SemanticContext *ctx, ASTNode *site, const char *detail);
bool collection_owned_is_string_array(const Type *type);
void *collection_owned_requirement_rows_reserve(
    void *rows, size_t *capacity, size_t needed, size_t row_size);
bool collection_owned_requirement_parameter_row(
    CollectionOwnedElementRequirementStore *store,
    uint32_t function_id, uint32_t parameter_id, size_t *row_out);

bool collection_owned_record_default_aggregate_actual(
    ASTNode *call, ASTNode *callee_decl,
    size_t argument_index, size_t formal_index,
    ASTNode *argument, const Type *argument_type,
    ParamMode parameter_mode, SemanticContext *ctx);
bool collection_owned_validate_member_pushes(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store);
bool collection_owned_validate_aggregate_member_requirement(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    uint32_t function_id, uint32_t formal_id,
    const uint32_t *field_ids, size_t field_count, ASTNode *site);
bool collection_owned_validate_direct_member_actual_elements(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    const OwnedElementActualSnapshot *actual);
bool collection_owned_require_prior_member_reads(
    CollectionOwnedElementRequirementStore *store,
    uint32_t source_function_id, uint32_t source_formal_id,
    const ASTNode *member_source, uint32_t member_source_id,
    uint32_t root_binding_id, uint32_t leaf_field_id,
    uint64_t source_birth_serial, const uint32_t *field_ids,
    size_t field_count);
bool collection_owned_element_graph_index_build(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store);
const OwnedElementAggregateAssignmentIndexBucket *
collection_owned_assignment_index_find(
    const CollectionOwnedElementRequirementStore *store,
    uint32_t function_id, uint32_t binding_id);
const OwnedElementAggregateReturnIndexBucket *
collection_owned_return_index_find(
    const CollectionOwnedElementRequirementStore *store,
    uint32_t function_id);
const OwnedElementAggregateActualIndexBucket *
collection_owned_actual_index_find(
    const CollectionOwnedElementRequirementStore *store,
    uint32_t function_id, uint32_t formal_id);
OwnedElementAggregateMemoLookup collection_owned_proof_memo_begin(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    ASTNode *site, OwnedElementAggregateProofKind kind,
    uint32_t function_id, uint32_t value_id, uint32_t call_context_id,
    const uint32_t *field_ids, size_t field_count,
    size_t *memo_index_out);
bool collection_owned_proof_memo_resolve(
    CollectionOwnedElementRequirementStore *store,
    size_t memo_index, bool proved, bool cache_proved);

#endif

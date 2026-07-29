#ifndef PERGYRA_AIR_H
#define PERGYRA_AIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../parser/ast.h"
#include "dir.h"
#include "hir.h"
#include "mir.h"
#include "rir.h"
#include "execution_lane.h"

typedef struct SemanticResult SemanticResult;

typedef enum
{
    AIR_SYNC_UNKNOWN,
    AIR_SYNC_SYNC,
    AIR_SYNC_ASYNC,
    AIR_SYNC_EITHER
} AIRSyncClass;

typedef enum
{
    AIR_FAILURE_UNKNOWN,
    AIR_FAILURE_RECOVERABLE,
    AIR_FAILURE_FATAL,
    AIR_FAILURE_COMPENSABLE
} AIRFailureClass;

typedef enum
{
    AIR_BOUNDARY_UNKNOWN,
    AIR_BOUNDARY_ZONE,
    AIR_BOUNDARY_WORLD,
    AIR_BOUNDARY_PARALLEL,
    AIR_BOUNDARY_IO,
    AIR_BOUNDARY_CHANNEL,
    AIR_BOUNDARY_EXECUTION
} AIRBoundaryKind;

typedef enum
{
    AIR_COMPRESSION_UNKNOWN,
    AIR_COMPRESSION_RETAIN,
    AIR_COMPRESSION_SUMMARIZE,
    AIR_COMPRESSION_ERASE,
    AIR_COMPRESSION_FORBID
} AIRCompressionBudget;

/*
 * Why a compression decision keeps runtime evidence instead of erasing it.
 * Orthogonal to the budget verb: the budget says *what* (retain/summarize/...),
 * the cause says *why a retain was unavoidable*. This is the measurable
 * A/B/C decomposition of abstraction-loss residue (docs/semantics/14):
 *   - INHERENT  (A): the boundary is a runtime fact (concurrency, transfer,
 *                    authority) - no analysis can erase it.
 *   - POLICY    (B): kept by deliberate policy/traceability (a summary digest,
 *                    an always-on safety tag) - removable by opt-out.
 *   - UNPROVEN  (C): retained only because the static analysis could not
 *                    discharge it; a stronger analysis could erase it. This is
 *                    the sole improvable bucket and must trend downward.
 */
typedef enum
{
    AIR_RETAIN_CAUSE_NONE,      /* erased / forbidden - nothing retained */
    AIR_RETAIN_CAUSE_INHERENT,  /* bucket A */
    AIR_RETAIN_CAUSE_POLICY,    /* bucket B */
    AIR_RETAIN_CAUSE_UNPROVEN   /* bucket C */
} AIRRetainCause;

typedef enum
{
    AIR_DRIFT_NONE,
    AIR_DRIFT_SYNC_ASYNC_CONFLICT,
    AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
    AIR_DRIFT_EFFECT_PROPAGATION_MISSING,
    AIR_DRIFT_RELATION_PROPAGATION_MISSING,
    AIR_DRIFT_DAG_DEAD_END_PRESENT,
    /* AIR's declared compression (erase/retain + A/B/C cause) disagrees with the
       measured physical residue: e.g. the program declares nothing retained yet
       an axis runtime call / sync primitive survives the optimized object, or a
       declared erase still emits a call. Populated by the out-of-band erasure
       harness (tests/air_erasure), which holds both the AIR JSON and the `nm`
       facts AIR itself cannot see at compile time. */
    AIR_DRIFT_COMPRESSION_RESIDUE_MISMATCH
} AIRDriftKind;

typedef enum
{
    AIR_EVIDENCE_HIR_ROUTINE,
    AIR_EVIDENCE_HIR_CFG,
    AIR_EVIDENCE_RIR_BOUNDARY,
    AIR_EVIDENCE_RIR_AUTHORITY,
    AIR_EVIDENCE_MIR_CLEANUP,
    AIR_EVIDENCE_MIR_PIN_CLEANUP,
    AIR_EVIDENCE_MIR_TERMINATOR,
    AIR_EVIDENCE_MIR_SELECT_RECEIVE,
    AIR_EVIDENCE_DAG_METADATA,
    AIR_EVIDENCE_DAG_GENERIC,
    AIR_EVIDENCE_DAG_ABILITY,
    AIR_EVIDENCE_RIR_EFFECT_PROPAGATION,
    AIR_EVIDENCE_RIR_RELATION_PROPAGATION,
    AIR_EVIDENCE_OBSERVABILITY_SCHEMA,
    AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY,
    AIR_EVIDENCE_KIND_COUNT
} AIREvidenceKind;

typedef enum
{
    AIR_EVIDENCE_PROVIDER_UNKNOWN,
    AIR_EVIDENCE_PROVIDER_HIR,
    AIR_EVIDENCE_PROVIDER_RIR,
    AIR_EVIDENCE_PROVIDER_MIR,
    AIR_EVIDENCE_PROVIDER_DAG,
    AIR_EVIDENCE_PROVIDER_RUNTIME,
    AIR_EVIDENCE_PROVIDER_COUNT
} AIREvidenceProviderKind;

typedef enum
{
    AIR_EVIDENCE_SUBJECT_UNKNOWN,
    AIR_EVIDENCE_SUBJECT_ROUTINE,
    AIR_EVIDENCE_SUBJECT_CFG,
    AIR_EVIDENCE_SUBJECT_BOUNDARY,
    AIR_EVIDENCE_SUBJECT_AUTHORITY,
    AIR_EVIDENCE_SUBJECT_CLEANUP,
    AIR_EVIDENCE_SUBJECT_PIN_CLEANUP,
    AIR_EVIDENCE_SUBJECT_TERMINATOR,
    AIR_EVIDENCE_SUBJECT_SELECT_RECEIVE,
    AIR_EVIDENCE_SUBJECT_METADATA,
    AIR_EVIDENCE_SUBJECT_GENERIC,
    AIR_EVIDENCE_SUBJECT_ABILITY,
    AIR_EVIDENCE_SUBJECT_EFFECT_PROPAGATION,
    AIR_EVIDENCE_SUBJECT_RELATION_PROPAGATION,
    AIR_EVIDENCE_SUBJECT_OBSERVABILITY_SCHEMA,
    AIR_EVIDENCE_SUBJECT_FRONTIER_POLICY,
    AIR_EVIDENCE_SUBJECT_COUNT
} AIREvidenceSubjectKind;

typedef struct
{
    const char      *intent_owner;
    const char      *step_name;
    size_t           step_index;
    ASTNode         *ast;
    AIRSyncClass     sync_class;
    AIRFailureClass  failure_class;
    const char      *compensation_hook;
    bool             who_from_intent_default;
    bool             who_from_on_receiver;
    bool             who_from_single_participant;
    bool             requires_from_action;
    bool             causes_from_action;
} AIRIntentNode;

typedef struct
{
    AIRBoundaryKind kind;
    const char     *owner_name;
    const char     *source_name;
    size_t          intent_index;
    size_t          step_index;
    ASTNode        *ast;
    uint32_t        source_stable_id;
    AIRSyncClass    sync_class;
    bool            authority_required;
    bool            source_from_intent_default;
    bool            source_from_action;
    bool            source_from_transfer;
    /* Retained for pgy.air.graph.v1 compatibility; active approval provenance
       is explicit or action-inherited, never derived from local `who`. */
    bool            authority_from_zone;
    bool            authority_from_action;
    const char    **authority_names;
    size_t          authority_name_count;
    /* The abilities/contracts this boundary's authority participants must hold
       (the `requires` clause). In Pergyra authority is contract-based, not
       PGY_CAP_*; this is the per-boundary authority<->contract binding a
       capability machine gates on. Strings are owned by the AIR name pool. */
    const char    **required_abilities;
    size_t          required_ability_count;
    bool            has_hir_routine_evidence;
    bool            has_hir_cfg_evidence;
    bool            has_rir_boundary_evidence;
    bool            has_rir_authority_evidence;
    bool            has_mir_pin_cleanup_evidence;
    bool            has_rir_await_local_evidence;
    bool            has_rir_movability_requirement_evidence;
    bool            has_rir_deterministic_fork_join_evidence;
    bool            has_rir_zone_pin_evidence;
    bool            has_rir_live_view_capture_evidence;
    bool            has_rir_raw_slot_capture_evidence;
    bool            has_rir_raw_channel_capture_evidence;
    bool            has_mir_value_capture_evidence;
    /* Surface-declared `spawn blocking` marker (parser fact, docs/146). A
       declaration, not an inference: it feeds the IO/FFI/blocking effect
       evidence so the blocking lane is decided by the classifier, never by a
       backend-side branch on source spelling. */
    bool            has_declared_blocking_evidence;
    const char     *hir_routine_evidence_name;
    const char     *rir_boundary_evidence_scope;
    const char     *rir_authority_evidence_name;
    /* SEA BoundaryCaptureFact (docs/146): the input fact used to classify this
       boundary's ExecutionLane. Stored on AIR so JSON/verifiers can audit the
       evidence instead of trusting a backend-side lane guess. */
    BoundaryCaptureFact boundary_capture;
    /* SEA ExecutionLane fact (docs/146): which runtime lane this boundary's task
       is permitted, derived from boundary evidence by air_boundary_classify_lane.
       Zero-initialised to PGY_LANE_REJECT (0) — an unclassified boundary is
       fail-closed, not silently runnable, until classification sets its lane. */
    PgyExecutionLane execution_lane;
} AIRBoundaryNode;

/*
 * SEA: derive the ExecutionLane for a concurrency boundary from boundary-local
 * authority plus RIR/MIR evidence bits. The classifier must not recover
 * pin/raw/value facts from source kind, boundary kind, source text, or
 * routine-level correlation; missing producer coverage stays conservative. See
 * docs/146 §5.
 */
BoundaryCaptureFact air_boundary_capture_fact(const AIRBoundaryNode *boundary);
PgyExecutionLane air_boundary_classify_lane(const AIRBoundaryNode *boundary);

typedef struct
{
    AIRDriftKind kind;
    size_t       intent_index;
    size_t       boundary_index;
    const char  *message;
} AIRDrift;

typedef struct
{
    AIREvidenceKind         kind;
    AIREvidenceProviderKind provider_kind;
    AIREvidenceSubjectKind  subject_kind;
    size_t                  boundary_index;
    const char             *provider_name;
    const char             *subject_name;
    bool                    has_boundary_shape;
    AIRBoundaryKind         boundary_kind;
    const char             *boundary_owner_name;
    const char             *boundary_source_name;
    size_t                  fact_count;
    size_t                  fallback_count;
} AIREvidenceNode;

typedef struct
{
    AIREvidenceKind kind;
    const char     *provider_name;
    const char     *subject_name;
} AIRPropagationRequirement;

/* A capability-bearing slot operation site (SecureSlot/DeviceSlot), captured from
   the MIR resource-op walk so AIR owns slot identity (type + owning routine), not
   just the bucket-B retain count. Strings are borrowed from MIR, which outlives
   the AIR dump. See docs/semantics/18 (machine-neutral) / 19 (slot facet). */
typedef struct {
    const char *slot;       /* the slot handle, e.g. "hp" (MIR slot_anchor) */
    const char *op;         /* the operation, e.g. "Write"/"Read"/"Release" */
    const char *routine;    /* the routine the slot op lives in */
} AIRSlotSite;

/* Machine-layer contact sites are separate from the generic slot table: AIR
 * owns the target-manifest admission fact and its proof obligations. */
typedef struct {
    const char *slot;
    const char *operation;
    const char *manifest_id;
    const char *physical_grant_id;
    uint64_t    physical_base;
    uint64_t    physical_size;
    const char *physical_mode;
    const char *runtime_operation;
    const char *routine;
    bool        hardware_adequate;
    bool        authority_required;
    bool        live_lease_required;
} AIRMachineLayerSite;

/* A per-operation effect site: a gated ambient builtin call (e.g. Random) bound
   to the capability it requires (RANDOM). This is what lets a capability machine
   gate each effect operation -- the per-operation granularity the program-wide
   capability mask lacks. See docs/semantics/18/19. */
typedef struct {
    const char *op;         /* the gated builtin, e.g. "Random" */
    const char *effect;     /* the capability name, e.g. "RANDOM" */
    uint32_t    cap;        /* the PGY_CAP_* bit */
    const char *routine;    /* the routine the effect op lives in */
} AIREffectSite;

/* A MIR-owned function-parameter flow row projected into AIR.  AIR keeps the
 * stable source identity and routine-local parameter index together so later
 * evidence consumers do not reopen HIR/AST bodies to rediscover the summary. */
typedef struct {
    uint32_t    source_syntax_id;
    const char *routine;
    size_t      parameter_index;
    size_t      parameter_count;
    uint32_t    mask;
} AIRFunctionParamFlowSummary;

#define AIR_LIFECYCLE_NAME_LEN   64
#define AIR_LIFECYCLE_MAX_STATES 32
#define AIR_LIFECYCLE_MAX_OPS    32

typedef struct {
    char     name[AIR_LIFECYCLE_NAME_LEN];
    uint32_t valid_from_mask;
} AIRLifecycleOpFact;

typedef struct {
    char subject[AIR_LIFECYCLE_NAME_LEN];
    char states[AIR_LIFECYCLE_MAX_STATES][AIR_LIFECYCLE_NAME_LEN];
    size_t state_count;
    AIRLifecycleOpFact ops[AIR_LIFECYCLE_MAX_OPS];
    size_t op_count;
} AIRLifecycleStateSpace;

typedef struct AIRProgram
{
    AIRIntentNode   *intents;
    size_t           intent_count;
    AIRBoundaryNode *boundaries;
    size_t           boundary_count;
    AIRDrift        *drifts;
    size_t           drift_count;
    size_t           drift_capacity;
    AIREvidenceNode *evidence_nodes;
    size_t           evidence_count;
    size_t           evidence_capacity;
    AIRPropagationRequirement *propagation_requirements;
    size_t           propagation_requirement_count;
    size_t           propagation_requirement_capacity;
    bool             strict_evidence;
    bool             has_hir_input;
    bool             has_rir_input;
    bool             has_mir_input;
    /* MIR evidence is a one-shot anchored import.  AIR owns the copied
       evidence after this boundary; a second collection would create a
       second authority/lifetime and is therefore rejected. */
    bool             mir_evidence_collection_started;
    bool             mir_evidence_bound;
    uint64_t         mir_evidence_binding_fingerprint;
    size_t           hir_routine_evidence_count;
    size_t           hir_cfg_evidence_count;
    size_t           rir_boundary_evidence_count;
    size_t           rir_authority_evidence_count;
    size_t           mir_cleanup_evidence_count;
    size_t           mir_pin_cleanup_evidence_count;
    size_t           mir_terminator_evidence_count;
    size_t           mir_select_receive_evidence_count;
    size_t           dag_metadata_evidence_count;
    size_t           dag_generic_evidence_count;
    size_t           dag_ability_evidence_count;
    size_t           rir_effect_propagation_required_count;
    size_t           rir_effect_propagation_evidence_count;
    size_t           rir_relation_propagation_required_count;
    size_t           rir_relation_propagation_evidence_count;
    size_t           observability_schema_evidence_count;
    size_t           runtime_frontier_policy_evidence_count;
    /* Bucket C: runtime retains the static analysis could not erase
       (lifecycle CHECK guards at ambiguous joins). The improvable residue. */
    size_t           unproven_retain_count;
    /* Bucket A: inherent concurrency retains (parallel/async/spawn/channel) -
       runtime coordination no analysis can erase. Declared program-wide so a
       bare parallel/channel (not an intent-step boundary) still reports its
       irreducible residue instead of leaving a declared-vs-measured gap. */
    size_t           inherent_concurrency_count;
    /* Bucket B: policy retains for capability-bearing slot operations. These
       are declared from MIR type-layout facts because SecureSlot/DeviceSlot
       token checks may survive even when no intent-step boundary exists. */
    size_t           slot_capability_retain_count;
    char           **owned_names;
    size_t           owned_name_count;
    size_t           owned_name_capacity;
    /* Program-wide capability mask (SemanticResult.program_capabilities),
       captured during dag-evidence collection so AIR -- not a separate manifest
       pipeline -- owns the capability fact. Closes the machine-neutral gap
       (docs/semantics/18: capability was orphaned from AIR) and is the first of
       the calculus terms (docs/semantics/19) AIR must own. */
    uint32_t         program_capabilities;
    /* Slot identity table (SecureSlot/DeviceSlot operation sites), so AIR owns
       which slots exist and where, not just a count. Populated in
       air_collect_mir_evidence. */
    AIRSlotSite     *slot_sites;
    size_t           slot_site_count;
    size_t           slot_site_capacity;
    AIRMachineLayerSite *machine_layer_sites;
    size_t           machine_layer_site_count;
    size_t           machine_layer_site_capacity;
    /* Per-operation effect sites (gated builtin calls bound to their cap),
       populated in air_collect_mir_evidence. */
    AIREffectSite   *effect_sites;
    size_t           effect_site_count;
    size_t           effect_site_capacity;
    AIRFunctionParamFlowSummary *function_param_flow_summaries;
    size_t           function_param_flow_summary_count;
    size_t           function_param_flow_summary_capacity;
    bool             has_function_param_flow_facts;
    /* Declared lifecycle state spaces from semantic lifecycle facts. This is
       the FUZZ-2 manifest surface: state-space tools consume the declared FSM
       here instead of reconstructing lifecycle rules from source text. */
    AIRLifecycleStateSpace *lifecycle_state_spaces;
    size_t           lifecycle_state_space_count;
    /* Set only after the final MIR evidence pass and AIR verification.  The
       projection planner consumes this immutable owner certificate instead of
       rebuilding AIR evidence from source or backend state. */
    bool             verification_certificate_valid;
    uint64_t         verification_certificate_fingerprint;
} AIRProgram;

AIRProgram *air_synthesize(const HIRProgram *hir,
                           const DIRProgram *dir,
                           const RIRProgram *rir,
                           char **error_message);
bool        air_validate(const AIRProgram *air, char **error_message);
bool        air_verify(AIRProgram *air, char **error_message);
bool        air_check_drift(AIRProgram *air, char **error_message);
bool        air_boundary_requires_hir_evidence(const AIRBoundaryNode *boundary);
bool        air_boundary_requires_rir_evidence(const AIRBoundaryNode *boundary);
size_t      air_boundary_authority_name_count(
                const AIRBoundaryNode *boundary);
const char *air_boundary_authority_name_at(
                const AIRBoundaryNode *boundary,
                size_t index);
size_t      air_boundary_required_ability_count(
                const AIRBoundaryNode *boundary);
const char *air_boundary_required_ability_at(
                const AIRBoundaryNode *boundary,
                size_t index);
bool        air_boundary_has_evidence(const AIRProgram *air,
                                      size_t boundary_index,
                                      AIREvidenceKind kind);
const AIREvidenceNode *air_boundary_evidence_node(const AIRProgram *air,
                                                  size_t boundary_index,
                                                  AIREvidenceKind kind);
const char *air_boundary_evidence_provider(const AIRProgram *air,
                                           size_t boundary_index,
                                           AIREvidenceKind kind);
const char *air_boundary_evidence_subject(const AIRProgram *air,
                                          size_t boundary_index,
                                          AIREvidenceKind kind);
bool        air_collect_mir_evidence(AIRProgram *air,
                                     const MIRProgram *mir,
                                     char **error_message);
bool        air_collect_dag_evidence(AIRProgram *air,
                                     const SemanticResult *sem,
                                     char **error_message);
void        air_destroy(AIRProgram *air);
void        air_dump(const AIRProgram *air, FILE *out);
void        air_dump_json(const AIRProgram *air, FILE *out);

const char *air_sync_class_name(AIRSyncClass sync_class);
const char *air_failure_class_name(AIRFailureClass failure_class);
const char *air_boundary_kind_name(AIRBoundaryKind kind);
const char *air_compression_budget_name(AIRCompressionBudget budget);
const char *air_drift_kind_name(AIRDriftKind kind);
const char *air_evidence_kind_name(AIREvidenceKind kind);
const char *air_evidence_provider_kind_name(AIREvidenceProviderKind kind);
const char *air_evidence_subject_kind_name(AIREvidenceSubjectKind kind);
AIRCompressionBudget air_intent_compression_budget(const AIRProgram *air,
                                                   size_t intent_index);
const char *air_intent_compression_reason(const AIRProgram *air,
                                          size_t intent_index);
AIRCompressionBudget air_boundary_compression_budget(
                const AIRBoundaryNode *boundary);
const char *air_boundary_compression_reason(const AIRBoundaryNode *boundary);
const char *air_retain_cause_name(AIRRetainCause cause);
AIRRetainCause air_boundary_retain_cause(const AIRBoundaryNode *boundary);
size_t      air_unproven_retain_count(const AIRProgram *air);
size_t      air_inherent_concurrency_count(const AIRProgram *air);
size_t      air_slot_capability_retain_count(const AIRProgram *air);
uint32_t    air_program_capabilities(const AIRProgram *air);
size_t      air_slot_site_count(const AIRProgram *air);
const AIRSlotSite *air_slot_site_at(const AIRProgram *air, size_t index);
size_t      air_machine_layer_site_count(const AIRProgram *air);
const AIRMachineLayerSite *air_machine_layer_site_at(const AIRProgram *air,
                                                     size_t index);
bool        air_collect_slot_sites(AIRProgram *air, const MIRRoutine *routine,
                                   const char *routine_name);
size_t      air_effect_site_count(const AIRProgram *air);
const AIREffectSite *air_effect_site_at(const AIRProgram *air, size_t index);
size_t      air_function_param_flow_summary_count(const AIRProgram *air);
const AIRFunctionParamFlowSummary *air_function_param_flow_summary_at(
                const AIRProgram *air,
                size_t index);
bool        air_collect_effect_sites(AIRProgram *air, const MIRRoutine *routine,
                                     const char *routine_name);
size_t      air_lifecycle_state_space_count(const AIRProgram *air);
const AIRLifecycleStateSpace *air_lifecycle_state_space_at(
                const AIRProgram *air,
                size_t index);
size_t      air_intent_node_count(const AIRProgram *air);
const AIRIntentNode *air_intent_node_at(const AIRProgram *air, size_t index);
size_t      air_boundary_node_count(const AIRProgram *air);
const AIRBoundaryNode *air_boundary_node_at(const AIRProgram *air,
                                            size_t index);
size_t      air_drift_count(const AIRProgram *air);
const AIRDrift *air_drift_at(const AIRProgram *air, size_t index);

#endif

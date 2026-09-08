# Typed Intent Execution Transition Contract

Status: `BOUNDED SUBSTITUTING / typed transition v3 remains REACHABLE; bounded source-LLVM compiler-purpose root is SUBSTITUTING`
Date: 2026-07-29

## Objective card

- Objective: execute an intent action's typed enum outcome without collapsing
  it to `Bool`, and compensate only transitions that have explicit success
  completion evidence.
- Priority: semantic identity and one SoT; exact payload definitions; explicit
  predecessor topology; fail-closed admission; then backend projection size.
- Named execution subfacts: `mir.intent_step_transition` for each action
  step and `mir.intent_terminal_transition` for each typed intent exit. Both
  are identities under `mir.execution_graph`, not independent top-level
  registry authorities.
- Stable handles: `IntentStepTransitionId` and
  `IntentTerminalTransitionId`, both seeded from stable syntax identity.
- Last legitimate consumers: the intent control-flow planner, native C/LLVM
  projections, and the admitted self-C plan emitter derived from that one
  admitted plan.
- Forbidden fallback: result-to-Bool collapse, success/failure classification
  by variant spelling, payload type reinference, predecessor recovery from
  source/array order, completion after an arbitrary call, rollback of every
  lower source index, or AST/source compensation rescan.
- Falsifying gate: cross-wire otherwise valid outcome type/definition,
  enum-variant identity, tobject payload type, predecessor identity, terminal
  source role, or success completion and reject it before execution.

## Canonical source meaning

This executable rung does not redefine `intent` as a generic multi-action
orchestration function.  The canonical language meaning remains
`docs/01_intent_first_design.md` and `docs/173_intent_axis_strengthening.md`:
an intent closes a real-world purpose and is a source-level cross-axis binder
that elaborates into verification-plane coordination, authority, effect,
boundary, compensation, and trace facts.  Those facts keep their axis-specific
owners; the intent binds and attributes them to one purpose identity.  The
transition plan below is the bounded executable projection of the coordination,
boundary, and compensation subset, not a new universal intent owner.  Neither
one action nor many actions is by itself evidence that an intent is warranted.

The bounded typed form has an explicit intent result and explicit step roles.
The parser may render these as typed internal rows, but MIR never recovers them
from text.

```pergyra
intent Workflow(...) -> WorkflowOutcome {
    step A {
        on outcomeA: worker.RunA(...);
        success: ACommitted(receiptA);
        failure: ARejected(problemA);
        compensate: worker.UndoA(...);
    }

    step B after A {
        on outcomeB: worker.RunB(...);
        success: BCommitted(receiptB);
        failure: BRejected(problemB);
        compensate: worker.UndoB(...);
    }

    success B: WorkflowCommitted(receiptB);
    failure A: WorkflowFailedA(problemA);
    failure B: WorkflowFailedB(problemB);
}
```

The typed AST carrier names are currently frozen as
`IntentReturns`, `IntentStepSuccess`, `IntentStepFailure`,
`IntentTerminalSuccess`, and `IntentTerminalFailure`.  Those typed rows, not
the example spelling above, are the input to semantic/DIR resolution.

Legacy no-arrow intents have an explicit `legacy_bool` result mode and exact
`Bool` return type.  `missing return type => Bool` is not a consumer rule.

## Legacy completion expression carriage

For parsed legacy source, omission of `success:` is normalized to an explicit
Bool `true` expression by the parser. DIR retains that clause's node identity
in `success_expression_node_ids`; its exact-identity receipt includes the row.
Typed intents have no legacy completion row. Source header predicates must be
Bool in the purpose-formal environment, independently of backend selection.

The MIR producer emits exactly one `IntentCheck success` attached to the
enclosing Intent, not to a step. The phase consumer requires its graph and
purpose identity, rejects missing/duplicate/crossed rows, and preserves it in
the existing expression-order projection. Native HIR no longer duplicates
success as an entry-body statement; its direct-call summary remains intact.

The source-C consumer evaluates this graph once after successful step execution.
An earlier step failure skips it and returns false. A false completion result
does not itself introduce a new compensation policy. Native C/LLVM consumers
also reject a missing completion fact; they do not recreate the source default.
Legacy `failure:` evaluation/precedence remains a separate open contract and is
not established by checking its source type.

The MIR step-plan owner joins phase occurrences with the existing participant,
action, placement and typed transition facts once. Routine tree reconstruction
consumes that plan rather than repeating the joins. Typed steps take their
already admitted topology row by step index and cross-seal purpose, step,
action and outcome identity; a tree-owned whole-plan name search is forbidden.
This is a transient view
under the existing Intent execution owner, not a new MIR layer or permission
for ordinary GraphPlan to execute an Intent without placement/cleanup lowering.

Routine mode, priority, binding/phase inventory and cleanup contracts are
admitted before tree projection by `mir_lower/intent_routine_plan_owner.pgy`.
Placement retains participant/declaration rows and the chosen slot's exact
SyntaxNodeId. It reuses the action's admitted receiver binding row; only the
binding owner resolves aliases. `semantic/intent_subject_slot_policy_owner.pgy` owns selection:
an exact compatible alias wins; otherwise one compatible subject slot in the
same Zone is required. A field from another Zone, an unlisted C-environment
field or a non-subject slot cannot satisfy that placement. Direct GraphPlan
target consumers must project the admitted slot identity, not choose another
field by spelling.
The source-C environment adapter uses the same policy while its tree bridge
remains; this does not itself replace ordinary GraphPlan placement/cleanup.

The evaluation phase preserves the DIR target kind: action calls use `on`,
direct nested Intent calls use `intent`. This matches the existing native MIR
carriers. The shared phase owner admits both distinct forms, but the step plan
cross-checks each against its graph target and exact declaration ID; they are
not interchangeable spellings. Nested evaluation cannot carry an action outcome
binding. Target kind, not absence of Zone placement, chooses nested-call emission.

`tests/concept_semantics/intent_predicates.py` checks source observations and
Bool admission. `tests/self_hosted/parity/intent_completion_projection.py`
checks both producers through MIR-to-C, including no-artifact refusal of the
removed entry-statement mirror. The common identity-epoch owner canonicalizes
method spelling using admitted declaration IDs; source-local names are only
cross-checks. These gates do not close ordinary Main/direct GraphPlan LLVM.
The bounded legacy/nested/composite emitters must refuse a nonconstant
completion they cannot execute, rather than silently returning true.

Legacy step execution also belongs solely to the step-attached `IntentEval`
phase row. Native HIR keeps its step/block skeleton and independent direct-call
summary, but no second on/check/compensation statement graph; self MIR no longer
constructs that mirror either. The MIR consumer rejects reintroduced executable
body mirrors instead of selecting one by expression text. Two different steps
may call the same action with identical text: each phase row still executes once.
The repeated-step completion control checks this through both MIR producers.
Typed native MIR's redundant first-step/body graphs and explicitly marked
mirror blocks retain their separate exact-coverage contract until their own
producer retirement. They never supply the executable on row.

## Formal binder identity at the MIR boundary

Intent formals retain their original source declaration identity in the
`binding_source_syntax_id` field of `IntentBinding` and its matching
`IntentParticipant` or `IntentValue` carrier. The native producer projects the
already captured MIR source ID; the Pergyra producer carries the DIR participant
node ID through `SelfMirInstructionRows`. Neither uses the caller's local ID,
the instruction number, nor an ordinal-derived substitute.

The shared binding projection requires positive, distinct IDs within an Intent
and exactly one mirror with the same ID, alias, type and participant/value kind.
Missing, repeated, non-integer or crossed IDs fail closed; nonbinding carriers
must not claim this field. Typed phase projection retains an internal zero in
the aligned nonbinding column, not a fabricated binder identity. The existing
Intent graph receipt includes the retained formal IDs in its seal. These are
identities under the existing MIR owner, not another semantic authority or a
claim that external MIR proves source safety.

The binding projection carries the enclosing Intent's source identity. The
common direct-MIR routine signature joins that exact identity to the indexed
header and retains kind `intent`, formal order, declaration IDs and roles.
Nominal subject/zone/vessel participants retain `participant` / `indirect`;
supported scalar value formals retain `value` / `direct`. This is the existing
Intent binding protocol, not ordinary `ref`, implicit source `inout`, or an
invented copy-out contract. Aggregate value ABI without an owning fact refuses.
The routine instance inventory and legacy graph consume the common signature;
the graph no longer re-derives its formals from raw binding arrays.

Routine-header inventory owns exact identity and array-envelope validation for
both common and legacy generic signatures. Parameter admission owns the Intent
representation rule and checks it even after lower parameter facts are resealed.
The common callable parameter classifier recognizes these admitted participants
through the existing identity-cell layout owner, preserving `participant` and
indirect passing. The same mode on an ordinary function, a crossed ABI or a
missing cell layout is rejected. This classification is not a body-execution plan.
A valid signature alone does not authorize ordinary GraphPlan execution of an
Intent: its purpose, placement, phase and cleanup plan must also be consumed.
General Main composition and direct GraphPlan legalization remain separate
execution obligations.

The legacy Intent execution gate exercises both source pipelines and invokes
the common binder/signature owner on both MIR producers. Its admission-only
probe uses native C, as does driver bootstrap; source C/LLVM execution remains
independently compared. Header-field and resealed protocol negatives exercise
the owners directly, not merely outer checksums. The phase-carrier gate also
checks participant/value ID refusal at the C publication boundary. Neither
executes malformed MIR inputs.

## Producer-owned CFG roles and executable identity

Intent MIR blocks carry `intent_block_role`: `entry`, `body`, `cleanup`,
`rollback`, `invalidation`, `execution`, or `mirror`. Non-Intent blocks omit it.
The native routine/CFG and Pergyra source-CFG construction own these roles.
Consumers require unique roots and a complete, acyclic carrier-body path;
missing roles do not authorize fixed block-count or position-based recovery.
The admitted transition plan exclusively owns typed execution blocks.

Native typed MIR may retain detached HIR mirror blocks. They must be unreachable
and have no normal edges. Their graphs, together with body-carried statement
mirrors, must exactly cover the plan's expression multiset before erasure.
Pergyra typed construction emits no legacy statement spine: on/compensation
expressions live only in the execution plan. It does not fabricate redundant
mirrors to satisfy a native layout assumption. Rollback compensation obligations
still retain the same admitted action identity and are checked against that plan.

The typed expression adapter follows the existing call spine through argument
wrappers and joins its call root to the admitted action declaration ID.
Source-local and canonical names are cross-checks derived from the same callable
owner, not alternative lookup authorities. Missing or foreign IDs and crossed
spellings refuse; canonicalization preserves identity.

`tests/self_hosted/parity/intent_block_role_projection.py` is the focused
both-producer MIR-to-C execution/no-artifact-refusal gate. It reuses the existing
typed compensation gate's independent observations. The identity-prefix fixture
also checks missing/foreign target IDs, crossed names and already canonical
input directly. These gates do not establish direct GraphPlan C/LLVM closure.

## Why step and terminal facts are separate

The step fact owns action execution and branch state.  The routine signature
already owns the intent return type.  A terminal fact consumes one exact step
payload and constructs the routine result.  Combining these facts would make
a step row a second owner of the routine signature and would lose the
distinction between `failure A` and `failure B`.

```text
action result definition
  -> mir.intent_step_transition(A/B)
       -> explicit success payload
       -> explicit failure payload
       -> success-only completion
       -> explicit predecessor compensation
  -> mir.intent_terminal_transition(success B / failure A / failure B)
       -> exact WorkflowOutcome constructor
       -> routine return owned by MIRRoutine.return_type
```

## Step transition seal

One `IntentStepTransitionId` binds all of the following:

- routine and step syntax identity;
- exact action syntax identity;
- outcome instruction block/ID, result definition, declared action return
  type, and enum definition syntax identity;
- source-declared success and failure roles, each sealed by
  `{enum syntax ID, variant index, variant name}` plus payload definition/type
  and exact payload tobject declaration syntax ID;
- exact branch instruction and success/failure successor blocks;
- completion instruction located only in the success successor;
- explicit predecessor transition ID plus predecessor step syntax ID/name;
- ordered compensation expression syntax ID, instruction identity, graph root
  and digest, and exact call-target syntax identity.

The current enum AST stores all variants inside the enum declaration node, so
there is no independent variant node ID yet.  `{enum syntax ID, variant index,
variant name}` is therefore the bounded stable composite identity.  Index or
name alone is never authoritative.

The predecessor row is looked up by its carried handle and checked against its
step syntax ID/name.  Row position is not used to choose it.  The predecessor
graph must be cycle-free.  A failed transition does not compensate itself;
the execution consumer begins from the carried predecessor transition and
walks already-completed transitions only.

## Terminal transition seal

Each terminal row binds:

- success/failure role and terminal syntax identity;
- exact source step transition, source enum variant, payload definition/type,
  and payload tobject declaration syntax ID;
- exact result instruction and expression graph identity;
- result enum definition and `{variant index, variant name}`;
- result payload definition/type/declaration ID, which must be the carried
  source payload identity;
- result type, which must match the enclosing MIR routine return type.

Every step has exactly one failure terminal in the bounded rung.  Only a leaf
step may have a success terminal.  This preserves distinct `failure A` and
`failure B` paths instead of reducing them to one generic failure result.

## tobject payload boundary

An enum payload such as `ReceiptB` or `ProblemB` may be a `tobject`.  Its
nominal declaration owns the tobject shape; the enum declaration owns the
variant payload type; the step transition owns the branch-local payload
definition; the terminal transition owns its last legitimate use.

MIR declaration headers and JSON now carry their stable source syntax IDs.
The producer seals the semantic enum and tobject declaration IDs, while the
plan validator cross-checks the composite variant row and nominal declaration
kind/name/type.  A consumer must not approximate either join with a name-only
success/failure or payload-type rule.

## Current implementation and promotion rule

`src/self_hosted/mir/intent_execution_fact_owner.pgy` implements the in-memory
fact families, explicit predecessor/cycle validation, terminal coverage,
tobject payload cross-seals, and mutation digest.  The focused contract gate
is `tests/self_hosted/parity/intent_execution_fact_contract_owner.sh`.

Native and self frontend/DIR now preserve `IntentReturns`, explicit `after`,
step success/failure payload patterns and labeled terminals.  The focused
frontend gate is
`tests/self_hosted/parity/intent_typed_transition_frontend_owner.sh`.

Native MIR now preserves the exact intent return signature, materializes one
validated `MIRIntentExecutionPlan`, and projects it as
`pgy.selfhost.mir-intent-execution-plan.v3`.  Its step and terminal rows bind
stable declaration, variant, payload-definition, predecessor, completion and
compensation identities. Version 2 added exact success/failure/source/result
payload tobject declaration IDs and removes the version-1 name-only payload
join without a compatibility dual read. Version 3 adds each step's exact
`where` zone spelling and declaration syntax identity; native and self
admission cross-seal the pair against one zone declaration, and target-specific
C/LLVM consumers use the admitted zone without an AST or source re-read.
Native C and LLVM return early into target-specific
projections of that plan; typed mode does not fall through to the legacy Bool
emitter or rescan the AST.  The native execution gate
`tests/intent_typed_transition_native_execution_smoke.sh` observes success,
`failure A`, `failure B`, failure-B compensation of completed A only, and
reverse traversal of multiple predecessor compensations on both backends.

The self top-level machine admission in
`src/self_hosted/mir_lower/machine_layer_fact_owner.pgy` now reads the native
projection exactly once, cross-seals the typed routine result and all routine/action/enum/tobject/
instruction identities, and returns one admitted carrier.  Plan-owned `on`,
compensation, and terminal expression graphs require the persisted sealed shape
`{root,digest,nodes}`.  Codegen and compiler consumers cannot call plan
readiness/digest or reconstruct an expression graph.  The production driver
reaches the Pergyra plan consumer, and the typed direct branch/rollback bypass
is absent and statically ratcheted.

The bounded executable seam therefore satisfies the promotion conditions:

1. one self MIR JSON admission cross-seals the native plan and routine result;
2. admitted self C joins by exact carried identity without source, AST,
   name-only, or row-order recovery;
3. self C, native C, and native LLVM parity holds for success, `failure A`,
   `failure B`, predecessor-only compensation, reverse multiple compensation,
   duplicate expression spelling, zero compensation, and exact step/zone
   observability;
4. schema, digest, graph-shape, action-target, payload-declaration, and zero-
   compensation scaffold mutations reject before any partial self C artifact;
5. the production self-host driver reaches the admitted consumer and the old
   typed direct/rollback path cannot reappear.

The already-installed version-2 transition consumer is `SUBSTITUTING` evidence
only for the bounded input-language MIR-to-self-C transition slice. The
version-3 zone/observability extension is currently `REACHABLE`: its stage-0
self C plus native C/LLVM execution gate is green, but the canonical
Pergyra-built codegen exceeded the unchanged 3072MiB boundary during
definition emission before a replacement installed driver was produced. It
does not promote the typed-transition plan into a universal compiler owner.
A separate 2026-08-25 production rung now calls one bounded real-purpose
`CompilePergyraProgram` intent for source-to-LLVM, consumes a typed terminal
outcome, and deletes the C host's source-MIR/backend subprocess pair. That
exact purpose is `SUBSTITUTING`; it does not install v3, make every compiler
stage a lifecycle step, or close the broader `selfhost.intent_declaration_rows`
registry row. The row remains `BRIDGE` while its remaining declaration/policy
consumers and transition semantics are open.

The active executable gate is
`tests/self_hosted/parity/intent_typed_outcome_compensation_owner.sh`; protocol
admission and mutation ratchets are rooted at
`tests/self_hosted/parity/intent_execution_plan_json_admission_owner.sh` and
`tests/self_hosted/parity/intent_execution_protocol_static_owner.py`.

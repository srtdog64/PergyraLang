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

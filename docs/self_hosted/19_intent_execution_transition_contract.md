# Typed Intent Execution Transition Contract

Status: `OPEN / native MIR plan and C/LLVM execution landed; self JSON admission and hard substitution pending`
Date: 2026-07-29

## Objective card

- Objective: execute an intent action's typed enum outcome without collapsing
  it to `Bool`, and compensate only transitions that have explicit success
  completion evidence.
- Priority: semantic identity and one SoT; exact payload definitions; explicit
  predecessor topology; fail-closed admission; then backend projection size.
- Fact owners: `mir.intent_step_transition` for each action step and
  `mir.intent_terminal_transition` for each typed intent exit.
- Stable handles: `IntentStepTransitionId` and
  `IntentTerminalTransitionId`, both seeded from stable syntax identity.
- Last legitimate consumers: the intent control-flow planner and the C/LLVM
  projections derived from that one admitted plan.
- Forbidden fallback: result-to-Bool collapse, success/failure classification
  by variant spelling, payload type reinference, predecessor recovery from
  source/array order, completion after an arbitrary call, rollback of every
  lower source index, or AST/source compensation rescan.
- Falsifying gate: cross-wire otherwise valid outcome type/definition,
  enum-variant identity, tobject payload type, predecessor identity, terminal
  source role, or success completion and reject it before execution.

## Canonical source meaning

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
  `{enum syntax ID, variant index, variant name}` plus payload definition/type;
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
- exact source step transition, source enum variant, and payload
  definition/type;
- exact result instruction and expression graph identity;
- result enum definition and `{variant index, variant name}`;
- result payload definition/type, which must be the carried source payload;
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
`pgy.selfhost.mir-intent-execution-plan.v1`.  Its step and terminal rows bind
stable declaration, variant, payload-definition, predecessor, completion and
compensation identities.  Native C and LLVM return early into target-specific
projections of that plan; typed mode does not fall through to the legacy Bool
emitter or rescan the AST.  The native execution gate
`tests/intent_typed_transition_native_execution_smoke.sh` observes success,
`failure A`, `failure B`, failure-B compensation of completed A only, and
reverse traversal of multiple predecessor compensations on both backends.

The self producer also preserves the exact typed routine result signature and
its target-neutral in-memory fact owner is split into schema, digest and fact
responsibilities.  The remaining executable boundary is the self top-level MIR
JSON read/admission: it does not yet cache and cross-seal the native routine
return and execution plan, so admitted self C parity remains deliberately
fail-closed rather than borrowing the native plan.

This is not substitution evidence yet.  The seam remains `OPEN` until all of
the following land together:

1. self MIR JSON admission reads the native projection once into the Pergyra
   execution-plan owner and cross-seals its routine result signature;
2. admitted self C consumes that plan without source, AST, name or row-order
   recovery;
3. self C, native C, and native LLVM parity holds for success, `failure A`,
   `failure B`, and ordered multiple compensation;
4. valid-MIR mutations are rejected before any partial self C artifact;
5. the production self-host entrypoint reaches the plan consumer and its old
   direct branch/rollback bypass is deleted and ratcheted.

Only then may the two owner rows be promoted to `ACTIVE` and later `CLOSED` in
`docs/semantics/sot_owner_spine_registry.md`.

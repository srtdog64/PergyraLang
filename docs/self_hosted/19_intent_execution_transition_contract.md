# Typed Intent Execution Transition Contract

Status: `OPEN / frontend, DIR, and fact owner landed; MIR producer, JSON admission, CFG, and execution consumer pending`
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

The MIR declaration JSON does not yet carry stable declaration IDs for enum or
tobject declarations.  The producer must therefore seal the semantic enum
node ID, while admission cross-checks the composite variant row and nominal
declaration kind/name/type.  Adding stable declaration IDs to `pgy.mir.v1` is
a separate SoT closure item; it must not be approximated with a name-only
success/failure rule.

## Current implementation and promotion rule

`src/self_hosted/mir/intent_execution_fact_owner.pgy` implements the in-memory
fact families, explicit predecessor/cycle validation, terminal coverage,
tobject payload cross-seals, and mutation digest.  The focused contract gate
is `tests/self_hosted/parity/intent_execution_fact_contract_owner.sh`.

Native and self frontend/DIR now preserve `IntentReturns`, explicit `after`,
step success/failure payload patterns and labeled terminals.  The focused
frontend gate is
`tests/self_hosted/parity/intent_typed_transition_frontend_owner.sh`.  This is
still carrier and validation evidence: current native MIR JSON emits no step or
terminal transition rows, the intent routine has no typed return signature, and
the HIR CFG has no outcome-tag branch successors.  Native C consequently still
emits a `Bool` intent and cannot define the predecessor payload binding.  A
codegen-side AST rescan is forbidden rather than a temporary implementation.

This is not substitution evidence yet.  The seam remains `OPEN` until all of
the following land together:

1. MIR routine return signature plus typed transition production from the
   landed semantic/DIR rows;
2. MIR JSON projection and one admission read into `MirIntentExecutionPlan`;
3. exact instruction and outcome-tag CFG joins;
4. a production intent entrypoint that consumes the admitted plan;
5. self C, native C, and native LLVM parity for success, `failure A`, and
   `failure B`;
6. valid-MIR mutations rejected before any partial C artifact;
7. deletion and ratcheting of the legacy direct branch/rollback bypass for the
   typed mode.

Only then may the two owner rows be promoted to `ACTIVE` and later `CLOSED` in
`docs/semantics/sot_owner_spine_registry.md`.

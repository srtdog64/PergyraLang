# TeX Semantics Lessons For Pergyra

Last updated: 2026-05-19

Anti-hype rule:

- This document is not a claim that Pergyra implements TeX-like semantics.
- TeX is used here as a mature stress case for specifying scanner boundaries,
  delayed effects, builder/search parameters, diagnostics, and regression
  artifacts.
- A lesson becomes a Pergyra contract only after it has a source owner, a
  diagnostic owner, and a regression gate.

Related documents:

- `docs/37_compiler_contracts.md`
- `docs/73_diagnostic_vocabulary.md`
- `docs/100_beta_readiness_checklist.md`
- `docs/104_air_compiler_architecture.md`
- `docs/112_observability_trace_schema.md`
- `docs/122_managing_intent_drift.md`
- `docs/125_source_of_truth_spine.md`

## 0. Why TeX Is Useful Here

TeX looks strange because it does not behave like a single parser followed by
a single evaluator. It is a set of small scanners, builders, delayed effects,
and output commits. That makes it useful for Pergyra because many Pergyra
surfaces also cross ownership boundaries:

- syntax -> semantic contract;
- semantic contract -> HIR/DIR/RIR/MIR fact;
- high-level intent -> backend/runtime mechanics;
- planner/search decision -> materialized runtime result;
- diagnostic surface -> source-of-truth owner.

The useful lesson is not "copy TeX." The useful lesson is:

> Every boundary must say what is consumed, what is restored, what is captured,
> what is delayed, and what is materialized.

## 1. Scanner Boundary Contract

TeX observations:

- A value scanner can stop before the next visible token.
- A failed lookahead can be returned to input instead of discarded.
- A sentinel such as `\relax` can terminate a scan without becoming part of the
  scanned value.
- A keyword can be recognized without whitespace after a numeric register index.

Pergyra rule:

Every scanner-like surface must declare:

1. the owner of the scan;
2. the stop condition;
3. whether the first non-matching token is consumed, restored, or rejected;
4. whether a sentinel exists;
5. which diagnostic reports the adopted value and the remaining input.

Recommended contract shape:

```text
scanner: <name>
owner: parser | semantic | resolver | runtime input adapter
stop: token class / delimiter / sentinel / EOF
lookahead policy: consume | restore | reject
recovery value: none | default | clamped | explicit reject
diagnostic: code + adopted value + remaining token class
gate: positive case + boundary negative + recovery witness
```

Pergyra application points:

| Surface | Contract Risk | TeX Lesson |
|---|---|---|
| generic type argument parsing | accepting a partial list silently | stop and recovery must be explicit |
| reserved syntax patterns | "not implemented" without reason | diagnostic must explain owner and stable spelling |
| keyword compression | adjacent token ambiguity | whitespace independence must be a deliberate policy |
| module/package resolution | fallback path rediscovery | owner must say what was consumed and what remains |

## 2. Error Recovery Must Expose The Adopted Value

TeX observations:

- Some bad values recover to a default or clamped value.
- The surprising part is not recovery itself. The surprising part is when the
  user cannot see which value was adopted.

Pergyra rule:

Recoverable input repair must expose the adopted value or transformed contract
fact. Silent repair is only acceptable when the repair is not semantically
observable.

Diagnostic minimum:

```text
Reason: <why the source form was outside the contract>
Adopted: <value/fact/path used after recovery>
Remaining: <input or contract fragment not consumed, if relevant>
Fix: <stable spelling or explicit boundary>
```

This fits the existing diagnostic vocabulary direction: actionable `Reason:`
and `Fix:` text is not enough when recovery changes state. The diagnostic also
needs an `Adopted:` fact.

## 3. Delayed Effects Need A Capture Point And A Commit Point

TeX observations:

- `\write` can be immediate or delayed until shipout.
- `\insert` can be accounted by the page builder, then held or dispatched by
  the output routine.
- `\splitmaxdepth` can be captured in an insertion node before later splitting.

Pergyra rule:

Every delayed effect must name:

1. capture point: when source/runtime state is frozen;
2. planner point: when the effect contributes to scheduling or feasibility;
3. commit point: when the effect becomes observable;
4. rollback/cancel point: whether the effect can be dropped before commit;
5. trace point: the minimum event needed to replay it.

Recommended trace vocabulary:

```text
captured:<effect-id> source=<span> owner=<stage> captured=<facts>
planned:<effect-id> planner=<stage> contribution=<summary>
committed:<effect-id> sink=<runtime/backend/output> materialized=<facts>
discarded:<effect-id> reason=<contract reason>
```

Pergyra application points:

| Pergyra Surface | Capture Point | Commit Point |
|---|---|---|
| `intent` step contract | semantic/DIR step validation | runtime step execution or explicit rejection |
| `authority` evidence | RIR/AIR evidence construction | runtime authority check or backend wrapper emission |
| `parallel` frontier | RIR/MIR frontier facts | scheduler/runtime pass boundary |
| Slot/Pin cleanup | MIR cleanup facts | backend cleanup emission and runtime release |
| observability `history` | runtime event append | query materialization |

## 4. Planner-Only Parameters Must Not Look Like Runtime Material

TeX observation:

- `\emergencystretch` changes line-break search. It is not added to the final
  line glue. Overfull boxes may still be materialized.

Pergyra rule:

If a parameter affects planning, search, scheduling, or proof effort but does
not directly materialize in output/runtime state, it must be named and
documented as planner-only.

Bad pattern:

```text
timeout = 100ms
```

when the value is only a scheduler heuristic and not a hard runtime boundary.

Better pattern:

```text
scheduler_hint timeout_budget = 100ms
hard_timeout = 100ms
```

or a contract that says exactly which one it is.

Application points:

- AIR strictness versus backend emission;
- runtime frontier pass limits;
- parallel scheduling hints;
- diagnostics that rank likely causes;
- intent compression heuristics.

## 5. Search Preferences Are Not Hard Constraints

TeX observation:

- `\looseness` asks the paragraph builder to prefer a line-count delta, but it
  may accept the closest feasible result.

Pergyra rule:

Any surface that expresses preference must be typed or labeled differently from
a hard contract.

Recommended naming split:

| Meaning | Suggested Vocabulary |
|---|---|
| must hold or reject | `require`, `must`, `contract`, `invariant` |
| prefer if feasible | `prefer`, `hint`, `bias`, `budget` |
| observe without enforcing | `trace`, `record`, `sample` |
| recover if violated | `fallback`, `recover`, `Result` |

This matters for Pergyra because `intent` syntax is tempting to over-read as a
guarantee. An intent should state a contract only when the compiler/runtime has
an owner and an evidence path for that guarantee.

## 6. Deferred Projection Must Reveal The Chosen Branch

TeX observation:

- A discretionary node has no-break, pre-break, and post-break material. When
  a break is chosen, TeX materializes the pre/post branches and discards the
  replacement branch.

Pergyra rule:

Any projection or compressed authoring surface that can lower to multiple
branches must make the selected branch observable in diagnostics or traces.

Application points:

- `object` versus `tobject` projection;
- `intent` compression that derives `who` or `on`;
- match/action contract inheritance;
- async/select/channel branch selection;
- Result fallback and recovery paths.

Minimum trace:

```text
projection selected=<branch-id> source=<span> reason=<contract fact>
discarded=<branch-id...> owner=<stage>
```

## 7. Accounting And Materialization Can Diverge

TeX observation:

- In held insertion mode, the page builder can account for scaled insertion
  height while the held insertion node remains at natural size for the output
  routine.

Pergyra rule:

If a planner/accounting model uses a transformed value, and the runtime or
materialized object keeps the original value, both values must have names.

Recommended wording:

```text
accounted_value: value used by planner/search/budget
material_value: value carried into runtime/output
```

Application points:

- resource budgets versus actual allocations;
- ABI lowering cost versus source contract size;
- frontier pass estimates versus runtime events;
- diagnostic summary counts versus evidence-node inventory.

Do not let a summary counter become semantic truth. This matches the existing
source-of-truth spine: compatibility counters are transitional only; retired
paths should become quarantine sentinels, while inventory/evidence owners remain
authoritative.

## 8. Exit Hooks Need State-View And Ordering Contracts

TeX observations:

- `\afterassignment` runs after the assignment completes and sees the adopted
  value.
- `\aftergroup` runs after local group restoration and sees the restored outer
  state.
- Multiple `\aftergroup` hooks have observable execution order.

Pergyra rule:

Every cleanup, defer, finalizer, assignment hook, boundary-exit hook, or
post-commit diagnostic hook must declare two facts:

```text
state_view: old | adopted | local-before-restore | outer-after-restore | captured | dual
order_policy: fifo | lifo | priority | dependency | unspecified-forbidden
```

Application points:

| Surface | Required Contract |
|---|---|
| `defer` cleanup | whether cleanup sees local frame state or restored outer state |
| Slot/Pin release | whether release sees pre-drop or post-drop resource facts |
| assignment diagnostics | whether the diagnostic sees rejected, repaired, or adopted value |
| `parallel` boundary finalization | ordering of child cleanup and parent continuation |
| runtime observability append | ordering between state mutation and trace publication |

Test requirement:

Do not only assert that cleanup happened. Assert the ordering witness and the
state view seen by the hook.

## 9. Test Artifacts Must Be Reviewable, Not Just Passing

TeX bug hunting only became useful after each case had:

- a minimal source file;
- a trace source file;
- a run directory;
- log and console output;
- an index row;
- a report explaining expected versus actual behavior;
- a verdict separating bug, documentation gap, baseline, and duplicate.

Pergyra should use the same shape for contract audits:

```text
case/
  source.pgy
  expected.txt
  actual.txt
  trace.json
  report.md

index.csv:
  id,status,area,owner,expected,actual,verdict,gate
```

Status vocabulary:

| Status | Meaning |
|---|---|
| `baseline` | expected behavior, kept as a fixture |
| `candidate` | possible implementation bug |
| `documentation-gap` | implementation is coherent but contract wording is unclear |
| `explicit-reject-gap` | unsupported surface lacks deterministic rejection |
| `duplicate-known` | already covered by an existing issue or gate |

## 10. Pergyra Checklist

Before freezing a new surface, answer these:

```text
[ ] What is the source-of-truth owner?
[ ] Which layer may only consume the fact?
[ ] What exactly is consumed?
[ ] What is restored or left for later?
[ ] What is captured for delayed execution?
[ ] What is planner-only?
[ ] What is materialized?
[ ] What branch was selected?
[ ] What state view does an exit hook observe?
[ ] What ordering policy do multiple hooks use?
[ ] What diagnostic exposes recovery or rejection?
[ ] What trace event can replay the decision?
[ ] What regression gate prevents drift?
```

## 11. Immediate Adoption Targets

These are useful near-term without widening the beta surface:

1. Add `Adopted:` facts to recoverable diagnostics that silently choose a
   fallback value.
2. Mark planner-only values in docs and traces, especially around AIR,
   scheduling, frontier pass limits, and compressed intent derivation.
3. Require branch-selection trace wording for projection, `parallel`, select,
   and derived intent/action contract paths.
4. Require cleanup/defer tests to assert state view and ordering, not only that
   cleanup occurred.
5. Add `documentation-gap` and `explicit-reject-gap` as audit verdict labels
   where current smokes only say pass/fail.
6. Keep every new contract tied to `docs/125_source_of_truth_spine.md`: one
   owner, many consumers, no rediscovery.

## 12. Probe-Order Semantics And Recovery Artifacts

Additional TeX/METAFONT observations from cases 0051-0055:

- `var_delimiter` is not a global search over all possible successors. It is a
  probe-order algorithm: stop at the first sufficient variant, otherwise keep
  the greatest variant seen.
- A short phrase such as "smallest variant" is dangerous unless the spec also
  states the traversal order and the early-stop rule.
- METAFONT duplicate `ligtable` labels are diagnosed, but nonstop mode can
  still write a TFM with unreachable instructions.
- `boundarychar` has two different boundary roles: `||:` for invisible left
  boundary behavior and `boundarychar` for invisible right boundary behavior.
- Invalid recovery output is useful for debugging, but it must not be promoted
  into a semantic oracle.
- METAFONT `skipto` has a numeric jump boundary: 127 intervening lig/kern steps
  are accepted, 128 are rejected, and a missing target label is diagnosed and
  normalized away.
- Capacity claims must be tied to the active executable/source pair. A
  historical patch fragment saying `max_kerns=500` did not predict the local
  executable, which accepted 501 distinct kern amounts.

Pergyra rule:

Any feature that searches candidates, repairs input, or continues after a
diagnostic must expose these facts:

```text
candidate_order: source | priority | cost | implementation | unspecified-forbidden
selection_rule: first-sufficient | best-global | greatest-seen | fallback | reject
recovery_artifact: none | diagnostic-only | executable-but-noncanonical | canonical
jump_bound: none | exact-numeric | implementation-limit
missing_target: reject | diagnose-and-drop | diagnose-and-fallback | undefined-forbidden
capacity_source: active-source | runtime-config | historical-note | empirical-only
```

Application points:

| Surface | Required Contract |
|---|---|
| overload selection | traversal order and tie behavior |
| pattern matching | first-match versus best-match semantics |
| rewrite/lowering passes | whether later candidates can override earlier choices |
| diagnostic repair | whether repaired output is executable or only inspectable |
| boundary hooks | left-boundary and right-boundary behavior named separately |
| local jumps / labels | exact maximum distance and missing-target policy |
| capacity limits | the source of the limit and whether it is configurable |

Test requirement:

Construct at least one deliberately non-monotone candidate chain. A passing test
must prove whether the implementation uses first-sufficient, best-global, or
greatest-seen behavior. Do not rely on monotone examples only.

For bounded jumps, test the maximum valid value, the first invalid value, and a
missing target. The report should say whether post-error artifacts are canonical
or recovery-only.

For capacity limits, never cite an old note as the active contract. Record the
runtime version, the source/change file or configuration that owns the limit,
and an empirical boundary witness.

## 13. Token Identity, Late Freezing, And Arithmetic Drift

Additional TeX observations from cases 0061-0065:

- Token identity includes the category code captured when the token was read.
- `ifx`-style comparison must say whether it expands recursively, compares
  top-level structure, or compares captured token identity.
- A phrase like "before typesetting begins" is too broad unless it is mapped to
  a concrete state bit. For TeX patterns, the useful state is trie readiness,
  not merely whether a box command has run.
- Reentry guards can prevent immediate recursion while still allowing later
  churn if the handler contributes new triggering work.
- Integer fixed-point arithmetic can drift far from real-number algebra in long
  compound chains without violating the implementation contract.

Pergyra rule:

For any tokenizer, deferred compiler table, hook, or fixed-point numeric
surface, specify:

```text
token_identity: spelling | codepoint | codepoint+category | interned-symbol | meaning
comparison_depth: identity | top-level-structure | expanded-normal-form
freeze_condition: explicit-call | first-use | dump | state-bit | never
reentry_guard: none | immediate-only | until-drained | transaction
numeric_oracle: real | integer-truncating | fixed-point-rounded | saturating | checked
```

Test requirement:

Use adversarial witnesses that separate the human reading from the operational
condition:

- same spelling with different captured token category;
- post-box but pre-freeze table mutation;
- handler that triggers immediate work versus later work;
- algebraic identity that drifts under integer truncation.

## 14. Lattice Witnesses Beat Anecdotal Edge Cases

The scaled arithmetic follow-up used an explicit lattice instead of isolated
examples:

```text
start dimension
  x increment in sp
  x multiply/divide pair
  x iteration count
```

Result:

```text
672 lattice points
0 integer-oracle mismatches
546 real-arithmetic drift points
```

Pergyra rule:

For every numeric, scheduling, parser, hook, or resource primitive with
multiple interacting parameters, define at least one lattice witness:

```text
axis_1 x axis_2 x axis_3 x axis_4 -> oracle comparison
```

The report should separate:

```text
oracle-mismatch  -> candidate bug
drift            -> expected difference from a weaker mental model
diagnostic       -> expected rejection/recovery path
timeout          -> harness or liveness issue
```

Application points:

| Surface | Lattice Axes |
|---|---|
| fixed-point arithmetic | start x increment x operation chain x iteration |
| overload selection | candidate order x specificity x ambiguity x fallback |
| parser recovery | error token x lookahead x nesting x recovery production |
| hooks/finalizers | state view x ordering x reentry x failure mode |
| resource budgets | allocation size x lifetime x nesting x cleanup timing |

Do not promote a surprising lattice cell into a bug until it fails the strong
oracle. The lattice exists to find oracle mismatches, not to collect surprise.

## 15. Shared Tables Need Consumer-Specific Trigger Semantics

TeX case 0066 tested math ligatures against a font that also has text-mode
boundary ligatures:

```text
boundarychar := 90
ligtable ||: "A" =: "B"
ligtable "A": "Z" kern 1pt
```

Observed:

```text
text "A"  -> B via virtual left boundary
math $A$  -> A, no virtual boundary trigger
math $AZ$ -> A kern Z via literal adjacency
```

Pergyra rule:

If one table is consumed by multiple subsystems, each subsystem must declare
its trigger semantics separately:

```text
consumer: text-shaper | math-layout | code-folder | optimizer | renderer
trigger_source: literal-adjacency | virtual-left-boundary | virtual-right-boundary | synthetic-node | none
node_materialization: preserve-source-node | synthesize-node | merge-node | annotate-only
```

Shared data does not imply shared trigger behavior. A visible result can match
while the mechanism differs, so tests should include both:

```text
same visible output / different trigger path
different visible output / same table
```

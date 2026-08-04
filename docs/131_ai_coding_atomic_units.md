# AI Coding Atomic Units

Last updated: 2026-07-31

Anti-hype rule:

- This document is a design thesis, not a claim that Pergyra already solves
  AI coding.
- "AI-first language" must mean verifiable work decomposition, not vague
  automation.
- Current implementation anchors are `intent`, diagnostics, AIR evidence,
  CFG/MIR validation, DAG resolution, and test/smoke gates. Anything beyond
  that is a research direction.

Related documents:

- `docs/19_design_philosophy.md` — systems/domain identity
- `docs/104_air_compiler_architecture.md` — AIR as verification layer
- `docs/118_slot_model_rigor_audit.md` — anti-hype vocabulary
- `docs/120_vision_and_capability_audit.md` — current capability vs vision
- `docs/121_types_as_domain_medium.md` — types as domain-coordinate carriers
- `docs/122_managing_intent_drift.md` — drift management discipline
- `docs/169_agent_boundary_sentinel_library.md` — recognizable wrong-boundary
  patterns and owner-directed corrections

## 0. Thesis

Software paradigms differ by the smallest unit they treat as stable:

| Paradigm | Stable decomposition unit |
|---|---|
| OOP | Object / responsibility |
| FP | Function / composition |
| DOP | Data / transformable structure |
| Systems programming | Resource / ABI boundary |
| Pergyra | Intent / boundary / authority / state transition |
| AI coding | Verifiable intent atom |

The AI-coding pressure is not merely "generate more code." It is:

> **decompose complexity into small units whose intent can be stated and whose
> result can be verified.**

The proposed name for this pressure is:

> **Atomic Decomposition Pressure**

This is the software-engineering analogue of a common convergence pattern:
when a substrate must mix information without stalling, it tends to evolve
toward small repeated transformations. AES has substitution/permutation rounds;
numerical methods approximate nonlinear systems by smaller steps; human
reasoning breaks hard nonlinear problems into tractable substructures. AI
coding imposes a similar pressure on software work: large vague tasks must be
split into small verifiable transformations.

## 1. The Smallest Useful Unit: Intent-Verification Pair

The smallest AI-coding unit should not be "a file," "a function," or "a
prompt." Those are convenient containers, but they are not always meaningful.

The better unit is:

> **Intent-Verification Pair (IVP)**: a formalized intent plus the test,
> contract, diagnostic, or evidence that verifies it.

Examples:

| Intent | Verification |
|---|---|
| "Move MIR cleanup validation out of `mir.c`." | `test-mir`, source inventory gate, no behavior diff |
| "Reject token crossing a spawn boundary." | semantic regression with deterministic diagnostic |
| "Make compressed intent derive `who` from `on:`." | positive example, ambiguity negative case, provenance diagnostic |
| "Keep LLVM/C slot pin ABI identical." | ABI smoke, backend compare, same-process ABI precheck |

This mirrors several older ideas without being identical to them:

- Hoare logic: precondition / postcondition.
- Result-first programming: `Result<Success, Failure>` makes success and
  failure explicit.
- Design by Contract: user-facing contract is part of the program.
- Dependent-type discipline: stronger claims require stronger evidence.

For AI coding, the important part is not full formal proof. The important part
is that every task has a named intent and a machine-checkable verification
surface.

## 2. Pattern + Context Is The Practical Atom

A reusable pattern is not just a code snippet. A useful AI-era pattern has:

1. A name.
2. A context where it applies.
3. A skeleton implementation.
4. A known failure mode.
5. A verification gate.

This is why instructions like "apply the Result pattern" work better than
"do not use exceptions." The pattern carries an intent, a context, and a
verification expectation.

In Pergyra terms:

```text
pattern = named intent + applicability context + implementation skeleton + gate
```

The existing pattern corpus should be treated as a growing set of AI-coding
atoms. The value is not the number of patterns; the value is whether each
pattern has a clear intent-verification pair.

## 3. Specification Gradient

AI coding rarely starts with a perfect spec. It starts with a rough intent and
then tightens through repeated passes. The useful concept is:

> **Specification Gradient**: the staged refinement from vague intent to
> executable contract, where each stage adds verification pressure.

One possible gradient:

| Stage | Form | Verification |
|---|---|---|
| 0 | Natural-language goal | Human review |
| 1 | Named intent | Scope and non-goals |
| 2 | Intent-verification pair | Test/smoke/diagnostic exists |
| 3 | Contracted implementation | CI gate consumes it |
| 4 | Evidence graph | AIR / trace / provenance can explain it |
| 5 | Formal obligation | Proof sketch or mechanized proof |

This gradient is important because demanding Stage 5 from the start stalls
work, while staying at Stage 0 creates unreviewable code. The engineering
discipline is to move each important task up the gradient as risk increases.

## 3.1. AI Has An Implicit Objective Function

An AI coding agent does not choose a neutral "next step." It infers an
objective function from the prompt, nearby code, existing tests, and common
architectural patterns. If the repository does not state that objective, the
agent will usually optimize some mixture of:

- making the nearest failing command pass;
- minimizing the immediate patch;
- continuing the most familiar architecture;
- preserving local consistency;
- producing a plausible implementation quickly.

Those defaults are useful, but they are not necessarily the project's goals.
They can prefer a locally convenient compatibility path over source-of-truth
closure, copy a Rust/SIL/MLIR-shaped solution whose assumptions do not belong
to Pergyra, or flatten a distinctive semantic model into a familiar compiler
pipeline. A likely continuation is not proof of the right continuation.

The human or repository must therefore provide the frame. Use AI as a
constraint solver inside that frame, not as the owner of the objective
function.

For Pergyra repository work, the default priority order is:

```text
1. Preserve Pergyra semantic identity and one source of truth.
2. Move decisions behind the fact owner that can verify them.
3. Remove aliases, semantic fallbacks, and backend reconstruction.
4. Add a negative gate that prevents the old path from returning.
5. Minimize blast radius after the ownership boundary is correct.
6. Prefer familiar architecture only when it satisfies 1-5.
```

Before asking an AI to make a structural change, write an objective card:

```text
Goal:
Priority order:
Semantic facts that must remain first-class:
Owner that must make the decision:
Last legitimate consumer:
Allowed bridge:
Forbidden fallback or convergence:
Runtime/materialization budget:
Positive and negative gates:
Non-goals:
Stop/reject condition:
```

This card need not be long. Its purpose is to prevent an unstated objective
from silently becoming architecture.

When borrowing from another compiler, do not request "use Rust MIR" or "copy
SIL." Ask the agent to answer these questions first:

1. What exact problem does the borrowed structure solve?
2. Which invariant gives that structure its shape in the source compiler?
3. Do those assumptions hold in Pergyra?
4. Which Pergyra owner produces the corresponding fact?
5. Who is its last legitimate consumer, and what may be erased afterward?
6. Would the import create a second source of truth or a backend recovery path?
7. Which negative fixture would disprove the proposed mapping?

If these questions have no concrete answer, the external structure is a
reference, not an implementation plan.

### Prompt Shape

Weak:

```text
Refactor this compiler like Rust/Swift and make the tests pass.
```

Strong:

```text
Goal: remove backend reconstruction of local-binding types.
Owner: artifact-bound semantic local-binding facts.
Preserve: Pergyra intent/evidence layering; AIR remains verification-only.
Forbidden: AST fallback, alias owner, inferred i32 default.
Bridge: initializer provenance may remain an explicitly named HIR view.
Gate: missing semantic type fails closed; old accessor cannot reappear.
Non-goal: redesign expression typing in this patch.
```

The strong form changes the AI's effective objective from "produce the most
likely compiler refactor" to "satisfy this repository-specific contract."

## 3.2. Do Not Run Pergyra As A Software Factory

An agent that is rewarded only for the nearest green test has no built-in
penalty for duplicate authority, repeated whole-program work, compatibility
fallbacks, or a graph that no production entrypoint consumes. More tokens,
agents, fixtures, documents, and generated files can make that failure mode
look productive while the executable architecture gets worse.

For this repository, a test is a falsifier of a named ownership claim. It is
not the objective function and its row count is not progress. Before
implementation, fix one vertical slice:

```text
production entrypoint
-> direct C bypass to delete
-> existing Pergyra fact owner
-> last orchestration consumer
-> focused positive/parity/negative gate
-> installed-driver evidence
```

The time saved by generated implementation and rework belongs first to
planning and alignment. Spend it on the objective card, call graph, evidence
lifetime, materialization budget, and falsifying fixture before opening more
implementation fronts.

The operating rules are:

1. Keep one active executable self-host rung. Documentation, registries,
   fixtures, and owner files support that rung; they do not independently
   count as replacement.
2. A green test cannot excuse a second source of truth, an old fallback, or
   repeated reconstruction of an already admitted artifact.
3. Count work at the semantic execution target. If several inventory rows map
   to one import-composed program graph, validate that target once and project
   the result back to the rows.
4. Validate a complete graph once at its owner boundary. Consumers carry the
   admitted fact or receipt; they do not rescan, recopy, or revalidate the
   cumulative graph.
5. Parallel agents are allowed only for bounded, independent evidence tasks
   after the owner, edit boundary, and integration gate are fixed. The primary
   task still owns reconciliation. Agent count and token use are not progress.
6. Stop expanding the factory when memory, wall time, or file count grows
   faster than the semantic input. Name the repeated operation and its owner
   before adding a cache, shard, timeout, worker, or memory allowance.
7. Do not open an unrelated query engine, library adoption track, architecture
   rewrite, or cosmetic folder split while an executable rung is open.

Every material run records at least:

```text
revision and dirty state
semantic input/target identity
number of attributed rows
number of unique executions and reuses
elapsed time and process-scoped peak memory
last reached owner phase
result: PASS, FAIL, TIMEOUT, or POLICY-STOP
```

This is the control loop:

```text
align objective
-> implement one owner-directed delta
-> run the narrow falsifier
-> inspect cost and ownership
-> delete the bypass
-> broaden only at the integration boundary
```

If a run cannot name the bypass it replaces or the fact owner it exercises,
it is diagnostic exploration, not self-host progress.

## 3.3. Trust Moves From Code Volume To Verification Structure

When an agent can generate a patch in thirty minutes that takes a human three
days to reconstruct line by line, the nominal generation speed is not the
engineering speed. The cost has merely moved from implementation to
verification. Traditional review assumes that a reviewer can recover the
author's model from the code at roughly the rate the code is produced. That
assumption stops holding once generation outruns human comprehension.

The trust sequence therefore changes:

```text
traditional programming
understand the implementation
-> decide that the implementation is correct

agent-assisted programming
state permitted and forbidden outcomes
-> identify the authority and blast-radius boundaries
-> obtain evidence from bounded gates
-> trust only the claims covered by that evidence
```

This does not mean "stop reading code and trust tests." It means replacing
uniform, line-by-line understanding with **selective understanding plus
mechanical verification**. A human must still understand the objective, the
fact owner, authority changes, irreversible effects, failure propagation, and
the boundary at which another component begins trusting the result. Routine
local implementation can receive shallower review when independent evidence
covers it and the change is cheaply reversible.

The persistent mismatch behind this change is:

> **Epistemic impedance mismatch**: the gap between how a human maintains a
> project model and how an AI generates a locally plausible implementation
> from bounded context.

Humans usually reason from purpose, responsibility, forbidden states, failure
radius, and long-term direction. A model produces a concrete arrangement from
the prompt, visible context, learned conventions, and existing gates. The
output can be locally correct while encoding a different module boundary or
failure model from the one the human believes exists. Better prompting reduces
this mismatch but does not eliminate it; the mismatch must be managed through
explicit owners, objective cards, falsifiers, evidence scope, and bounded
rollback.

### External anecdote: language consistency is steerable; architecture is not

An independent language author described essentially the same asymmetry in an
[Hacker News comment](https://news.ycombinator.com/item?id=49158565): AI was
useful for design and implementation, language consistency could be steered,
but architecture steering forced the author to learn substantially more
compiler internals than the side project was meant to require.

This is one anecdote, not benchmark evidence. It is useful because it separates
two failure classes that are often conflated:

- surface consistency can be constrained with one registry, syntax examples,
  diagnostics, and parity fixtures;
- architecture needs explicit fact ownership, evidence lifetime, last
  consumers, forbidden fallbacks, hard caps, and negative gates.

Pergyra's response is not to claim that agents understand compiler
architecture. It is to reduce the architecture they are allowed to invent:
one active executable rung, one objective card, responsibility-named owners,
no generic helper expansion, no dual authority, and integration evidence that
cannot be replaced by fixture count or generated LOC.

The bounded String-array push rung is a concrete example. Output-only testing
could have accepted values pre-baked into final storage while leaving LLVM's
length as an immutable stale snapshot. The architecture constraint instead
fixed one operation-ordered length owner, one mutable backend object, a strict
store-before-length-update rule, and negatives for missing empty graphs,
reordered or late pushes, entry-block re-entry, capacity-as-length, stale
receivers, and backend rediscovery. The re-entry case is especially
architectural: the emitted values remain locally consistent, yet a CFG edge
back to block zero repeats mutation and invalidates the bounded-capacity proof.

The following bounded `Array<Int>` loop-push rung exposed a second local
example. The language use was already internally consistent: an empty array,
`ArrayPush`, `ArrayLength`, indexing, and integer arithmetic all existed. The
first production failure was nevertheless architectural: scalar-local
inventory had no collection claimant for the same LocalRef, and a local patch
would have created either a second collection compiler or a weaker scalar
admission path. The closed design instead added one collection mutation fact
to the existing target-neutral `GraphPlan`, extracted shared CFG responsibilities
from the older String owners, and made both backends consume that receipt. Its
negative matrix changes receiver identity, loop/backedge effects, graph leaves,
current length, read identity, and ABI before artifact publication. Exact
`30`/`5` parity is supporting evidence; the important architecture evidence is
that those mutations cannot be accepted by a hidden alternate route.

The initialized `Array<Int>` sum/set rung then tested whether that architecture
would survive extension. It did not add another fixture planner. The same
program receipt gained explicit, mutually exclusive dynamic-push and
initialized-static-set modes, and the existing `GraphPlan` remained the sole
target-neutral claimant. C and LLVM consume the selected receipt; neither tries
the other mode after rejection. Exact `60`/`99`/`3` execution matters, but the
stronger evidence is that 35 graph, identity, topology, order, length, ABI, and
extra-use mutations publish no artifact and cannot escape through the older
dynamic path. This is the architectural ratchet the external anecdote says AI
does not supply on its own.

The next read-only `Array<Int>` range-maximum rung exposed the same distinction
inside the implementation itself. The syntax and generated MIR were already
consistent, but an initial implementation gave the outer receipt the obsolete
`LoopMutationFact` identity, duplicated generic range block facts, treated the
semantic call-target `none` sentinel as integer zero, and compared SSA ValueIds
as though they were source-local names. Output testing alone could not explain
or prevent those errors. The corrected design renamed the single receipt to
`ProgramFact` without an alias, kept range topology with the existing range
owner, used `SemanticCallTargetNone()`, joined `best.1`/`best.4`/`best.7` through
the source-local `best` identity, and sealed exact predecessor/value phi pairs.
Its C/LLVM gate executes `9`, first-max `12`, last-max `14`, and all-negative
`-2`; 23 malformed identity, range, read, CFG, phi, and Log variants publish no
artifact and never retry another mode.

The fresh `ArrayReverse` rung added a sharper architecture falsifier. Its first
implementation used the exact valid-reverse predicate as the route claimant.
Changing the call target or edge therefore made the predicate false and let a
malformed transform fall through to the older one-block compiler path. Exact
output, valid-input parity, and most negative tests were green while ownership
was still wrong. The repair deliberately overclaims every `Array<Int>`-valued
call graph, including an invalid expression sequence; exact admission then
accepts or rejects it, and a negative gate forbids the legacy local-inventory
diagnostic. A separate audit also found that operation 20 (`ArrayReverse`) was
missing from the inactive Array-program inventory. That inventory now has one
named owner for every `Array<Int>` operation kind, so a future mode cannot
exist without its program receipt. This is repository-local evidence for the
external author's architecture warning, not merely agreement with it.

The lesson is that consistency tests constrain language surface;
owner/consumer/fallback gates constrain compiler architecture. The external
comment is therefore a warning signal, while these executable rungs are
the repository-local evidence.

The central rule is:

> **When AI-generated code volume exceeds human understanding speed,
> programming becomes less about trusting code and more about designing and
> trusting verification gates.**

The trusted object is not one green gate. It is the whole verification
structure:

- the gate is independent enough not to repeat the implementation's mistake;
- its verification scope is explicit, and trust never exceeds the claim it
  actually checks;
- changes to the goal or gate have a separate history from implementation;
- a failure keeps its blast radius inside a named boundary;
- a checkpoint identifies what can be restored without guessing.

An agent that writes both an implementation and its approving test may copy
one misunderstanding into both:

```text
misread requirement
├─ plausible but wrong implementation
└─ test that approves the same wrong interpretation
```

Therefore, evidence independence is a design property, not a claim that two
files exist. Independence may come from an existing oracle, a separately owned
semantic invariant, cross-backend parity, generated adversarial inputs, a
stable protocol specification, or human review of the high-risk boundary. If
implementation and gate share the same unstated assumption, the gate is useful
regression coverage but not independent confirmation of the requirement.

Gate density follows risk rather than generated LOC:

| Risk shape | Typical evidence |
|---|---|
| Local, reversible, immediately visible failure | compile, type, lint, focused unit test |
| Shared fact owner, integration seam, latent semantic or performance failure | parity, integration, property test, bounded performance evidence, architecture ratchet |
| Authority, privacy, money, memory safety, ABI, destructive or wide-radius change | explicit invariant, adversarial/fuzz or model evidence, independent gate owner, human boundary review, staged rollout or rollback checkpoint |

Do not add every item in a row mechanically. Choose the smallest set that can
falsify the material claim and contain its failure. Gate count, test count, and
coverage percentage are not substitutes for independence or scope.

## 3.4. Chat Is Transport; The Verification Graph Owns Work State

Chat is a chronological transport:

```text
request -> response -> patch -> error -> correction -> next request
```

A real project is a dependency and verification graph. Chronological text does
not make confirmed invariants, rejected approaches, claim scope, gate changes,
or rollback points stable first-class objects. At sufficient scale the human
stops directing the project and starts reconstructing project state from chat
history. That reconstruction cost is itself an agentic-development bottleneck.

The durable work model should connect at least these node types:

```text
Goal
Constraint
Artifact
Agent Task
Evidence
Negative Gate
Human Approval
Checkpoint
Rollback
```

Useful edges include `implements`, `constrained-by`, `produces`, `checks`,
`falsifies`, `approves`, `depends-on`, and `restores`. The important state
is not an agent's hidden reasoning. It is:

- what changed;
- which claim the change is meant to satisfy;
- which evidence was observed at which revision and semantic input;
- what remains unverified;
- which forbidden result the negative gate excludes;
- how far failure propagates and which checkpoint bounds recovery.

An implementation agent may change the path used to reach a goal. It may not
silently weaken the goal, edit its own negative gate to obtain green status,
or promote partial evidence into a broader claim. A material gate change is a
separate reviewed artifact with a reason, scope delta, and history. Failure
invalidates dependent claims; it does not erase unrelated green evidence or
invite a whole-project rewrite.

Pergyra already represents this model with repository artifacts even though it
does not yet provide a dedicated canvas UI:

| Verification-graph object | Current repository representation |
|---|---|
| Goal and constraint | objective card, owner contract, active handoff card |
| Artifact | source diff, generated owner artifact, protocol/ABI row |
| Evidence | observed test result, diagnostic, parity output, pressure summary |
| Negative gate | executable old-path or missing-fact rejection |
| Human approval | explicit authorization for a risk/authority boundary |
| Checkpoint and rollback | exact Git revision, dirty state, rejected experiment record |

A future canvas should expose this graph directly rather than render chat cards
on a larger surface. That is a research direction, not a claim about the
current implementation. Until then, owner registries, executable gates, Git,
and the top active handoff card are the durable state; chat is navigation and
coordination only.

## 4. Agent Pipeline Implication

AI agents should not be large monolithic "do everything" workers. They should
be narrow transformers with strict inputs and outputs:

| Agent role | Input | Output |
|---|---|---|
| Context reader | Code, docs, git diff | Bounded factual map |
| Intent splitter | Goal, factual map | Intent-verification pairs |
| Logic editor | One IVP | Patch |
| Compiler fixer | Build error | Minimal compile fix |
| Contract auditor | Patch, tests | Missing gate/failure case |
| Documentation updater | Accepted IVP | Progress note and claim boundary |

This resembles a conveyor belt. MCP-like protocols matter because each agent
needs a small, typed contract:

```text
input: bounded context + one intent
output: patch or evidence
reject: ambiguity, missing verification, scope creep
```

The point is not parallelism by itself. The point is **controlled mixing**:
small independent transformations are repeatedly applied until the system
converges on a verified state.

## 5. Pergyra Relevance

Pergyra already has vocabulary that fits this model:

| AI-coding concept | Pergyra construct |
|---|---|
| Intent-verification pair | `intent` + semantic/runtime/backend gate |
| Specification gradient | `docs/100` + TODO progress gates + smoke tests |
| Evidence graph | AIR evidence nodes |
| Boundary-safe agent output | CFG/MIR/DAG contracts |
| Pattern + context | domain kit / stable subset examples |
| Failure visibility | `Result<T, E>`, diagnostics, trace schema |

This suggests a future dogfood direction:

- The compiler-adjacent self-host tools should be written as IVP pipelines.
- AIR graph validators should expose evidence in an agent-consumable JSON
  format.
- Diagnostic catalog checks should require each diagnostic to name intent,
  reason, fix, and verification.
- Refactoring work should be framed as one IVP per patch, not one vague
  "cleanup" sprint.

## 6. What To Avoid

| Temptation | Why it fails |
|---|---|
| "AI will just understand the whole repo" | Context windows and reasoning reliability still punish vague scope |
| "More agents means better output" | Without IVP boundaries, agents amplify drift |
| "Prompt is the atom" | A prompt is an instruction container, not a verifiable unit |
| "Test is enough" | A test without named intent does not explain what should remain stable |
| "Full formal proof first" | Over-specification stalls exploratory implementation |

## 7. Working Name

Recommended terms:

- **Atomic Decomposition Pressure** — the general pressure across systems.
- **Intent-Verification Pair** — the smallest practical AI-coding unit.
- **Pattern-Context Unit** — reusable named pattern with applicability context.
- **Specification Gradient** — staged movement from vague intent to verified
  contract.
- **Epistemic Impedance Mismatch** — the persistent gap between the human
  project model and an AI-generated implementation model.
- **Verification Graph** — the durable graph of goals, constraints, artifacts,
  evidence, gates, approvals, checkpoints, and rollback edges.

If a shorter public phrase is needed:

> **AI coding should be organized around verifiable intent atoms.**

This phrase is precise enough to guide engineering and modest enough to avoid
overclaiming.

## 8. Handling A Stale Architecture Review

The August 4 external review observed `5c889253` and still described
`array_max.pgy` as red. Current source had already closed maximum, reverse, and
pop before the review was read, so its status table and P0 queue were not safe
resume state. They remain dated evidence only.

Its architectural falsifier was useful: another topology-specific Array mode
or program receipt would preserve a collection of verified micro-compilers.
During the next indexed-assignment rung an initial uncommitted
`IndexAssignmentProgramFact` design was therefore discarded. The accepted
shape embeds target-neutral collection value versions, ordered
`Initialize`/`Get`/`Set` operations, and observations in the existing sealed
`GraphPlan`; selected C and LLVM consumers see that plan, not a fixture or a
topology name.

This does not establish general collections. The admitted slice is still
bounded to nonescaping local literal `Array<Int>` and `Array<String>` values,
static indices, literal set inputs, integer-sum observation, and two-operand
String concat. The value/storage/Set columns are reusable, but GraphPlan
operations 23/24 still encode those two observation topologies, so the owner is
`BRIDGE/ACTIVE`, not a closed general collection SoT. Push, reserve, move,
alias invalidation, reallocation, cleanup, and failure policy have not migrated
to the plan. A review can change the architecture objective when its falsifier
survives current evidence, but its old HEAD, queue, and completion claims
cannot replace the active handoff or executable gates.

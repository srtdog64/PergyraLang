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

If a shorter public phrase is needed:

> **AI coding should be organized around verifiable intent atoms.**

This phrase is precise enough to guide engineering and modest enough to avoid
overclaiming.

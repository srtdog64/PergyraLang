# AI Coding Atomic Units

Last updated: 2026-05-04

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

# Pergyra Keyword Orthogonality

Last updated: 2026-10-08

This document fixes the semantic question answered by each Pergyra keyword
family. The goal is not to reduce the number of keywords mechanically. The
goal is to keep different semantic axes from silently owning the same question.

## 0. Game-Derived World Ontology

The number of keywords is a real cost, but it is not automatically accidental
feature growth. Pergyra's core vocabulary is best understood as a world-modeling
ontology extracted from games, not as genre-specific game nouns.

Games are one of the most compact and common human forms for modeling a world:
they expose actors, places, resources, abilities, roles, relations, effects,
rules, authority, goals, failure, recovery, and observable history. Business
workflows, logistics, permissions, simulations, and agent orchestration keep
recreating the same shape. Pergyra promotes a recurring coordinate into a core
language primitive only when the compiler must understand that coordinate as a
semantic fact with a proof obligation.

That is the keyword adequacy rule:

- a core keyword must name a distinct world-modeling coordinate;
- it must have a compiler fact owner;
- it must carry a proof, diagnostic, backend, runtime, or verifier obligation;
- it must remain orthogonal to neighboring keywords;
- domain-specific nouns such as `quest`, `item`, `inventory`, `npc`, `player`,
  `buff`, or `scene` belong in libraries or kits, not core syntax.

So the target is not fewer keywords by default. Each distinct semantic fact
must have one authoritative owner, with no duplicate truth path. This is not
an injective keyword-to-axis mapping or a claim that every use of one spelling
has the same semantic owner.

## 0.1 Human Cognition: Bounded Activation

Pergyra's keyword count is not judged by the rule "a beginner must memorize the
whole vocabulary before writing a program." That would be the wrong cognitive
model. The intended model is **bounded activation**:

```text
human load =
  active semantic registers in this program
  + cross-register conflicts
  + omission recovery cost
  + diagnostic recovery cost
```

The full keyword set is a catalog. A specific program activates only the subset
of registers its world needs. This is the same reason large card games,
tabletop systems, and legal/medical rulebooks remain learnable despite large
vocabularies: users do not keep the whole rulebook active. They learn a small
active register, then add adjacent registers when their domain needs them.

Pergyra therefore has two vocabulary layers:

- **global language catalog**: every orthogonal keyword the compiler can
  understand as a semantic coordinate;
- **program-local register**: the small subset a concrete program activates.

The learning contract is:

```text
core programming        -> let / func / if / for / while / match / return
resource register       -> slot / own / ref / pin / unsafe / extern
execution register      -> parallel / spawn / async / await / select
domain topology register-> subject / intent / zone / world / relation / effect
contract register       -> class / struct / object / tobject / ability / role
```

This makes keyword count a secondary metric. The primary metric is whether a
selected grammatical use has an explicit semantic role and fact owner, and
whether it composes with the other active registers without hidden owner drift.

The compiler-facing rule is stricter than the human rule:

- a program may activate only a subset of registers;
- composing two subsets must not create a new hidden fact;
- if two keywords introduce the same semantic fact, the fact must have the same
  owner axis;
- compact syntax may omit text only when the omitted fact is recovered from one
  declared owner; ambiguous recovery fails closed.

Coq can model this structural part. It cannot prove "humans find the language
easy." It can prove the preconditions that make chunked learning possible:
register membership is explicit, keyword subsets are closed under composition,
and shared facts cannot silently cross to another owner axis. That model lives
in `docs/semantics/proofs/AxisOwnership.v` alongside the existing keyword
orthogonality proof.

## 1. Four Top-Level Semantic Fact Axes

| Axis | Question | Surface |
| --- | --- | --- |
| Resource | Which resource or handle is held across which boundary? | `slot`, `own`, `ref`, `pin`, `unsafe`, `extern` |
| Execution | When, where, and under what concurrency relation does work run? | `parallel`, `spawn`, `async`, `await`, `select` |
| Domain | Who acts, in which boundary, under which authority, relation, or effect? | `subject`, `role`, `intent`, `zone`, `world`, `authority`, `relation`, `effect`, `projection` |
| Type/Contract | Which shape or ability contract must a value satisfy? | `class`, `struct`, `ability`, generic `where` |

These axes are not mutually isolated sublanguages. They meet in the verifier
graph. Ownership is kept by the axis that owns the final fact.

## 1.1 Registry Primary Categories Are Not Fact Ownership

The registry's single `axis` field is a **spelling-level primary category** for
navigation and tooling. Its existing five enum values and wire names remain
stable. It is not a dispatch table for semantic checks and does not assert
that every grammatical use of that word introduces the same fact. The four
fact axes above and the five metadata categories below are different levels.

The registry owns the complete vocabulary; these examples are consistency
assertions, not a second 147-word authority:

| Category | Purpose | Representative spellings |
| --- | --- | --- |
| GENERAL | Core bindings, literals, module/visibility structure, and shared combinators | `let`, `true`, `false`, `in`, `all`, `any`, `export`, `import`, `use`, `namespace`, `public`, `private` |
| RESOURCE | Resource/loan/foreign-boundary surface | `slot`, `own`, `ref`, `pin`, `with`, `unsafe`, `extern` |
| EXECUTION | Control flow, scheduling, joins and orchestration paths | `if`, `else`, `while`, `for`, `loop`, `break`, `continue`, `return`, `match`, `case`, `default`, `parallel`, `spawn`, `async`, `await`, `select`, `join`, `sum`, `product`, `min`, `max`, `step`, `success`, `failure`, `priority` |
| DOMAIN | Participant placement and domain/action topology | `subject`, `role`, `intent`, `zone`, `world`, `action`, `on`, `authority`, `relation`, `effect`, `projection`, `authorized`, `causes`, `within` |
| TYPE_CONTRACT | Shapes, signatures and capability/type constraints | `class`, `struct`, `ability`, `event`, `where`, `requires` |

GENERAL means shared/base surface, not an ownerless semantic fact. MODULE is
already a grammar context bit; adding a top-level fact axis merely to empty
GENERAL is not justified. Module visibility and authority retain their actual
owners (see `202_module_authority_boundary_design.md`). `extern` remains a
foreign resource boundary, even though it also occurs in module syntax.

Use-specific examples explain why one primary category cannot prove ownership:

| Surface use | Semantic responsibility |
| --- | --- |
| `join with all/any` | Parallel join selection (Execution). |
| world `state ready: all/any ...` | Composition of domain state (Domain). |
| `any T` | Type-position modifier parsed by the type owner; complete existential semantics are not established by that parser support. |
| generic `where T: A` / step `where: Z` | Type constraint / zone binding (Type/Contract / Domain). |
| `with slot` / `with caps` / `with effects` | Resource loan / capability contract / callable effect contract; the selected production fixes the owner. Parsed but unimplemented resilience clauses are not execution support. |
| intent `on: actor.Action()` / zone `maintain effect on target` | Action binding / effect participant topology (Domain). Reactive parallel `on` is declared but non-executable. |
| `role R for S` / role ability satisfaction | Concrete domain placement / Type-Contract obligation. Native role admission binds a subject or primitive domain; grouping ability and role in a learning register does not change that distinction. |
| `event E(args)` / event subscription | Callable signature / reactive behavior; the declaration's primary category does not assign subscription ownership. |

Same-parent clauses need not share a fact owner. In particular, `requires`
stays Type/Contract while `authorized`, `causes` and `within` stay Domain;
`intent` is Domain while `step`, `success` and `failure` describe Execution.
`AxisOwnership.v` proves representative fact-use composition, not the semantic
adequacy of all 147 primary-category labels. The axis consistency gate compares
actual values and uses mutation controls; it is not a full-language proof.

## 2. Core Definitions

| Keyword | Orthogonal meaning |
| --- | --- |
| `subject` | Identity-bearing actor or state-transition host in a domain model. |
| `class` | General reusable shape or behavior provider; it does not claim domain identity. |
| `struct` | Plain data shape; it does not carry behavior identity by itself. |
| `vessel` | Internal state container for a `subject`; it is not an actor. |
| `object` | Local/internal projection view. |
| `tobject` | Transfer or publication projection view for a boundary. |
| `relation` | Persistent relation between identities. |
| `effect` | State influence caused by an action or condition. |
| `zone` | Execution, authority, and resource boundary where actions are allowed. |
| `world` | Outer composition boundary that owns zones, handoff, scheduling, and failure propagation. |
| `ability` | Static contract describing what a value or actor can do. |
| `role` | Concrete placement of an ability on a subject/class/host. |
| `action` | Verifiable behavior with `requires`, `within`, `authorized by`, and `causes` contracts. |
| `intent` | Orchestration spine that orders actions, compensation, rollback, and observability. |

## 3. Intent Is Not A Universal Owner

`intent` is the spine of code, but it is not the owner of every authority or
resource fact. Intent combines facts from other axes and records provenance.

| Clause | Final owner |
| --- | --- |
| `who` | participant / subject binding |
| `where` / `within` | zone/world boundary |
| `requires` | ability/capability contract |
| `authorized by` | authority boundary |
| `causes` | effect lifecycle |
| `success` / `failure` / `rollback` / `compensate` | intent orchestration path |

`who` and `authorized by` are intentionally separate. `who` records the actor
and execution/provenance binding for a step. `authorized by` records the
approval subject that satisfies an authority boundary. They can use the same
participant alias in a small example, but the compiler must not promote a
`who` clause into an `authorized by` clause. Missing authority remains
fail-closed and must be explicit or inherited from an explicit action contract.

If this rule is broken, `intent` becomes a generic workflow VM and the meaning
of `zone`, `authority`, and `effect` collapses.

## 4. Current Pain Point: Fillable Intent Frames, Not Orthogonality

The current pain point is not that the axes are wrong. The pain point is that
the authoring surface often asks humans or AI agents to repeat facts that are
already declared on actions, zones, participants, or authority policies.

Intent is not a natural-language interpreter and the compiler must not invent a
login policy, token lifetime, or authority rule from a goal sentence. The beta
direction is a human-readable and AI-fillable verification frame:

- A human can write and review a compact intent skeleton without memorizing
  every low-level Slot/token/runtime detail.
- An AI agent can expand that skeleton into explicit participants, actions,
  authority clauses, failure paths, and state transitions.
- The compiler answers `YES` or `NO` with source spans, `Reason:`, `Fix:`, and
  owner-layer provenance.
- The AI or human patches the explicit frame and repeats until the contract is
  accepted.

Compact intent is still useful, but it is not magic inference:

- `on: hero.Guard()` can infer `who` from receiver `hero`.
- The action header `within BattleZone` can infer the step `where`.
- If the intent parameter list has exactly one `BattleZone` value, `using` can
  be inferred from that value.
- `authorized by self` on an action can be inherited by a matching step because
  the action contract explicitly declared that approval edge.
- A local `who` clause never creates an `authorized by` edge by itself.
- Explicit clauses still win. If inferred and explicit clauses conflict, the
  compiler must fail closed with a diagnostic that names the missing or
  ambiguous axis owner.

The IR must remain explicit after inference. Compact syntax is authoring
ergonomics, not a hidden semantic shortcut.

Diagnostics should name the concrete axis that supplied a fact. For example,
action-contract reuse should say `reused who`, `reused zone`, `reused
requires`, `reused causes`, or `reused authorized by`, rather than collapsing
those facts into a generic "reused contract" message.

In short: a human states or reviews the goal, AI may propose or fill the intent frame,
and the compiler verifies the frame. The compiler may derive only facts that
have declared language evidence, and every derived fact must be inspectable.

## 5. Important Distinctions

### ability/role vs authority

`ability` and `role` describe static capability contracts. `authority` verifies
whether a participant may exercise that capability across a particular
zone/resource boundary.

### zone vs world

`zone` is the immediate execution and authority boundary. `world` composes one
or more zones and owns handoff, scheduler, outer failure propagation, and
cross-zone freshness.

### subject vs party vs role

`subject` is identity-bearing domain state. `role` is a capability placement.
`party` is a role-bearing participation aggregate. A party may contain subjects
or role slots, but it should not erase the difference between identity and
capability.

### Slot / Pin vs Static Lifetime

`slot` is not a Rust-style static lifetime. It is a runtime-validated handle
model. Static safety comes from CFG/AIR boundary verification and MIR cleanup
facts; runtime safety comes from generation, token, release, and pin checks.

## 6. Orthogonality Audit Procedure

When adding or changing a keyword, clause, or diagnostic, answer these
questions:

1. Which axis owns the semantic fact: Resource, Execution, Domain, or
   Type/Contract?
2. Is another keyword already answering the same question?
3. Where is the final owner fact fixed: DIR, RIR, MIR, AIR, or DAG metadata?
4. Does a later phase consume the owner fact, or does it rediscover semantics
   by walking AST payloads again?
5. Is AIR verifying boundary evidence, or incorrectly becoming the owner of
   domain semantics?

AIR is not the owner of domain semantics; it verifies boundary evidence.

Rule 4 is the practical failure detector: if backend code walks AST again to
rediscover a semantic fact, it is an orthogonality violation.

## 7. Layered Diagnostics

User-facing diagnostics should expose the layer that owns the failure:

- `syntax` for parse/lex errors.
- `type` for type and contract mismatches.
- `resource` for Slot, Pin, raw escape, cleanup, and handle violations.
- `concurrency` for `parallel`, `spawn`, `await`, and `select`
  boundary failures.
- `domain` for `intent`, `zone`, `world`, `authority`, `relation`, `effect`,
  and projection failures.
- `backend` or `driver` only when the failure is not a language-level user
  error.

This keeps the language layered, not jumbled: the surface may show multiple
layers on one page, but each error must say which layer owns the broken rule.

# Pergyra Keyword Orthogonality

Last updated: 2026-05-20

This document fixes the semantic question answered by each Pergyra keyword
family. The goal is not to reduce the number of keywords mechanically. The
goal is to keep different semantic axes from silently owning the same question.

## 0. Four Top-Level Axes

| Axis | Question | Surface |
| --- | --- | --- |
| Resource | Which resource or handle is held across which boundary? | `slot`, `own`, `ref`, `pin`, `unsafe`, `extern` |
| Execution | When, where, and under what concurrency relation does work run? | `parallel`, `spawn`, `async`, `await`, `select`, `channel` |
| Domain | Who acts, in which boundary, under which authority, relation, or effect? | `subject`, `intent`, `zone`, `world`, `authority`, `relation`, `effect`, `projection` |
| Type/Contract | Which shape or ability contract must a value satisfy? | `class`, `struct`, `ability`, `role`, generic `where` |

These axes are not mutually isolated sublanguages. They meet in the verifier
graph. Ownership is kept by the axis that owns the final fact.

## 1. Core Definitions

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

## 2. Intent Is Not A Universal Owner

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

## 3. Current Pain Point: Fillable Intent Frames, Not Orthogonality

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

## 4. Important Distinctions

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

## 5. Orthogonality Audit Procedure

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

## 6. Layered Diagnostics

User-facing diagnostics should expose the layer that owns the failure:

- `syntax` for parse/lex errors.
- `type` for type and contract mismatches.
- `resource` for Slot, Pin, raw escape, cleanup, and handle violations.
- `concurrency` for `parallel`, `spawn`, `await`, `select`, and `channel`
  boundary failures.
- `domain` for `intent`, `zone`, `world`, `authority`, `relation`, `effect`,
  and projection failures.
- `backend` or `driver` only when the failure is not a language-level user
  error.

This keeps the language layered, not jumbled: the surface may show multiple
layers on one page, but each error must say which layer owns the broken rule.

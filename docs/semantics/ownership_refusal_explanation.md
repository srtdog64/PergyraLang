# Bounded ownership refusal explanations

Status: **formal model and reference checker; production refinement OPEN**.
Owner: [`proofs/OwnershipRefusalExplanation.v`](proofs/OwnershipRefusalExplanation.v).
Language direction remains [`27_ownership_clean.md`](27_ownership_clean.md).

## Admitted input and judgments

The model consumes a fixed, finite graph of admitted facts. A natural-number
fact identifies a place in this model; an unguarded rule is always enabled and
a guarded rule reads one boolean summary key. A true key is an **adverse fact**:
may-share, may-carry, may-mutate, or an explicitly unavailable analysis fact.
Removing such facts is the precise direction used by the monotonicity theorem.
This does not order arbitrary source edits or all possible ownership summaries.

Seeds name constructor overlap, an unconsumed alias, ref/index origin, a shared
result field, or unavailable analysis. Unary transfer edges name assignment,
argument-to-result flow and field access. Non-exclusivity requires one cause,
so a witness stores one derivation, not every alternative cause. The graph may
have multiple definitions, alternatives and cycles. Its completeness, field-path
identity and dependence on consumed-copy facts are importer obligations.

`Shared` is the least inductive closure. `Exclusive` is its complement and is
proved to satisfy the greatest fixed-point equation. Synchronous rounds start
empty and retain admitted facts; they stabilize within the number of distinct
graph facts. At first discovery, the round equals the **minimum** derivation
height. A cyclic graph can also have longer valid derivations of the same fact.
The production EV owner mutates facts in place within a round and grows copy
edges and dependencies. Its iteration count is not this theorem's round count.

A boundary fixes a site, target, effect guard and a concrete model trace. The
decision refuses an enabled boundary when its target is in the sharing closure.
An unknown requested boundary is not a refusal of a source program: both the
decision and checker return false. There is no source acceptance/safety theorem.

## Descriptor execution and the two explanation classes

The execution model reuses `OwnershipCleanCore.Block`, `beqb`, `bmem` and `free`.
It contains allocation, shallow descriptor copy, mutation, and read. An actual
relocation retires the old buffer and updates only the mutated descriptor.
Allocation identities are never reissued. `copied_descriptor_growth_stale`
proves a read through the original copy reaches retired storage.

`relocates` describes an **actual relocation in this execution**, not merely a
builtin that could relocate. A production Array push or Map set is an instance
only after a refinement establishes its capacity/allocation event. Map payload
replacement/removal is a distinct retirement obligation, not proved by calling
it descriptor relocation. Graph-store growth was already represented by
`OwnershipGraphLinks.unguarded_grow_dangles`; this adds descriptor explanation
contracts rather than claiming no previous growth model existed.

The checker binds the requested boundary, location, target, every rule and its
parent, and every required summary guard. It then checks one disjoint class:

- `Hazard`: the boundary's concrete model trace actually reaches a stale read
  of the **requested target**. An unrelated register's stale read, invalid
  allocation, unknown register or a read before growth cannot certify it.
- `ExclusivityAt target`: the graph cannot establish that target's exclusivity,
  and the supplied trace does not establish this target's hazard. The missing
  item is the named exclusivity obligation. This may be conservative refusal
  of safe code; it is not proof of an unsafe source execution or a minimal
  missing source-language fact.

The trace belongs to the admitted program input. The witness cannot replace it
with an arbitrary trace. Connecting it to a source CFG, branch choices, calls,
loops and return paths remains OPEN. Let/call/if/loop/return syntax is not
silently assumed covered merely because it can eventually produce graph facts.
Async, concurrency, FFI, actual allocator reuse and runtime integration are
outside this model.

## Proved contracts

| Contract | Theorem |
| --- | --- |
| Copy, actual relocation, original read reaches retired storage | `copied_descriptor_growth_stale` |
| Every sharing fact has a finite derivation | `shared_has_derivation` |
| Finite saturation and exact sharing closure | `finite_saturation`, `saturation_is_sharing` |
| Bounded derivation height and first-discovery minimum | `rounds_derivation_height`, `first_round_is_minimum_height` |
| Complement is the greatest exclusivity fixed point | `exclusive_fixed_point`, `exclusive_greatest` |
| Tree checker agrees with rule validity | `check_tree_spec` |
| Witness checker is sound and complete for the model relation | `check_witness_sound`, `check_witness_complete` |
| Every modeled refusal has a checked witness | `refusal_has_witness` |
| Accepted witnesses imply the requested refusal | `checked_witness_refuses` |
| Exhaustive, disjoint hazard/missing classification | `witness_classified`, `hazard_and_missing_disjoint` |
| Equal cited summary facts preserve the same witness and refusal | `witness_sufficient`, `witness_preserves_refusal` |
| New refusal cites at least one changed summary fact | `remote_break_attributed` |
| Removing adverse facts cannot introduce a refusal | `fewer_effects_fewer_refusals` |

Locality fixes the graph, boundaries, trace and relocation interpretation.
The support of one successful witness preserves **that refusal**; it is not
a complete dependency set for every accepting decision or every alternative
explanation. In an acceptance-to-refusal flip, the theorem uses a witness from
the refusing version. The reverse direction uses the same theorem with the
versions exchanged. An unrelated graph change is outside these premises.

These contracts add no axioms or admitted proofs. The existing assumption
budget is unchanged. Independent proposition consumers are in
[`OwnershipRefusalExplanationAudit.v`](../../tests/coq/OwnershipRefusalExplanationAudit.v).

## Executable evidence and remaining production work

Run on Linux through the pinned Rocq 9.3.0 / Stdlib 9.2.0 wrapper:

```sh
bash scripts/run_rocq_toolchain.sh timeout 300 bash tests/ownership_refusal_explanation_smoke.sh
bash scripts/run_rocq_toolchain.sh timeout 300 bash tests/ownership_refusal_explanation_selftest.sh
bash scripts/run_rocq_toolchain.sh timeout 1800 bash tests/formal_semantics_smoke.sh
```

Set an explicit installed `OPAMROOT` first. The focused gate uses the existing
kernel/assumption-budget owner, extracts a fresh OCaml checker, executes
independently assembled positive and forged-input controls, and binds sources
and artifacts with SHA-256 receipts under `.tmp/ownership-refusal-explanation/`.
Naturals retain their inductive representation; there is no unchecked native
integer extraction. Extraction and the OCaml toolchain remain trusted tools.
The executable exposes model self-tests; its generated `.ml`/`.mli` library
exposes `check_witness`. There is no production artifact parser yet.

The self-test plants wrong-rule acceptance, unrelated-reader acceptance, and
a weakened responsibility theorem in isolated copies. The last must break the
independent proposition consumer even if the owner theorem name is unchanged.

Before translation validation can cover DRV-2 refusals, production must export
the authoritative input graph, exact generation and identity bindings, reason
provenance, missing-fact category, and feasible execution evidence when claiming
danger. The importer must validate that input against the admitted source/MIR
facts; checking a self-invented graph would prove nothing about the compiler.
Every refusal in a fixed DRV-2 census must then carry an accepted witness, with
forged facts rejected. None of those production obligations is closed here.

# Valen group borrowing compared with Pergyra exclusive places

Date: 2026-10-11 (Asia/Seoul)
Status: READ-ONLY DESIGN COMPARISON COMPLETE. Decision: not adopted as a
model; one counterexample becomes an open obligation of the active DRV-2 rung.

This review compares the design in Evan Ovadia's post "Group borrowing: a
near miss, and huge potential" (Valen,
<https://verdagon.dev/blog/valen-group-borrowing>, read 2026-10-11) with the
ownership model Pergyra already specifies and implements. It starts no
implementation track, adds no syntax and changes no semantic owner. The
semantic authorities stay `docs/semantics/27_ownership_clean.md` and
`docs/semantics/28_memory_boundary_composition.md`; the active work record is
`docs/agent_work_directives/drv2_ownership_entry_frontier_2026-10-11.md`.

## What the post specifies

Group borrowing is Nick Smith's design (first aimed at Mojo); Valen builds on
it. The claims below are the ones the post states, checked against its text.

- **Single ownership and paths.** Every object has one owner and therefore
  one path, starting at a local, a parameter or a group parameter and
  extended by `.member`, `[]` (collection elements) or `.Variant`. A
  reference's type records the path it points into, e.g.
  `&Entity in world.entities[]`.
- **One reference kind, aliasing mutation allowed.** Several references to
  the same object may coexist and all may write. The safety condition is
  relaxed from shared-xor-mutable to "no use-after-free".
- **Invalidation is inside-only.** Mutating an object invalidates references
  pointing *inside* it (an element after `push`, the old contents after a
  container is reassigned), not references to the object itself.
- **Explicit function contracts.** A signature states what it mutates:
  `mut(self)`, or a narrower group such as
  `func step(world &World, entity in world.entities[] mut)` (mutable inside an
  immutable whole). Callers keep references the declared mutation cannot
  reach. Returned references can use a wildcard descendant path
  (`-> &Gem in self.gem...`).
- **Type-stability exception** (appendix, not implemented): mutation never
  invalidates type-stable data, so `my_vec.push` leaves a pointer to
  `my_vec.size` valid.
- **Costs.** The post presents the benefit as ergonomics and flexibility, not
  speed; it hedges the zero-run-time-cost claim and says optimizer gains
  would need LLVM work because LLVM reasons in `noalias`.
- **Open in the post:** unique groups for Rust `&mut` interop, reference
  counting and generational references, associated and multi-typed groups.

## Where Pergyra differs

| Question | Valen | Pergyra |
| --- | --- | --- |
| What an alias is | A borrow reference carrying a path | No references in ordinary values (D1 value semantics). An alias is a descriptor copy sharing a buffer |
| Safety condition | No use-after-free; aliasing writes allowed | Mutation needs exclusivity for the places actually accessed (28 §"Shared connections and mutation") |
| Unit of invalidation | References inside the mutated path | Readers of the storage a growth retires, per first field (exclusive-value growth summaries) |
| Function contract | Written by the user in the signature (`mut(...)`, `in path`) | Inferred and recorded in IR: mutating formals, grown fields, carrying paths, shared result fields; observer-visible, not syntax (27 §3) |
| Mutable part of a readable whole | Allowed (`world` read, `entity` mutable) | An `inout` member path is a focus; other arguments cannot name the root (27 §3) |
| Long-lived links | Structs may hold mutable references to a shared subsystem; RC and generational references planned | Ordinary values store no references; links are Slot/store handles with generation checks (28) |
| Reclamation | Not covered by this post | Owner inventory only; no tracing collector, no reference counting (28) |

## Corrections to an earlier comparison

An earlier assistant comparison of the two designs reached the same decision
but on some premises the post does not support:

- **"Valen's mutual-isolation condition"** does not appear in this post (no
  occurrence of "isolat"). It may come from Nick Smith's original proposal;
  this source does not establish it, so it is not used as a reason here.
- **Action at a distance cuts the other way.** Valen's `mut(...)` contract is
  written in the signature, so a body that mutates more than it declares
  fails in the callee. Distant breakage is the cost of *inferred* summaries,
  which is Pergyra's chosen design: 27 §3 already states that a body change
  can make a caller need `Clone(x)` and that the diagnostic names why. The
  real trade-off is explicit contracts versus inferred, inspectable facts.
- **"Child groups"** are not a concept in this post. The ideas that transfer
  are inside-only invalidation and the type-stability exception.
- The post itself hedges zero cost; that point stands.

## Decision

Not adopted:

- first-class `group` syntax or group annotations in function contracts;
- several live mutable aliases of one place;
- "no use-after-free" as the mutation rule in place of place exclusivity;
- group borrowing as a new foundation for memory safety.

Kept as reference, mapped to existing Pergyra facts:

- **Field-precise effect summaries.** Already the exclusive-value direction:
  disjoint first fields are separated automatically, summaries carry the
  read, written and grown places, and a double mutation of one place, or of
  places not known to be disjoint, is refused. Release paths stay with
  cleanup synthesis, not with these summaries.
- **Type-stability exception.** Matches the decision that `ArraySet` and an
  indexed assignment write in place and retire no buffer, so they are not
  growth.
- **Inside-only invalidation as a diagnostic.** Explaining why an element
  view dies after its container grows is the form refusals should take.

## Counterexample this comparison exposed

The inside-only rule is exactly the hazard behind a defect found the same
day. The S/T slice (`8c5cc3fe`) classified a sink argument into
`let copied = copies;`, a value copy of a `HashMap<String, Int>`. A HashMap
value copy shares its buckets, so a `MapSet` that rehashed through the copy
retired the storage the original descriptor still read. The B slice moves
the map out of the facts and back instead.

The exclusive-value model would not have caught it: its growth summaries
count `ArrayPush`, `ArrayPop` and `ArrayPushOwnedString`, but not `MapSet` or
`MapRemove`, while AGENTS.md lists HashMap among growable runtime containers.

**Open obligation (DRV-2 rung):** treat every growable container operation
that may move storage (map insert and remove, and the equivalents for
`Slice`, `List`, `Queue` and `Set`) as growth of the place it targets, so the
rule "growth invalidates views inside the grown storage" covers all of them.
The falsifier is a fixture that copies a map descriptor, grows the copy and
then reads the original; it must be refused. The obligation closes when that
fixture is registered and refused and the corpus recount reports no new
admission.

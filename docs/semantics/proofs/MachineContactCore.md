# The Machine-Contact Layer: the primitive below the slot

This is the durable record of a systems-language design question: **what is the
point that actually touches the machine, and how does Pergyra express it without
a raw pointer?** It states the design, the machine-checked chain that grounds it
([`MachineContactCore.v`](MachineContactCore.v)), and the implementation roadmap
that other agents should follow.

The short answer: **the layer below the slot is a `Region` — a raw typed span
that carries extent, access-mode, and grant-rooted provenance as evidence, and is
grounded in a DECLARED machine `Grant`. It is not a raw pointer; it is the place
where the four facts a raw pointer forgets are recovered and checked. The keystone
`place : Region -> Slot` is proven to preserve the grant-rooted safety chain, with
zero axioms.**

## 1. The question (and why it is load-bearing)

A slot (`docs/semantics/08_slot_capability_calculus.md`,
[`SlotCalculus.v`](SlotCalculus.v), [`SlotLifecycleCore.v`](SlotLifecycleCore.v))
is a *high* abstraction: typed, owned, lifecycle-tracked, with interprocedural
static UAF checking above it. A systems language that intends to reach
freestanding / bare-metal operation (the road to a next-generation OS) needs the
layer *below* the slot — the object that reads and writes actual memory: a page
from the MMU, an MMIO register window, a linker-defined section, a DMA buffer.

In C/Rust/Zig that object is a raw pointer / `unsafe` / `[*]u8`. That object is
the **most meaning-destroyed value in systems programming**: a machine word that
has forgotten
- **how far it extends** (extent / bounds),
- **who backs it** (provenance: which grant / page / MMIO window),
- **how the machine must treat it** (access mode: plain RAM vs volatile MMIO vs
  atomic),
- **where it came from and how long it is valid** (lifetime).

Pergyra's thesis is lost-meaning recovery (`docs/semantics/19_theoretical_foundations.md`).
The layer below the slot must therefore *recover* exactly those four facts as
evidence — not re-forget them behind an escape hatch.

## 2. The primitive and the chain

```
  Slot        typed cell (sizeof T, plain), owned, lifecycle      (HIGH)
    ▲  place : Region -> option Slot   (bounds + alignment + Plain-mode gated)
    │
  Region      { base, extent, mode(Plain|Volatile|Atomic), prov: GrantId }   (LOW)
    ▲  carve : Region -> off,len -> option Region   (allocator split)
    │
  Grant       { id, base, size, mode }  -- the DECLARED machine ground truth   (AXIOM)
              (a boot/extern declaration of a real address range: page, MMIO,
               section, DMA. The "deed".)
```

- A **`Region`** is the raw typed span. It is `region_valid` w.r.t. a `Machine`
  (a list of grants) iff it traces to a grant with the same id and mode, its
  bytes lying within that grant. That is provenance made structural.
- A **`Grant`** is data, not an axiom of the logic. The *language* treats the
  first page / MMIO window / linker section as **declared** — you cannot analyze
  the machine's memory map into existence, you declare it and everything above is
  checked against it, failing closed otherwise. This is decide-vs-declare
  (`docs/semantics/19`, the Rice corner) reaching all the way to the metal.
- **`place`** reinterprets a region's bytes as a typed data slot. It fails closed
  unless the region is `Plain` (a data slot may not sit on MMIO/atomic memory),
  the type fits, and the base is aligned.
- Touching the metal is **capability-gated** (`place_guarded`): the metal is an
  effect. Ordinary code holding no metal capability provably produces no slot from
  a region — the basis of "prove no hidden access" for freestanding code.

## 3. What is proven ([`MachineContactCore.v`](MachineContactCore.v))

Fully constructive: **0 admits, 0 axioms.** Verified against
`rocq/rocq-prover:9.0.1`: all corpus proofs compile and the `rocqchk` axiom
budget is unchanged (exactly `SlotCalculus.MaxSlotId` + `SlotCalculus.verify_token`,
the three unsafe-feature sections `<none>`).

| Theorem | What it establishes |
| --- | --- |
| **`place_grounds_slot`** (keystone) | placing a slot on a valid region yields a slot GROUNDED in a real grant: provenance, mode, and bounds preserved |
| `chain_grant_carve_place_grounded` | end-to-end grant→carve→place stays grounded — nothing an allocator hands out escapes the declared machine |
| `no_wild_slot` | no slot exists without provenance to a declared grant |
| `grant_yields_valid_region` | the full region of a declared grant is valid |
| `carve_preserves_validity` / `carve_disjoint` | allocator split preserves validity and yields disjoint sub-regions |
| `place_rejects_volatile` / `place_rejects_atomic` | a data slot may NOT be placed on MMIO / atomic memory (access-mode discipline) |
| `place_oversize_fail_closed` / `place_misaligned_fail_closed` | placement fails closed on overflow and misalignment |
| `placed_slots_disjoint` | slots placed on disjoint regions do not alias |
| `cap_gate_fail_closed` / `guarded_place_grounds_slot` | no metal capability ⇒ no slot; the gate restricts who may cross it without weakening grounding |

## 4. How malloc, pthread, and freestanding map onto this

The design deliberately rejects Zig's "thread an `Allocator` through every
signature" (that is allocator-coloring, the same viral tax as async coloring).
Instead:

- **Allocation = capability + zone, not a threaded allocator.** The right to
  allocate is a capability (freestanding code that is not granted it provably does
  not heap-allocate); the allocation *strategy* is a scoped `Zone` that carves
  `Region`s, not a parameter.
- **`malloc` is one grant backend.** The hosted root zone is malloc-backed;
  freestanding installs a different root `Grant` (page allocator / bump / pool) at
  the boundary — one place, not every function.
- **`pthread` is one lane-scheduler backend.** Concurrency is already abstracted
  behind channels + the SEA execution-lane contract (`docs/semantics/...`, the
  SEA/`PgyLaneScheduler` split). `pthread` is the hosted implementation of that
  lane seam; freestanding slots a bare-metal lane scheduler into the same seam.
  This is a runtime swap the architecture already anticipates, not a language
  change.

A kernel then falls out: boot declares the physical map + MMIO windows as
`Grant`s (the axioms); the page allocator is a `Zone` carving `Region`s; drivers
receive volatile `Region`s and place typed register slots on them; the scheduler
and filesystems are ordinary slot/zone Pergyra above. "No hidden allocation" is
provable because allocation is a capability; "no wild pointer" is provable because
there are no naked pointers, only grant-rooted regions; MMIO correctness is
enforced because access-mode is typed.

## 5. Implementation roadmap (this proof is the invariant to implement against)

This file is **design + proof-of-concept**, not compiler implementation. There is
no `Region` primitive on the language surface or in codegen yet. The order:

1. **Surface + IR wiring** for `Region` / `Grant` / `grant` / `carve` / `place`
   (and `Zone` as the scoped carver), threading the four evidence fields through
   HIR→RIR→MIR.
2. **`raw` / `mmio` capabilities** added to the effect system as first-class
   refinements (ambient-propagated via `with caps`, not passed as values), so
   `place_guarded`'s gate is real.
3. **Relocate `malloc`** to be a root-grant backend; introduce the freestanding
   root-grant backends (page/bump/pool).
4. **Bare-metal lane backend** at the SEA/`PgyLaneScheduler` seam.

Each step references the theorems above as its implementation invariant: e.g.
codegen for `place` must preserve `place_grounds_slot` (a placed slot's runtime
metadata must trace to its grant), and the `raw`/`mmio` capability check must make
`cap_gate_fail_closed` a compile-time reality.

## 6. Negative scope (do not over-claim)

- This is the **static grounding chain** (extent + mode + provenance) and its
  fail-closed gates. It is **not** full pointer-provenance soundness under
  arbitrary aliasing and type-punning — the Stacked/Tree-Borrows-grade obligation.
  That, plus the composition of raw-layer aliasing with slot ownership, is the
  named open research corner of this layer.
- Concurrent atomics' memory ordering is modeled only as an access mode here, not
  as an ordering semantics.
- `Grant` is not yet bound to a live boot memory map; the binding of this model
  onto real hardware declarations is a separate adequacy obligation.
- Do not describe the result as Rust-equivalent memory safety. State precisely
  what the theorems establish (grant-rooted grounding, access-mode discipline,
  fail-closed placement) and what remains open (provenance soundness).

## See also

- [`MachineContactCore.v`](MachineContactCore.v) — the proof itself.
- `docs/semantics/08_slot_capability_calculus.md` — the slot, one layer up.
- `docs/semantics/04_ownership_abi.md` — `own`/`ref` and anchored slot handles.
- `docs/semantics/19_theoretical_foundations.md` — lost-meaning recovery and the
  decide-vs-declare (Rice) corner this layer instantiates at the metal.
- `docs/semantics/15_capability_sandbox.md` — capability-as-effect, the gate model
  the `raw`/`mmio` capabilities extend.

# 168. Fortran-Derived Data-Parallel Evidence

Status: `partially-landed`, language evidence and projection contract
(audited 2026-07-16)

## Decision And Claim Scope

The near-term competitive bar is game-expression sufficiency, not Fortran parity.
Per-frame entity updates, spatial grids, deterministic reductions, and
event channels are the workloads this ladder is climbed for. BLAS-class numeric
supremacy is not the current claim.

Pergyra remains C-family row-major by default. Column-major, tiled, blocked, and
device layouts are target facts rather than language-wide defaults.

The declared join surface has already landed two important Fortran-shaped
facts:

- `parallel (i in lo..hi)` produces checked index-disjoint write evidence;
- `join with sum|product|min|max` names a deterministic index-order fold with
  checked arithmetic and fail-closed empty `min` / `max`.

These are narrow, real instances of DP-3 and DP-4. They are not general loop
independence or a general `ReductionFact` over arbitrary loops. No
Fortran-class optimization claim is allowed until general facts and a vector or
non-CPU projection consume them.

This is a language/compiler capability contract and a Pergyra competitiveness
axis. It is intended to become user-visible language power.
It is not a repository hygiene library.

## Plane Split

There are two separate planes:

- **Language plane**: Fortran-derived data-parallel evidence, owned by Pergyra
  semantics, MIR/AIR/ABI facts, and target projection.
- **Repository-authoring plane**: codebase gates that steer future LLM-written
  code away from source-of-truth violations.

The Language plane is the programming-language feature. The
Repository-authoring plane is maintenance policy and must not be presented as a
user feature or stdlib package. This document owns only the language plane.

The competitive claim is specific. Pergyra should preserve data-parallel
intent, ownership, layout, and projection evidence so future CPU, GPU, tensor,
NPU, or dataflow backends do not have to recover meaning from CPU-shaped code.

## Current Compiler Structure

The live implementation is an evidence pipeline, not a thread API attached
directly to syntax:

```text
parallel surface
  -> semantic admission and capture-disposition facts
  -> MIR-owned parallel boundary rows
  -> AIR BoundaryCaptureFact
  -> ExecutionLaneFact
  -> C / LLVM lane facade
  -> runtime executor
```

The current owners and limitations are:

| Layer | Landed owner | Current fact or behavior | Honest limitation |
|---|---|---|---|
| Surface | parser parallel owner | arm form, collection/range join, `all`, `any`, `give`, and `sum|product|min|max` | reactive `parallel on` remains declared and fail-closed |
| Semantic | parallel flow owners | rejects outer replicated writes, growable collection pointer sharing, slot conflicts, and scalar write races | specialized admission, not a general affine dependence solver |
| Capture evidence | `SemanticParallelCaptureBoundaryFact` | `snapshot_copy`, `join_index_disjoint`, and `join_readonly` rows keyed by stable boundary identity | general no-alias, layout, and arbitrary-loop dependence facts are not produced |
| MIR | parallel capture import owner | copies and validates sealed semantic rows; missing or duplicate facts fail closed | vocabulary is narrower than the future DP fact set |
| AIR / SEA | `BoundaryCaptureFact -> ExecutionLaneFact` | pin, live view, raw slot/channel, value-only, authority crossing, IO/FFI, await-local, and deterministic fork-join evidence | precise value-capture coverage is incomplete for every boundary shape |
| C / LLVM | lane facade consumers | both projections use lane-owned spawn, await, cancel, and channel entry points; join lowering consumes MIR capture rows | join creates one task per element and owns no grain/chunk plan |
| Runtime | `PgyLaneScheduler` | Reject is fail-closed; Inline/PinnedZone stay local; pool-shaped lanes preserve the lane fact | the self-hosted contract reports `scaffold-synchronous`; dedicated executors are incomplete |

The implemented surface has three distinct families:

1. `parallel { ... }` is scoped task fan-out with a join. It admits only
   boundary patterns for which resource and scalar sharing are sound.
2. `parallel (x in xs)` and `parallel (i in lo..hi)` are data-parallel join
   forms. They support ordered result collection, `any`, declared reductions,
   and the current read-only stencil form.
3. `parallel on (...)` is a reserved reactive family. It must remain
   fail-closed until lifecycle, virtual-clock, cancellation, and lane facts are
   executable.

`spawn`, async/await, select, and channel operations belong to the execution
family but do not replace the `parallel` data relationship.

## Evidence That Is Already Real

Three properties distinguish the current design from ordinary thread-pool
sugar.

### Snapshot readers

For primitive scalars, one task may retain the exclusive live writer while
reader tasks receive a pre-boundary copy. Semantic admission records
`snapshot_copy` with the writer task. C and LLVM consume the imported MIR row;
they do not rediscover the rule from source text.

Two writers are rejected. A writer plus a reader of a non-snapshot-eligible
value is rejected. Growable collection storage is not admitted by raw pointer.

### Disjoint index writes and stencil reads

The range join admits supported array writes only when each replicated task
writes through its own index binding. The semantic owner records
`join_index_disjoint`.

A separate array may be admitted as `join_readonly` for stencil-shaped reads.
Because an array handle copy may still alias the same backing buffer, both
backends emit a fan-out alias guard between read-only and index-written
captures. Alias is an authority mismatch, not permission to race.

### Lane derivation after safety evidence

`BoundaryCaptureFact` classifies pins, live views, raw slots, raw channels,
value-only capture, authority crossing, blocking effects, await-local work, and
deterministic fork-join shape. The pure decision table derives one
`ExecutionLaneFact`.

The ordering is important:

```text
capture and effect evidence
  -> safety / movability verdict
  -> execution lane
  -> concrete executor
```

`WorkerPool` says where already-admitted work may run. It is not proof of
no-alias, purity, layout, or iteration independence.

## Fortran Lessons And Pergyra Owners

Fortran's useful effect is that array programs often expose facts the compiler
can optimize without heroic pointer analysis:

- procedure arguments are usually not arbitrary aliases;
- whole-array and elemental operations expose bulk intent;
- `do concurrent` declares iteration independence;
- reductions are explicit;
- contiguous arrays and simple strides are common.

Pergyra should import those facts, not Fortran syntax or hidden trust.

| Fortran lesson | Pergyra fact direction | Current |
|---|---|---|
| no arbitrary argument aliasing | `NoAliasViewFact` / `UniqueWriteFact` from Slot/View ownership | partial, join-local evidence only |
| `do concurrent` independence | `IterationIndependenceFact` from read/write sets and effect summaries | declared join forms only |
| `elemental` / `pure` procedures | `ElementalPureFact` from effect-free bodies and value-only captures | not landed as a general fact |
| contiguous arrays and regular slices | `ArrayLayoutFact` from ABI/layout owner | ABI rows exist; loop-consumable fact not landed |
| reductions | `ReductionFact` with operator, identity, accumulator type, and result owner | fixed join combinators only |
| forbidden hidden fallback | `ProjectionFallbackFact` from the target capability owner | contract exists; DP target consumption is not landed |

The names in this table are owner directions unless the Current column says
otherwise. They must not be treated as an already stable source API.

## Projection Contract

A data-parallel fast path is sound only when:

1. Every mutable output has one owner or a disjoint slice proof.
2. Shared inputs are value copies or pinned read-only views.
3. Iteration write sets are pairwise disjoint, except for explicit reductions.
4. Effects are absent or proven iteration-local.
5. Layout facts name element ABI, stride, contiguity, and address space.
6. Reductions name the operator, identity, accumulator type, order contract,
   and result owner.
7. The target profile proves support for the selected projection.
8. Missing evidence rejects or emits a visible fallback reason.

Heuristic evidence must not enable `noalias`, `llvm.assume`, vector metadata,
or a non-CPU projection.

## Strategic Position

Rust primarily makes ownership, borrowing, and data-race freedom explicit for
safe memory access. Pergyra's parallel problem setting is broader: it attempts
to preserve resource ownership, effect, authority, boundary movability,
deterministic coordination, layout, and target projection in one compiler fact
chain.

That is a larger architectural claim, not a maturity claim. Rust has a
battle-tested implementation. Pergyra has a partially landed contract whose
value must be demonstrated by CPU performance and at least one non-CPU
projection.

The user should normally state the data relationship and result contract. The
compiler should derive capture, lane, and projection facts. New syntax is
justified only for a real ownership, authority, loss, interoperability, or
observable-cost boundary.

## What Not To Import

Do not import:

- implicit typing;
- hidden alias assumptions;
- global/common storage patterns;
- a language-wide column-major default;
- a lane choice presented as a proof of vector safety;
- backend-local vectorization that bypasses MIR/AIR/ABI facts;
- silent CPU fallback when a requested projection lacks evidence.

Layout is a fact, not a language destiny.

## Implementation Ladder

| Rung | Work | Current | Gate |
|---|---|---|---|
| DP-0 | document owners and forbid hidden fallback | landed | this document, index link, parallel contract gate |
| DP-1 | emit `ArrayLayoutFact` for known arrays and slices | not landed as a loop-consumable owner fact | ABI/layout golden |
| DP-2 | derive read/write sets for simple counted loops | declared join forms only | CFG/body dataflow smoke |
| DP-3 | prove disjoint-by-index writes | landed for index-mode join captures, not general loops | data-parallel evidence smoke |
| DP-4 | add explicit reduction facts | fixed combinator surface; general `ReductionFact` is not landed | reduction positive/negative fixtures |
| DP-5 | lower eligible loops to vector-friendly C/LLVM | worker lowering landed; vector/chunk plan absent | output, behavior, and performance parity |
| DP-6 | add a non-CPU projection prototype | not landed | projection golden plus visible fallback reasons |

Pergyra must not claim general Fortran-class data-parallel optimization before
general DP-3. It must not claim NPU/tensor support before DP-6.

## Closure Order

The next work should deepen the existing path rather than add another parallel
surface:

1. Complete precise boundary capture production without source-kind or
   routine-wide heuristics.
2. Introduce an owner for grain, chunking, and bounded fan-out so a large join
   does not require one runtime task per element.
3. Promote join-local disjoint, layout, and reduction rows into general DP
   facts only with positive, aliasing, empty, overflow, and effectful negative
   fixtures.
4. Give BlockingPool, LocalAsync, WorkerPool, and MovableScheduler distinct
   executors while preserving one lane contract.
5. Demonstrate one projection beyond ordinary CPU worker execution. An
   unsupported target must emit a visible fallback or reject.

## Related

- [`146_sea_execution_lanes.md`](146_sea_execution_lanes.md)
- [`178_parallel_boundary_evidence.md`](178_parallel_boundary_evidence.md)
- [`181_parallel_surface_full_design.md`](181_parallel_surface_full_design.md)
- [`182_parallel_remaining_bones_work_orders.md`](182_parallel_remaining_bones_work_orders.md)
- [`semantics/05_parallel_execution.md`](semantics/05_parallel_execution.md)

# 168. Fortran-Derived Data-Parallel Evidence

Status: `design-contract`, post-beta projection work (2026-07-06)

This document records the part of Fortran worth importing into Pergyra's
parallel model. The target is not Fortran syntax, column-major defaults, or
implicit compiler trust. The target is the evidence shape that lets a compiler
lower array-heavy code aggressively without guessing.

This is a language/compiler capability contract. It belongs to Pergyra program
semantics and target projection, not to repository authoring guidance or
LLM/agent guardrails.

Fortran's useful effect is that array programs often give the compiler strong
facts:

- procedure arguments are usually not arbitrary aliases;
- whole-array and elemental operations expose bulk intent;
- `do concurrent` says loop iterations are independent;
- reductions are explicit enough for parallel lowering;
- contiguous arrays and simple strides are common and optimizable.

Pergyra should import those as owned facts, not as hidden assumptions.

## Pergyra Mapping

| Fortran lesson | Pergyra fact | Owner direction |
|---|---|---|
| No arbitrary argument aliasing | `NoAliasViewFact` / `UniqueWriteFact` | derived from Slot/View ownership and parameter modes |
| `do concurrent` independence | `IterationIndependenceFact` | derived from loop body read/write sets and effect summary |
| `elemental` / `pure` procedures | `ElementalPureFact` | derived from effect-free function body and value-only captures |
| contiguous arrays and regular slices | `ArrayLayoutFact` | derived from ABI/layout owner: base, length, stride, contiguity |
| reductions | `ReductionFact` | explicit accumulator, operator, identity, and output slot |
| forbidden hidden fallback | `ProjectionFallbackFact` | target capability envelope records why CPU fallback is required |

The spelling above is not a landed API. It names the owner facts future work
must provide before a data-parallel backend or SIMD/vector lowering can claim
support.

## Contract

A data-parallel projection may use a fast path only when all required facts are
present:

1. Every mutable output has a single owner or a disjoint slice proof.
2. Shared inputs are read-only views or value copies.
3. Each iteration's write set is disjoint from every other iteration's write
   set, except for explicit reductions.
4. The function body is effect-free or its effects are proven iteration-local.
5. Layout facts name contiguity, stride, element ABI shape, and address-space
   boundary.
6. Reductions name the operator, identity, accumulator type, and result owner.
7. Missing evidence fails closed or records an explicit fallback reason.

This is intentionally stricter than classic "the compiler probably knows"
vectorization. Pergyra's advantage is not blind optimism; it is visible evidence.

## Relation To SEA And Projection Replacement

SEA decides whether a boundary can move and which execution lane it may use.
Fortran-derived data-parallel evidence is narrower: it decides whether a
specific bulk loop, map, filter, transform, stencil, or reduction can become a
SIMD, worker-pool, GPU, tensor, NPU, or dataflow projection.

The two layers compose:

```text
intent/effect/authority facts
  -> BoundaryCaptureFact / ExecutionLaneFact
  -> data-parallel facts
  -> target projection
```

The execution lane answers "where can this work run safely?" The data-parallel
facts answer "can this work be split, vectorized, fused, or moved to a bulk
backend without changing the result?"

## Syntax Direction

Do not add a broad `fortran` mode or backend-specific keyword. The surface
should remain Pergyra-shaped:

```pergyra
intent UpdateParticles(frame: FrameZone)
{
    step integrate
    {
        where: frame;
        on: particles.each disjoint by index {
            velocity[i] = velocity[i] + acceleration[i] * dt;
            position[i] = position[i] + velocity[i] * dt;
        }
    }
}
```

The example is intentionally future syntax. The semantics are the important
part:

- `disjoint by index` must lower to a checked disjoint-write proof.
- `particles.each` must name the bulk collection owner.
- captured values must be value-only or read-only views.
- missing layout or independence evidence must reject or use a visible fallback
  fact.

Reductions should be explicit rather than inferred from arbitrary writes:

```pergyra
let total: Float =
    particles.reduce sum identity 0.0 over mass;
```

Again, this is future surface. The required fact is a `ReductionFact`, not the
exact spelling.

## What Not To Import

Do not import:

- implicit typing;
- hidden alias assumptions;
- global/common storage patterns;
- a language-wide column-major layout default;
- backend-local vectorization that bypasses AIR/MIR/ABI facts;
- silent CPU fallback when a data-parallel projection is missing evidence.

Layout is a fact, not a language destiny. A target may prefer column-major,
row-major, tiled, blocked, or device-specific placement, but that choice belongs
to `ArrayLayoutFact` and target projection facts.

## Implementation Ladder

| Rung | Work | Gate |
|---|---|---|
| DP-0 | Document owner fact names and forbid hidden fallback | this document plus index link |
| DP-1 | Emit `ArrayLayoutFact` for known `Array<T>` / slice shapes | ABI/layout golden |
| DP-2 | Derive read/write sets for simple counted loops | CFG/body dataflow smoke |
| DP-3 | Prove disjoint-by-index writes for same-length arrays | data-parallel evidence smoke |
| DP-4 | Add explicit reduction facts | reduction positive/negative fixtures |
| DP-5 | Lower eligible loops to C/LLVM vector-friendly form | C/LLVM output parity |
| DP-6 | Add a non-CPU projection prototype | projection-specific golden tests plus visible fallback reasons |

Until DP-3, Pergyra should not claim Fortran-class data-parallel optimization.
Until DP-6, it should not claim NPU/tensor backend support. The near-term value
is to reserve the correct evidence shape now so future projections do not have
to recover meaning from source text or CPU-shaped MIR.

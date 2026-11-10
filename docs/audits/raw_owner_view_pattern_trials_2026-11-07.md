# Raw access, owner/View and allocator experiments

Status: bounded executable comparison complete. This is an audit and a set of
test implementations, not a new semantic owner or production allocator policy.
The active receipt is linked from
[`raw_owner_trials`](../truth/state/raw_owner_trials.md).

## Result and abstraction boundary

Owner -> temporary View -> actual loads/stores is a useful interface for the
tested programs. The owner supplies lifetime and retirement authority. A View
supplies an admitted range and access to that owner's storage. An address
wrapped in a View does not create an allocation owner or prove a lifetime.

The allocator is another dimension: ordinary malloc, a phase bump region and
a reusable pool can all sit below the same ownership contract. Logical last
use, physical return to an allocator and return of resident pages to the OS
are different observations. The receipts measure actual allocator calls and
requested/retained bytes; they do not equate free with an immediate RSS drop.

The six real Pergyra algorithms use existing Array/Slice, scalar variables and
ordinary functions. Their source has no manual retirement. Equivalent C
programs manage pointer lifetimes explicitly. The independent oracles test
outputs rather than duplicating the same data-structure implementation.
The programs are fixed-capacity workloads, not a generic library release.

## Implemented pointer idioms

| Idiom | Implemented form | Scope of the evidence |
| --- | --- | --- |
| pointer + offset + length | Array owner and Slice subrange | native C/LLVM result and actual retirement |
| graph adjacency pointers | immutable CSR adjacency borrowed as Slice; indexed heap and distance arrays | Dijkstra/A* test programs |
| intrusive links and parent pointers | indexed LRU prev/next arrays and Union-find parents | local indices inside one owner's lifetime; no externally retained stale index |
| flat hash storage | Robin Hood distances and backward-shift erase | fixed scalar table; not a claim about every table or a production hashmap API |
| many allocations with one phase lifetime | existing PgyRegion and test-only next-fit cursor | real C runtime, plain byte payloads; no new source annotation or automatic region selection |
| reusable identity + hot-loop access | existing manager generation and scoped pin | C manager-handle ABI; not source Slot<T>'s value/occupied ABI |

Runtime chain: `Array` storage -> `Slice` origin/range -> MIR cleanup and
C/LLVM emission -> runtime raw accesses -> allocator retirement. Region
blocks additionally have a reset/destroy owner; their payload addresses stay
stable during a phase. Manager handles additionally need a valid manager,
generation and lease. The benchmark never derives those facts from an address.

## What became easier, and what remains costly

The six fixed workloads exercise compiler-owned retirement without adding
own/ref annotations, restoration carriers, RC or GC. Their initialization uses
fixed array literals, which is more verbose than C calloc. This does not prove
that growing arrays or factory-returned storage have the same cleanup support.
The separate survey records their missing retirement and never times them as
successful ownership implementations.

The C/LLVM timings still exceed the corresponding direct C implementations.
Allocation observations identify repeated LLVM literal growth even when the
literal length is fixed. That is an observed backend cost, not an inherent
cost of the ownership model. Remaining access checks and runtime calls are
additional optimization candidates; this experiment does not attribute all
elapsed time to them or remove them without admitted extent/alias evidence.

The existing Region allocator's reset path reuses an older zeroed block once,
then skips it because `used != 0`, despite its remaining tail capacity. Source
inspection and repeated-phase measurements agree on that mechanism. The
test-only cursor instead consumes retained blocks sequentially and continues
bumping in the current phase block. One owner still destroys every block.
It preserves total_allocated accounting and leaves the production PgyRegion
ABI unchanged. This is a measured implementation candidate for this workload.

The runtime pin-once comparison demonstrates the value of checking a boundary
and then borrowing a pointer for a hot loop. Its lifecycle/control operations
are included in the measured program. It does not prove that arbitrary C code
can keep a raw pointer after unpin/reset/release, or that every source-level
View has complete static admission and final retirement today.

## Research checked against the implementation boundary

- [Tree Borrows, PLDI 2025](https://plf.inf.ethz.ch/research/pldi25-tree-borrows.html)
  is an operational aliasing model for unsafe Rust. Its proof is not a static
  allocator-selection algorithm or a proof of this compiler. It motivates
  preserving origin and access history rather than trusting an address.
- [Place Capability Graphs (2025)](https://arxiv.org/abs/2503.21691)
  concerns static capability reasoning. Existing typed owners should produce
  those facts; this trial does not introduce another compiler query system.
- [Polonius Alpha, 2026-08-04](https://blog.rust-lang.org/2026/08/04/enabling-polonius-alpha-on-nightly/)
  improves borrow analysis. The [2026-11-04 verified implementation preprint](https://arxiv.org/abs/2611.06319)
  proves a core Rust analysis under its stated model. Neither is evidence of
  a runtime speedup or a Pergyra end-to-end proof. No proof tool was installed.
- [bumpalo](https://github.com/fitzgen/bumpalo) and its
  [changelog](https://github.com/fitzgen/bumpalo/blob/main/CHANGELOG.md)
  distinguish bulk byte retirement from object destructors; its failure-path
  rewind work is relevant to phase retention. It is not a replacement for
  ownership or an excuse to defer observable resource cleanup.
- [mimalloc](https://github.com/microsoft/mimalloc) v3.6.0 and
  [TLSF](https://github.com/mattconte/tlsf) were actually built/run at pinned
  revisions. TLSF's bounded pool and explicit exhaustion are useful isolation
  properties; the scalar timing workload does not measure worst-case latency.
- [Swiss Tables](https://abseil.io/about/design/swisstables) uses grouped
  metadata/SIMD filtering. The current
  [unordered_dense documentation](https://github.com/martinus/unordered_dense)
  also discusses dense storage, reference stability and operation-specific
  tradeoffs. These are useful advanced comparators; the trial does not label
  scalar Robin Hood hashing the universal fastest table or implement either
  library's current grouped design.
- [slotmap](https://github.com/orlp/slotmap) motivates generation-checked
  reusable keys. The actual trial uses the repository's existing manager,
  not that library. Rust's [from_raw_parts contract](https://doc.rust-lang.org/std/slice/fn.from_raw_parts.html)
  separately requires valid allocation, range, initialization, lifetime and
  alias conditions; an integer address does not provide them.

## Follow-up boundaries

These are proposals, not additional V.001 completion conditions or an active
parallel compiler implementation track.

1. Reproduce the survey's growth/factory/outward-View retirement obligations
   on a newly pinned main executable. Repair existing typed-origin -> MIR
   cleanup production and its last emitter consumer; retain live-view growth
   and release refusals. Do not add manual drops as the default workaround.
2. For literal allocation cost, follow the admitted literal length through
   LLVM collection construction and its Array runtime ABI. Reuse that fact
   for allocation capacity rather than reconstructing it or weakening checks.
3. Any Region production change needs the existing owner/ABI decision, with
   reset, alignment, oversized allocations, accounting and destroy behavior
   checked beyond this fixed plain-byte case. The test cursor is not a silent
   replacement layout for PgyRegion.
4. Access-check elimination must consume validated extent/origin facts, with
   an invalid-origin/range falsifier. A new unchecked pointer route would
   bypass the mechanism being evaluated.

Installed self-host drivers, canonical full-input admission, gen2 == gen3,
required CI and a published V.001 commit were not validated by these trials.
Current main state remains owned by the main lane. Public arbitrary raw
pointer issue/load/store and whole-language memory safety remain outside this
bounded result; source Array/Slice accesses are the actual Pergyra path tested.

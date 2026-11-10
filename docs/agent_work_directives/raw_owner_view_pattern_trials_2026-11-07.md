# Raw access through ownership and View: bounded pattern trials

Status: IMPLEMENTATION COMPLETE (bounded test implementations and comparison).
Current receipt: [raw_owner_trials](../truth/state/raw_owner_trials.md).
No production compiler policy change.
The user reopened this comparison on 2026-11-07. Main retains V.001 integration.
Base: HEAD `60fd1e26d8ea3987efbcbfc65f59b892b234701a` plus preserved dirty work.
This lane owns only its new fixtures, benchmark, audit and directive. Main's
checker/flow owners and V.001 state are excluded. The experiment integrator is
this lane; one complete comparison receipt is its integration gate. Commands
are pinned native source emission, C/LLVM linking and bounded execution.

## Objective and boundary

- Objective: translate representative pointer idioms into existing owner/View
  authoring and measure convenience, correctness, allocation and runtime cost.
- Priority: equivalent work and actual cleanup; one ownership authority;
  supported source/ABI; honest unsupported cases; runtime and source ceremony.
- Owner: existing source/semantic backing facts, MIR cleanup and Array/Slice
  ABI. Allocators own byte placement, not borrow or release authority.
- Last consumers: C/LLVM access and retirement, allocator observations and
  the common benchmark sink. Region reset additionally ends a whole phase.
- Forbidden fallback: ownership from an address; public raw-pointer syntax
  invented by the experiment; manual drops counted as automatic; comparing a
  leaking program as fast; hidden copies/unchecked aliases or changed work.
- Gates: same checksum/work count, actual allocation/free and sanitizer
  checks, use-after-free/leak controls, existing source refusal outcomes,
  repeated release measurements with pinned source/executables and one CPU.

## Whole affected chain

Pergyra program -> existing native typed ownership facts -> MIR storage and
View origins -> cleanup synthesis -> C/LLVM raw loads/stores and free.
The raw C comparison performs the same work and has explicit cleanup. A
separate runtime-level arena/handle experiment is labeled as such; it is not
production Pergyra support. The chain is read before selecting any change to
an existing runtime owner. No new allocation mode or checker architecture is
adopted merely because a recent paper describes it.

## Complete comparison set

1. Fixed buffer and subrange: raw pointer offset versus owned Array plus Slice,
   with direct owned indexing as the compiler/control comparison.
2. Growth: grow before a View; grow with a live View as a refusal; a result
   escaping its local owner as a separate cleanup obligation.
3. Phase-local allocation: individually freed bytes versus the real chained
   PgyRegion allocator, including repeated reset, stable pointers and alignment.
4. Reusable references: existing generation/lease design versus persistent raw
   aliases; do not equate canonical value Slot ABI with the manager handle ABI.

The user then explicitly selected representative **algorithms and allocators
together**. Extend the executable set with LSD radix sort, Robin Hood hashing
with backward-shift deletion, indexed-heap Dijkstra, A*, path-halving/size
Union-find, and an index-linked LRU. Each gets a real Pergyra implementation,
an equivalent C implementation and an independent result oracle. Fixed
benchmark capacities are workload configuration, not compiler admission rules.
Allocator comparisons cover libc, the existing PgyRegion, and pinned primary
TLSF/mimalloc implementations when obtainable. External implementations remain
isolated experiment inputs, not production dependencies or new defaults.

For each case establish the complete success/refusal set before extending a
repair. Unproved source cases are recorded as gaps, not papered over with
manual cleanup or annotations. Production edits require a measured reached
owner, full consumer/ABI map and a falsifying case first.

The next-fit candidate must maintain the existing owner's total_allocated
accounting and reset/destroy obligations. Its two borrowed phase cursors live
only in the test wrapper; they do not reinterpret or enlarge production
PgyRegion. Outward Slice storage retention remains an existing positive;
missing final retirement is surveyed separately from premature retirement.

## Research mapping

Review primary sources for Tree Borrows (PLDI 2025), Place Capability Graphs
(2025), Polonius Alpha (2026) and its 2026-11-04 verified abstract-interpretation
preprint. These concern alias semantics or static analysis, not an allocator
speedup. Check current bumpalo implementation notes and generational stores
for allocator/reset and stable-reference patterns. Record dates and limits.

Native source/runtime/executable are frozen for this experiment. Existing
formal proofs supply their stated premises; no whole-compiler or arbitrary
public raw-pointer proof is inferred. Budgets remain 60 s static, 5 min per
focused backend shard, 30 min integration. Timing is separate from sanitizer
and allocation instrumentation. No stage, commit or push is authorized here.

## LLVM follow-up: bounded 1.3x target

The user requested the LLVM version as the next comparison and set at most
1.3x the direct C runtime as the target. Use the same six semantic workloads,
CPU, flags and pinned inputs. This is an experiment target per workload, not
a universal performance guarantee or permission to remove safety checks.

- Objective: isolate literal construction and out-of-line runtime costs;
  preserve results, checked indexing and compiler-owned retirement.
- Priority: safety and equivalent work, fixed identities, then <=1.3x on each
  workload. A failing workload stays open; averages do not hide it.
- Fact owner: the existing typed constant Array literal/count and Array ABI;
  runtime exports own checked mutation/allocation/drop. Optimizer facts must
  be derived from those definitions, not invented alias/nonnull assumptions.
- Last consumer: LLVM machine code and the actual free/allocation observer.
- Forbidden fallback: unchecked access, narrower inputs, leaked fast paths,
  new user annotations, manual drops, selective benchmark/compiler flags.
- Changes within the frozen experiment: constant scalar literal construction
  through an ABI-safe output-pointer shim using the existing typed allocator;
  separately compare original versus whole-module runtime visibility, then
  combine both. Give the direct C comparator the same whole-module option.
- Gates: independent 251-seed oracles, measured allocation/free, instrumented
  LLVM memory checks with a failing UAF control, negative read/write indices,
  and repeated interleaved timings. If a cost remains, inspect actual emitted
  instructions before extending the experiment. Main retains production edits.

The first factorial run leaves every LLVM case above target. LLVM remarks
identify missing caller target attributes, then inlining cost for checked
accessors. Refine only this boundary: copy the actual toolchain target facts,
retain the runtime bodies/guards and test optimization hints; expose constant
construction to the optimizer. Do not invent alias or nonnull facts.
The production `llvm_runtime_attrs.c` deliberately excludes guarded runtime
bodies because of historical panic/overflow miscompiles. This experiment uses
Clang on emitted IR and does not remove that policy. Any production adoption
must first replay its exact arithmetic/bounds refusals through that actual
compiler optimization path; a fast scratch artifact is insufficient.

The user's method review also includes direct owned indexing versus a Slice
on the same fixed buffer, actual release and negative accesses. Distinguish
this static Slice ABI (pointer and length) from dynamic manager/generation/pin
checks. Existing growing/returned-storage cleanup gaps remain explicit and
cannot be closed by a performance measurement.

Follow-up receipt: [LLVM cost and model review](../audits/llvm_owner_view_cost_and_model_review_2026-11-07.md).
The scratch O3 candidate passes the six algorithm and Slice performance targets;
the additional direct-owner buffer remains above the strict cutoff at 1.300606x.
O3 actual retirement and eight original panic diagnostics pass. Main's native
optimizer exclusions and installed-driver paths were not changed or declared
closed. The remaining performance result is recorded rather than rounded away.

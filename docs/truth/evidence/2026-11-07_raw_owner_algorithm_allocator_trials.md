# Raw owner/View algorithm and allocator run receipt

Frozen evidence from 2026-11-07 KST. Outcome: the bounded comparison is
complete, with explicit cleanup gaps. No production compiler/runtime change,
staging, commit, push, installed-driver validation or V.001 closure occurred.

## Input and executable identities

- Shared checkout: local main `60fd1e26d8ea3987efbcbfc65f59b892b234701a`
  plus preserved dirty work. The tested native executable is identified by
  bytes, not asserted to be a rebuild of every current dirty compiler file.
- Native emitter: `0ba9ac10dfb2b37ce61587bdd819478579b4107d161f71f711b7d8aaa4150aa3`.
- Runtime snapshot: `/root/pgy-oracle-rss-20261106/source`; all frozen runtime
  hashes were checked again at packaging. Both emitted C and LLVM use it.
- Inputs, generated artifacts and actual executable SHA-256 identities:
  `D:\PergyraLang\.tmp\raw-owner-patterns-20261107\delivery\artifacts.json`.
- Every group retains `inputs.json`, exact output, diagnostics and binaries
  under `/root/pgy-raw-owner-patterns-20261107/<group>/`.
- Final groups: `algorithms-final`, `allocators-final`, `phase-final`, `buffers-verified`, `slots-verified`, `existing-controls-final`.
- TLSF: `deff9ab509341f264addbd3c8ada533678591905`.
- mimalloc v3.6.0: `b18ab4eef383f655e760881b02eff8af02800879`;
  library SHA-256 `13781486ff1b980862809448d7697438b4508a78944cb989712c3f8867478fe3`.

## Method and validation limits

WSL2 x86_64, CPU 15, clang/opt 18.1.3, Python 3.12.3;
mimalloc Release was built with GCC 13.3. Release artifacts use `-O2 -g0
-fwrapv -fno-strict-aliasing -pthread`. Sanitizer work is separate from
seven-sample release timing. Median/min/max and every sample are retained.
Algorithm samples are interleaved with a fixed shuffle seed. This is a local
shared WSL machine, not a cross-machine benchmark or a universal ranking.

Each algorithm is checked at all 251 seed positions independently against
Python sorted/dict/heapq/BFS/OrderedDict, then run 503 times with allocation
observation and sanitizers. This is 4,518 per-seed result observations across
six algorithms and three implementations. The grid weight generator depends
on seed modulo 9: its 251 positions represent nine distinct weighted graphs,
not 251 independent graph topologies. Capacities and inputs are fixed.

Source C and runtime C use ASan/UBSan. Generated LLVM additionally receives
`sanitize_address` via LLVM forceattrs before clang ASan instrumentation.
UBSan arithmetic is not retroactively inserted into arbitrary emitted IR;
the tested Int arithmetic stays within bounded ranges. A direct LLVM
post-free load is caught. The same bare IR with ASan linkage alone was
not caught, demonstrating why linking a sanitizer runtime is insufficient.
See [LLVM forceattrs](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-18.1.3/llvm/lib/Transforms/IPO/ForceFunctionAttrs.cpp).

Custom allocator runs check results, allocation/retirement equality and TLSF
pool integrity. They do not prove ASan coverage of custom suballocations.
TLSF uses a fixed 16 MiB pool, a max_align_t-preserving realloc adapter and
assertions-enabled clang -O2. mimalloc uses its default Release configuration.
Pool preparation and final allocator collection are excluded from timing.
Requested-byte counters exclude that TLSF reservation and allocator internals.

Observed build warnings were retained: runtime ATOMIC_VAR_INIT deprecation,
LLVM target-triple override, and a GCC mimalloc atomic-store overflow warning
in its guarded/profile allocation code. These are not a warning-free build
or an adoption decision. Actual workloads and stated controls passed.

## Six Pergyra algorithms versus equivalent direct C

All six Pergyra programs have compiler-owned retirement, no manual drop and
no added own/ref annotations. C counterparts use explicit pointer retirement.
The graph uses borrowed Slice adjacency; LRU and Union-find use owner-local
indices. These are executable fixed workloads, not generic library APIs.

12,000 Work invocations per sample; elapsed milliseconds. A ratio above 1
means the Pergyra output took longer than corresponding direct C.

| Algorithm | Direct C | Pergyra C | Pergyra LLVM | C / direct | LLVM / direct |
| --- | ---: | ---: | ---: | ---: | ---: |
| radix_sort | 29.034 | 59.293 | 103.874 | 2.04 | 3.58 |
| robin_hood | 48.362 | 77.122 | 86.822 | 1.59 | 1.80 |
| dijkstra | 13.113 | 40.187 | 60.525 | 3.06 | 4.62 |
| astar | 12.908 | 37.538 | 61.556 | 2.91 | 4.77 |
| union_find | 54.031 | 71.903 | 84.770 | 1.33 | 1.57 |
| lru | 18.865 | 32.048 | 63.515 | 1.70 | 3.37 |

All 18 sanitizer runs have matching observed allocations/frees and zero
live blocks. Per Work, observed C reallocations are zero. LLVM literal
growth calls per Work are recorded below; no allocator leak is counted
as a performance win.

| Algorithm | Initial allocations / Work | C reallocations | LLVM reallocations |
| --- | ---: | ---: | ---: |
| radix_sort | 3 | 0 | 18 |
| robin_hood | 3 | 0 | 18 |
| dijkstra | 7 | 0 | 33 |
| astar | 7 | 0 | 33 |
| union_find | 2 | 0 | 12 |
| lru | 5 | 0 | 21 |

## Three real allocators on all six algorithms

All 54 implementation/allocator combinations match the independent result
and retire every observed allocation. Alignment, calloc zero/overflow and
realloc preservation controls pass. TLSF exhaustion and failed realloc
stay inside its pool; the old allocation remains valid, with no libc fallback.
Timings below use the same wrapper/adapter boundary for all allocators.

| Algorithm | Implementation | libc ms | TLSF ms | mimalloc ms |
| --- | --- | ---: | ---: | ---: |
| radix_sort | raw-c | 30.419 | 30.737 | 29.828 |
| radix_sort | c | 63.436 | 63.727 | 62.289 |
| radix_sort | llvm | 107.523 | 117.616 | 114.062 |
| robin_hood | raw-c | 49.055 | 51.552 | 49.822 |
| robin_hood | c | 77.877 | 81.837 | 77.746 |
| robin_hood | llvm | 93.609 | 100.844 | 96.872 |
| dijkstra | raw-c | 14.240 | 15.561 | 12.892 |
| dijkstra | c | 37.557 | 42.556 | 39.692 |
| dijkstra | llvm | 63.410 | 86.922 | 68.412 |
| astar | raw-c | 13.631 | 15.126 | 12.733 |
| astar | c | 38.797 | 41.369 | 38.407 |
| astar | llvm | 64.792 | 82.518 | 68.781 |
| union_find | raw-c | 56.644 | 54.405 | 53.891 |
| union_find | c | 72.827 | 74.573 | 71.165 |
| union_find | llvm | 91.702 | 90.163 | 87.454 |
| lru | raw-c | 19.587 | 20.851 | 18.314 |
| lru | c | 35.691 | 35.499 | 33.203 |
| lru | llvm | 62.293 | 80.453 | 68.408 |

## Repeated phase allocation and reset

64 phases, 4,096 simultaneous 48-byte payloads per phase, alignment 16.
Every payload is written/read and checked; pointers stay stable until
phase retirement. Same checksum 342255. Destroy is included in the phase
program timing. Retained bytes are before destroy, not RSS. All observed
blocks are actually freed at the end. The original Region remains unchanged.

| Configuration | Median ms | Min ms | Max ms | First blocks | Final blocks | Retained bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| libc | 4.320 | 4.166 | 7.688 | 0 | 0 | 0 |
| tlsf | 13.748 | 13.412 | 15.692 | 0 | 0 | 0 |
| mimalloc | 2.482 | 2.307 | 2.901 | 0 | 0 | 0 |
| region | 43.792 | 42.383 | 45.432 | 25 | 1262 | 10368592 |
| cursor-prototype | 2.023 | 1.829 | 2.140 | 25 | 25 | 205400 |

The test-only next-fit prototype takes 21.65x less elapsed time than
the original Region on this workload. Its owner still charges block
acquisition, maintains total_allocated, resets accounting and destroys
all blocks. The per-phase accounting assertion and sanitizer run pass.
No production Region ABI or compiler-selected lifetime was changed.

## Reusable manager handles and scoped pointer access

C runtime only, one million scalar read/update steps, checksum 498763.
Program lifecycle and negative controls are included; these numbers do
not isolate a per-load View overhead. Source Slot<T> is not reinterpreted.

| Configuration | Median ms | Min ms | Max ms | Allocations / frees |
| --- | ---: | ---: | ---: | ---: |
| raw-c | 1.424 | 1.259 | 1.612 | 1 / 1 |
| pin-once | 1.721 | 1.483 | 2.421 | 10 / 10 |
| checked-each-access | 72.216 | 58.246 | 97.494 | 10 / 10 |

Wrong generation, foreign manager, release/write during a lease, double
unpin and a handle retained across release/reclaim all refuse. Runtime
C ASan/UBSan and exact allocation/retirement observations pass. This does
not authorize retaining a C raw pointer after the lease ends.

## Buffer survey: missing retirement remains red

Fixed owner and fixed Slice each pass on C/LLVM: 257 allocations and
257 frees, zero live blocks, checksum 327585. The following admitted
cases return correct values but omit final free. They are excluded from
timing; all eight show real LeakSanitizer diagnostics in this final run.

| Case | Backend | Work invocations | Allocations | Frees | Live blocks | Requested live bytes |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| grow_then_view | c | 257 | 257 | 0 | 257 | 65792 |
| grow_then_view | llvm | 257 | 257 | 0 | 257 | 65792 |
| borrow_branch_then_growth | c | 1 | 1 | 0 | 1 | 24 |
| borrow_branch_then_growth | llvm | 1 | 1 | 0 | 1 | 16 |
| returned_owner | c | 1 | 1 | 0 | 1 | 12 |
| returned_owner | llvm | 1 | 1 | 0 | 1 | 16 |
| returned_view | c | 1 | 1 | 0 | 1 | 12 |
| returned_view | llvm | 1 | 1 | 0 | 1 | 16 |

A View deliberately returned with its storage is admitted, as required by
the existing premature-retirement positive. Its final retirement is
still missing in this fixed binary. It is not mislabeled as a compiler
escape-rejection failure. Last-use-before-growth in the same lexical
scope remains conservatively refused, and a live View used after growth
is refused at the intended Slice storage guard, on both backends.

Fixed buffer release timings: 200,000 Work invocations, milliseconds.

| Implementation | Median ms | Min ms | Max ms |
| --- | ---: | ---: | ---: |
| fixed_owner-c | 73.437 | 68.235 | 94.097 |
| fixed_owner-llvm | 93.748 | 85.467 | 252.726 |
| fixed_view-c | 71.764 | 67.325 | 187.116 |
| fixed_view-llvm | 123.236 | 119.191 | 242.975 |
| raw-c | 39.847 | 31.251 | 41.186 |

## Original controls and verification-path falsifiers

- Existing branch_exit unsafe(raw) program: C/LLVM output 75 then 100;
  both observe 2 allocations and 2 frees.
- Existing scalar_arrays: Int/Long/Float/Double/Bool output is checked;
  both observe 5 allocations and 5 frees.
- Existing returned_view_retains_storage is still admitted on C/LLVM.
- Existing release_before_read_invalid refuses ArrayDrop at the live
  Slice guard on both backends. Those source fixtures were not edited.
- Deliberate C leak, C post-free read and direct LLVM post-free load
  are caught. Leak control retires its worker stack before root inspection;
  counters also prevent a conservative LSan root from being mistaken for free.
- Bare LLVM IR with ASan linkage alone did not catch its post-free load;
  explicit ASan function instrumentation did. No production artifact was
  modified to bypass an ownership or backend refusal.

## Handoff and remaining boundary

Source programs, runners, audit and state are new/uncommitted files.
The shared production tree and existing tests were preserved. Patch
application is verified in an empty scratch directory, not staged here.
Local generated binaries, original logs and their hashes are retained.
The patch/receipt package is in
`D:\PergyraLang\.tmp\raw-owner-patterns-20261107\delivery`.

The [audit](../../audits/raw_owner_view_pattern_trials_2026-11-07.md)
maps researched patterns to admitted owners and names proposed follow-up
boundaries. These tests neither validate the latest installed driver nor
replace canonical/full-input, gen2 == gen3, required CI or a published
V.001 acceptance record. The broader language/raw proof is unchanged.

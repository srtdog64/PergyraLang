# Owner/View algorithms and allocator trials

These are executable fixed-workload experiments, not generic container APIs.
Run receipts and limitations are owned by
[`raw_owner_trials`](../../../docs/truth/state/raw_owner_trials.md).
The compiler/runtime under test is explicitly selected; the scripts do not
change production allocation or ownership policy.

| Pergyra program | Implemented algorithm | Independent result oracle |
| --- | --- | --- |
| `radix_sort.pgy` | two-pass base-256 LSD sort of 256 unsigned 16-bit keys | Python sorted |
| `robin_hood.pgy` | 256-slot Robin Hood table, 192 keys, backward-shift erase, hit/miss lookup | Python dict |
| `dijkstra.pgy` | CSR 8x8 weighted grid, indexed binary heap, decrease-key | Python heapq Dijkstra |
| `astar.pgy` | same graph, Manhattan consistent heuristic, indexed heap | independent Dijkstra result |
| `union_find.pgy` | 256 elements, 512 unions, union by size and path halving | connected-component BFS |
| `lru.pgy` | capacity 64, indexed doubly linked LRU, 512 requests | Python OrderedDict |

The `raw_*.c` programs perform the equivalent work using pointers and explicit
retirement. All six Pergyra programs use compiler-owned Array retirement.
Graph adjacency is borrowed through Slice. Counts and numeric domains are
workload inputs, not new compiler routes. This experiment does not rank all
algorithms or cover all input sizes, graph families or production APIs.

## Running

Use Linux/WSL with clang-18, opt-18, Python 3 and OpenSSL development headers.
Every output directory must be new. Each run freezes its fixture inputs,
records SHA-256 identities and writes logs; timed executions are uninstrumented.
The native compiler and runtime snapshot are required arguments, with no
installed-driver or current-worktree fallback.

```sh
python3 tests/raw_owner_pattern_trials.py \
  --native /absolute/path/to/native-pgy \
  --runtime-root /absolute/path/to/runtime-source-root \
  --output /absolute/path/to/new-algorithm-results --timing

python3 tests/raw_owner_buffer_trials.py \
  --baseline /absolute/path/to/new-algorithm-results \
  --output /absolute/path/to/new-buffer-results

python3 tests/raw_owner_slot_trials.py \
  --baseline /absolute/path/to/new-algorithm-results \
  --output /absolute/path/to/new-slot-results
```

The buffer command is a **survey**: its receipt separately records admitted
cases with missing retirement and excludes them from timing. A successful
survey does not mean that every buffer case is cleaned up. Returned Slice
storage retention is admitted; final retirement is a different obligation.

## Real allocator inputs

Dependencies remain outside the repository and are not installed. Prepare an
isolated directory containing these clean checkouts:

- `mimalloc/`: commit `b18ab4eef383f655e760881b02eff8af02800879`
  (v3.6.0, tag object `79cf666d6a78c3ed59dc9b77e171a29425aec6ba`).
- `tlsf/`: commit `deff9ab509341f264addbd3c8ada533678591905`.
- `mimalloc-build/libmimalloc.a`, built with the following configuration:

```sh
cmake -S /absolute/path/to/dependencies/mimalloc \
  -B /absolute/path/to/dependencies/mimalloc-build \
  -DCMAKE_BUILD_TYPE=Release -DMI_OVERRIDE=OFF -DMI_BUILD_SHARED=OFF \
  -DMI_BUILD_OBJECT=OFF -DMI_BUILD_TESTS=OFF
cmake --build /absolute/path/to/dependencies/mimalloc-build --parallel 2

python3 tests/raw_owner_allocator_trials.py \
  --baseline /absolute/path/to/new-algorithm-results \
  --dependencies /absolute/path/to/dependencies \
  --output /absolute/path/to/new-allocator-results
```

The observed build used GCC 13.3 for mimalloc; Pergyra/C artifacts used
clang 18.1.3. The runner checks dependency revisions and clean source, records
the actual library SHA, and never falls back to libc on TLSF exhaustion.
TLSF is compiled with clang -O2 and its assertions enabled; mimalloc uses its
Release CMake configuration. These are the measured configurations, not a
claim about either allocator's minimum possible cost.
TLSF has a preallocated 16 MiB pool and a max_align_t-preserving realloc
adapter. Initialization/final allocator collection is outside timed work.
Do not interpret its observed requested-byte counter as total reserved RSS.

`phase_allocator.c` additionally compares 64 phases of 4,096 live 48-byte
payloads using individual frees, existing PgyRegion, and a **test-only**
next-fit cursor. That prototype preserves owner accounting and the original
block/destroy ABI. It does not change the production PgyRegion, synthesize
source-level region selection, or skip finalizers for resource-bearing data.
Use `--phase-only` to run that separate shard without repeating algorithms.

`slot_view_runtime.c` exercises the existing C manager-handle ABI. It checks
generation, foreign-manager, pinned release/write, double-unpin and recycled
handle failures. Its pin-once loop uses a volatile borrowed pointer. This is
not a reinterpretation of the source `Slot<T>` value/occupied ABI.

## What the checks establish

Source C and runtime C use ASan/UBSan. Generated LLVM gets `sanitize_address`
through LLVM's forceattrs pass before clang instrumentation; a direct IR
post-free load must be caught. UBSan arithmetic checks are not retroactively
inserted into arbitrary emitted LLVM IR. The experiment's integer operations
stay within bounded Int ranges. Deliberate C leak/UAF controls and a direct
LLVM UAF control validate the observation path.

Custom allocator runs validate output, logical allocations/retirements and
TLSF integrity. They do not establish ASan coverage inside TLSF's pool or
mimalloc suballocations. Slot and phase C runs have their own sanitizer checks.
All timing uses one CPU, seven repetitions, fixed work and checked results.
These local WSL measurements are not universal performance guarantees or
whole-compiler/installed-driver ownership closure.

# Array member mutable-place and pixel storage closure audit — 2026-09-27

Base revision: `68a82abfe37e7c93c879baa0a6b0baae0cf73c45 == origin/main`.
The shared checkout was dirty, so the pre-fix executable was built in a clean
detached worktree at that exact revision. This document is evidence and
navigation, not a semantic or completion owner.

## Objective and result

The active P0 was the native C loss of caller-visible `Array<T>` descriptor
write-back for an effectful call such as:

```pergyra
ArrayPush(frame.buffer.values, Value(42));
```

The ordered-call emitter copied `frame.buffer.values` into a value temporary.
The push changed that copy's length/capacity, while the parent member retained
length zero. LLVM recursively formed the member lvalue and did not lose the
write-back.

The C owner now captures the exact addressable Array receiver before evaluating
later arguments and passes the dereferenced place to the existing Array
builtin emitter. Semantic admission rejects an Array mutator receiver that is
not rooted in a named binding. The admission change is Array-specific; it does
not impose a new mutator rule on `ListGet` or the other collection families.

## Independent correctness evidence

| Leg | Clean pre-fix `68a82abf` | Current patch |
|---|---|---|
| Alrescha native C | compile 0, run 1, `FAIL member ArrayPush lost length write-back` | compile 0, run 0, `MEMBER ARRAY PUSH PASS` |
| Alrescha native LLVM | compile 0, run 0, `MEMBER ARRAY PUSH PASS` | compile 0, run 0, `MEMBER ARRAY PUSH PASS` |
| One-level member | not an exact falsifier | fixed literal oracle checks length 1 and value 41 |
| Nested member | C lost caller-visible length | fixed literal oracle checks pushed value 42, later set value 43, and final length 1 |
| Argument order | no member-address ratchet | trace is exactly `1234` on C and LLVM |
| Temporary receiver | admitted before codegen | `PGY_SEM_BUILTIN_ARGS_INVALID`, exact named-binding diagnostic |

`tests/array_member_mutation_smoke.sh` also inspects emitted C for address
capture of both `buffer.values` and `frame.buffer.values` and rejects the old
value-descriptor temporary. C/LLVM each compare against a fixed stdout oracle;
backend agreement alone is not the oracle.

Observed supporting gates:

- `tests/array_member_mutation_smoke.sh`: PASS.
- `tests/array_drop_storage_smoke.sh`: PASS, including release and
  use-after-release refusals.
- `tests/runtime_panic_codegen_smoke.sh`: PASS, including collection
  out-of-bounds panic classes on generated C and LLVM.
- `tests/collection_parameter_boundary_smoke.sh`: PASS.
- `tests/collection_own_parameter_boundary_smoke.sh`: PASS.
- `make -j1 pgy` in MSYS2/UCRT64: PASS.

`tests/call_argument_evaluation_order_smoke.sh` passed its native C and LLVM
legs, then failed at `default-c`: the installed self-host route still evaluated
the first fixture right to left (`c,b,a`, `21`, `y,x`, `q,p`, `32`). This is
explicit OPEN self-host evidence, not a native P0 regression.

## Program-internal pixel storage benchmark

The same Pergyra program compares three pre-populated storage shapes. Every
timed kernel performs the same clear, indexed write/read, premultiplied-alpha
blend, and fixed checksum. Setup growth is outside timing. Each shape runs 3
warmups and 101 rotated-order samples over 4,096 pixels and 2,000 rounds. The
fixed checksum `31,662,080,000` is checked for every sample, not just the final
sample. p50/p95/p99 are sorted order statistics at indices 50/95/99.

The table reports milliseconds as `p50 / p95 / p99`.

| Backend | Compiler state | Flat `Array<Int>` RGBA | `Array<Pixel>` | Nested flat member |
|---|---|---:|---:|---:|
| C | clean pre-fix | `47 / 63 / 63` | `31 / 47 / 47` | `47 / 63 / 63` |
| C | current patch | `47 / 63 / 63` | `31 / 47 / 47` | `47 / 63 / 63` |
| LLVM | clean pre-fix, adjacent rerun | `141 / 157 / 172` | `78 / 94 / 94` | `141 / 172 / 172` |
| LLVM | current patch, adjacent rerun | `141 / 157 / 172` | `78 / 94 / 94` | `141 / 172 / 172` |

The first LLVM pair moved together under host load: pre-fix flat was
`109 / 125 / 141`, while the first current run was `156 / 172 / 172`; nominal
and nested moved in the same direction. Re-running the already-built pre-fix
and current executables adjacently produced the matching rows above. Therefore
the broad first-pair shift is treated as host drift, not patch causality.

The same-layout regression criterion is nested p95 no more than 135% of flat
p95, with flat p50 required to be at least 32 ms. It passed C (`63 <= 86`) and
LLVM (`172 <= 233`). This is a host-local manual performance gate, not a
cross-machine absolute budget.

Evidence limits are explicit:

- `timed_growth=0` is derived from the timed operation inventory: only indexed
  `ArraySet`/reads occur after pre-population.
- Allocation count is `UNMEASURED`; there is no public counter.
- Storage is Main-scoped and reused across samples; final cleanup/free is
  `UNMEASURED`.
- `Slot<Array<Int>>`/`PinWrite<Array<Int>>` is not an executable candidate:
  current C/LLVM runtime rows for claiming, reading, and writing Array-valued
  slots do not exist.

## Alrescha storage contract

Use a pre-populated, fixed-length `Array<Pixel>` as the CPU compositing hot-path
storage. `Pixel` is a concrete four-channel value, so one indexed operation
updates one pixel and the benchmark performs fewer checked collection calls
than four independent `Array<Int>` channel operations. Keep a flat interleaved
RGBA staging buffer only at the backend/upload boundary that actually requires
that representation. This follows the project rule: generic typing at the
semantic resource boundary, concrete representation in the hot loop.

Do not use repeated `ArrayPush` in the render loop. Establish length before
timing/rendering and use indexed updates. A nested owner such as
`PixelFrame.buffer.values` is now correct and showed no sustained overhead over
the same flat layout, but nesting is an ownership choice, not a performance
optimization.

## Remaining OPEN scope

- This closes the native Array member P0 only. It is not self-host
  `SUBSTITUTING` evidence.
- The same outer ordered-call sequencer can still value-copy nested receivers
  for List/Set/Queue operations, and Map has a separate inner sequencer. Those
  families need their own supported-shape owner and independent gates; they
  were not silently claimed by this Array fix.
- The self-host default C argument-order failure remains OPEN. The active
  self-host SoT registry row must not be marked CLOSED from this audit.

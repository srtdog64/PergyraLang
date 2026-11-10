# Frozen receipt: production owner/View optimization and installed blocker

Observed 2026-11-07 KST. HEAD `60fd1e26d8ea3987efbcbfc65f59b892b234701a`
plus the preserved shared dirty checkout (880 entries at this observation).
No staging, commit or push. This is native production adoption, not installed
C/LLVM performance closure, a self-host fixed point, or a published CI verdict.

## Fixed source and executable

- Frozen native validation tree: `/root/pgy-owner-production-20261107/candidate-r4`.
- Native executable SHA-256: `a219e386125c2c29f8e190679e1328af53f434e5f8ce19695f872a1cdef5c7a2`.
- Runtime bitcode SHA-256: `fc5b23f9c9a604baacebebfa32bd1b23a1ce864e85d58fa36dc4289fca6ba691`.
- Full snapshot manifest: `../candidate-r4-source.json`; SHA-256
  `bb2d2bf3948a727af1ca6928efb4a5309c57b5a81c65bad096b9ce26272a7a30`.
- Integration added main's five independently verified own-String native fixes;
  `main-integration-inputs.json` records their exact bytes. This lane changed no
  checker/flow/MIR ownership rule or self-host semantic owner.
- Handoff package: `D:\PergyraLang\.tmp\owner-production-20261107\handoff`.
  The patch is relative to the frozen dirty baseline, not clean HEAD. Its base
  and final per-file hashes are in `patch-inputs.json`. A fresh isolated replay
  passed `git apply --check` and produced every expected final byte.
- Patch SHA-256: `2427f5770e0aae9d3411690dcdfb636bb5197459786e53835c7cb71e4f29d6af`.

## Production changes and failure boundaries

Constant typed Int literals now allocate once and bulk-initialize independent
storage. Effectful/owning elements retain ordered construction. Private source
C functions retain their existing visibility so callers can propagate ranges.
LLVM emitted functions carry the actual target's CPU/features. Typed Array/Slice
access delegates failing checks to the existing external panic owner; only those
repaired bodies and storage retirement receive inlining preferences.

Target-compatible inlining exposed a latent source-aggregate versus Clang sret
mismatch in Array<String> Clone. A complete declaration census matched 611 runtime
names and found 90 ABI boundaries requiring external calls. The optimizer now
consumes only matching direct signatures; C aggregate lowering remains at the
existing physical call boundary. Clone passes both dev and release again.

Partial/runtime loop expansion is disabled; small full unrolls remain bounded.
The existing shared safe-divisor policy may consume exact LLVM i32 constants
after propagation. Zero, -1 and unknown divisors retain checked runtime calls.
No alias/range/non-null premise was invented. Runtime state, budget, capability,
checked arithmetic and other collection safety exclusions remain. Pass errors
now return a compiler error instead of being discarded.

LLVM's documented full/partial/runtime distinction is the optimization-policy
reference, not proof of speed: [LLVM loop metadata](https://llvm.org/docs/TransformMetadata.html).

## Actual native release timing

These are unchanged `.pgy` fixtures compiled through the actual native compiler
command, not rewritten generated C/IR. Clang/LLVM 18, CPU 15, nine shuffled
interleaved samples on a shared machine. Each ratio divides by the fastest median
of ordinary raw C, whole-module raw C and native-target whole-module raw C.
120,000 Work calls per algorithm; 200,000 per buffer; every checksum is checked.

| Fixed workload | C / direct C | LLVM / direct C |
|---|---:|---:|
| fixed_owner | 1.071501 | 1.249182 |
| fixed_view | 1.211872 | 1.270486 |
| radix_sort | 1.087659 | 1.118309 |
| robin_hood | 0.870349 | 0.854108 |
| dijkstra | 1.014109 | 0.999641 |
| astar | 1.047374 | 1.062761 |
| union_find | 0.963397 | 0.959641 |
| lru | 1.074382 | 1.066979 |

All 16 native rows meet 1.3 on this run. This is a local measurement, not a
whole-language or cross-machine performance bound. The earlier frozen compiler
was also run on both buffers and Dijkstra (three interleaved samples): C took
2.32-2.93x and LLVM 4.02-5.91x direct C. Those baseline results have their own
commands, binaries and matching comparator measurements.

## Observed verification

- Full existing `function_param_flow_summary_smoke.sh`: PASS, 307.344 seconds,
  including success/refusal, C/LLVM executions, recursion and demand budgets.
  The prior candidate's 300-second focused attempt timed out; it was not green.
  The final complete matrix ran at the repository's integration boundary.
- Existing native `runtime_panic_codegen_smoke.sh`: PASS on C and LLVM, retaining
  division/modulo zero, overflow, collection bounds and invariant panic classes.
- New behavior controls: 30 PASS across C/LLVM and dev/release, including two
  independent literals, growth, effect order, empty literal, Int edge, scalar
  views, external namespace export, Array<String> Clone, derived invalid
  divisors, and live-view growth/release refusals.
- User extern named like a panic returns normally; its following call executes
  on C/LLVM and its LLVM declaration has no runtime attributes. This is the
  native part's evidence, not the installed half of that existing gate.
- All eight workloads run through C and explicitly instrumented LLVM ASan;
  runtime/C UBSan is enabled. LLVM arithmetic is not retroactively all UBSan-
  instrumented. The 16 runs have 29,174 allocations and 29,174 frees, zero live
  blocks. Leak, C UAF and direct LLVM-load UAF falsifiers are detected.
- The memory harness first omitted the control's iteration environment and then
  discovered a missing copied `.ll` witness. Neither attempt was counted green;
  the corrected controls and unchanged 16 positive executions are retained.
- `collection_borrowed_descriptor_read.sh` stops before execution on three
  existing self-host source caps (129/115, 513/410, 231/222). No cap was raised
  and no pass is claimed for that gate.

## Exact installed boundary still open

The admitted `make self-host-compiler` workflow ran in the `integration` tree,
using its fixed native producer. Gen0 reached 0 errors / 17 warnings; original
call-argument controls passed 2 executions and 12 pre-emission refusals.
After 801.341 seconds, seed admission failed on the unchanged MIR-root fixture:

```
mir_collection_receiver_root.pgy
borrow_boundary_escape: node=93606
boundary: unproved_formal_indexed_read_entry
type: Array<String>
```

Main independently observed the same refusal and owns that semantic repair.
No seed gate was skipped and no direct native-built replacement was installed.
There is no new installed pair to test, so installed C/LLVM actual retirement,
performance and gen2 == gen3 remain unverified by this lane. The installed
emitter/build path is distinct from native optimization and must be measured;
these native ratios cannot be attributed to it.

After main's repair, build the admitted pair from the integrated source and run
`tests/raw_owner_driver_trials.py --route installed` for the same eight inputs,
plus `tests/owner_view_production_optimization_smoke.py --route installed`.
The runner pins both launcher and driver and explicitly disables native routing.
An admission or emission refusal is a failure, not permission for a native
substitution. Revisit the existing general emitter/build boundary if its cost
still exceeds the fixed target. Growing/returned-owner cleanup beyond the two
fixed buffers remains outside this performance claim.

# Frozen receipt: LLVM owner/View cost trials

Observed 2026-11-07 KST at HEAD
`60fd1e26d8ea3987efbcbfc65f59b892b234701a` plus preserved shared dirty work.
No production source, installed executable, commit or push changed.

## Identity and method

- Native emitter SHA-256:
  `0ba9ac10dfb2b37ce61587bdd819478579b4107d161f71f711b7d8aaa4150aa3`.
- Runtime: `/root/pgy-oracle-rss-20261106/source`.
- Original inputs: `/root/pgy-raw-owner-patterns-20261107/algorithms-final`
  and sibling `buffers-verified`. All 228 original manifest hashes were
  checked again after the experiment.
- Clang/LLVM 18.1.3; Linux x86_64; CPU 15; shared, non-exclusive machine.
  Common `-g0 -fwrapv -fno-strict-aliasing -pthread`; no fast-math or
  hand-supplied alias/non-null premises. Both release comparators use O3.
- Nine shuffled interleaved samples; 120,000 Work calls per algorithm and
  200,000 per buffer. Timing excludes process startup and instrumentation;
  every checksum is verified.
- Denominator is the smaller median of direct C compiled normally and with
  matched whole-module visibility. The latter materially improves the buffer;
  the threshold was not changed to hide that improvement.

## Final O3 medians

| Workload | Faster direct C ms | Scratch LLVM ms | Ratio | <=1.3 |
|---|---:|---:|---:|---|
| Radix sort | 323.138 | 274.099 | 0.848 | PASS |
| Robin Hood | 569.106 | 449.369 | 0.790 | PASS |
| Dijkstra | 150.424 | 158.695 | 1.055 | PASS |
| A* | 159.150 | 168.608 | 1.059 | PASS |
| Union-find | 588.353 | 444.539 | 0.756 | PASS |
| LRU | 216.925 | 224.282 | 1.034 | PASS |
| Direct owner buffer | 26.363 | 34.288 | 1.300606 | OPEN |
| Slice buffer | 26.582 | 30.582 | 1.150494 | PASS |

O2 remains recorded: the six algorithms reached 0.828-1.119x, while the extra
owner buffer was 1.326x and Slice 1.285x. O3 is the native LLVM release pass
level and was applied to both sides. The owner-buffer threshold is unproven,
including under measurement variation; a rounded 1.30 is not closure.

## Verification

- O3 compares 251 independent answers/prefix answers for three modes and
  eight workloads: 6,024 checks; 2,008 belong to the candidate.
- Final O3 observation totals 14,095 allocations and 14,095 frees, with zero
  reallocations and zero live blocks across all eight workloads.
- Explicitly instrumented LLVM ASan plus C/runtime UBSan checks pass all
  eight. UBSan does not retroactively instrument every originally emitted
  LLVM arithmetic operation; tested integer workloads are bounded.
- Exact sources from `tests/runtime_panic_codegen_smoke.sh` retain eight
  intended diagnostics at O2 and O3: division/modulo zero, signed division
  overflow, Array get/set, returned Array get, returned Slice get, Slice
  reslice out of bounds. Each SIGABRTs with its expected panic class.
- ASan catches a direct IR use-after-free. The unchanged pinned compiler
  refuses growth while a Slice remains live at the intended source guard.
- Sanitizer instrumentation runs once after linking. Early experimental
  harness attempts failed on attribute/metadata placement, used-list constness
  and double instrumentation. Their logs remain retained and are not passes
  or evidence of product failures.

## Artifacts and limits

Under `/root/pgy-raw-owner-patterns-20261107/`:

- `speed-diagnosis-llvm`: initial factorial comparison.
- `speed-diagnosis-llvm-final`: target/access experiment, including the
  subsequently corrected C-inline constructor boundary.
- `speed-diagnosis-llvm-aligned`: corrected LLVM allocation path.
- `speed-diagnosis-llvm-close-input`: runtime bodies with final retirement
  and registered cold-panic preferences.
- `speed-diagnosis-llvm-closed`: O2 results and `safety/` observations.
- `speed-diagnosis-llvm-release`: O3 results and `retirement/` observations.

Reproduction scripts and copied receipts:
`D:\PergyraLang\.tmp\raw-owner-speed-diagnosis-20261107\` and its
`llvm-receipts/` directory. Commands, all samples and byte identities are
retained; `artifacts.json` contains 490 artifact identities.
`close_llvm_trials.py` runs O2 and safety; `llvm_release_comparison.py` and
`verify_llvm_release_retirement.py` run O3 and actual retirement. Scripts need
fresh output directories.

Native/installed optimization exclusions, full input, gen2 == gen3, CI,
arbitrary raw-pointer safety and growing/returned-storage cleanup remain
outside this scratch proof. Adoption requires the actual driver paths.

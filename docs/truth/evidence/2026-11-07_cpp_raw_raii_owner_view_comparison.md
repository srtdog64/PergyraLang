# Frozen receipt: C++ raw/RAII and Pergyra owner/View comparison

Observed 2026-11-07 KST at HEAD
`60fd1e26d8ea3987efbcbfc65f59b892b234701a` plus preserved shared dirty work.
This is an isolated comparison, not a production compiler modification or
installed-driver result. No stage, commit or push was performed.

## Fixed input and method

- Reuses the unchanged scratch optimized LLVM executables from the
  [LLVM receipt](2026-11-07_llvm_owner_view_cost_trials.md). Original native
  emitter SHA-256 is
  `0ba9ac10dfb2b37ce61587bdd819478579b4107d161f71f711b7d8aaa4150aa3`.
  All 228 original compiler/runtime/fixture hashes were checked again.
- Seven semantic controls: six identical algorithms plus a fixed buffer.
  The same buffer control serves both Pergyra owner and Slice comparisons.
- C++20, Clang/LLVM 18.1.3, O3, Linux x86_64, CPU 15 on a shared machine.
  Common flags include `-g0 -fwrapv -fno-strict-aliasing -pthread`.
  C++ and the matched C comparison both receive whole-module visibility;
  the ordinary direct C executable is also timed.
- C++ raw preserves the direct C algorithm and libc allocation sizes.
  RAII wraps each allocation in `unique_ptr<int32_t[], FreeInts>` and removes
  explicit final frees. Dijkstra/A* borrowed buffers and the fixed-buffer
  subrange use `span<int32_t>`. This does not substitute different STL
  containers or algorithms, or add equivalent lifetime checks to C++.
- Seven shuffled interleaved timing repetitions; 120,000 Work calls per
  algorithm and 200,000 per buffer. In-process timing excludes startup and
  instrumentation. Every timed checksum is checked.

## Observed medians

Milliseconds; ratio divides Pergyra time by the faster C++ median.

| Workload | C++ raw | C++ RAII/span | Pergyra LLVM | Ratio |
|---|---:|---:|---:|---:|
| Radix sort | 310.178 | 312.207 | 271.742 | 0.876 |
| Robin Hood | 526.272 | 535.459 | 421.450 | 0.801 |
| Dijkstra | 142.074 | 141.597 | 150.834 | 1.065 |
| A* | 145.804 | 147.195 | 158.492 | 1.087 |
| Union-find | 576.533 | 569.116 | 418.224 | 0.735 |
| LRU | 211.517 | 202.780 | 203.544 | 1.004 |
| Direct owner buffer | 24.984 | 25.228 | 31.136 | 1.246 |
| Slice buffer | 24.984 | 25.228 | 27.459 | 1.099 |

All seven C++ raw/RAII pairs produced byte-identical release executables.
Their measured timing differences are therefore run variation, not different
ownership machinery. This is bounded evidence for these programs, not a
claim about arbitrary C++ code or a whole-language performance ranking.

The faster direct C buffer median in this run is 23.903 ms; Pergyra owner is
about 1.303x that baseline. The previous strict 1.3x C gate remains **OPEN**.
This comparison does not replace its denominator or close that gate.

## Verification and artifacts

- New C++ variants passed 3,514 independent answer/prefix-answer checks.
- ASan/UBSan runs at 503 Work calls observed 28,168 allocations and 28,168
  frees, zero live blocks and zero reallocations across the 14 C++ variants.
- Pergyra executables are unchanged from the prior receipt, including its
  cleanup and intended-panic evidence. This run reran their timed checksums;
  it did not rerun the installed-driver, canonical-input or bootstrap gates.
- Rust and Go were not benchmarked here.

WSL artifacts: `/root/pgy-raw-owner-patterns-20261107/cpp-owner-comparison/`.
Windows receipt copies:
`D:\PergyraLang\.tmp\raw-owner-speed-diagnosis-20261107\cpp-receipts\`.
The latter contains results, exact commands, input/artifact hashes and
compiler/work-count metadata. SHA-256:

- `results.json`:
  `2555f0370c0f576ac0dc643663ca9a67258e578a05bba76b730bd8d22ed7b9fc`
- `inputs.json`:
  `f38d34b8a2585874cfad0acf2de8d656e29a387b6c6de39a904ebb4c74eafb77`
- `artifacts.json`:
  `ceb2cbd5cf8c5e24838d19b09bb6093298b970f016a8e0e2788d5ffd83940528`
- Runner, `../compare_cpp.py` relative to that receipt-copy directory:
  `b6aa03c2aeddcf7827465f6cc586435a8f6a22707d943cd4d6285a2b1645eef4`

The compiler's production optimization exclusions remain unchanged. These
small fixed workloads support feasibility of comparable low-level speed;
they do not establish production consistency, arbitrary raw-pointer safety,
or completed ownership automation.
